#include "awget_bits.h"
#include "awgolomb.h"
#include "CdxHevc.h"

#define MAX_SPATIAL_SEGMENTATION 4096 // max. value of u(12) field
#define FINDMIN(a,b) ((a) > (b) ? (b) : (a))
#define FINDMAX(a,b) ((a) > (b) ? (a) : (b))
#define AWERROR(e) (-(e))

#define HEVC_AW_RB32(x)  ((((const cdx_uint8*)(x))[0] << 24) | \
                     (((const cdx_uint8*)(x))[1] << 16) | \
                     (((const cdx_uint8*)(x))[2] <<  8) | \
                      ((const cdx_uint8*)(x))[3])
#define HEVC_AW_RB24(x)                           \
    ((((const cdx_uint8*)(x))[0] << 16) |         \
     (((const cdx_uint8*)(x))[1] <<  8) |         \
      ((const cdx_uint8*)(x))[2])

typedef struct HVCCNALUnitArray {
    cdx_uint8  array_completeness;
    cdx_uint8  NAL_unit_type;
    uint16_t numNalus;
    uint16_t *nalUnitLength;
    cdx_uint8  **nalUnit;
} HVCCNALUnitArray;

typedef struct HEVCDecoderConfigurationRecord {
    cdx_uint8  configurationVersion;
    cdx_uint8  general_profile_space;
    cdx_uint8  general_tier_flag;
    cdx_uint8  general_profile_idc;
    cdx_uint32 general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    cdx_uint8  general_level_idc;
    uint16_t min_spatial_segmentation_idc;
    cdx_uint8  parallelismType;
    cdx_uint8  chromaFormat;
    cdx_uint8  bitDepthLumaMinus8;
    cdx_uint8  bitDepthChromaMinus8;
    uint16_t avgFrameRate;
    cdx_uint8  constantFrameRate;
    cdx_uint8  numTemporalLayers;
    cdx_uint8  temporalIdNested;
    cdx_uint8  lengthSizeMinusOne;
    cdx_uint8  numOfArrays;
    HVCCNALUnitArray *array;
} HEVCDecoderConfigurationRecord;

typedef struct HVCCProfileTierLevel {
    cdx_uint8  profile_space;
    cdx_uint8  tier_flag;
    cdx_uint8  profile_idc;
    cdx_uint32 profile_compatibility_flags;
    uint64_t constraint_indicator_flags;
    cdx_uint8  level_idc;
} HVCCProfileTierLevel;

static void hvccUpdatePtl(HEVCDecoderConfigurationRecord *hvcc,
                            HVCCProfileTierLevel *ptl)
{
    /*
     * The value of general_profile_space in all the parameter sets must be
     * identical.
     */
    hvcc->general_profile_space = ptl->profile_space;

    /*
     * The level indication general_level_idc must indicate a level of
     * capability equal to or greater than the highest level indicated for the
     * highest tier in all the parameter sets.
     */
    if (hvcc->general_tier_flag < ptl->tier_flag)
        hvcc->general_level_idc = ptl->level_idc;
    else
        hvcc->general_level_idc = FINDMAX(hvcc->general_level_idc, ptl->level_idc);

    /*
     * The tier indication general_tier_flag must indicate a tier equal to or
     * greater than the highest tier indicated in all the parameter sets.
     */
    hvcc->general_tier_flag = FINDMAX(hvcc->general_tier_flag, ptl->tier_flag);

    /*
     * The profile indication general_profile_idc must indicate a profile to
     * which the stream associated with this configuration record conforms.
     *
     * If the sequence parameter sets are marked with different profiles, then
     * the stream may need examination to determine which profile, if any, the
     * entire stream conforms to. If the entire stream is not examined, or the
     * examination reveals that there is no profile to which the entire stream
     * conforms, then the entire stream must be split into two or more
     * sub-streams with separate configuration records in which these rules can
     * be met.
     *
     * Note: set the profile to the highest value for the sake of simplicity.
     */
    hvcc->general_profile_idc = FINDMAX(hvcc->general_profile_idc, ptl->profile_idc);

    /*
     * Each bit in general_profile_compatibility_flags may only be set if all
     * the parameter sets set that bit.
     */
    hvcc->general_profile_compatibility_flags &= ptl->profile_compatibility_flags;

    /*
     * Each bit in general_constraint_indicator_flags may only be set if all
     * the parameter sets set that bit.
     */
    hvcc->general_constraint_indicator_flags &= ptl->constraint_indicator_flags;
}

static void hvccParsePtl(GetBitContext *gb,
                           HEVCDecoderConfigurationRecord *hvcc,
                           cdx_uint32 max_sub_layers_minus1)
{
    cdx_uint32 i;
    HVCCProfileTierLevel general_ptl;
    cdx_uint8 sub_layer_profile_present_flag[HEVC_MAX_NUM_SUB_LAYERS];
    cdx_uint8 sub_layer_level_present_flag[HEVC_MAX_NUM_SUB_LAYERS];

    general_ptl.profile_space               = get_bits(gb, 2);
    general_ptl.tier_flag                   = get_bits1(gb);
    general_ptl.profile_idc                 = get_bits(gb, 5);
    general_ptl.profile_compatibility_flags = get_bits_long(gb, 32);
    general_ptl.constraint_indicator_flags  = get_bits64(gb, 48);
    general_ptl.level_idc                   = get_bits(gb, 8);
    hvccUpdatePtl(hvcc, &general_ptl);

    for (i = 0; i < max_sub_layers_minus1; i++) {
        sub_layer_profile_present_flag[i] = get_bits1(gb);
        sub_layer_level_present_flag[i]   = get_bits1(gb);
    }

    if (max_sub_layers_minus1 > 0)
        for (i = max_sub_layers_minus1; i < 8; i++)
            skip_bits(gb, 2); // reserved_zero_2bits[i]

    for (i = 0; i < max_sub_layers_minus1; i++) {
        if (sub_layer_profile_present_flag[i]) {
            /*
             * sub_layer_profile_space[i]                     u(2)
             * sub_layer_tier_flag[i]                         u(1)
             * sub_layer_profile_idc[i]                       u(5)
             * sub_layer_profile_compatibility_flag[i][0..31] u(32)
             * sub_layer_progressive_source_flag[i]           u(1)
             * sub_layer_interlaced_source_flag[i]            u(1)
             * sub_layer_non_packed_constraint_flag[i]        u(1)
             * sub_layer_frame_only_constraint_flag[i]        u(1)
             * sub_layer_reserved_zero_44bits[i]              u(44)
             */
            skip_bits_long(gb, 32);
            skip_bits_long(gb, 32);
            skip_bits     (gb, 24);
        }

        if (sub_layer_level_present_flag[i])
            skip_bits(gb, 8);
    }
}

static void SkipSubLayerHrdParameters(GetBitContext *gb,
                                          cdx_uint32 cpb_cnt_minus1,
                                          cdx_uint8 sub_pic_hrd_params_present_flag)
{
    cdx_uint32 i;

    for (i = 0; i <= cpb_cnt_minus1; i++) {
        get_ue_golomb_long(gb); // bit_rate_value_minus1
        get_ue_golomb_long(gb); // cpb_size_value_minus1

        if (sub_pic_hrd_params_present_flag) {
            get_ue_golomb_long(gb); // cpb_size_du_value_minus1
            get_ue_golomb_long(gb); // bit_rate_du_value_minus1
        }

        skip_bits1(gb); // cbr_flag
    }
}

static int SkipHrdParameters(GetBitContext *gb, cdx_uint8 cprms_present_flag,
                                cdx_uint32 max_sub_layers_minus1)
{
    cdx_uint32 i;
    cdx_uint8 sub_pic_hrd_params_present_flag = 0;
    cdx_uint8 nal_hrd_parameters_present_flag = 0;
    cdx_uint8 vcl_hrd_parameters_present_flag = 0;

    if (cprms_present_flag) {
        nal_hrd_parameters_present_flag = get_bits1(gb);
        vcl_hrd_parameters_present_flag = get_bits1(gb);

        if (nal_hrd_parameters_present_flag ||
            vcl_hrd_parameters_present_flag) {
            sub_pic_hrd_params_present_flag = get_bits1(gb);

            if (sub_pic_hrd_params_present_flag)
                /*
                 * tick_divisor_minus2                          u(8)
                 * du_cpb_removal_delay_increment_length_minus1 u(5)
                 * sub_pic_cpb_params_in_pic_timing_sei_flag    u(1)
                 * dpb_output_delay_du_length_minus1            u(5)
                 */
                skip_bits(gb, 19);

            /*
             * bit_rate_scale u(4)
             * cpb_size_scale u(4)
             */
            skip_bits(gb, 8);

            if (sub_pic_hrd_params_present_flag)
                skip_bits(gb, 4); // cpb_size_du_scale

            /*
             * initial_cpb_removal_delay_length_minus1 u(5)
             * au_cpb_removal_delay_length_minus1      u(5)
             * dpb_output_delay_length_minus1          u(5)
             */
            skip_bits(gb, 15);
        }
    }

    for (i = 0; i <= max_sub_layers_minus1; i++) {
        cdx_uint32 cpb_cnt_minus1            = 0;
        cdx_uint8 low_delay_hrd_flag             = 0;
        cdx_uint8 fixed_pic_rate_within_cvs_flag = 0;
        cdx_uint8 fixed_pic_rate_general_flag    = get_bits1(gb);

        if (!fixed_pic_rate_general_flag)
            fixed_pic_rate_within_cvs_flag = get_bits1(gb);

        if (fixed_pic_rate_within_cvs_flag)
            get_ue_golomb_long(gb); // elemental_duration_in_tc_minus1
        else
            low_delay_hrd_flag = get_bits1(gb);

        if (!low_delay_hrd_flag) {
            cpb_cnt_minus1 = get_ue_golomb_long(gb);
            if (cpb_cnt_minus1 > 31)
                return -2;
        }

        if (nal_hrd_parameters_present_flag)
            SkipSubLayerHrdParameters(gb, cpb_cnt_minus1,
                                          sub_pic_hrd_params_present_flag);

        if (vcl_hrd_parameters_present_flag)
            SkipSubLayerHrdParameters(gb, cpb_cnt_minus1,
                                          sub_pic_hrd_params_present_flag);
    }

    return 0;
}

static void SkipTimingInfo(GetBitContext *gb)
{
    skip_bits_long(gb, 32); // num_units_in_tick
    skip_bits_long(gb, 32); // time_scale

    if (get_bits1(gb))          // poc_proportional_to_timing_flag
        get_ue_golomb_long(gb); // num_ticks_poc_diff_one_minus1
}

static void hvccParseVui(GetBitContext *gb,
                           HEVCDecoderConfigurationRecord *hvcc,
                           cdx_uint32 max_sub_layers_minus1)
{
    cdx_uint32 min_spatial_segmentation_idc;

    if (get_bits1(gb))              // aspect_ratio_info_present_flag
        if (get_bits(gb, 8) == 255) // aspect_ratio_idc
            skip_bits_long(gb, 32); // sar_width u(16), sar_height u(16)

    if (get_bits1(gb))  // overscan_info_present_flag
        skip_bits1(gb); // overscan_appropriate_flag

    if (get_bits1(gb)) {  // video_signal_type_present_flag
        skip_bits(gb, 4); // video_format u(3), video_full_range_flag u(1)

        if (get_bits1(gb)) // colour_description_present_flag
            /*
             * colour_primaries         u(8)
             * transfer_characteristics u(8)
             * matrix_coeffs            u(8)
             */
            skip_bits(gb, 24);
    }

    if (get_bits1(gb)) {        // chroma_loc_info_present_flag
        get_ue_golomb_long(gb); // chroma_sample_loc_type_top_field
        get_ue_golomb_long(gb); // chroma_sample_loc_type_bottom_field
    }

    /*
     * neutral_chroma_indication_flag u(1)
     * field_seq_flag                 u(1)
     * frame_field_info_present_flag  u(1)
     */
    skip_bits(gb, 3);

    if (get_bits1(gb)) {        // default_display_window_flag
        get_ue_golomb_long(gb); // def_disp_win_left_offset
        get_ue_golomb_long(gb); // def_disp_win_right_offset
        get_ue_golomb_long(gb); // def_disp_win_top_offset
        get_ue_golomb_long(gb); // def_disp_win_bottom_offset
    }

    if (get_bits1(gb)) { // vui_timing_info_present_flag
        SkipTimingInfo(gb);

        if (get_bits1(gb)) // vui_hrd_parameters_present_flag
            SkipHrdParameters(gb, 1, max_sub_layers_minus1);
    }

    if (get_bits1(gb)) { // bitstream_restriction_flag
        /*
         * tiles_fixed_structure_flag              u(1)
         * motion_vectors_over_pic_boundaries_flag u(1)
         * restricted_ref_pic_lists_flag           u(1)
         */
        skip_bits(gb, 3);

        min_spatial_segmentation_idc = get_ue_golomb_long(gb);

        /*
         * cdx_uint32(12) min_spatial_segmentation_idc;
         *
         * The min_spatial_segmentation_idc indication must indicate a level of
         * spatial segmentation equal to or less than the lowest level of
         * spatial segmentation indicated in all the parameter sets.
         */
        hvcc->min_spatial_segmentation_idc = FINDMIN(hvcc->min_spatial_segmentation_idc,
                                                   min_spatial_segmentation_idc);

        get_ue_golomb_long(gb); // max_bytes_per_pic_denom
        get_ue_golomb_long(gb); // max_bits_per_min_cu_denom
        get_ue_golomb_long(gb); // log2_max_mv_length_horizontal
        get_ue_golomb_long(gb); // log2_max_mv_length_vertical
    }
}

static void SkipSubLayerOrderingInfo(GetBitContext *gb)
{
    get_ue_golomb_long(gb); // max_dec_pic_buffering_minus1
    get_ue_golomb_long(gb); // max_num_reorder_pics
    get_ue_golomb_long(gb); // max_latency_increase_plus1
}

static int hvccParseVps(GetBitContext *gb,
                          HEVCDecoderConfigurationRecord *hvcc)
{
    cdx_uint32 vps_max_sub_layers_minus1;

    /*
     * vps_video_parameter_set_id u(4)
     * vps_reserved_three_2bits   u(2)
     * vps_max_layers_minus1      u(6)
     */
    skip_bits(gb, 12);

    vps_max_sub_layers_minus1 = get_bits(gb, 3);

    /*
     * numTemporalLayers greater than 1 indicates that the stream to which this
     * configuration record applies is temporally scalable and the contained
     * number of temporal layers (also referred to as temporal sub-layer or
     * sub-layer in ISO/IEC 23008-2) is equal to numTemporalLayers. Value 1
     * indicates that the stream is not temporally scalable. Value 0 indicates
     * that it is unknown whether the stream is temporally scalable.
     */
    hvcc->numTemporalLayers = FINDMAX(hvcc->numTemporalLayers,
                                    vps_max_sub_layers_minus1 + 1);

    /*
     * vps_temporal_id_nesting_flag u(1)
     * vps_reserved_0xffff_16bits   u(16)
     */
    skip_bits(gb, 17);

    hvccParsePtl(gb, hvcc, vps_max_sub_layers_minus1);

    /* nothing useful for hvcC past this point */
    return 0;
}

static void SkipScalingListData(GetBitContext *gb)
{
    int i, j, k, num_coeffs;

    for (i = 0; i < 4; i++)
        for (j = 0; j < (i == 3 ? 2 : 6); j++)
            if (!get_bits1(gb))         // scaling_list_pred_mode_flag[i][j]
                get_ue_golomb_long(gb); // scaling_list_pred_matrix_id_delta[i][j]
            else {
                num_coeffs = FINDMIN(64, 1 << (4 + (i << 1)));

                if (i > 1)
                    get_se_golomb_long(gb); // scaling_list_dc_coef_minus8[i-2][j]

                for (k = 0; k < num_coeffs; k++)
                    get_se_golomb_long(gb); // scaling_list_delta_coef
            }
}

static int ParseRps(GetBitContext *gb, cdx_uint32 rps_idx,
                     cdx_uint32 num_rps,
                     cdx_uint32 num_delta_pocs[HEVC_MAX_NUM_SHORT_TERM_RPS_COUNT])
{
    cdx_uint32 i;

    if (rps_idx && get_bits1(gb)) { // inter_ref_pic_set_prediction_flag
        /* this should only happen for slice headers, and this isn't one */
        if (rps_idx >= num_rps)
            return -2;

        skip_bits1        (gb); // delta_rps_sign
        get_ue_golomb_long(gb); // abs_delta_rps_minus1

        num_delta_pocs[rps_idx] = 0;

        /*
         * From libavcodec/mux_ps.c:
         *
         * if (is_slice_header) {
         *    //foo
         * } else
         *     rps_ridx = &sps->st_rps[rps - sps->st_rps - 1];
         *
         * where:
         * rps:             &sps->st_rps[rps_idx]
         * sps->st_rps:     &sps->st_rps[0]
         * is_slice_header: rps_idx == num_rps
         *
         * thus:
         * if (num_rps != rps_idx)
         *     rps_ridx = &sps->st_rps[rps_idx - 1];
         *
         * NumDeltaPocs[RefRpsIdx]: num_delta_pocs[rps_idx - 1]
         */
        for (i = 0; i <= num_delta_pocs[rps_idx - 1]; i++) {
            cdx_uint8 use_delta_flag = 0;
            cdx_uint8 used_by_curr_pic_flag = get_bits1(gb);
            if (!used_by_curr_pic_flag)
                use_delta_flag = get_bits1(gb);

            if (used_by_curr_pic_flag || use_delta_flag)
                num_delta_pocs[rps_idx]++;
        }
    } else {
        cdx_uint32 num_negative_pics = get_ue_golomb_long(gb);
        cdx_uint32 num_positive_pics = get_ue_golomb_long(gb);

        if ((num_positive_pics + (uint64_t)num_negative_pics) * 2 > get_bits_left(gb))
            return -2;

        num_delta_pocs[rps_idx] = num_negative_pics + num_positive_pics;

        for (i = 0; i < num_negative_pics; i++) {
            get_ue_golomb_long(gb); // delta_poc_s0_minus1[rps_idx]
            skip_bits1        (gb); // used_by_curr_pic_s0_flag[rps_idx]
        }

        for (i = 0; i < num_positive_pics; i++) {
            get_ue_golomb_long(gb); // delta_poc_s1_minus1[rps_idx]
            skip_bits1        (gb); // used_by_curr_pic_s1_flag[rps_idx]
        }
    }

    return 0;
}

static int hvccParseSps(GetBitContext *gb,
                          HEVCDecoderConfigurationRecord *hvcc)
{
    cdx_uint32 i, sps_max_sub_layers_minus1, log2_max_pic_order_cnt_lsb_minus4;
    cdx_uint32 num_short_term_ref_pic_sets, num_delta_pocs[HEVC_MAX_NUM_SHORT_TERM_RPS_COUNT];

    skip_bits(gb, 4); // sps_video_parameter_set_id

    sps_max_sub_layers_minus1 = get_bits (gb, 3);

    /*
     * numTemporalLayers greater than 1 indicates that the stream to which this
     * configuration record applies is temporally scalable and the contained
     * number of temporal layers (also referred to as temporal sub-layer or
     * sub-layer in ISO/IEC 23008-2) is equal to numTemporalLayers. Value 1
     * indicates that the stream is not temporally scalable. Value 0 indicates
     * that it is unknown whether the stream is temporally scalable.
     */
    hvcc->numTemporalLayers = FINDMAX(hvcc->numTemporalLayers,
                                    sps_max_sub_layers_minus1 + 1);

    hvcc->temporalIdNested = get_bits1(gb);

    hvccParsePtl(gb, hvcc, sps_max_sub_layers_minus1);

    get_ue_golomb_long(gb); // sps_seq_parameter_set_id

    hvcc->chromaFormat = get_ue_golomb_long(gb);

    if (hvcc->chromaFormat == 3)
        skip_bits1(gb); // separate_colour_plane_flag

    get_ue_golomb_long(gb); // pic_width_in_luma_samples
    get_ue_golomb_long(gb); // pic_height_in_luma_samples

    if (get_bits1(gb)) {        // conformance_window_flag
        get_ue_golomb_long(gb); // conf_win_left_offset
        get_ue_golomb_long(gb); // conf_win_right_offset
        get_ue_golomb_long(gb); // conf_win_top_offset
        get_ue_golomb_long(gb); // conf_win_bottom_offset
    }

    hvcc->bitDepthLumaMinus8          = get_ue_golomb_long(gb);
    hvcc->bitDepthChromaMinus8        = get_ue_golomb_long(gb);
    log2_max_pic_order_cnt_lsb_minus4 = get_ue_golomb_long(gb);

    /* sps_sub_layer_ordering_info_present_flag */
    i = get_bits1(gb) ? 0 : sps_max_sub_layers_minus1;
    for (; i <= sps_max_sub_layers_minus1; i++)
        SkipSubLayerOrderingInfo(gb);

    get_ue_golomb_long(gb); // log2_min_luma_coding_block_size_minus3
    get_ue_golomb_long(gb); // log2_diff_max_min_luma_coding_block_size
    get_ue_golomb_long(gb); // log2_min_transform_block_size_minus2
    get_ue_golomb_long(gb); // log2_diff_max_min_transform_block_size
    get_ue_golomb_long(gb); // max_transform_hierarchy_depth_inter
    get_ue_golomb_long(gb); // max_transform_hierarchy_depth_intra

    if (get_bits1(gb) && // scaling_list_enabled_flag
        get_bits1(gb))   // sps_scaling_list_data_present_flag
        SkipScalingListData(gb);

    skip_bits1(gb); // amp_enabled_flag
    skip_bits1(gb); // sample_adaptive_offset_enabled_flag

    if (get_bits1(gb)) {           // pcm_enabled_flag
        skip_bits         (gb, 4); // pcm_sample_bit_depth_luma_minus1
        skip_bits         (gb, 4); // pcm_sample_bit_depth_chroma_minus1
        get_ue_golomb_long(gb);    // log2_min_pcm_luma_coding_block_size_minus3
        get_ue_golomb_long(gb);    // log2_diff_max_min_pcm_luma_coding_block_size
        skip_bits1        (gb);    // pcm_loop_filter_disabled_flag
    }

    num_short_term_ref_pic_sets = get_ue_golomb_long(gb);
    if (num_short_term_ref_pic_sets > HEVC_MAX_NUM_SHORT_TERM_RPS_COUNT)
        return -2;

    for (i = 0; i < num_short_term_ref_pic_sets; i++) {
        int ret = ParseRps(gb, i, num_short_term_ref_pic_sets, num_delta_pocs);
        if (ret < 0)
            return ret;
    }

    if (get_bits1(gb)) {                               // long_term_ref_pics_present_flag
        unsigned num_long_term_ref_pics_sps = get_ue_golomb_long(gb);
        if (num_long_term_ref_pics_sps > 31U)
            return -2;
        for (i = 0; i < num_long_term_ref_pics_sps; i++) { // num_long_term_ref_pics_sps
            int len = FINDMIN(log2_max_pic_order_cnt_lsb_minus4 + 4, 16);
            skip_bits (gb, len); // lt_ref_pic_poc_lsb_sps[i]
            skip_bits1(gb);      // used_by_curr_pic_lt_sps_flag[i]
        }
    }

    skip_bits1(gb); // sps_temporal_mvp_enabled_flag
    skip_bits1(gb); // strong_intra_smoothing_enabled_flag

    if (get_bits1(gb)) // vui_parameters_present_flag
        hvccParseVui(gb, hvcc, sps_max_sub_layers_minus1);

    /* nothing useful for hvcC past this point */
    return 0;
}

static int hvccParsePps(GetBitContext *gb,
                          HEVCDecoderConfigurationRecord *hvcc)
{
    cdx_uint8 tiles_enabled_flag, entropy_coding_sync_enabled_flag;

    get_ue_golomb_long(gb); // pps_pic_parameter_set_id
    get_ue_golomb_long(gb); // pps_seq_parameter_set_id

    /*
     * dependent_slice_segments_enabled_flag u(1)
     * output_flag_present_flag              u(1)
     * num_extra_slice_header_bits           u(3)
     * sign_data_hiding_enabled_flag         u(1)
     * cabac_init_present_flag               u(1)
     */
    skip_bits(gb, 7);

    get_ue_golomb_long(gb); // num_ref_idx_l0_default_active_minus1
    get_ue_golomb_long(gb); // num_ref_idx_l1_default_active_minus1
    get_se_golomb_long(gb); // init_qp_minus26

    /*
     * constrained_intra_pred_flag u(1)
     * transform_skip_enabled_flag u(1)
     */
    skip_bits(gb, 2);

    if (get_bits1(gb))          // cu_qp_delta_enabled_flag
        get_ue_golomb_long(gb); // diff_cu_qp_delta_depth

    get_se_golomb_long(gb); // pps_cb_qp_offset
    get_se_golomb_long(gb); // pps_cr_qp_offset

    /*
     * pps_slice_chroma_qp_offsets_present_flag u(1)
     * weighted_pred_flag               u(1)
     * weighted_bipred_flag             u(1)
     * transquant_bypass_enabled_flag   u(1)
     */
    skip_bits(gb, 4);

    tiles_enabled_flag               = get_bits1(gb);
    entropy_coding_sync_enabled_flag = get_bits1(gb);

    if (entropy_coding_sync_enabled_flag && tiles_enabled_flag)
        hvcc->parallelismType = 0; // mixed-type parallel decoding
    else if (entropy_coding_sync_enabled_flag)
        hvcc->parallelismType = 3; // wavefront-based parallel decoding
    else if (tiles_enabled_flag)
        hvcc->parallelismType = 2; // tile-based parallel decoding
    else
        hvcc->parallelismType = 1; // slice-based parallel decoding

    /* nothing useful for hvcC past this point */
    return 0;
}

static cdx_uint8 *NalUnitExtractRbsp(const cdx_uint8 *src, cdx_uint32 src_len,
                                      cdx_uint32 *dst_len)
{
    cdx_uint8 *dst;
    cdx_uint32 i, len;

    dst = mux_malloc(src_len + AW_INPUT_BUFFER_PADDING_SIZE);
    if (!dst)
        return NULL;

    /* NAL unit header (2 bytes) */
    i = len = 0;
    while (i < 2 && i < src_len)
        dst[len++] = src[i++];

    while (i + 2 < src_len)
        if (!src[i] && !src[i + 1] && src[i + 2] == 3) {
            dst[len++] = src[i++];
            dst[len++] = src[i++];
            i++; // remove emulation_prevention_three_byte
        } else
            dst[len++] = src[i++];

    while (i < src_len)
        dst[len++] = src[i++];

    *dst_len = len;
    return dst;
}



static void NalUnitParseHeader(GetBitContext *gb, cdx_uint8 *nal_type)
{
    skip_bits1(gb); // forbidden_zero_bit

    *nal_type = get_bits(gb, 6);

    /*
     * nuh_layer_id          u(6)
     * nuh_temporal_id_plus1 u(3)
     */
    skip_bits(gb, 9);
}

static int HvccArrayAddNalUnit(cdx_uint8 *nal_buf, cdx_uint32 nal_size,
                                   cdx_uint8 nal_type, int PsArrayCompleteness,
                                   HEVCDecoderConfigurationRecord *hvcc)
{
    int ret;
    cdx_uint8 index;
    uint16_t numNalus;
    HVCCNALUnitArray *array;

    for (index = 0; index < hvcc->numOfArrays; index++)
        if (hvcc->array[index].NAL_unit_type == nal_type)
            break;

    if (index >= hvcc->numOfArrays) {
        cdx_uint8 i;

        ret = mux_reallocp_array(&hvcc->array, index + 1, sizeof(HVCCNALUnitArray));
        if (ret < 0)
            return ret;

        for (i = hvcc->numOfArrays; i <= index; i++)
            memset(&hvcc->array[i], 0, sizeof(HVCCNALUnitArray));
        hvcc->numOfArrays = index + 1;
    }

    array    = &hvcc->array[index];
    numNalus = array->numNalus;

    ret = mux_reallocp_array(&array->nalUnit, numNalus + 1, sizeof(cdx_uint8*));
    if (ret < 0)
        return ret;

    ret = mux_reallocp_array(&array->nalUnitLength, numNalus + 1, sizeof(uint16_t));
    if (ret < 0)
        return ret;

    array->nalUnit      [numNalus] = nal_buf;
    array->nalUnitLength[numNalus] = nal_size;
    array->NAL_unit_type           = nal_type;
    array->numNalus++;

    /*
     * When the sample entry name is ‘hvc1’, the default and mandatory value of
     * array_completeness is 1 for arrays of all types of parameter sets, and 0
     * for all other arrays. When the sample entry name is ‘hev1’, the default
     * value of array_completeness is 0 for all arrays.
     */
    if (nal_type == HEVC_NAL_TYPE_VPS || nal_type == HEVC_NAL_TYPE_SPS || nal_type == HEVC_NAL_TYPE_PPS)
        array->array_completeness = PsArrayCompleteness;

    return 0;
}

static int HvccAddNalUnit(cdx_uint8 *nal_buf, cdx_uint32 nal_size,
                             int PsArrayCompleteness,
                             HEVCDecoderConfigurationRecord *hvcc)
{
    int ret = 0;
    GetBitContext gbc;
    cdx_uint8 nal_type;
    cdx_uint8 *rbsp_buf;
    cdx_uint32 rbsp_size;

    rbsp_buf = NalUnitExtractRbsp(nal_buf, nal_size, &rbsp_size);
    if (!rbsp_buf) {
        ret = AWERROR(ENOMEM);
        goto end;
    }

    ret = init_get_bits8(&gbc, rbsp_buf, rbsp_size);
    if (ret < 0)
        goto end;

    NalUnitParseHeader(&gbc, &nal_type);

    /*
     * Note: only 'declarative' SEI messages are allowed in
     * hvcC. Perhaps the SEI playload type should be checked
     * and non-declarative SEI messages discarded?
     */
    switch (nal_type) {
    case HEVC_NAL_TYPE_VPS:
    case HEVC_NAL_TYPE_SPS:
    case HEVC_NAL_TYPE_PPS:
    case HEVC_NAL_TYPE_SEI_PREFIX:
    case HEVC_NAL_TYPE_SEI_SUFFIX:
        ret = HvccArrayAddNalUnit(nal_buf, nal_size, nal_type,
                                      PsArrayCompleteness, hvcc);
        if (ret < 0)
            goto end;
        else if (nal_type == HEVC_NAL_TYPE_VPS)
            ret = hvccParseVps(&gbc, hvcc);
        else if (nal_type == HEVC_NAL_TYPE_SPS)
            ret = hvccParseSps(&gbc, hvcc);
        else if (nal_type == HEVC_NAL_TYPE_PPS)
            ret = hvccParsePps(&gbc, hvcc);
        if (ret < 0)
            goto end;
        break;
    default:
        ret = -2;
        goto end;
    }

end:
    mux_free(rbsp_buf);
    return ret;
}

static void hvccInit(HEVCDecoderConfigurationRecord *hvcc)
{
    memset(hvcc, 0, sizeof(HEVCDecoderConfigurationRecord));
    hvcc->configurationVersion = 1;
    hvcc->lengthSizeMinusOne   = 3; // 4 bytes

    /*
     * The following fields have all their valid bits set by default,
     * the ProfileTierLevel parsing code will unset them when needed.
     */
    hvcc->general_profile_compatibility_flags = 0xffffffff;
    hvcc->general_constraint_indicator_flags  = 0xffffffffffff;

    /*
     * Initialize this field with an invalid value which can be used to detect
     * whether we didn't see any VUI (in which case it should be reset to zero).
     */
    hvcc->min_spatial_segmentation_idc = MAX_SPATIAL_SEGMENTATION + 1;
}

static void hvccClose(HEVCDecoderConfigurationRecord *hvcc)
{
    cdx_uint8 i;

    for (i = 0; i < hvcc->numOfArrays; i++) {
        hvcc->array[i].numNalus = 0;
        mux_freep(&hvcc->array[i].nalUnit);
        mux_freep(&hvcc->array[i].nalUnitLength);
    }

    hvcc->numOfArrays = 0;
    mux_freep(&hvcc->array);
}

static int hvccWrite(ByteIOContext *pb, HEVCDecoderConfigurationRecord *hvcc)
{
    cdx_uint8 i;
    uint16_t j, vps_count = 0, sps_count = 0, pps_count = 0;

    /*
     * We only support writing HEVCDecoderConfigurationRecord version 1.
     */
    hvcc->configurationVersion = 1;

    /*
     * If min_spatial_segmentation_idc is invalid, reset to 0 (unspecified).
     */
    if (hvcc->min_spatial_segmentation_idc > MAX_SPATIAL_SEGMENTATION)
        hvcc->min_spatial_segmentation_idc = 0;

    /*
     * parallelismType indicates the type of parallelism that is used to meet
     * the restrictions imposed by min_spatial_segmentation_idc when the value
     * of min_spatial_segmentation_idc is greater than 0.
     */
    if (!hvcc->min_spatial_segmentation_idc)
        hvcc->parallelismType = 0;

    /*
     * It's unclear how to properly compute these fields, so
     * let's always set them to values meaning 'unspecified'.
     */
    hvcc->avgFrameRate      = 0;
    hvcc->constantFrameRate = 0;

    logv("configurationVersion:                %d\n",
            hvcc->configurationVersion);
    logv("general_profile_space:               %d\n",
            hvcc->general_profile_space);
    logv("general_tier_flag:                   %d\n",
            hvcc->general_tier_flag);
    logv("general_profile_idc:                 %d\n",
            hvcc->general_profile_idc);
    logv("general_profile_compatibility_flags: 0x%08u\n",
            hvcc->general_profile_compatibility_flags);
    logv("general_constraint_indicator_flags:  0x%012llu\n",
            hvcc->general_constraint_indicator_flags);
    logv("general_level_idc:                   %d\n",
            hvcc->general_level_idc);
    logv("min_spatial_segmentation_idc:        %d\n",
            hvcc->min_spatial_segmentation_idc);
    logv("parallelismType:                     %d\n",
            hvcc->parallelismType);
    logv("chromaFormat:                        %d\n",
            hvcc->chromaFormat);
    logv("bitDepthLumaMinus8:                  %d\n",
            hvcc->bitDepthLumaMinus8);
    logv("bitDepthChromaMinus8:                %d\n",
            hvcc->bitDepthChromaMinus8);
    logv("avgFrameRate:                        %d\n",
            hvcc->avgFrameRate);
    logv("constantFrameRate:                   %d\n",
            hvcc->constantFrameRate);
    logv("numTemporalLayers:                   %d\n",
            hvcc->numTemporalLayers);
    logv("temporalIdNested:                    %d\n",
            hvcc->temporalIdNested);
    logv("lengthSizeMinusOne:                  %d\n",
            hvcc->lengthSizeMinusOne);
    logv("numOfArrays:                         %d\n",
            hvcc->numOfArrays);
    for (i = 0; i < hvcc->numOfArrays; i++) {
        logv("array_completeness[%d]:               %d\n",
                i, hvcc->array[i].array_completeness);
        logv("NAL_unit_type[%d]:                    %d\n",
                i, hvcc->array[i].NAL_unit_type);
        logv("numNalus[%d]:                         %d\n",
                i, hvcc->array[i].numNalus);
        for (j = 0; j < hvcc->array[i].numNalus; j++)
            logv("nalUnitLength[%d][%d]:                 %d\n",
                    i, j, hvcc->array[i].nalUnitLength[j]);
    }

    /*
     * We need at least one of each: VPS, SPS and PPS.
     */
    for (i = 0; i < hvcc->numOfArrays; i++)
        switch (hvcc->array[i].NAL_unit_type) {
        case HEVC_NAL_TYPE_VPS:
            vps_count += hvcc->array[i].numNalus;
            break;
        case HEVC_NAL_TYPE_SPS:
            sps_count += hvcc->array[i].numNalus;
            break;
        case HEVC_NAL_TYPE_PPS:
            pps_count += hvcc->array[i].numNalus;
            break;
        default:
            break;
        }
    if (!vps_count || vps_count > HEVC_MAX_NUM_VPS_COUNT ||
        !sps_count || sps_count > HEVC_MAX_NUM_SPS_COUNT ||
        !pps_count || pps_count > HEVC_MAX_NUM_PPS_COUNT)
        return -2;

    /* cdx_uint32(8) configurationVersion = 1; */
    //avio_w8(pb, hvcc->configurationVersion);
    put_byte_cache(NULL, pb, hvcc->configurationVersion);

    /*
     * cdx_uint32(2) general_profile_space;
     * cdx_uint32(1) general_tier_flag;
     * cdx_uint32(5) general_profile_idc;
     */
//    avio_w8(pb, hvcc->general_profile_space << 6 |
//                hvcc->general_tier_flag     << 5 |
//                hvcc->general_profile_idc);
    put_byte_cache(NULL, pb, hvcc->general_profile_space << 6 |
                hvcc->general_tier_flag     << 5 |
                hvcc->general_profile_idc);
    /* cdx_uint32(32) general_profile_compatibility_flags; */
    //avio_wb32(pb, hvcc->general_profile_compatibility_flags);
    put_be32_cache(NULL, pb, hvcc->general_profile_compatibility_flags);

    /* cdx_uint32(48) general_constraint_indicator_flags; */
    //avio_wb32(pb, hvcc->general_constraint_indicator_flags >> 16);
    put_be32_cache(NULL, pb, hvcc->general_constraint_indicator_flags >> 16);
    //avio_wb16(pb, hvcc->general_constraint_indicator_flags);
    put_be16_cache(NULL, pb, hvcc->general_constraint_indicator_flags);

    /* cdx_uint32(8) general_level_idc; */
    //avio_w8(pb, hvcc->general_level_idc);
    put_byte_cache(NULL, pb, hvcc->general_level_idc);

    /*
     * bit(4) reserved = ‘1111’b;
     * cdx_uint32(12) min_spatial_segmentation_idc;
     */
    //avio_wb16(pb, hvcc->min_spatial_segmentation_idc | 0xf000);
    put_be16_cache(NULL, pb, hvcc->min_spatial_segmentation_idc | 0xf000);

    /*
     * bit(6) reserved = ‘111111’b;
     * cdx_uint32(2) parallelismType;
     */
    //avio_w8(pb, hvcc->parallelismType | 0xfc);
    put_byte_cache(NULL, pb, hvcc->parallelismType | 0xfc);

    /*
     * bit(6) reserved = ‘111111’b;
     * cdx_uint32(2) chromaFormat;
     */
    //avio_w8(pb, hvcc->chromaFormat | 0xfc);
    put_byte_cache(NULL, pb, hvcc->chromaFormat | 0xfc);

    /*
     * bit(5) reserved = ‘11111’b;
     * cdx_uint32(3) bitDepthLumaMinus8;
     */
    //avio_w8(pb, hvcc->bitDepthLumaMinus8 | 0xf8);
    put_byte_cache(NULL, pb, hvcc->bitDepthLumaMinus8 | 0xf8);

    /*
     * bit(5) reserved = ‘11111’b;
     * cdx_uint32(3) bitDepthChromaMinus8;
     */
    //avio_w8(pb, hvcc->bitDepthChromaMinus8 | 0xf8);
    put_byte_cache(NULL, pb, hvcc->bitDepthChromaMinus8 | 0xf8);

    /* bit(16) avgFrameRate; */
    //avio_wb16(pb, hvcc->avgFrameRate);
    put_be16_cache(NULL, pb, hvcc->avgFrameRate);

    /*
     * bit(2) constantFrameRate;
     * bit(3) numTemporalLayers;
     * bit(1) temporalIdNested;
     * cdx_uint32(2) lengthSizeMinusOne;
     */
//    avio_w8(pb, hvcc->constantFrameRate << 6 |
//                hvcc->numTemporalLayers << 3 |
//                hvcc->temporalIdNested  << 2 |
//                hvcc->lengthSizeMinusOne);
    put_byte_cache(NULL, pb, hvcc->constantFrameRate << 6 |
                hvcc->numTemporalLayers << 3 |
                hvcc->temporalIdNested  << 2 |
                hvcc->lengthSizeMinusOne);

    /* cdx_uint32(8) numOfArrays; */
    //avio_w8(pb, hvcc->numOfArrays);
    put_byte_cache(NULL, pb, hvcc->numOfArrays);

    for (i = 0; i < hvcc->numOfArrays; i++) {
        /*
         * bit(1) array_completeness;
         * cdx_uint32(1) reserved = 0;
         * cdx_uint32(6) NAL_unit_type;
         */
//        avio_w8(pb, hvcc->array[i].array_completeness << 7 |
//                    hvcc->array[i].NAL_unit_type & 0x3f);
        put_byte_cache(NULL, pb, hvcc->array[i].array_completeness << 7 |
                    (hvcc->array[i].NAL_unit_type & 0x3f));

        /* cdx_uint32(16) numNalus; */
        //avio_wb16(pb, hvcc->array[i].numNalus);
        put_be16_cache(NULL, pb, hvcc->array[i].numNalus);

        for (j = 0; j < hvcc->array[i].numNalus; j++) {
            /* cdx_uint32(16) nalUnitLength; */
            //avio_wb16(pb, hvcc->array[i].nalUnitLength[j]);
            put_be16_cache(NULL, pb, hvcc->array[i].nalUnitLength[j]);

            /* bit(8*nalUnitLength) nalUnit; */
//            avio_write(pb, hvcc->array[i].nalUnit[j],
//                       hvcc->array[i].nalUnitLength[j]);
            put_buffer_cache(NULL, pb, (char*)hvcc->array[i].nalUnit[j],
                       hvcc->array[i].nalUnitLength[j]);
        }
    }

    return 0;
}

cdx_int32 hvccWriteSpsPps(ByteIOContext *pb, cdx_uint8 *data,
                       cdx_int32 size, cdx_int32 PsArrayCompleteness)
{
    cdx_int32 ret = 0;
    cdx_uint8 *buf, *end, *start = NULL;
    HEVCDecoderConfigurationRecord hvcc;

    hvccInit(&hvcc);

    if (size < 6) {
        /* We can't write a valid hvcC from the provided data */
        ret = -2;
        goto end;
    } else if (*data == 1) {
        /* Data is already hvcC-formatted */
        put_buffer_cache(NULL, pb, (char*)data, size);
        goto end;
    } else if (!(HEVC_AW_RB24(data) == 1 || HEVC_AW_RB32(data) == 1)) {
        /* Not a valid Annex B start code prefix */
        ret = -2;
        goto end;
    }

    ret = parseAvcNalus(data, &start, &size);
    if (ret < 0)
        goto end;

    buf = start;
    end = start + size;

    while (end - buf > 4) {
        cdx_uint32 len = FINDMIN(HEVC_AW_RB32(buf), end - buf - 4);
        cdx_uint8 type = (buf[4] >> 1) & 0x3f;

        buf += 4;

        switch (type) {
        case HEVC_NAL_TYPE_VPS:
        case HEVC_NAL_TYPE_SPS:
        case HEVC_NAL_TYPE_PPS:
        case HEVC_NAL_TYPE_SEI_PREFIX:
        case HEVC_NAL_TYPE_SEI_SUFFIX:
            ret = HvccAddNalUnit(buf, len, PsArrayCompleteness, &hvcc);
            if (ret < 0)
                goto end;
            break;
        default:
            break;
        }

        buf += len;
    }

    ret = hvccWrite(pb, &hvcc);

end:
    hvccClose(&hvcc);
    mux_free(start);
    return ret;
}

static cdx_uint32 GetHvccWriteSize(HEVCDecoderConfigurationRecord *hvcc)
{
    cdx_uint32 hvccWriteSize = 0;
    cdx_uint8 i;
    uint16_t j, vps_count = 0, sps_count = 0, pps_count = 0;

    /*
     * We only support writing HEVCDecoderConfigurationRecord version 1.
     */
    hvcc->configurationVersion = 1;

    /*
     * If min_spatial_segmentation_idc is invalid, reset to 0 (unspecified).
     */
    if (hvcc->min_spatial_segmentation_idc > MAX_SPATIAL_SEGMENTATION)
        hvcc->min_spatial_segmentation_idc = 0;

    /*
     * parallelismType indicates the type of parallelism that is used to meet
     * the restrictions imposed by min_spatial_segmentation_idc when the value
     * of min_spatial_segmentation_idc is greater than 0.
     */
    if (!hvcc->min_spatial_segmentation_idc)
        hvcc->parallelismType = 0;

    /*
     * It's unclear how to properly compute these fields, so
     * let's always set them to values meaning 'unspecified'.
     */
    hvcc->avgFrameRate      = 0;
    hvcc->constantFrameRate = 0;

    logv("configurationVersion:                %d\n",
            hvcc->configurationVersion);
    logv("general_profile_space:               %d\n",
            hvcc->general_profile_space);
    logv("general_tier_flag:                   %d\n",
            hvcc->general_tier_flag);
    logv("general_profile_idc:                 %d\n",
            hvcc->general_profile_idc);
    logv("general_profile_compatibility_flags: 0x%08u\n",
            hvcc->general_profile_compatibility_flags);
    logv("general_constraint_indicator_flags:  0x%012llu\n",
            hvcc->general_constraint_indicator_flags);
    logv("general_level_idc:                   %d\n",
            hvcc->general_level_idc);
    logv("min_spatial_segmentation_idc:        %d\n",
            hvcc->min_spatial_segmentation_idc);
    logv("parallelismType:                     %d\n",
            hvcc->parallelismType);
    logv("chromaFormat:                        %d\n",
            hvcc->chromaFormat);
    logv("bitDepthLumaMinus8:                  %d\n",
            hvcc->bitDepthLumaMinus8);
    logv("bitDepthChromaMinus8:                %d\n",
            hvcc->bitDepthChromaMinus8);
    logv("avgFrameRate:                        %d\n",
            hvcc->avgFrameRate);
    logv("constantFrameRate:                   %d\n",
            hvcc->constantFrameRate);
    logv("numTemporalLayers:                   %d\n",
            hvcc->numTemporalLayers);
    logv("temporalIdNested:                    %d\n",
            hvcc->temporalIdNested);
    logv("lengthSizeMinusOne:                  %d\n",
            hvcc->lengthSizeMinusOne);
    logv("numOfArrays:                         %d\n",
            hvcc->numOfArrays);
    for (i = 0; i < hvcc->numOfArrays; i++) {
        logv("array_completeness[%d]:               %d\n",
                i, hvcc->array[i].array_completeness);
        logv("NAL_unit_type[%d]:                    %d\n",
                i, hvcc->array[i].NAL_unit_type);
        logv("numNalus[%d]:                         %d\n",
                i, hvcc->array[i].numNalus);
        for (j = 0; j < hvcc->array[i].numNalus; j++)
            logv("nalUnitLength[%d][%d]:                 %d\n",
                    i, j, hvcc->array[i].nalUnitLength[j]);
    }

    /*
     * We need at least one of each: VPS, SPS and PPS.
     */
    for (i = 0; i < hvcc->numOfArrays; i++)
        switch (hvcc->array[i].NAL_unit_type) {
        case HEVC_NAL_TYPE_VPS:
            vps_count += hvcc->array[i].numNalus;
            break;
        case HEVC_NAL_TYPE_SPS:
            sps_count += hvcc->array[i].numNalus;
            break;
        case HEVC_NAL_TYPE_PPS:
            pps_count += hvcc->array[i].numNalus;
            break;
        default:
            break;
        }
    if (!vps_count || vps_count > HEVC_MAX_NUM_VPS_COUNT ||
        !sps_count || sps_count > HEVC_MAX_NUM_SPS_COUNT ||
        !pps_count || pps_count > HEVC_MAX_NUM_PPS_COUNT)
        return hvccWriteSize;

    /* cdx_uint32(8) configurationVersion = 1; */
    //avio_w8(pb, hvcc->configurationVersion);
    hvccWriteSize += 1;

    /*
     * cdx_uint32(2) general_profile_space;
     * cdx_uint32(1) general_tier_flag;
     * cdx_uint32(5) general_profile_idc;
     */
//    avio_w8(pb, hvcc->general_profile_space << 6 |
//                hvcc->general_tier_flag     << 5 |
//                hvcc->general_profile_idc);
    hvccWriteSize += 1;
    /* cdx_uint32(32) general_profile_compatibility_flags; */
    //avio_wb32(pb, hvcc->general_profile_compatibility_flags);
    hvccWriteSize += 4;

    /* cdx_uint32(48) general_constraint_indicator_flags; */
    //avio_wb32(pb, hvcc->general_constraint_indicator_flags >> 16);
    hvccWriteSize += 4;
    //avio_wb16(pb, hvcc->general_constraint_indicator_flags);
    hvccWriteSize += 2;

    /* cdx_uint32(8) general_level_idc; */
    //avio_w8(pb, hvcc->general_level_idc);
    hvccWriteSize += 1;

    /*
     * bit(4) reserved = ‘1111’b;
     * cdx_uint32(12) min_spatial_segmentation_idc;
     */
    //avio_wb16(pb, hvcc->min_spatial_segmentation_idc | 0xf000);
    hvccWriteSize += 2;

    /*
     * bit(6) reserved = ‘111111’b;
     * cdx_uint32(2) parallelismType;
     */
    //avio_w8(pb, hvcc->parallelismType | 0xfc);
    hvccWriteSize += 1;

    /*
     * bit(6) reserved = ‘111111’b;
     * cdx_uint32(2) chromaFormat;
     */
    //avio_w8(pb, hvcc->chromaFormat | 0xfc);
    hvccWriteSize += 1;

    /*
     * bit(5) reserved = ‘11111’b;
     * cdx_uint32(3) bitDepthLumaMinus8;
     */
    //avio_w8(pb, hvcc->bitDepthLumaMinus8 | 0xf8);
    hvccWriteSize += 1;

    /*
     * bit(5) reserved = ‘11111’b;
     * cdx_uint32(3) bitDepthChromaMinus8;
     */
    //avio_w8(pb, hvcc->bitDepthChromaMinus8 | 0xf8);
    hvccWriteSize += 1;

    /* bit(16) avgFrameRate; */
    //avio_wb16(pb, hvcc->avgFrameRate);
    hvccWriteSize += 2;

    /*
     * bit(2) constantFrameRate;
     * bit(3) numTemporalLayers;
     * bit(1) temporalIdNested;
     * cdx_uint32(2) lengthSizeMinusOne;
     */
//    avio_w8(pb, hvcc->constantFrameRate << 6 |
//                hvcc->numTemporalLayers << 3 |
//                hvcc->temporalIdNested  << 2 |
//                hvcc->lengthSizeMinusOne);
    hvccWriteSize += 1;

    /* cdx_uint32(8) numOfArrays; */
    //avio_w8(pb, hvcc->numOfArrays);
    hvccWriteSize += 1;

    for (i = 0; i < hvcc->numOfArrays; i++) {
        /*
         * bit(1) array_completeness;
         * cdx_uint32(1) reserved = 0;
         * cdx_uint32(6) NAL_unit_type;
         */
//        avio_w8(pb, hvcc->array[i].array_completeness << 7 |
//                    hvcc->array[i].NAL_unit_type & 0x3f);
        hvccWriteSize += 1;

        /* cdx_uint32(16) numNalus; */
        //avio_wb16(pb, hvcc->array[i].numNalus);
        hvccWriteSize += 2;

        for (j = 0; j < hvcc->array[i].numNalus; j++) {
            /* cdx_uint32(16) nalUnitLength; */
            //avio_wb16(pb, hvcc->array[i].nalUnitLength[j]);
            hvccWriteSize += 2;

            /* bit(8*nalUnitLength) nalUnit; */
//            avio_write(pb, hvcc->array[i].nalUnit[j],
//                       hvcc->array[i].nalUnitLength[j]);
            hvccWriteSize += hvcc->array[i].nalUnitLength[j];
        }
    }

    return hvccWriteSize;
}

extern int parseAvcNalus(cdx_uint8 *buf_in, cdx_uint8 **buf, int *size);
cdx_uint32 hvccGetSpsPpsSize(cdx_uint8 *data, cdx_int32 size, cdx_int32 PsArrayCompleteness)
{
    cdx_uint32 hvcc_size = 0;
    int ret = 0;
    cdx_uint8 *buf, *end, *start = NULL;
    HEVCDecoderConfigurationRecord hvcc;

    hvccInit(&hvcc);

    if (size < 6) {
        /* We can't write a valid hvcC from the provided data */
        ret = -2;
        goto end;
    } else if (*data == 1) {
        /* Data is already hvcC-formatted */
        //put_buffer_cache(NULL, pb, (char*)data, size);
        hvcc_size = size;
        goto end;
    } else if (!(HEVC_AW_RB24(data) == 1 || HEVC_AW_RB32(data) == 1)) {
        /* Not a valid Annex B start code prefix */
        ret = -2;
        goto end;
    }

    ret = parseAvcNalus(data, &start, &size);
    if (ret < 0)
        goto end;

    buf = start;
    end = start + size;

    while (end - buf > 4) {
        cdx_uint32 len = FINDMIN(HEVC_AW_RB32(buf), end - buf - 4);
        cdx_uint8 type = (buf[4] >> 1) & 0x3f;

        buf += 4;

        switch (type) {
        case HEVC_NAL_TYPE_VPS:
        case HEVC_NAL_TYPE_SPS:
        case HEVC_NAL_TYPE_PPS:
        case HEVC_NAL_TYPE_SEI_PREFIX:
        case HEVC_NAL_TYPE_SEI_SUFFIX:
            ret = HvccAddNalUnit(buf, len, PsArrayCompleteness, &hvcc);
            if (ret < 0)
                goto end;
            break;
        default:
            break;
        }

        buf += len;
    }

    //ret = hvccWrite(pb, &hvcc);
    hvcc_size = GetHvccWriteSize(&hvcc);

end:
    hvccClose(&hvcc);
    mux_free(start);
    logv("hvcc size[%d]", hvcc_size);
    return hvcc_size;
}

