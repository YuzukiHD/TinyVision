/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : EncoderTest.c
 * Description : EncoderTest
 * History :
 *
 */

#include "cdc_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vencoder.h"
#include "EncAdapter.h"
#include <sys/time.h>
#include <time.h>
#include <memoryAdapter.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

#include "avtimer.h"

#include "veAdapter.h"

#define ENCODER_MAX_NUM (5)
#define DEMO_FILE_NAME_LEN 256
#define USE_H265_ENC
#define ROI_NUM 4
#define NO_READ_WRITE 0
#define SAVE_AWSP     0
//#define YU12_NV12
//#define USE_AFBC_INPUT
//#define YU12_NV21
//#define VBR
#define SET_INPUTFORMAT_YUV420P

//#define USE_SVC
//#define USE_VIDEO_SIGNAL
//#define USE_ASPECT_RATIO
//#define USE_SUPER_FRAME

//#define GET_MB_INFO
//#define SET_MB_INFO
//#define SET_SMART
//#define DETECT_MOTION

#define ENABLE_OVERLAY_BY_WATERMARK (0)
#define ICON_PIC_PATH "/sdcard/watermark"

//* test the function of overlay/lbc/afbc by  "./demoVencoder -overlay/-afbc/-lbc"
#if 0
#define TEST_OVERLAY_FUNC (0)
#define USE_AFBC_INPUT
#define USE_LBC_INPUT
#define USE_LBC_LOSSY_COM_EN_2x (0)
#define USE_LBC_LOSSY_COM_EN_2_5x (0)
#endif

#define ALIGN_XXB(y, x) (((x) + ((y)-1)) & ~((y)-1))

#define my_printf() logd("func:%s, line:%d\n", __func__, __LINE__)

//extern int gettimeofday(struct timeval *tv, struct timezone *tz);
static long long GetNowUs()
{
    long long curr;
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
    return curr;
}

typedef struct {
    int     argc;
    char**  argv;
    int nChannel;
}encoder_Context;

typedef struct {
    EXIFInfo                exifinfo;
    int                     quality;
    int                     jpeg_mode;
    VencJpegVideoSignal     vs;
    int                     jpeg_biteRate;
    int                     jpeg_frameRate;
    VencBitRateRange        bitRateRange;
    VencOverlayInfoS        sOverlayInfo;
    VencCopyROIConfig       pRoiConfig;
}jpeg_func_t;

typedef struct {
    VencHeaderData          sps_pps_data;
    VencH264Param           h264Param;
    VencMBModeCtrl          h264MBMode;
    VencMBInfo              MBInfo;
    VencFixQP           fixQP;
    VencSuperFrameConfig    sSuperFrameCfg;
    VencH264SVCSkip         SVCSkip; // set SVC and skip_frame
    VencH264AspectRatio     sAspectRatio;
    VencH264VideoSignal     sVideoSignal;
    VencCyclicIntraRefresh  sIntraRefresh;
    VencROIConfig           sRoiConfig[ROI_NUM];
    VeProcSet               sVeProcInfo;
    VencOverlayInfoS        sOverlayInfo;
    VencSmartFun            sH264Smart;
}h264_func_t;

typedef struct {
    VencH265Param               h265Param;
    VencH265GopStruct           h265Gop;
    VencHVS                     h265Hvs;
    VencH265TendRatioCoef       h265Trc;
    VencSmartFun                h265Smart;
    VencMBModeCtrl              h265MBMode;
    VencMBInfo                  MBInfo;
    VencFixQP               fixQP;
    VencSuperFrameConfig        sSuperFrameCfg;
    VencH264SVCSkip             SVCSkip; // set SVC and skip_frame
    VencH264AspectRatio         sAspectRatio;
    VencH264VideoSignal         sVideoSignal;
    VencCyclicIntraRefresh      sIntraRefresh;
    VencROIConfig               sRoiConfig[ROI_NUM];
    VencAlterFrameRateInfo sAlterFrameRateInfo;
    int                         h265_rc_frame_total;
    VeProcSet               sVeProcInfo;
    VencOverlayInfoS        sOverlayInfo;
}h265_func_t;


typedef struct {
    char             intput_file[256];
    char             output_file[256];
    char             reference_file[256];
    char             overlay_file[256];
    char             log_file[256];
    int              compare_flag;
    int              log_flag;
    int              compare_result;

    unsigned int  encode_frame_num;
    unsigned int  encode_format;

    unsigned int src_size;
    unsigned int dst_size;

    unsigned int src_width;
    unsigned int src_height;
    unsigned int dst_width;
    unsigned int dst_height;

    int frequency;
    int bit_rate;
    int frame_rate;
    int maxKeyFrame;
    unsigned int test_cycle;
    unsigned int test_overlay_flag;
    unsigned int test_lbc;
    unsigned int test_afbc;
    unsigned int limit_encode_speed; //* limit encoder speed by framerate
    unsigned int qp_min;
    unsigned int qp_max;

    jpeg_func_t jpeg_func;
    h264_func_t h264_func;
    h265_func_t h265_func;
    unsigned int nChannel;
    VencCopyROIConfig pRoiConfig;
}encode_param_t;

typedef enum {
    INPUT,
    HELP,
    ENCODE_FRAME_NUM,
    ENCODE_FORMAT,
    OUTPUT,
    SRC_SIZE,
    DST_SIZE,
    COMPARE_FILE,
    LOG_FILE,
    FREQUENCY,
    BIT_RATE,
    TEST_CYCLE,
    TEST_OVERLAY,
    TEST_OVERLAY_IN,
    TEST_LBC,
    TEST_AFBC,
    LIMIT_SPEED,
    QP_MIN,
    QP_MAX,
    ENCODER_NUM,
    INVALID
}ARGUMENT_T;

typedef struct {
    char Short[16];
    char Name[128];
    ARGUMENT_T argument;
    char Description[512];
}argument_t;

static const argument_t ArgumentMapping[] =
{
    { "-h",  "--help",    HELP,
        "Print this help" },
    { "-i",  "--input",   INPUT,
        "Input file path" },
    { "-n",  "--encode_frame_num",   ENCODE_FRAME_NUM,
        "After encoder n frames, encoder stop" },
    { "-f",  "--encode_format",  ENCODE_FORMAT,
        "0:h264 encoder, 1:jpeg_encoder, 3:h265 encoder, 5: isp" },
    { "-o",  "--output",  OUTPUT,
        "output file path" },
    { "-s",  "--srcsize",  SRC_SIZE,
        "src_size,can be 1920x1080 or 2160,1080,720,480,288" },
    { "-d",  "--dstsize",  DST_SIZE,
        "dst_size,can be 1920x1080 or 2160,1080,720,480,288" },
    { "-c",  "--compare",  COMPARE_FILE,
        "compare file:reference file path" },
    { "-q",  "--frequency", FREQUENCY,
        "frequency: the frequency of video engine"},
    { "-b",  "--bitrate",  BIT_RATE,
        "bitRate:kbps" },
    { "-t",  "--test_cycle",   TEST_CYCLE,
        "Cycle num of testing" },
    { "-l",  "--logfile",  LOG_FILE,
        "Log file path" },
    { "-overlay",  "--overlay",  TEST_OVERLAY,
        "add the overlay function" },
    { "-overlay_in",  "--overlay_int",  TEST_OVERLAY_IN,
        "overlay intput data file" },
    { "-lbc",  "--lbc ",  TEST_LBC,
        "1: no lossy lbc, 2: lossy_2x, 3: lossy_2.5x" },
    { "-afbc",  "--afbc",  TEST_AFBC,
        "1: test afbc input data format" },
    { "-limit_sp",  "--limit speed",  LIMIT_SPEED,
        "1: limit encoder speed by framerate" },
    { "-qp_min",  "--qp min",  QP_MIN,
        "set the qp_min value" },
    { "-qp_max",  "--qp max",  QP_MAX,
        "set the qp_max value" },
    { "-enc_num",  "--encoder num",  ENCODER_NUM,
        "create multi encoder" },
};

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int width_aligh16;
    unsigned int height_aligh16;
    unsigned char* argb_addr;
    unsigned int size;
}BitMapInfoS;

BitMapInfoS bit_map_info[13];//= {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}};

int yu12_nv12(unsigned int width, unsigned int height, unsigned char *addr_uv,
          unsigned char *addr_tmp_uv)
{
    unsigned int i, chroma_bytes;
    unsigned char *u_addr = NULL;
    unsigned char *v_addr = NULL;
    unsigned char *tmp_addr = NULL;

    chroma_bytes = width*height/4;

    u_addr = addr_uv;
    v_addr = addr_uv + chroma_bytes;
    tmp_addr = addr_tmp_uv;

    for(i=0; i<chroma_bytes; i++)
    {
        *(tmp_addr++) = *(u_addr++);
        *(tmp_addr++) = *(v_addr++);
    }

    memcpy(addr_uv, addr_tmp_uv, chroma_bytes*2);

    return 0;
}

int yu12_nv21(unsigned int width, unsigned int height, unsigned char *addr_uv,
          unsigned char *addr_tmp_uv)
{
    unsigned int i, chroma_bytes;
    unsigned char *u_addr = NULL;
    unsigned char *v_addr = NULL;
    unsigned char *tmp_addr = NULL;

    chroma_bytes = width*height/4;

    u_addr = addr_uv;
    v_addr = addr_uv + chroma_bytes;
    tmp_addr = addr_tmp_uv;

    for(i=0; i<chroma_bytes; i++)
    {
        *(tmp_addr++) = *(v_addr++);
        *(tmp_addr++) = *(u_addr++);
    }

    memcpy(addr_uv, addr_tmp_uv, chroma_bytes*2);

    return 0;
}


ARGUMENT_T GetArgument(char *name)
{
    int i = 0;
    int num = sizeof(ArgumentMapping) / sizeof(argument_t);
    while(i < num)
    {
        logv("input_name:%s, i:%d, name:%s, short:%s, argument:%d, num:%d\n",
                                                    name,
                                                    i,
                                                    ArgumentMapping[i].Name,
                                                    ArgumentMapping[i].Short,
                                                    ArgumentMapping[i].argument,
                                                    num);
        if((0 == strcmp(ArgumentMapping[i].Name, name)) ||
            ((0 == strcmp(ArgumentMapping[i].Short, name)) &&
             (0 != strcmp(ArgumentMapping[i].Short, "--"))))
        {
            return ArgumentMapping[i].argument;
        }
        i++;
    }
    return INVALID;
}

static void PrintDemoUsage(void)
{
    int i = 0;
    int num = sizeof(ArgumentMapping) / sizeof(argument_t);
    printf("Usage:");
    while(i < num)
    {
        printf("%-12s %-32s %s", ArgumentMapping[i].Short, ArgumentMapping[i].Name,
                ArgumentMapping[i].Description);
        printf("\n");
        i++;
    }
}

void ParseArgument(encode_param_t *encode_param, char *argument, char *value)
{
    ARGUMENT_T arg;

    arg = GetArgument(argument);

    switch(arg)
    {
        case HELP:
            PrintDemoUsage();
            exit(-1);
        case INPUT:
            memset(encode_param->intput_file, 0, sizeof(encode_param->intput_file));
            sscanf(value, "%255s", encode_param->intput_file);
            printf("get input file: %s\n", encode_param->intput_file);
            break;
        case ENCODE_FRAME_NUM:
            sscanf(value, "%32u", &encode_param->encode_frame_num);
            printf("encode:%u frames\n", encode_param->encode_frame_num);
            break;
        case ENCODE_FORMAT:
            sscanf(value, "%32u", &encode_param->encode_format);
            printf("encode_format:%u 0:h264,1:jpeg,3:h265,5: isp\n", encode_param->encode_format);
            break;
        case OUTPUT:
            memset(encode_param->output_file, 0, sizeof(encode_param->output_file));
            char outFile[64] = {0};
            sscanf(value, "%63s",outFile);
            //TODO: nChannle would cause "0_/mnt/pic.dat" in t507 linux if set OUTPUT arg as /mnt/pic.dat
            sprintf(encode_param->output_file, "%s",
                   /* (int)encode_param->nChannel,*/ outFile);
            printf("get output file: %s\n", encode_param->output_file);
            break;
        case SRC_SIZE:
            if(strlen(value) > 5)
            {
                sscanf(value, "%32ux%32u", &encode_param->src_width,&encode_param->src_height);
            }
            else
            {
                sscanf(value, "%32u", &encode_param->src_size);
                if(encode_param->src_size == 1080)
                {
                    encode_param->src_width = 1920;
                    encode_param->src_height = 1080;
                }
                else if(encode_param->src_size == 1088)
                {
                    encode_param->src_width = 1920;
                    encode_param->src_height = 1088;
                }
                else if(encode_param->src_size == 720)
                {
                    encode_param->src_width = 1280;
                    encode_param->src_height = 720;
                }
                else if(encode_param->src_size == 480)
                {
                    encode_param->src_width = 640;
                    encode_param->src_height = 480;
                }
                else if(encode_param->src_size == 368)
                {
                    encode_param->src_width = 640;
                    encode_param->src_height = 368;
                }
                else if(encode_param->src_size == 2160)
                {
                    encode_param->src_width = 3840;
                    encode_param->src_height = 2160;
                }
                else if(encode_param->src_size == 3456)
                {
                    encode_param->src_width = 3840;
                    encode_param->src_height = 2160;
                }
                else if(encode_param->src_size == 288)
                {
                    encode_param->src_width = 352;
                    encode_param->src_height = 288;
                }
                else if(encode_param->src_size == 1344)
                {
                    encode_param->src_width = 1344;
                    encode_param->src_height = 768;
                }
                else if(encode_param->src_size == 1536)
                {
                    encode_param->src_width = 1536;
                    encode_param->src_height = 864;
                }
                else
                {
                    encode_param->src_width = 1280;
                    encode_param->src_height = 720;
                    logw("encoder demo only support the size 1080p,720p,480p, \
                     now use the default size 720p\n");
                }
            }
            printf("get src_size: %ux%u\n", encode_param->src_width,encode_param->src_height);
            break;
        case DST_SIZE:
            if(strlen(value) > 5)
            {
                sscanf(value, "%32ux%32u", &encode_param->dst_width,&encode_param->dst_height);
            }
            else
            {
                sscanf(value, "%32u", &encode_param->dst_size);
                if(encode_param->dst_size == 1080)
                {
                    encode_param->dst_width = 1920;
                    encode_param->dst_height = 1080;
                }
                else if(encode_param->dst_size == 1088)
                {
                    encode_param->dst_width = 1920;
                    encode_param->dst_height = 1088;
                }
                else if(encode_param->dst_size == 1344)
                {
                    //encode_param->frame_rate = 60;
                    encode_param->dst_width = 1344;
                    encode_param->dst_height = 756;
                }
                else if(encode_param->dst_size == 1536)
                {
                    //encode_param->frame_rate = 30;
                    encode_param->dst_width = 1536;
                    encode_param->dst_height = 864;
                }
                else if(encode_param->dst_size == 720)
                {
                    encode_param->dst_width = 1280;
                    encode_param->dst_height = 720;
                }
                else if(encode_param->dst_size == 480)
                {
                    encode_param->dst_width = 640;
                    encode_param->dst_height = 480;
                }
                else if(encode_param->dst_size == 368)
                {
                    encode_param->dst_width = 640;
                    encode_param->dst_height = 368;
                }
                else if(encode_param->dst_size == 2160)
                {
                    encode_param->dst_width = 3840;
                    encode_param->dst_height = 2160;
                }
                else if(encode_param->dst_size == 288)
                {
                    encode_param->dst_width = 352;
                    encode_param->dst_height = 288;
                }
                else
                {
                    encode_param->dst_width = 1280;
                    encode_param->dst_height = 720;
                    logw("encoder demo only support the size 1080p,720p,480p,\
                     now use the default size 720p\n");
                }
            }
            printf("get dst_size: %ux%u\n", encode_param->dst_width,encode_param->dst_height);
            break;
            case COMPARE_FILE:
            memset(encode_param->reference_file, 0, sizeof(encode_param->reference_file));
            sscanf(value, "%255s", encode_param->reference_file);
            encode_param->compare_flag = 1;
            printf("get reference file: %s\n", encode_param->reference_file);
            break;
        case FREQUENCY:
            sscanf(value, "%32d", &encode_param->frequency);
            printf("frequency: %d\n", encode_param->frequency);
            break;
        case BIT_RATE:
            sscanf(value, "%32d", &encode_param->bit_rate);
            printf("bit rate: %d\n", encode_param->bit_rate);
            break;
        case LOG_FILE:
            memset(encode_param->log_file, 0, sizeof(encode_param->log_file));
            sscanf(value, "%255s", encode_param->log_file);
            encode_param->log_flag = 1;
            printf("get log file: %s\n", encode_param->log_file);
            break;
        case TEST_CYCLE:
            sscanf(value, "%32u", &encode_param->test_cycle);
            printf("test cycle: %u\n", encode_param->test_cycle);
            break;
        case TEST_OVERLAY:
            sscanf(value, "%32u", &encode_param->test_overlay_flag);
            printf("test overlay flag: %u\n", encode_param->test_overlay_flag);
            break;
        case TEST_OVERLAY_IN:
            memset(encode_param->overlay_file, 0, sizeof(encode_param->overlay_file));
            sscanf(value, "%255s", encode_param->overlay_file);
            printf("get overlay file: %s\n", encode_param->overlay_file);
            break;
        case TEST_LBC:
            sscanf(value, "%32u", &encode_param->test_lbc);
            printf("test lbc: %u\n", encode_param->test_lbc);
            break;
        case TEST_AFBC:
            sscanf(value, "%32u", &encode_param->test_afbc);
            printf("test afbc: %u\n", encode_param->test_afbc);
            break;
        case LIMIT_SPEED:
            sscanf(value, "%32u", &encode_param->limit_encode_speed);
            printf("limit speed: %u\n", encode_param->limit_encode_speed);
            break;
        case QP_MIN:
            sscanf(value, "%32u", &encode_param->qp_min);
            printf("qp_min: %u\n", encode_param->qp_min);
            break;
        case QP_MAX:
            sscanf(value, "%32u", &encode_param->qp_max);
            printf("qp_max: %u\n", encode_param->qp_max);
            break;
        case INVALID:
        default:
            logd("unknowed argument :  %s\n", argument);
            break;
    }
}

int SeekPrefixNAL(char* begin)
{
    unsigned int i;
    char* pchar = begin;
    char isPrefixNAL = 1;
    char NAL[4] = {0x00, 0x00, 0x00, 0x01};

    if(!pchar)
    {
        return -1;
    }

    for(i=0; i<4; i++)
    {
        if(pchar[i] != NAL[i])
        {
            isPrefixNAL = 0;
            break;
        }
    }
    if(isPrefixNAL == 1)
    {
        isPrefixNAL = 0;
        char PrefixNAL[3] = {0x6e, 0x4e, 0x0e};
        for(i=0; i<3; i++)
        {
            if(pchar[4] == PrefixNAL[i])
            {
                isPrefixNAL = 1;
                break;
            }
        }
    }
    // read temporal_id
    if(isPrefixNAL == 1)
    {
        char TemporalID = pchar[7];
        TemporalID >>= 5;
        return TemporalID;
    }

    return -1;
}

void init_jpeg_exif(EXIFInfo *exifinfo)
{
    exifinfo->ThumbWidth = 640;
    exifinfo->ThumbHeight = 480;

    strcpy((char*)exifinfo->CameraMake,        "allwinner make test");
    strcpy((char*)exifinfo->CameraModel,        "allwinner model test");
    strcpy((char*)exifinfo->DateTime,         "2014:02:21 10:54:05");
    strcpy((char*)exifinfo->gpsProcessingMethod,  "allwinner gps");

    exifinfo->Orientation = 0;

    exifinfo->ExposureTime.num = 2;
    exifinfo->ExposureTime.den = 1000;

    exifinfo->FNumber.num = 20;
    exifinfo->FNumber.den = 10;
    exifinfo->ISOSpeed = 50;

    exifinfo->ExposureBiasValue.num= -4;
    exifinfo->ExposureBiasValue.den= 1;

    exifinfo->MeteringMode = 1;
    exifinfo->FlashUsed = 0;

    exifinfo->FocalLength.num = 1400;
    exifinfo->FocalLength.den = 100;

    exifinfo->DigitalZoomRatio.num = 4;
    exifinfo->DigitalZoomRatio.den = 1;

    exifinfo->WhiteBalance = 1;
    exifinfo->ExposureMode = 1;

    exifinfo->enableGpsInfo = 1;

    exifinfo->gps_latitude = 23.2368;
    exifinfo->gps_longitude = 24.3244;
    exifinfo->gps_altitude = 1234.5;

    exifinfo->gps_timestamp = (long)time(NULL);

    strcpy((char*)exifinfo->CameraSerialNum,  "123456789");
    strcpy((char*)exifinfo->ImageName,  "exif-name-test");
    strcpy((char*)exifinfo->ImageDescription,  "exif-descriptor-test");
}

void init_h265_gop(VencH265GopStruct *h265Gop)
{
    h265Gop->gop_size = 8;
    h265Gop->intra_period = 16;

    h265Gop->use_lt_ref_flag = 1;
    if(h265Gop->use_lt_ref_flag)
    {
        h265Gop->max_num_ref_pics = 2;
        h265Gop->num_ref_idx_l0_default_active = 2;
        h265Gop->num_ref_idx_l1_default_active = 2;
        h265Gop->use_sps_rps_flag = 0;
    }
    else
    {
        h265Gop->max_num_ref_pics = 1;
        h265Gop->num_ref_idx_l0_default_active = 1;
        h265Gop->num_ref_idx_l1_default_active = 1;
        h265Gop->use_sps_rps_flag = 1;
    }
    //1:user config the reference info; 0:encoder config the reference info
    h265Gop->custom_rps_flag = 0;
}

void init_mb_mode(VencMBModeCtrl *pMBMode, encode_param_t *encode_param)
{
    unsigned int mb_num;
    //unsigned int j;

    mb_num = (ALIGN_XXB(16, encode_param->dst_width) >> 4)
                * (ALIGN_XXB(16, encode_param->dst_height) >> 4);
    pMBMode->p_info = malloc(sizeof(VencMBModeCtrlInfo) * mb_num);
    pMBMode->mode_ctrl_en = 1;

    #if 0
    for (j = 0; j < mb_num / 2; j++)
    {
        pMBMode->p_info[j].mb_en = 1;
        pMBMode->p_info[j].mb_skip_flag = 0;
        pMBMode->p_info[j].mb_qp = 22;
    }
    for (; j < mb_num; j++)
    {
        pMBMode->p_info[j].mb_en = 1;
        pMBMode->p_info[j].mb_skip_flag = 0;
        pMBMode->p_info[j].mb_qp = 32;
    }
    #endif
}

void init_mb_info(VencMBInfo *MBInfo, encode_param_t *encode_param)
{
    if(encode_param->encode_format == VENC_CODEC_H265)
    {
        MBInfo->num_mb = (ALIGN_XXB(32, encode_param->dst_width) *
                            ALIGN_XXB(32, encode_param->dst_height)) >> 10;
    }
    else
    {
        MBInfo->num_mb = (ALIGN_XXB(16, encode_param->dst_width) *
                            ALIGN_XXB(16, encode_param->dst_height)) >> 8;
    }
    MBInfo->p_para = (VencMBInfoPara *)malloc(sizeof(VencMBInfoPara) * MBInfo->num_mb);
    if(MBInfo->p_para == NULL)
    {
        loge("malloc MBInfo->p_para error\n");
        return;
    }
    logv("mb_num:%d, mb_info_queue_addr:%p\n", MBInfo->num_mb, MBInfo->p_para);
}


void init_fix_qp(VencFixQP *fixQP)
{
    fixQP->bEnable = 1;
    fixQP->nIQp = 35;
    fixQP->nPQp = 35;
}

void init_super_frame_cfg(VencSuperFrameConfig *sSuperFrameCfg)
{
    sSuperFrameCfg->eSuperFrameMode = VENC_SUPERFRAME_NONE;
    sSuperFrameCfg->nMaxIFrameBits = 30000*8;
    sSuperFrameCfg->nMaxPFrameBits = 15000*8;
}

void init_svc_skip(VencH264SVCSkip *SVCSkip)
{
    SVCSkip->nTemporalSVC = T_LAYER_4;
    switch(SVCSkip->nTemporalSVC)
    {
        case T_LAYER_4:
            SVCSkip->nSkipFrame = SKIP_8;
            break;
        case T_LAYER_3:
            SVCSkip->nSkipFrame = SKIP_4;
            break;
        case T_LAYER_2:
            SVCSkip->nSkipFrame = SKIP_2;
            break;
        default:
            SVCSkip->nSkipFrame = NO_SKIP;
            break;
    }
}

void init_aspect_ratio(VencH264AspectRatio *sAspectRatio)
{
    sAspectRatio->aspect_ratio_idc = 255;
    sAspectRatio->sar_width = 4;
    sAspectRatio->sar_height = 3;
}

void init_video_signal(VencH264VideoSignal *sVideoSignal)
{
    sVideoSignal->video_format = 5;
    sVideoSignal->src_colour_primaries = 0;
    sVideoSignal->dst_colour_primaries = 1;
}

void init_intra_refresh(VencCyclicIntraRefresh *sIntraRefresh)
{
    sIntraRefresh->bEnable = 1;
    sIntraRefresh->nBlockNumber = 10;
}

void init_roi(VencROIConfig *sRoiConfig)
{
    sRoiConfig[0].bEnable = 1;
    sRoiConfig[0].index = 0;
    sRoiConfig[0].nQPoffset = 10;
    sRoiConfig[0].sRect.nLeft = 0;
    sRoiConfig[0].sRect.nTop = 0;
    sRoiConfig[0].sRect.nWidth = 1280;
    sRoiConfig[0].sRect.nHeight = 320;

    sRoiConfig[1].bEnable = 1;
    sRoiConfig[1].index = 1;
    sRoiConfig[1].nQPoffset = 10;
    sRoiConfig[1].sRect.nLeft = 320;
    sRoiConfig[1].sRect.nTop = 180;
    sRoiConfig[1].sRect.nWidth = 320;
    sRoiConfig[1].sRect.nHeight = 180;

    sRoiConfig[2].bEnable = 1;
    sRoiConfig[2].index = 2;
    sRoiConfig[2].nQPoffset = 10;
    sRoiConfig[2].sRect.nLeft = 320;
    sRoiConfig[2].sRect.nTop = 180;
    sRoiConfig[2].sRect.nWidth = 320;
    sRoiConfig[2].sRect.nHeight = 180;

    sRoiConfig[3].bEnable = 1;
    sRoiConfig[3].index = 3;
    sRoiConfig[3].nQPoffset = 10;
    sRoiConfig[3].sRect.nLeft = 320;
    sRoiConfig[3].sRect.nTop = 180;
    sRoiConfig[3].sRect.nWidth = 320;
    sRoiConfig[3].sRect.nHeight = 180;
}

void init_alter_frame_rate_info(VencAlterFrameRateInfo *pAlterFrameRateInfo)
{
    memset(pAlterFrameRateInfo, 0 , sizeof(VencAlterFrameRateInfo));
    pAlterFrameRateInfo->bEnable = 1;
    pAlterFrameRateInfo->bUseUserSetRoiInfo = 1;
    pAlterFrameRateInfo->sRoiBgFrameRate.nSrcFrameRate = 25;
    pAlterFrameRateInfo->sRoiBgFrameRate.nDstFrameRate = 5;

    pAlterFrameRateInfo->roi_param[0].bEnable = 1;
    pAlterFrameRateInfo->roi_param[0].index = 0;
    pAlterFrameRateInfo->roi_param[0].nQPoffset = 10;
    pAlterFrameRateInfo->roi_param[0].roi_abs_flag = 1;
    pAlterFrameRateInfo->roi_param[0].sRect.nLeft = 0;
    pAlterFrameRateInfo->roi_param[0].sRect.nTop = 0;
    pAlterFrameRateInfo->roi_param[0].sRect.nWidth = 320;
    pAlterFrameRateInfo->roi_param[0].sRect.nHeight = 320;

    pAlterFrameRateInfo->roi_param[1].bEnable = 1;
    pAlterFrameRateInfo->roi_param[1].index = 0;
    pAlterFrameRateInfo->roi_param[1].nQPoffset = 10;
    pAlterFrameRateInfo->roi_param[1].roi_abs_flag = 1;
    pAlterFrameRateInfo->roi_param[1].sRect.nLeft = 320;
    pAlterFrameRateInfo->roi_param[1].sRect.nTop = 320;
    pAlterFrameRateInfo->roi_param[1].sRect.nWidth = 320;
    pAlterFrameRateInfo->roi_param[1].sRect.nHeight = 320;
}

void init_enc_proc_info(VeProcSet *ve_proc_set)
{
    ve_proc_set->bProcEnable = 1;
    ve_proc_set->nProcFreq = 60;
}

void init_overlay_info(VencOverlayInfoS *pOverlayInfo, encode_param_t *encode_param)
{
    int i;
    unsigned char num_bitMap = 2;
    BitMapInfoS* pBitMapInfo;
    unsigned int time_id_list[19];
    unsigned int start_mb_x;
    unsigned int start_mb_y;

    memset(pOverlayInfo, 0, sizeof(VencOverlayInfoS));

#if 0
    char filename[64];
    int ret;
    for(i = 0; i < num_bitMap; i++)
    {
        FILE* icon_hdle = NULL;
        int width  = 0;
        int height = 0;

        sprintf(filename, "%s%d.bmp", "/mnt/libcedarc/bitmap/icon_720p_",i);

        icon_hdle   = fopen(filename, "r");
        if (icon_hdle == NULL) {
            printf("get wartermark %s error\n", filename);
            return;
        }

        //get watermark picture size
        fseek(icon_hdle, 18, SEEK_SET);
        fread(&width, 1, 4, icon_hdle);
        fread(&height, 1, 4, icon_hdle);

        fseek(icon_hdle, 54, SEEK_SET);

        bit_map_info[i].argb_addr = NULL;
        bit_map_info[i].width = 0;
        bit_map_info[i].height = 0;

        bit_map_info[i].width = width;
        bit_map_info[i].height = height*(-1);

        bit_map_info[i].width_aligh16 = ALIGN_XXB(16, bit_map_info[i].width);
        bit_map_info[i].height_aligh16 = ALIGN_XXB(16, bit_map_info[i].height);
        if(bit_map_info[i].argb_addr == NULL) {
            bit_map_info[i].argb_addr =
            (unsigned char*)malloc(bit_map_info[i].width_aligh16*bit_map_info[i].height_aligh16*4);

            if(bit_map_info[i].argb_addr == NULL)
            {
                loge("malloc bit_map_info[%d].argb_addr fail\n", i);
                return;
            }
        }
        logd("bitMap[%d] size[%d,%d], size_align16[%d, %d], argb_addr:%p\n", i,
                                    bit_map_info[i].width,
                                    bit_map_info[i].height,
                                    bit_map_info[i].width_aligh16,
                                    bit_map_info[i].height_aligh16,
                                    bit_map_info[i].argb_addr);

        ret = fread(bit_map_info[i].argb_addr, 1,
            bit_map_info[i].width*bit_map_info[i].height*4, icon_hdle);
        if(ret != bit_map_info[i].width*bit_map_info[i].height*4)
            loge("read bitMap[%d] error, ret value:%d\n", i, ret);

        bit_map_info[i].size = bit_map_info[i].width_aligh16 * bit_map_info[i].height_aligh16 * 4;

        if (icon_hdle) {
            fclose(icon_hdle);
            icon_hdle = NULL;
        }
    }

    //time 2017-04-27 18:28:26
    time_id_list[0] = 2;
    time_id_list[1] = 0;
    time_id_list[2] = 1;
    time_id_list[3] = 7;
    time_id_list[4] = 11;
    time_id_list[5] = 0;
    time_id_list[6] = 4;
    time_id_list[7] = 11;
    time_id_list[8] = 2;
    time_id_list[9] = 7;
    time_id_list[10] = 10;
    time_id_list[11] = 1;
    time_id_list[12] = 8;
    time_id_list[13] = 12;
    time_id_list[14] = 2;
    time_id_list[15] = 8;
    time_id_list[16] = 12;
    time_id_list[17] = 2;
    time_id_list[18] = 6;

    logd("pOverlayInfo:%p\n", pOverlayInfo);
    pOverlayInfo->blk_num = 19;
#else
        FILE* icon_hdle = NULL;
        int width  = 464;
        int height = 32;
        memset(time_id_list, 0 ,sizeof(time_id_list));

        icon_hdle = fopen(encode_param->overlay_file, "r");
        logd("icon_hdle = %p",icon_hdle);
        if (icon_hdle == NULL) {
            printf("get icon_hdle error\n");
            return;
        }

        for(i = 0; i < num_bitMap; i++)
        {
            bit_map_info[i].argb_addr = NULL;
            bit_map_info[i].width = width;
            bit_map_info[i].height = height;

            bit_map_info[i].width_aligh16 = ALIGN_XXB(16, bit_map_info[i].width);
            bit_map_info[i].height_aligh16 = ALIGN_XXB(16, bit_map_info[i].height);
            if(bit_map_info[i].argb_addr == NULL) {
                bit_map_info[i].argb_addr =
            (unsigned char*)malloc(bit_map_info[i].width_aligh16*bit_map_info[i].height_aligh16*4);

                if(bit_map_info[i].argb_addr == NULL)
                {
                    loge("malloc bit_map_info[%d].argb_addr fail\n", i);
                    if (icon_hdle) {
                        fclose(icon_hdle);
                        icon_hdle = NULL;
                    }

                    return;
                }
            }
            logd("bitMap[%d] size[%d,%d], size_align16[%d, %d], argb_addr:%p\n", i,
                                                        bit_map_info[i].width,
                                                        bit_map_info[i].height,
                                                        bit_map_info[i].width_aligh16,
                                                        bit_map_info[i].height_aligh16,
                                                        bit_map_info[i].argb_addr);

            int ret;
            ret = fread(bit_map_info[i].argb_addr, 1,
                        bit_map_info[i].width*bit_map_info[i].height*4, icon_hdle);
            if(ret != (int)(bit_map_info[i].width*bit_map_info[i].height*4))
            loge("read bitMap[%d] error, ret value:%d\n", i, ret);

            bit_map_info[i].size
                = bit_map_info[i].width_aligh16 * bit_map_info[i].height_aligh16 * 4;
            fseek(icon_hdle, 0, SEEK_SET);
        }
        if (icon_hdle) {
            fclose(icon_hdle);
            icon_hdle = NULL;
        }
#endif
        pOverlayInfo->argb_type = VENC_OVERLAY_ARGB8888;
        pOverlayInfo->blk_num = num_bitMap;
        logd("blk_num:%d, argb_type:%d\n", pOverlayInfo->blk_num, pOverlayInfo->argb_type);
        //pOverlayInfo->invert_threshold = 200;
        //pOverlayInfo->invert_mode = 3;

        start_mb_x = 0;
        start_mb_y = 0;
        for(i=0; i<pOverlayInfo->blk_num; i++)
        {
            //id = time_id_list[i];
            //pBitMapInfo = &bit_map_info[id];
            pBitMapInfo = &bit_map_info[i];

            pOverlayInfo->overlayHeaderList[i].start_mb_x = start_mb_x;
            pOverlayInfo->overlayHeaderList[i].start_mb_y = start_mb_y;
            pOverlayInfo->overlayHeaderList[i].end_mb_x = start_mb_x
                                        + (pBitMapInfo->width_aligh16 / 16 - 1);
            pOverlayInfo->overlayHeaderList[i].end_mb_y = start_mb_y
                                        + (pBitMapInfo->height_aligh16 / 16 -1);

            pOverlayInfo->overlayHeaderList[i].extra_alpha_flag = 0;
            pOverlayInfo->overlayHeaderList[i].extra_alpha = 8;
            if(i%3 == 0)
                pOverlayInfo->overlayHeaderList[i].overlay_type = LUMA_REVERSE_OVERLAY;
            else if(i%2 == 0 && i!=0)
                pOverlayInfo->overlayHeaderList[i].overlay_type = COVER_OVERLAY;
            else
                pOverlayInfo->overlayHeaderList[i].overlay_type = NORMAL_OVERLAY;


            if(pOverlayInfo->overlayHeaderList[i].overlay_type == COVER_OVERLAY)
            {
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_y = 0xff;
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_u = 0xff;
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_v = 0xff;
            }

            //pOverlayInfo->overlayHeaderList[i].bforce_reverse_flag = 1;

            pOverlayInfo->overlayHeaderList[i].overlay_blk_addr = pBitMapInfo->argb_addr;
            pOverlayInfo->overlayHeaderList[i].bitmap_size = pBitMapInfo->size;

            logv("blk_%d[%d], start_mb[%d,%d], end_mb[%d,%d],extra_alpha_flag:%d, extra_alpha:%d\n",
                                i,
                                time_id_list[i],
                                pOverlayInfo->overlayHeaderList[i].start_mb_x,
                                pOverlayInfo->overlayHeaderList[i].start_mb_y,
                                pOverlayInfo->overlayHeaderList[i].end_mb_x,
                                pOverlayInfo->overlayHeaderList[i].end_mb_y,
                                pOverlayInfo->overlayHeaderList[i].extra_alpha_flag,
                                pOverlayInfo->overlayHeaderList[i].extra_alpha);
            logv("overlay_type:%d, cover_yuv[%d,%d,%d], overlay_blk_addr:%p, bitmap_size:%d\n",
                                pOverlayInfo->overlayHeaderList[i].overlay_type,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_y,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_u,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_v,
                                pOverlayInfo->overlayHeaderList[i].overlay_blk_addr,
                                pOverlayInfo->overlayHeaderList[i].bitmap_size);
            //if(i != 5)
            {
                start_mb_x += pBitMapInfo->width_aligh16 / 16;
                start_mb_y += pBitMapInfo->height_aligh16 / 16;
            }
        }

    return;
}

int init_Roi_config(VencBaseConfig *baseConfig, VencCopyROIConfig*pRoiConfig)
{
    unsigned int i, j, idx, num, nyLen;
    struct ScMemOpsS *_memops = baseConfig->memops;
    void *veOps = (void *)baseConfig->veOpsS;
    void *pVeopsSelf = baseConfig->pVeOpsSelf;
    CEDARC_UNUSE(_memops);
    CEDARC_UNUSE(veOps);
    CEDARC_UNUSE(pVeopsSelf);

    nyLen = 0;
    num = 16;
    pRoiConfig->bEnable = 1;
    pRoiConfig->num = num;

    for(i=0; i<4; i++)
    {
         for(j=0; j<4; j++)
         {
              idx = i*4 + j;
              pRoiConfig->sRect[idx].nTop = 270*i;
              pRoiConfig->sRect[idx].nLeft = 430*j;
              pRoiConfig->sRect[idx].nWidth= 320;
              pRoiConfig->sRect[idx].nHeight = 240;
              nyLen += ((640+15)&(~15))*((480+15)&(~15));
          }
     }

     pRoiConfig->pRoiYAddrVir = (unsigned char*)EncAdapterMemPalloc(nyLen);
     if(pRoiConfig->pRoiYAddrVir == NULL)
     {
          loge("error:palloc roi Y buffer error,return -1");
          return -1;
     }
     memset(pRoiConfig->pRoiYAddrVir, 0x80, nyLen);
     EncAdapterMemFlushCache(pRoiConfig->pRoiYAddrVir, nyLen);
     pRoiConfig->pRoiYAddrPhy =
            (unsigned long)EncAdapterMemGetPhysicAddress(pRoiConfig->pRoiYAddrVir);

      pRoiConfig->pRoiCAddrVir = (unsigned char*)EncAdapterMemPalloc(nyLen/2);
      if(pRoiConfig->pRoiCAddrVir == NULL)
      {
           loge("error:palloc roi C buffer error,return -1");
           EncAdapterMemPfree(pRoiConfig->pRoiYAddrVir);
           pRoiConfig->pRoiYAddrVir= NULL;
           return -1;
      }
      memset(pRoiConfig->pRoiCAddrVir, 0x80, nyLen/2);
      EncAdapterMemFlushCache(pRoiConfig->pRoiCAddrVir, nyLen/2);
      pRoiConfig->pRoiCAddrPhy =
            (unsigned long)EncAdapterMemGetPhysicAddress(pRoiConfig->pRoiCAddrVir);

      logd("nRoiYLen=%d", nyLen);
      logd("pRoiYAddrVir=%p, pRoiCAddrVir=%p", pRoiConfig->pRoiYAddrVir, pRoiConfig->pRoiCAddrVir);
      logd("pRoiYAddrPhy=%lx, pRoiCAddrPhy=%lx", pRoiConfig->pRoiYAddrPhy, pRoiConfig->pRoiCAddrPhy);
      return 0;
}



void init_jpeg_rate_ctrl(jpeg_func_t *jpeg_func)
{
    jpeg_func->jpeg_biteRate = 12*1024*1024;
    jpeg_func->jpeg_frameRate = 30;
    jpeg_func->bitRateRange.bitRateMax = 14*1024*1024;
    jpeg_func->bitRateRange.bitRateMin = 10*1024*1024;
}

int initH264Func(h264_func_t *h264_func, encode_param_t *encode_param)
{
    memset(h264_func, 0, sizeof(h264_func_t));

    //init h264Param
    h264_func->h264Param.bEntropyCodingCABAC = 1;
    h264_func->h264Param.nBitrate = encode_param->bit_rate;
    h264_func->h264Param.nFramerate = encode_param->frame_rate;
    h264_func->h264Param.nCodingMode = VENC_FRAME_CODING;
    h264_func->h264Param.nMaxKeyInterval = encode_param->maxKeyFrame;
    h264_func->h264Param.sProfileLevel.nProfile = VENC_H264ProfileHigh;
    h264_func->h264Param.sProfileLevel.nLevel = VENC_H264Level51;

#ifdef VBR
    h264_func->h264Param.sRcParam.eRcMode = AW_VBR;
#endif

    if(encode_param->qp_min > 0 && encode_param->qp_max)
    {
        h264_func->h264Param.sQPRange.nMinqp = encode_param->qp_min;
        h264_func->h264Param.sQPRange.nMaxqp = encode_param->qp_max;

    }
    else
    {
        h264_func->h264Param.sQPRange.nMinqp = 10;
        h264_func->h264Param.sQPRange.nMaxqp = 50;
    }
    //h264_func->h264Param.bLongRefEnable = 1;
    //h264_func->h264Param.nLongRefPoc = 0;

#if 1
    h264_func->sH264Smart.img_bin_en = 1;
    h264_func->sH264Smart.img_bin_th = 27;
    h264_func->sH264Smart.shift_bits = 2;
    h264_func->sH264Smart.smart_fun_en = 1;
#endif

    //init VencMBModeCtrl
    init_mb_mode(&h264_func->h264MBMode, encode_param);

    //init VencMBInfo
    init_mb_info(&h264_func->MBInfo, encode_param);

    //init VencH264FixQP
    init_fix_qp(&h264_func->fixQP);

    //init VencSuperFrameConfig
    init_super_frame_cfg(&h264_func->sSuperFrameCfg);

    //init VencH264SVCSkip
    init_svc_skip(&h264_func->SVCSkip);

    //init VencH264AspectRatio
    init_aspect_ratio(&h264_func->sAspectRatio);

    //init VencH264AspectRatio
    init_video_signal(&h264_func->sVideoSignal);

    //init CyclicIntraRefresh
    init_intra_refresh(&h264_func->sIntraRefresh);

    //init VencROIConfig
    init_roi(h264_func->sRoiConfig);

    //init proc info
    init_enc_proc_info(&h264_func->sVeProcInfo);

    //init VencOverlayConfig
    init_overlay_info(&h264_func->sOverlayInfo, encode_param);

    return 0;
}

int initH265Func(h265_func_t *h265_func, encode_param_t *encode_param)
{
    memset(h265_func, 0, sizeof(h264_func_t));

    //init h265Param
    h265_func->h265Param.nBitrate = encode_param->bit_rate;
    h265_func->h265Param.nFramerate = encode_param->frame_rate;
    h265_func->h265Param.sProfileLevel.nProfile = VENC_H265ProfileMain;
    h265_func->h265Param.sProfileLevel.nLevel = VENC_H265Level41;
    h265_func->h265Param.nQPInit = 30;
    h265_func->h265Param.idr_period = 30;
    h265_func->h265Param.nGopSize = h265_func->h265Param.idr_period;
    h265_func->h265Param.nIntraPeriod = h265_func->h265Param.idr_period;

#ifdef VBR
    h265_func->h265Param.sRcParam.eRcMode = AW_VBR;
#endif

    if(encode_param->qp_min > 0 && encode_param->qp_max)
    {
        h265_func->h265Param.sQPRange.nMaxqp = encode_param->qp_max;
        h265_func->h265Param.sQPRange.nMinqp = encode_param->qp_min;
    }
    else
    {
        h265_func->h265Param.sQPRange.nMaxqp = 52;
        h265_func->h265Param.sQPRange.nMinqp = 10;
    }


    //h265_func->h265Param.bLongTermRef = 1;

#if 1
    h265_func->h265Hvs.hvs_en = 1;
    h265_func->h265Hvs.th_dir = 24;
    h265_func->h265Hvs.th_coef_shift = 4;

    h265_func->h265Trc.inter_tend = 63;
    h265_func->h265Trc.skip_tend = 3;
    h265_func->h265Trc.merge_tend = 0;

    h265_func->h265Smart.img_bin_en = 1;
    h265_func->h265Smart.img_bin_th = 27;
    h265_func->h265Smart.shift_bits = 2;
    h265_func->h265Smart.smart_fun_en = 1;
#endif

    h265_func->h265_rc_frame_total = 20*h265_func->h265Param.nGopSize;

    //init H265Gop
    init_h265_gop(&h265_func->h265Gop);

    //init VencMBModeCtrl
    init_mb_mode(&h265_func->h265MBMode, encode_param);

    //init VencMBInfo
    init_mb_info(&h265_func->MBInfo, encode_param);

    //init VencH264FixQP
    init_fix_qp(&h265_func->fixQP);

    //init VencSuperFrameConfig
    init_super_frame_cfg(&h265_func->sSuperFrameCfg);

    //init VencH264SVCSkip
    init_svc_skip(&h265_func->SVCSkip);

    //init VencH264AspectRatio
    init_aspect_ratio(&h265_func->sAspectRatio);

    //init VencH264AspectRatio
    init_video_signal(&h265_func->sVideoSignal);

    //init CyclicIntraRefresh
    init_intra_refresh(&h265_func->sIntraRefresh);

    //init VencROIConfig
    init_roi(h265_func->sRoiConfig);

    //init alter frameRate info
    init_alter_frame_rate_info(&h265_func->sAlterFrameRateInfo);

    //init proc info
    init_enc_proc_info(&h265_func->sVeProcInfo);

    //init VencOverlayConfig
    init_overlay_info(&h265_func->sOverlayInfo, encode_param);

    return 0;
}

int initJpegFunc(jpeg_func_t *jpeg_func, encode_param_t *encode_param)
{
    memset(jpeg_func, 0, sizeof(jpeg_func_t));

    jpeg_func->quality = 95;
    if(encode_param->encode_frame_num > 1)
        jpeg_func->jpeg_mode = 1;
    else
        jpeg_func->jpeg_mode = 0;

    if(0 == jpeg_func->jpeg_mode)
        init_jpeg_exif(&jpeg_func->exifinfo);
    else if(1 == jpeg_func->jpeg_mode)
        init_jpeg_rate_ctrl(jpeg_func);
    else
    {
        loge("encoder do not support the jpeg_mode:%d\n", jpeg_func->jpeg_mode);
        return -1;
    }

     //init VencOverlayConfig
    init_overlay_info(&jpeg_func->sOverlayInfo, encode_param);

    return 0;

}

void getTimeString(char *str, int len)
{
    time_t timep;
    struct tm *p;
    int year, month, day, hour, min, sec;

    time(&timep);
    p = localtime(&timep);
    year = p->tm_year + 1900;
    month = p->tm_mon + 1;
    day = p->tm_mday;
    hour = p->tm_hour;
    min = p->tm_min;
    sec = p->tm_sec;

    snprintf(str, len, "%d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, min, sec);
}

int setEncParam(VideoEncoder *pVideoEnc ,encode_param_t *encode_param)
{
    int result = 0;
    VeProcSet mProcSet;
    mProcSet.bProcEnable = 1;
    mProcSet.nProcFreq = 30;
    mProcSet.nStatisBitRateTime = 1000;
    mProcSet.nStatisFrRateTime  = 1000;
    VideoEncSetParameter(pVideoEnc, VENC_IndexParamProcSet, &mProcSet);

    if(encode_param->encode_format == VENC_CODEC_JPEG)
    {
        result = initJpegFunc(&encode_param->jpeg_func, encode_param);
        if(result)
        {
            loge("initJpegFunc error, return\n");
            return -1;
        }

        if(1 == encode_param->jpeg_func.jpeg_mode)
        {
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamJpegEncMode, &encode_param->jpeg_func.jpeg_mode);
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamBitrate, &encode_param->jpeg_func.jpeg_biteRate);
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamFramerate, &encode_param->jpeg_func.jpeg_frameRate);
            VideoEncSetParameter(pVideoEnc,
                                    VENC_IndexParamSetBitRateRange, &encode_param->jpeg_func.bitRateRange);

        }
        else
        {
            unsigned int vbv_size = 4*1024*1024;
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamJpegQuality, &encode_param->jpeg_func.quality);
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamJpegExifInfo, &encode_param->jpeg_func.exifinfo);
        }

        if(encode_param->jpeg_func.pRoiConfig.bEnable == 1)
        {
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamRoi, &encode_param->jpeg_func.pRoiConfig);
        }

        if(encode_param->test_overlay_flag == 1)
        {
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetOverlay, &encode_param->jpeg_func.sOverlayInfo);
        }
    }
    else if(encode_param->encode_format == VENC_CODEC_H264)
    {
        result = initH264Func(&encode_param->h264_func, encode_param);
        if(result)
        {
            loge("initH264Func error, return\n");
            return -1;
        }
        unsigned int vbv_size = 12*1024*1024;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264Param, &encode_param->h264_func.h264Param);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &h264_func.fixQP);
        if(encode_param->test_overlay_flag == 1)
        {
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetOverlay, &encode_param->h264_func.sOverlayInfo);
        }

        VideoEncSetParameter(pVideoEnc, VENC_IndexParamProcSet, &encode_param->h264_func.sVeProcInfo);
#ifdef USE_VIDEO_SIGNAL
        VencH264VideoSignal mVencH264VideoSignal;
        memset(&mVencH264VideoSignal, 0, sizeof(VencH264VideoSignal));
        mVencH264VideoSignal.video_format = DEFAULT;
        mVencH264VideoSignal.full_range_flag = 1;
        mVencH264VideoSignal.src_colour_primaries = VENC_BT709;
        mVencH264VideoSignal.dst_colour_primaries = VENC_BT709;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264VideoSignal, &mVencH264VideoSignal);
#endif

#ifdef DETECT_MOTION
        MotionParam mMotionPara;
        mMotionPara.nMaxNumStaticFrame = 4;
        mMotionPara.nMotionDetectEnable = 1;
        mMotionPara.nMotionDetectRatio = 0;
        mMotionPara.nMV64x64Ratio = 0.01;
        mMotionPara.nMVXTh = 6;
        mMotionPara.nMVYTh = 6;
        mMotionPara.nStaticBitsRatio = 0.2;
        mMotionPara.nStaticDetectRatio = 2;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamMotionDetectStatus,
                                                &mMotionPara);
#endif

#ifdef USE_VIDEO_SIGNAL
        VencH264VideoSignal mVencH264VideoSignal;
        memset(&mVencH264VideoSignal, 0, sizeof(VencH264VideoSignal));
        mVencH264VideoSignal.video_format = DEFAULT;
        mVencH264VideoSignal.full_range_flag = 1;
        mVencH264VideoSignal.src_colour_primaries = VENC_BT709;
        mVencH264VideoSignal.dst_colour_primaries = VENC_BT709;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264VideoSignal, &mVencH264VideoSignal);
#endif

#ifdef DETECT_MOTION
        MotionParam mMotionPara;
        mMotionPara.nMaxNumStaticFrame = 4;
        mMotionPara.nMotionDetectEnable = 1;
        mMotionPara.nMotionDetectRatio = 0;
        mMotionPara.nMV64x64Ratio = 0.01;
        mMotionPara.nMVXTh = 6;
        mMotionPara.nMVYTh = 6;
        mMotionPara.nStaticBitsRatio = 0.2;
        mMotionPara.nStaticDetectRatio = 2;

        VideoEncSetParameter(pVideoEnc, VENC_IndexParamMotionDetectStatus,
                                                &mMotionPara);
#endif

        //int tmptmp = 60;
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamVirtualIFrame, &tmptmp);

#ifdef GET_MB_INFO
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamMBInfoOutput, &encode_param->h264_func.MBInfo);
#endif


#if 0
        unsigned char value = 1;
        //set the specify func
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264SVCSkip, &h264_func.SVCSkip);
        value = 0;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamIfilter, &value);
        value = 0; //degree
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamRotation, &value);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &h264_func.fixQP);
        VideoEncSetParameter(pVideoEnc,
            VENC_IndexParamH264CyclicIntraRefresh, &h264_func.sIntraRefresh);
        value = 720/4;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamSliceHeight, &value);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &h264_func.sRoiConfig[0]);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &h264_func.sRoiConfig[1]);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &h264_func.sRoiConfig[2]);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &h264_func.sRoiConfig[3]);
        value = 0;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetPSkip, &value);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264AspectRatio, &h264_func.sAspectRatio);
        value = 0;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamFastEnc, &value);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264VideoSignal, &h264_func.sVideoSignal);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamSuperFrameConfig, &h264_func.sSuperFrameCfg);
#endif
    }
    else if(encode_param->encode_format == VENC_CODEC_H265)
    {
        result = initH265Func(&encode_param->h265_func, encode_param);
        if(result)
        {
            loge("initH265Func error, return\n");
            return -1;
        }
        unsigned int vbv_size = 12*1024*1024;
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamH265Param, &encode_param->h265_func.h265Param);


        unsigned int value = 1;
        if(encode_param->test_overlay_flag == 1)
        {
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetOverlay, &encode_param->h265_func.sOverlayInfo);
        }
        //VideoEncSetParameter(pVideoEnc,
        //VENC_IndexParamAlterFrame, &h265_func.sAlterFrameRateInfo);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamChannelNum, &value);
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamProcSet, &encode_param->h265_func.sVeProcInfo);
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamVirtualIFrame, &encode_param->frame_rate);
        //value = 0;
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamPFrameIntraEn, &value);
        //value = 1;
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamEncodeTimeEn, &value);
        //VideoEncSetParameter(pVideoEnc,
        //VENC_IndexParamH265ToalFramesNum,  &h265_func.h265_rc_frame_total);
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamH265Gop, &h265_func.h265Gop);

        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &h265_func.sRoiConfig[0]);
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &h265_func.fixQP);
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamH265HVS, &h265_func.h265Hvs);
        //VideoEncSetParameter(pVideoEnc, VENC_IndexParamH265TendRatioCoef, &h265_func.h265Trc);
#ifdef GET_MB_INFO
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamMBInfoOutput, &encode_param->h265_func.MBInfo);
#endif
    }

    return 0;
}

void setMbMode(VideoEncoder *pVideoEnc, encode_param_t *encode_param)
{
    if(encode_param->encode_format == VENC_CODEC_H264 && encode_param->h264_func.h264MBMode.mode_ctrl_en)
    {
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamMBModeCtrl, &encode_param->h264_func.h264MBMode);
    }
    else if(encode_param->encode_format == VENC_CODEC_H265 && encode_param->h265_func.h265MBMode.mode_ctrl_en)
    {
        VideoEncSetParameter(pVideoEnc, VENC_IndexParamMBModeCtrl, &encode_param->h265_func.h265MBMode);
    }
    else
        return;
}

void getMbMinfo(encode_param_t *encode_param)
{
    VencMBInfo *pMBInfo;

    if(encode_param->encode_format == VENC_CODEC_H264 && encode_param->h264_func.h264MBMode.mode_ctrl_en)
    {
        pMBInfo = &encode_param->h264_func.MBInfo;
    }
    else if(encode_param->encode_format == VENC_CODEC_H265 && encode_param->h265_func.h265MBMode.mode_ctrl_en)
    {
        pMBInfo = &encode_param->h265_func.MBInfo;
    }
    else
        return;


    unsigned int i;
    for(i = 0; i < pMBInfo->num_mb; i++)
    {
        logv("No.%d MB: mad=%d, qp=%d, sse=%d, psnr=%f\n",i,pMBInfo->p_para[i].mb_mad,
        pMBInfo->p_para[i].mb_qp, pMBInfo->p_para[i].mb_sse, pMBInfo->p_para[i].mb_psnr);
    }
}

void releaseMb(encode_param_t *encode_param)
{
    VencMBInfo *pMBInfo;
    VencMBModeCtrl *pMBMode;
    if(encode_param->encode_format == VENC_CODEC_H264 && encode_param->h264_func.h264MBMode.mode_ctrl_en)
    {
        pMBInfo = &encode_param->h264_func.MBInfo;
        pMBMode = &encode_param->h264_func.h264MBMode;
    }
    else if(encode_param->encode_format == VENC_CODEC_H265 && encode_param->h265_func.h265MBMode.mode_ctrl_en)
    {
        pMBInfo = &encode_param->h264_func.MBInfo;
        pMBMode = &encode_param->h265_func.h265MBMode;
    }
    else
        return;

    if(pMBInfo->p_para)
        free(pMBInfo->p_para);
    if(pMBMode->p_info)
        free(pMBMode->p_info);
}

static int saveJpegPic(VencOutputBuffer *pOutputBuffer, const char* out_path,int nTestNum)
{
    char name[128];

    snprintf(name, sizeof(name), "%s_%d.jpg",out_path, nTestNum) < 0 ? abort() : (void)0;

    FILE *out_file = fopen(name, "wb");

    if(out_file == NULL)
    {
        logw("open file failed : %s", name);
        return -1;
    }

    fwrite(pOutputBuffer->pData0, 1, pOutputBuffer->nSize0, out_file);

    if(pOutputBuffer->nSize1)
    {
       fwrite(pOutputBuffer->pData1, 1, pOutputBuffer->nSize1, out_file);
    }

    if(pOutputBuffer->nSize2)
    {
       fwrite(pOutputBuffer->pData2, 1, pOutputBuffer->nSize2, out_file);
    }

    fclose(out_file);
    return 0;
}

#if 1
static int testIspFunction(encode_param_t* pEncodeParam)
{
    int bTestCropFlag        = 0;
    int bTestScaleFlag       = 1;
    int bTestRotateAngle     = 0;
    int bTestOverlayerFlag   = 0;
    int bTestHorizonflipFlag = 0;
    int nColorspaceYuv2Yuv   = 0;
    int nColorspaceRgb2Yuv   = 0;

    VideoEncoderIsp *pIsp = NULL;
    struct ScMemOpsS *memops = NULL;
    VencIspBufferInfo mInBuffer;
    VencIspBufferInfo mOutBuffer;
    VeOpsS*           veOpsS = NULL;
    void*             pVeOpsSelf = NULL;
    int ret = 0;

    FILE *in_file = NULL;
    FILE *out_file = NULL;
    char *input_path = NULL;
    char *output_path = NULL;
    input_path = pEncodeParam->intput_file;
    output_path = pEncodeParam->output_file;

    memset(&mInBuffer, 0, sizeof(VencIspBufferInfo));
    memset(&mOutBuffer, 0, sizeof(VencIspBufferInfo));

    mInBuffer.colorFormat = VENC_PIXEL_YUV420P;
    mInBuffer.nWidth   = ALIGN_XXB(16, pEncodeParam->src_width);
    mInBuffer.nHeight  = ALIGN_XXB(16, pEncodeParam->src_height);
    mOutBuffer.nWidth  = ALIGN_XXB(16, pEncodeParam->dst_width);
    mOutBuffer.nHeight = ALIGN_XXB(16, pEncodeParam->dst_height);

    in_file = fopen(input_path, "r");
    if(in_file == NULL)
    {
        loge("open in_file fail\n");
        ret = -1;
        goto EXIT;
    }
    out_file = fopen(output_path, "wb");
    if(out_file == NULL)
    {
        loge("open out_file fail\n");
        ret = -1;
        goto EXIT;
    }

    //* get mem ops
    memops = MemAdapterGetOpsS();
    if(memops == NULL)
    {
        loge("MemAdapterGetOpsS failed");
        ret = -1;
        goto EXIT;
    }
    CdcMemOpen(memops);

    //* get ve ops
    int type = VE_OPS_TYPE_NORMAL;
    veOpsS = GetVeOpsS(type);
    if(veOpsS == NULL)
    {
        loge("get ve ops failed , type = %d",type);
        ret = -1;
        goto EXIT;
    }


    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 0;
    mVeConfig.nEncoderFlag = 1;
    mVeConfig.nEnableAfbcFlag = 0;
    mVeConfig.nFormat = 0;
    mVeConfig.nWidth = 0;
    mVeConfig.nResetVeMode = 0;
    mVeConfig.memops = memops;
    pVeOpsSelf = CdcVeInit(veOpsS, &mVeConfig);
    if(pVeOpsSelf == NULL)
    {
        loge("init ve ops failed");
        ret = -1;
        goto EXIT;
    }

    pIsp = VideoEncIspCreate();
    if(pIsp == NULL)
    {
        loge("VideoEncIspCreate failed");
        ret = -1;
        goto EXIT;
    }

    //* palloc in and out buffer
    int nInBufferSize = mInBuffer.nWidth*mInBuffer.nHeight*3/2;
    mInBuffer.pAddrVirY = CdcMemPalloc(memops, nInBufferSize, veOpsS, pVeOpsSelf);
    if(mInBuffer.pAddrVirY == NULL)
    {
        loge("palloc failed , size = %d",nInBufferSize);
        ret = -1;
        goto EXIT;
    }
    mInBuffer.pAddrVirC = mInBuffer.pAddrVirY + mInBuffer.nWidth*mInBuffer.nHeight;
    mInBuffer.pAddrPhyY = CdcMemGetPhysicAddress(memops, mInBuffer.pAddrVirY);
    mInBuffer.pAddrPhyC0 = mInBuffer.pAddrPhyY + mInBuffer.nWidth*mInBuffer.nHeight;
    mInBuffer.pAddrPhyC1 = mInBuffer.pAddrPhyC0 + mInBuffer.nWidth*mInBuffer.nHeight/4;

    int nOutBufferSize = mOutBuffer.nWidth*mOutBuffer.nHeight*3/2;
    mOutBuffer.pAddrVirY = CdcMemPalloc(memops, nOutBufferSize, veOpsS, pVeOpsSelf);
    if(mOutBuffer.pAddrVirY == NULL)
    {
        loge("palloc failed , size = %d",nOutBufferSize);
        ret = -1;
        goto EXIT;
    }
    mOutBuffer.pAddrVirC = mOutBuffer.pAddrVirY + mOutBuffer.nWidth*mOutBuffer.nHeight;
    mOutBuffer.pAddrPhyY = CdcMemGetPhysicAddress(memops, mOutBuffer.pAddrVirY);
    mOutBuffer.pAddrPhyC0 = mOutBuffer.pAddrPhyY + mOutBuffer.nWidth*mOutBuffer.nHeight;
    mOutBuffer.pAddrPhyC1 = mOutBuffer.pAddrPhyC0 + mOutBuffer.nWidth*mOutBuffer.nHeight/4;

    //* read input data
    int size = fread(mInBuffer.pAddrVirY, 1, nInBufferSize, in_file);
    logv("fread data: %d, %d",size, nInBufferSize);
    if(size != nInBufferSize)
    {
        fseek(in_file, 0L, SEEK_SET);
        size = fread(mInBuffer.pAddrVirY, 1, nInBufferSize, in_file);
    }
    CdcMemFlushCache(memops, mInBuffer.pAddrVirY, nInBufferSize);

    //* call function
    VencIspFunction mIspFunction;
    VencRect mCropInfo;
    VencOverlayInfoS sOverlayInfo;
    memset(&mIspFunction, 0, sizeof(VencIspFunction));
    memset(&mCropInfo, 0, sizeof(VencRect));
    memset(&sOverlayInfo, 0, sizeof(VencOverlayInfoS));

    mIspFunction.bScaleFlag       = bTestScaleFlag;
    mIspFunction.nRotateAngle     = bTestRotateAngle;
    mIspFunction.bHorizonflipFlag = bTestHorizonflipFlag;
    mIspFunction.bCropFlag        = bTestCropFlag;
    mIspFunction.bOverlayerFlag   = bTestOverlayerFlag;
    mIspFunction.nColorSpaceYuv2Yuv = nColorspaceYuv2Yuv;
    mIspFunction.nColorSpaceRgb2Yuv = nColorspaceRgb2Yuv;

    if(mIspFunction.nRotateAngle == 1 || mIspFunction.nRotateAngle == 3)
    {
        unsigned int tmpSize = mOutBuffer.nWidth;
        mOutBuffer.nWidth  = mOutBuffer.nHeight;
        mOutBuffer.nHeight = tmpSize;
    }
    if(mIspFunction.bCropFlag == 1)
    {
        mCropInfo.nLeft = 128;
        mCropInfo.nTop  = 128;
        mCropInfo.nWidth = mInBuffer.nWidth - 128;
        mCropInfo.nHeight = mInBuffer.nHeight - 128;
        mIspFunction.pCropInfo = &mCropInfo;
    }
    if(mIspFunction.bOverlayerFlag == 1)
    {
        init_overlay_info(&sOverlayInfo, pEncodeParam);
        mIspFunction.pOverlayerInfo = &sOverlayInfo;
    }

    VideoEncIspFunction(pIsp, &mInBuffer, &mOutBuffer, &mIspFunction);

    //* save data
    CdcMemFlushCache(memops, mOutBuffer.pAddrVirY, nOutBufferSize);
    fwrite(mOutBuffer.pAddrVirY, 1, nOutBufferSize, out_file);

EXIT:
    //* free
    if(in_file)
        fclose(in_file);

    if(out_file)
        fclose(out_file);

    if(veOpsS)
    {
        if(memops)
        {
            if(mInBuffer.pAddrVirY)
                CdcMemPfree(memops, mInBuffer.pAddrVirY, veOpsS,pVeOpsSelf);
            if(mOutBuffer.pAddrVirY)
                CdcMemPfree(memops, mOutBuffer.pAddrVirY, veOpsS,pVeOpsSelf);
        }
        CdcVeRelease(veOpsS,pVeOpsSelf);
    }

    if(memops)
        CdcMemClose(memops);

    if(pIsp)
        VideoEncIspDestroy(pIsp);

    return ret;
}
#else
static int testIspFunction(encode_param_t* pEncodeParam)
{
    VideoEncoderIsp *pIsp = NULL;
    struct ScMemOpsS *memops = NULL;
    VencIspBufferInfo mInBuffer;
    VencIspBufferInfo mOutBuffer;
    VeOpsS*           veOpsS = NULL;
    void*             pVeOpsSelf = NULL;
    int ret = 0;

    FILE *in_file = NULL;
    FILE *out_file = NULL;
    int nInBufferSize;
    int nOutBufferSize;
    char *input_path = NULL;
    char *output_path = NULL;
    input_path = pEncodeParam->intput_file;
    output_path = pEncodeParam->output_file;

    memset(&mInBuffer, 0, sizeof(VencIspBufferInfo));
    memset(&mOutBuffer, 0, sizeof(VencIspBufferInfo));

    mInBuffer.colorFormat = VENC_PIXEL_YUV420P;
    mInBuffer.nWidth   = ALIGN_XXB(16, pEncodeParam->src_width);
    mInBuffer.nHeight  = ALIGN_XXB(16, pEncodeParam->src_height);
    mOutBuffer.nWidth  = ALIGN_XXB(16, pEncodeParam->dst_width);
    mOutBuffer.nHeight = ALIGN_XXB(16, pEncodeParam->dst_height);

    in_file = fopen(input_path, "r");
    if(in_file == NULL)
    {
        loge("open in_file fail\n");
        ret = -1;
        goto EXIT;
    }
    out_file = fopen(output_path, "wb");
    if(out_file == NULL)
    {
        loge("open out_file fail\n");
        ret = -1;
        goto EXIT;
    }

    //* get mem ops
    memops = MemAdapterGetOpsS();
    if(memops == NULL)
    {
        loge("MemAdapterGetOpsS failed");
        ret = -1;
        goto EXIT;
    }
    CdcMemOpen(memops);

    //* get ve ops
    int type = VE_OPS_TYPE_NORMAL;
    veOpsS = GetVeOpsS(type);
    if(veOpsS == NULL)
    {
        loge("get ve ops failed , type = %d",type);
        ret = -1;
        goto EXIT;
    }

    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 0;
    mVeConfig.nEncoderFlag = 1;
    mVeConfig.nEnableAfbcFlag = 0;
    mVeConfig.nFormat = 0;
    mVeConfig.nWidth = 0;
    mVeConfig.nResetVeMode = 0;
    mVeConfig.memops = memops;
    pVeOpsSelf = CdcVeInit(veOpsS, &mVeConfig);
    if(pVeOpsSelf == NULL)
    {
        loge("init ve ops failed");
        ret = -1;
        goto EXIT;
    }

    //* palloc in and out buffer
    nInBufferSize = mInBuffer.nWidth*mInBuffer.nHeight*3/2;
    mInBuffer.pAddrVirY = CdcMemPalloc(memops, nInBufferSize, veOpsS, pVeOpsSelf);
    if(mInBuffer.pAddrVirY == NULL)
    {
        loge("palloc failed , size = %d",nInBufferSize);
        ret = -1;
        goto EXIT;
    }
    mInBuffer.pAddrVirC = mInBuffer.pAddrVirY + mInBuffer.nWidth*mInBuffer.nHeight;
    mInBuffer.pAddrPhyY = CdcMemGetPhysicAddress(memops, mInBuffer.pAddrVirY);
    mInBuffer.pAddrPhyC0 = mInBuffer.pAddrPhyY + mInBuffer.nWidth*mInBuffer.nHeight;
    mInBuffer.pAddrPhyC1 = mInBuffer.pAddrPhyC0 + mInBuffer.nWidth*mInBuffer.nHeight/4;

    VencRect sCropInfo;
    sCropInfo.nLeft  = 320;
    sCropInfo.nTop= 320;
    sCropInfo.nWidth = 320;
    sCropInfo.nHeight = 320;

    nOutBufferSize = mOutBuffer.nWidth*mOutBuffer.nHeight*3/2;
    mOutBuffer.pAddrVirY = CdcMemPalloc(memops, nOutBufferSize, veOpsS, pVeOpsSelf);
    if(mOutBuffer.pAddrVirY == NULL)
    {
        loge("palloc failed , size = %d",nOutBufferSize);
        ret = -1;
        goto EXIT;
    }
    mOutBuffer.pAddrVirC = mOutBuffer.pAddrVirY + mOutBuffer.nWidth*mOutBuffer.nHeight;
    mOutBuffer.pAddrPhyY = CdcMemGetPhysicAddress(memops, mOutBuffer.pAddrVirY);
    mOutBuffer.pAddrPhyC0 = mOutBuffer.pAddrPhyY + mOutBuffer.nWidth*mOutBuffer.nHeight;
    mOutBuffer.pAddrPhyC1 = mOutBuffer.pAddrPhyC0 + mOutBuffer.nWidth*mOutBuffer.nHeight/4;

    logd("the_123, outbuffer:width=%d, height=%d, Y=%p, C0=%p, C1=%p", \
        mOutBuffer.nWidth, mOutBuffer.nHeight, mOutBuffer.pAddrPhyY, \
        mOutBuffer.pAddrPhyC0, mOutBuffer.pAddrPhyC1);
    //* read input data
    int size = fread(mInBuffer.pAddrVirY, 1, nInBufferSize, in_file);
    logd("fread data: %d, %d",size, nInBufferSize);
    if(size != nInBufferSize)
    {
        fseek(in_file, 0L, SEEK_SET);
        size = fread(mInBuffer.pAddrVirY, 1, nInBufferSize, in_file);
    }
    CdcMemFlushCache(memops, mInBuffer.pAddrVirY, nInBufferSize);

    AWCropYuv(&mInBuffer, &sCropInfo, &mOutBuffer);
    //* save data
    CdcMemFlushCache(memops, mOutBuffer.pAddrVirY, nOutBufferSize);
    char name[128];
    sprintf(name, "/data/camera/crop_%dx%d-%dx%d.yuv", \
        pEncodeParam->dst_width, pEncodeParam->dst_height, \
        mOutBuffer.nWidth, mOutBuffer.nHeight);
    out_file = fopen(name, "wb");
    if(out_file == NULL)
    {
        loge("open out_file fail\n");
        ret = -1;
        goto EXIT;
    }

    fwrite(mOutBuffer.pAddrVirY, 1, nOutBufferSize, out_file);

EXIT:
    //* free
    if(in_file)
        fclose(in_file);

    if(out_file)
        fclose(out_file);

    if(veOpsS)
    {
        if(memops)
        {
            if(mInBuffer.pAddrVirY)
                CdcMemPfree(memops, mInBuffer.pAddrVirY, veOpsS,pVeOpsSelf);
            if(mOutBuffer.pAddrVirY)
                CdcMemPfree(memops, mOutBuffer.pAddrVirY, veOpsS,pVeOpsSelf);
        }
        CdcVeRelease(veOpsS,pVeOpsSelf);
    }

    if(memops)
        CdcMemClose(memops);

    return ret;
}

#endif

void* ChannelThread(void* param)
{
    encoder_Context* pEncContext = (encoder_Context*)param;

    VencBaseConfig baseConfig;
    VencAllocateBufferParam bufferParam;
    VideoEncoder* pVideoEnc = NULL;
    VencInputBuffer inputBuffer;
    VencOutputBuffer outputBuffer;
    VencHeaderData sps_pps_data;
    unsigned char *uv_tmp_buffer = NULL;
    unsigned int afbc_header_size;

    int result = 0;
    int i = 0;
    long long pts = 0;
    unsigned char yu12_nv12_flag = 0;
    unsigned char yu12_nv21_flag = 0;
    char logcat_buf[1024];

    FILE *in_file = NULL;
    FILE *out_file = NULL;
    FILE *reference_file = NULL;
    FILE *log_file = NULL;
    char *input_path = NULL;
    char *output_path = NULL;
    char *reference_path = NULL;
    char *log_path = NULL;
    char *reference_buffer = NULL;
    char *log_buffer = logcat_buf;
    int log_len = 0;
    //int value;

    unsigned int m = 0;
    unsigned int cycle_num = 1;
    long long timeMin = 0;
    long long timeMax = 0;
    long long time1=0;
    long long time2=0;
    long long time3=0;
    long long time_start=0;
    long long time_end=0;
    long long time_begin=0;

    int bHadStartTimeFlag = 0;
    long long nFirstEncodePicTime = 0;
    long long curSysTime  = 0;

    int64_t nCurPts = 0;
    int64_t nDuration = 0;

    VencCopyROIConfig pRoiConfig;
    memset(&pRoiConfig, 0, sizeof(VencCopyROIConfig));

    double pre_psnr = 0;
    AvTimer* pAvTimer = AvTimerCreate();

    time_begin = GetNowUs()/1000/1000;
    printf("***************************************************************\n");
    printf("*******test begin time[%llds] *******\n", time_begin);
    printf("***************************************************************\n");
    printf("\n");
    for(m=0; m<cycle_num; m++)
    {
        time_start = GetNowUs()/1000/1000;
        printf("*************[%u] cycle demo start time[%llds]**************\n", m,time_start);
        VencMBSumInfo sMbSumInfo;
        memset(&sMbSumInfo, 0, sizeof(VencMBSumInfo));
        //unsigned long long sum_sse = 0;
        //unsigned long long min_sse = 0;
        //unsigned long long max_sse = 0;
        //unsigned long long avr_sse = 0;
        //unsigned int       min_sse_frame = 0;
        //unsigned int       max_sse_frame = 0;
        //unsigned long long avr_sse_period = 0;

        /******** begin set the default encode param ********/
        encode_param_t    encode_param;
        memset(&encode_param, 0, sizeof(encode_param));
        encode_param.src_width = 1280;
        encode_param.src_height = 736;
        encode_param.dst_width = 1280;
        encode_param.dst_height = 736;
        encode_param.bit_rate = 2*1024*1024;
        encode_param.frame_rate = 30;
        encode_param.maxKeyFrame = 30;
        encode_param.encode_format = VENC_CODEC_H264;
        encode_param.encode_frame_num = 100;
        encode_param.test_cycle = 1;
        encode_param.nChannel = pEncContext->nChannel;
        strcpy((char*)encode_param.intput_file,        "/data/camera/720p-30zhen.yuv");
        strcpy((char*)encode_param.output_file,        "/data/camera/720p.264");
        strcpy((char*)encode_param.reference_file,
        "/mnt/bsp_ve_test/reference_data/reference.jpg");

        if(encode_param.dst_width == 3840)
            encode_param.bit_rate = 20*1024*1024;
        else if(encode_param.dst_width == 1920)
            encode_param.bit_rate = 10*1024*1024;
        else if(encode_param.dst_width ==1280)
            encode_param.bit_rate = 6*1024*1024;//6*1024*1024;
        else if(encode_param.dst_width == 640)
            encode_param.bit_rate = 2*1024*1024;
        else if(encode_param.dst_width == 288)
            encode_param.bit_rate = 1*1024*1024;
        else
            encode_param.bit_rate = 4*1024*1024;
        /******** end set the default encode param ********/


        /******** begin parse the config paramter ********/
        if(pEncContext->argc >= 2)
        {
            printf("******************************\n");
            for(i = 1; i < (int)pEncContext->argc; i += 2)
            {
                ParseArgument(&encode_param, pEncContext->argv[i], pEncContext->argv[i + 1]);
            }
            printf("******************************\n");
        }
        else
        {
            printf(" we need more arguments\n");
            PrintDemoUsage();
            return 0;
        }
        /******** end parse the config paramter ********/

        if(encode_param.encode_format == 5)
        {
            testIspFunction(&encode_param);
            return 0;
        }

        nDuration = 1000/encode_param.frame_rate;

        /******** begin open input , output and reference file ********/
        input_path = encode_param.intput_file;
        output_path = encode_param.output_file;
        log_path = encode_param.log_file;
        cycle_num = encode_param.test_cycle;

        in_file = fopen(input_path, "r");
        if(in_file == NULL)
        {
            loge("open in_file fail\n");
            goto out;
        }

        out_file = fopen(output_path, "wb");
        if(out_file == NULL)
        {
            loge("open out_file fail\n");
            fclose(in_file);
            goto out;
        }

        log_file = fopen(log_path, "ab");
        if(log_file == NULL)
        {
            loge("open log_file fail\n");
        }

        if(encode_param.compare_flag)
        {
            reference_path = encode_param.reference_file;
            reference_file = fopen(reference_path, "r");
            if(reference_file == NULL)
            {
                loge("open reference_file fail\n");
                goto out;
            }

            reference_buffer = (char*)malloc(1*1024*1024);
            if(reference_buffer == NULL)
            {
                loge("malloc reference_buffer error\n");
                goto out;
            }
        }
        /******** end open input , output and reference file ********/

        /******** begin set baseConfig param********/
        memset(&baseConfig, 0 ,sizeof(VencBaseConfig));
        memset(&bufferParam, 0 ,sizeof(VencAllocateBufferParam));
        baseConfig.memops = MemAdapterGetOpsS();
        if (baseConfig.memops == NULL)
        {
            printf("MemAdapterGetOpsS failed\n");
            goto out;
        }
        CdcMemOpen(baseConfig.memops);
        baseConfig.nInputWidth= encode_param.src_width;
        baseConfig.nInputHeight = encode_param.src_height;
        baseConfig.nStride = encode_param.src_width;
        baseConfig.nDstWidth = encode_param.dst_width;
        baseConfig.nDstHeight = encode_param.dst_height;
        baseConfig.bEncH264Nalu = 0;
        /*
            * the format of yuv file is yuv420p, but the old ic only support the yuv420sp,
            * so use the func yu12_nv12() to config all the format.
            */
        //TODO: Set inputformat as input arg!!!
        #ifdef SET_INPUTFORMAT_YUV420P
        baseConfig.eInputFormat = VENC_PIXEL_YUV420P;
        #else
        baseConfig.eInputFormat = VENC_PIXEL_YVU420SP;
        #endif
        if(baseConfig.eInputFormat == VENC_PIXEL_YUV420P)
        {
            #ifdef YU12_NV12
                    baseConfig.eInputFormat = VENC_PIXEL_YUV420SP;
                    yu12_nv12_flag = 1;
            #endif

            #ifdef YU12_NV21
                baseConfig.eInputFormat = VENC_PIXEL_YVU420SP;
                yu12_nv21_flag = 1;
            #endif
        }
        //* ve require 16-align
        int nAlignW = (baseConfig.nInputWidth + 15)& ~15;
        int nAlignH = (baseConfig.nInputHeight + 15)& ~15;

        if(baseConfig.eInputFormat == VENC_PIXEL_YUYV422
           || baseConfig.eInputFormat == VENC_PIXEL_UYVY422)
        {
            bufferParam.nSizeY = nAlignW*nAlignH*2;
            bufferParam.nSizeC = 0;
        }
        else
        {
            bufferParam.nSizeY = nAlignW*nAlignH;
            bufferParam.nSizeC = nAlignW*nAlignH/2;
        }
        bufferParam.nBufferNum = 1;

        if(encode_param.test_afbc == 1)
        {
        afbc_header_size = ((baseConfig.nInputWidth +127)>>7)*((baseConfig.nInputHeight+31)>>5)*96;
        logd("size_y:%x, size_c:%x, afbc_header:%x\n",
                                    bufferParam.nSizeY,
                                    bufferParam.nSizeC,
                                    afbc_header_size);
        bufferParam.nSizeY += afbc_header_size + bufferParam.nSizeC;
        bufferParam.nSizeC = 0;
        logd("afbc buffer size:%x\n", bufferParam.nSizeY);

        baseConfig.eInputFormat = VENC_PIXEL_AFBC_AW;
        }

        if(encode_param.test_lbc != 0)
        {
        int y_stride = 0;
        int yc_stride = 0;
        int bit_depth = 8;
        int com_ratio_even = 0;
        int com_ratio_odd = 0;
        int pic_width_32align = (baseConfig.nInputWidth + 31) & ~31;
        int pic_height_16align = (baseConfig.nInputHeight + 15) & ~15;

            if(encode_param.test_lbc == 2)
        baseConfig.bLbcLossyComEnFlag2x = 1;
            else if(encode_param.test_lbc == 3)
        baseConfig.bLbcLossyComEnFlag2_5x = 1;

        if(baseConfig.bLbcLossyComEnFlag2x == 1)
        {
            com_ratio_even = 600;
            com_ratio_odd = 450;
            y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
            yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
        }
        else if(baseConfig.bLbcLossyComEnFlag2_5x == 1)
        {
            com_ratio_even = 440;
            com_ratio_odd = 380;
            y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
            yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
        }
        else
        {
            y_stride = ((pic_width_32align * bit_depth + pic_width_32align / 16 * 2 + 511) & (~511)) >> 3;
            yc_stride = ((pic_width_32align * bit_depth * 2 + pic_width_32align / 16 * 4 + 511) & (~511)) >> 3;
        }


        int total_stream_len = (y_stride + yc_stride) * pic_height_16align / 2;

        logd("LBC in buf:com_ratio: %d, %d, w32alin = %d, h16alin = %d, \
            y_s = %d, yc_s = %d, total_len = %d,\n",
             com_ratio_even, com_ratio_odd,
             pic_width_32align, pic_height_16align,
             y_stride, yc_stride, total_stream_len);

        bufferParam.nSizeY = total_stream_len;
        bufferParam.nSizeC = 0;
        baseConfig.eInputFormat = VENC_PIXEL_LBC_AW;

        }
        /******** end set baseConfig param********/

        //create encoder
        logd("encode_param.encode_format:%d\n", encode_param.encode_format);
        pVideoEnc = VideoEncCreate(encode_param.encode_format);

        //set enc parameter
        result = setEncParam(pVideoEnc ,&encode_param);
        if(result)
        {
            loge("setEncParam error, return");
            goto out;
        }

        VideoEncInit(pVideoEnc, &baseConfig);

        if((pRoiConfig.bEnable==1) && \
             (encode_param.encode_format == VENC_CODEC_H264 || \
               encode_param.encode_format == VENC_CODEC_JPEG || \
               encode_param.encode_format == VENC_CODEC_H265))
        {
            init_Roi_config(&baseConfig, &pRoiConfig);
            VideoEncSetParameter(pVideoEnc, VENC_IndexParamRoi, &pRoiConfig);
        }

        if(encode_param.frequency > 0)
            VideoEncoderSetFreq(pVideoEnc, encode_param.frequency);

        if(encode_param.encode_format == VENC_CODEC_H264 || \
        encode_param.encode_format == VENC_CODEC_H265)
        {
            //unsigned int head_num = 0;
            if(encode_param.encode_format == VENC_CODEC_H264)
            {
                VideoEncGetParameter(pVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
                //unsigned char value = 1;
                //VideoEncGetParameter(pVideoEnc, VENC_IndexParamAllParams, &value);
            }
            else if(encode_param.encode_format == VENC_CODEC_H265)
            {
                VideoEncGetParameter(pVideoEnc, VENC_IndexParamH265Header, &sps_pps_data);
                //unsigned char value = 1;
                //VideoEncGetParameter(pVideoEnc, VENC_IndexParamAllParams, &value);
            }

        #if SAVE_AWSP
            char mask[16] = "awspcialstream";
            logd(" make special stream save special data size: %d ", sps_pps_data.nLength);
            fwrite(mask, 14, sizeof(char), out_file);
            value = 0x115;
            fwrite(&value, 1, sizeof(int), out_file);
            value = 1280;
            fwrite(&value, 1, sizeof(int), out_file);
            value = 736;
            fwrite(&value , 1, sizeof(int), out_file);
            value = 0;
            fwrite(&value, 1, sizeof(int), out_file);
            fwrite(&sps_pps_data.nLength, 1, sizeof(int), out_file);
        #endif
            fwrite(sps_pps_data.pBuffer, 1, sps_pps_data.nLength, out_file);
            logd("sps_pps_data.nLength: %d", sps_pps_data.nLength);
            //for(head_num=0; head_num<sps_pps_data.nLength; head_num++)
                //logd("the sps_pps :%02x\n", *(sps_pps_data.pBuffer+head_num));

            if(encode_param.compare_flag)
            {
                result = fread(reference_buffer, 1, sps_pps_data.nLength, reference_file);
                if(result != (int)sps_pps_data.nLength)
                {
                    loge("read reference_file sps info error\n");
                    goto out;
                }

                for(i=0; i<(int)sps_pps_data.nLength; i++)
                {
                    if(sps_pps_data.pBuffer[i] != reference_buffer[i])
                    {
                        loge("the sps %d byte is not same, ref[%02x], cur[%02x]\n",
                                                            i,
                                                            reference_buffer[i],
                                                            sps_pps_data.pBuffer[i]);
                        goto out;
                    }
                }
            }
        }

        AllocInputBuffer(pVideoEnc, &bufferParam);

        if(yu12_nv12_flag || yu12_nv21_flag)
        {
            uv_tmp_buffer = (unsigned char*)malloc(baseConfig.nInputWidth
                                            * baseConfig.nInputHeight/2);
            if(uv_tmp_buffer == NULL)
            {
                loge("malloc uv_tmp_buffer fail\n");
                goto out;
            }
        }

#ifdef USE_SVC
        // used for throw frame test with SVC
        int TemporalLayer = -1;
        char p9bytes[9] = {0};
#endif

        unsigned int testNumber = 0;
        while(testNumber < encode_param.encode_frame_num)
        {
            GetOneAllocInputBuffer(pVideoEnc, &inputBuffer);
            {
#if NO_READ_WRITE
                if(testNumber == 0)
#endif
                {
                    if(encode_param.test_afbc == 1
                       || encode_param.test_lbc != 0
                       || baseConfig.eInputFormat == VENC_PIXEL_YUYV422
                       || baseConfig.eInputFormat == VENC_PIXEL_UYVY422)
                    {
                        unsigned int size1;
                        size1 = fread(inputBuffer.pAddrVirY, 1, bufferParam.nSizeY, in_file);

                        if(size1!= bufferParam.nSizeY)
                        {
                            fseek(in_file, 0L, SEEK_SET);
                            size1 = fread(inputBuffer.pAddrVirY, 1, bufferParam.nSizeY, in_file);
                        }
                    }
                    else
                    {

                        unsigned int size1, size2;
                        size1 = fread(inputBuffer.pAddrVirY, 1,
                                baseConfig.nInputWidth*baseConfig.nInputHeight, in_file);
                        size2 = fread(inputBuffer.pAddrVirC, 1,
                                  baseConfig.nInputWidth*baseConfig.nInputHeight/2, in_file);

                        if((size1!= baseConfig.nInputWidth*baseConfig.nInputHeight)
                         || (size2!= baseConfig.nInputWidth*baseConfig.nInputHeight/2))
                        {
                            fseek(in_file, 0L, SEEK_SET);
                            size1 = fread(inputBuffer.pAddrVirY, 1,
                                     baseConfig.nInputWidth*baseConfig.nInputHeight, in_file);
                            size2 = fread(inputBuffer.pAddrVirC, 1,
                                     baseConfig.nInputWidth*baseConfig.nInputHeight/2, in_file);
                        }

                        if(yu12_nv12_flag)
                        {
                            yu12_nv12(baseConfig.nInputWidth, baseConfig.nInputHeight,
                                 inputBuffer.pAddrVirC, uv_tmp_buffer);
                        }
                        else if(yu12_nv21_flag)
                        {
                           yu12_nv21(baseConfig.nInputWidth, baseConfig.nInputHeight,
                                 inputBuffer.pAddrVirC, uv_tmp_buffer);
                        }
                    }
                }
            }
            inputBuffer.bEnableCorp = 0;
            inputBuffer.sCropInfo.nLeft =  240;
            inputBuffer.sCropInfo.nTop  =  240;
            inputBuffer.sCropInfo.nWidth  =  240;
            inputBuffer.sCropInfo.nHeight =  240;
            FlushCacheAllocInputBuffer(pVideoEnc, &inputBuffer);
            pts += 1*1000/encode_param.frame_rate;
            inputBuffer.nPts = pts;
            AddOneInputBuffer(pVideoEnc, &inputBuffer);

#ifdef SET_MB_INFO
            setMbMode(pVideoEnc, &encode_param);
#endif

#ifdef SET_SMART
            if(testNumber == 0)
            {
                if(encode_param.encode_format == VENC_CODEC_H264)
                {
                    VideoEncSetParameter(pVideoEnc, VENC_IndexParamSmartFuntion,
                                                            &h264_func.sH264Smart);
                }
                else if(encode_param.encode_format == VENC_CODEC_H265)
                {
                    VideoEncSetParameter(pVideoEnc, VENC_IndexParamSmartFuntion,
                                                            &h265_func.h265Smart);
                }
            }
#endif

//#ifdef VBR
#if 0
            unsigned int bitRate = 50*1024*1024;
            if(testNumber == 0)
            {
                VideoEncSetParameter(pVideoEnc, VENC_IndexParamBitrate, &bitRate);
            }
            if(testNumber == 90)
            {
                bitRate = 80*1024*1024;
                VideoEncSetParameter(pVideoEnc, VENC_IndexParamBitrate, &bitRate);
            }
            else if(testNumber == 180)
            {
                bitRate = 50*1024*1024;
                VideoEncSetParameter(pVideoEnc, VENC_IndexParamBitrate, &bitRate);
            }
            else if(testNumber == 250)
            {
                bitRate = 80*1024*1024;
                VideoEncSetParameter(pVideoEnc, VENC_IndexParamBitrate, &bitRate);
            }
            else if(testNumber == 1500)
            {
                bitRate = 50*1024*1024;
                VideoEncSetParameter(pVideoEnc, VENC_IndexParamBitrate, &bitRate);
            }
#endif

#if ENABLE_OVERLAY_BY_WATERMARK
            VencWatermarkInfoS mWatermarkInfo;
            memset(&mWatermarkInfo, 0, sizeof(VencWatermarkInfoS));

            mWatermarkInfo.item_num = 1;
            mWatermarkInfo.item[0].start_pos_x = 16;
            mWatermarkInfo.item[0].start_pos_y = 16;

            getTimeString((char *)mWatermarkInfo.item[0].context, WATERMARK_CONTEXT_LEN);

            strcpy((char*)mWatermarkInfo.icon_pic_path, ICON_PIC_PATH);

            VideoEncSetParameter(pVideoEnc, VENC_IndexParamSetOverlayByWatermark,
                                 &mWatermarkInfo);
#endif

            time1 = GetNowUs();
            result = VideoEncodeOneFrame(pVideoEnc);
            if(result < 0)
            {
                logd("encoder error, goto out");
                goto out;
            }
            time2 = GetNowUs();
            long long diff  = time2-time1;
            if(timeMin == 0 || timeMin > diff)
                timeMin = diff;
            if(timeMax == 0 || timeMax < diff)
                timeMax = diff;
            time3 += diff;
            logv("[%d]encode frame %d use time is %lldus..\n",
                    pEncContext->nChannel, testNumber,(time2-time1));
            logv("\n");
            logv("\n");


#ifdef GET_MB_INFO
            getMbMinfo(&encode_param);

            if(encode_param.encode_format == VENC_CODEC_H264)
            {
                VideoEncGetParameter(pVideoEnc, VENC_IndexParamMBSumInfoOutput, &sMbSumInfo);
            }
            else if(encode_param.encode_format == VENC_CODEC_H265)
            {
                VideoEncGetParameter(pVideoEnc, VENC_IndexParamMBSumInfoOutput, &sMbSumInfo);
            }
            logv("mbInfo: mad = %d, sse = %lld, qp = %d, mbNum = %d",sMbSumInfo.sum_mad,sMbSumInfo.sum_sse,
                sMbSumInfo.sum_qp,h264_func.MBInfo.num_mb);

            unsigned pic_size = encode_param.dst_width * encode_param.dst_height;
            double cur_psnr = 10.0 * log10(255.0 * 255.0 * 1024 / (1.0 * sMbSumInfo.sum_sse / pic_size));
            avr_sse_period += sMbSumInfo.sum_sse;
            avr_sse += sMbSumInfo.sum_sse;
            sum_sse = avr_sse;
            if(testNumber%60 == 0)
            {
                avr_sse_period /= 60;
                double period_psnr = 10.0 * log10(255.0 * 255.0 * 1024 / (1.0 * avr_sse_period / pic_size));
                logd("frame num = %d, psnr = %0.2f , avr = %0.2f, diff = %0.2f, I frame",
                     testNumber,cur_psnr,
                     period_psnr, cur_psnr - period_psnr);
            }
            //else
            logv("frame num = %d, psnr = %0.2f, diff = %0.2f, sum_mad = %d",testNumber,cur_psnr,
            cur_psnr - pre_psnr, sMbSumInfo.sum_mad);
            pre_psnr = cur_psnr;
            if(testNumber == 0)
            {
                min_sse = sMbSumInfo.sum_sse;
                max_sse = sMbSumInfo.sum_sse;
            }
            else
            {
                if(sMbSumInfo.sum_sse < min_sse)
                {
                    min_sse = sMbSumInfo.sum_sse;
                    min_sse_frame = testNumber;
                }
                else if(sMbSumInfo.sum_sse > max_sse)
                {
                    max_sse = sMbSumInfo.sum_sse;
                    max_sse_frame = testNumber;
                }
            }
            if(testNumber == (encode_param.encode_frame_num - 1))
            {
                double min_psnr, max_psnr, avr_psnr;
                unsigned pic_size = encode_param.dst_width * encode_param.dst_height;

                avr_sse /= encode_param.encode_frame_num;
                max_psnr = 10.0 * log10(255.0 * 255.0 * 1024 / (1.0 * min_sse / pic_size));
                min_psnr = 10.0 * log10(255.0 * 255.0 * 1024 / (1.0 * max_sse / pic_size));
                avr_psnr = 10.0 * log10(255.0 * 255.0 * 1024 / (1.0 * avr_sse / pic_size));
                logd("frame[%d] get min_psnr:%f, frame[%d] get max_psnr:%f, average_psnr:%f\n",
                                                                    max_sse_frame,
                                                                    min_psnr,
                                                                    min_sse_frame,
                                                                    max_psnr,
                                                                    avr_psnr);
            }
#endif

#if 0
            VencEncodeTimeS sEncTime;
            VideoEncGetParameter(pVideoEnc, VENC_IndexParamGetEncodeTime, &sEncTime);
            logd("frame:%d, enc_time:%d, max_enc_time:%d, max_enc_time_frame:%d, avr_enc_time:%d",
                                sEncTime.frame_num, sEncTime.curr_enc_time, sEncTime.max_enc_time,
                                sEncTime.max_enc_time_frame_num,sEncTime.avr_enc_time);
            logd("empty_time:%d, max_empty_time:%d, max_empty_time_frame:%d, avr_empty_time:%d\n",
                                sEncTime.curr_empty_time, sEncTime.max_empty_time,
                                sEncTime.max_empty_time_frame_num,sEncTime.avr_empty_time);
#endif

            AlreadyUsedInputBuffer(pVideoEnc,&inputBuffer);
            ReturnOneAllocInputBuffer(pVideoEnc, &inputBuffer);

            if(encode_param.limit_encode_speed == 1)
            {
                if(bHadStartTimeFlag == 0)
                {
                    bHadStartTimeFlag = 1;
                    pAvTimer->SetTime(pAvTimer, nCurPts);
                    pAvTimer->Start(pAvTimer);

                    nFirstEncodePicTime = GetNowUs();
                }
                else
                {
                    nCurPts += (nDuration*1000);
                    int64_t nCurTime = pAvTimer->GetTime(pAvTimer);

                    int64_t nTimeDiff = nCurPts - nCurTime;
                    int64_t nWaitTimeMs = 0;
                    if(nTimeDiff > 5*1000)
                    {
                        nWaitTimeMs = nTimeDiff/1000;
                        usleep(nWaitTimeMs*1000);
                    }
                    logv("** encoder: nCurPts = %lld, nWaitTimeMs = %lld", (long long int)nCurPts, (long long int)nWaitTimeMs);

                }

                if((testNumber+1)%30 == 0)
                {
                    curSysTime = GetNowUs();
                    long long curTotalTime = curSysTime - nFirstEncodePicTime;
                    logd("The 1st encoder:encodeFrameCout=%d,costTime=%.2f s,renderFps=%.2f",
                         (testNumber+1), (double)curTotalTime/1000/1000,
                         (double)(testNumber+1)*1000*1000/curTotalTime);
                }
            }

            result = GetOneBitstreamFrame(pVideoEnc, &outputBuffer);
#ifdef USE_SUPER_FRAME
            if((sSuperFrameCfg.eSuperFrameMode==VENC_SUPERFRAME_DISCARD) && (result==-1))
            {
                logd("VENC_SUPERFRAME_DISCARD: discard frame %d\n",testNumber);
                continue;
            }
#endif
            if(result == -1)
            {
                goto out;
            }

#ifdef USE_SVC
            // used for throw frame test with SVC
            memcpy(p9bytes, outputBuffer.pData0, 9);
            TemporalLayer = SeekPrefixNAL(p9bytes);

            switch(TemporalLayer)
            {

                case 3:
                case 2:
                case 1:
                    logv("just write the PrefixNAL\n");
                    fwrite(outputBuffer.pData0, 1, 9, out_file);
                    break;

                default:
                    logv("\nTemporalLayer=%d,  testNumber=%d\n", TemporalLayer, testNumber);
                    fwrite(outputBuffer.pData0, 1, outputBuffer.nSize0, out_file);
                    //fwrite(outputBuffer.pData0+9, 1, outputBuffer.nSize0-9, out_file);
                    if(outputBuffer.nSize1)
                    {
                        fwrite(outputBuffer.pData1, 1, outputBuffer.nSize1, out_file);
                    }
                    break;
            }
#else

#if NO_READ_WRITE

#else
        #if SAVE_AWSP
            char mask[6] = "awsp";
            logd(" make special stream save data %d,%d ",outputBuffer.nSize0 , outputBuffer.nSize1);
            fwrite(mask, 4, sizeof(char), out_file);
            value =0;
            fwrite(&value, 1, sizeof(int), out_file);
            value = outputBuffer.nSize0 + outputBuffer.nSize1;
            fwrite(&value, 1, sizeof(int), out_file);
           int64_t value = 0;
            fwrite(&value, 1, sizeof(int64_t), out_file);
        #endif

            if(encode_param.encode_format == VENC_CODEC_JPEG && 0 == encode_param.jpeg_func.jpeg_mode)
            {
                saveJpegPic(&outputBuffer, encode_param.output_file, testNumber);
            }
            else
            {
                fwrite(outputBuffer.pData0, 1, outputBuffer.nSize0, out_file);

                if(outputBuffer.nSize1)
                {
                   fwrite(outputBuffer.pData1, 1, outputBuffer.nSize1, out_file);
                }
            }

           if(pRoiConfig.bEnable)
            {
                   int idx;
                   int nLen = 0;
                   for(idx=0; idx<pRoiConfig.num; idx++)
                   {
                        FILE *Roi_file;
                        char name[128];
                        sprintf(name, "/mnt/test/Roi_%d_%d.yuv", testNumber, idx);
                        Roi_file = fopen(name, "wb");
                        if(Roi_file == NULL)
                        {
                             loge("open out_file fail\n");
                             goto out;
                        }

                        struct ScMemOpsS *_memops = baseConfig.memops;
                        void *veOps = (void *)baseConfig.veOpsS;
                        void *pVeopsSelf = baseConfig.pVeOpsSelf;
                        CEDARC_UNUSE(_memops);
                        CEDARC_UNUSE(veOps);
                      CEDARC_UNUSE(pVeopsSelf);
                         int yLen = pRoiConfig.sRect[idx].nWidth*pRoiConfig.sRect[idx].nHeight;
                         EncAdapterMemFlushCache(pRoiConfig.pRoiYAddrVir +nLen, yLen);
                       EncAdapterMemFlushCache(pRoiConfig.pRoiCAddrVir+nLen/2 , yLen/2);

                       fwrite(pRoiConfig.pRoiYAddrVir+nLen, 1, yLen, Roi_file);
                       fwrite(pRoiConfig.pRoiCAddrVir+nLen/2, 1, yLen/2, Roi_file);
                         fclose(Roi_file);
                         nLen += yLen;
                    }
          }

        #endif
#endif

            if(encode_param.compare_flag)
            {
                result = fread(reference_buffer, 1, outputBuffer.nSize0, reference_file);
                if(result != (int)outputBuffer.nSize0)
                {
                    loge("read reference_file error\n");
                    goto out;
                }

                for(i=0; i<(int)outputBuffer.nSize0; i++)
                {
                    if(outputBuffer.pData0[i] != reference_buffer[i])
                    {
                        loge("the %d frame's ref_data_%d[%02x] and cur_data_%d[%02x] is not same\n",
                                                testNumber,
                                                i,
                                                reference_buffer[i],
                                                i,
                                                outputBuffer.pData0[i]);
                        goto out;
                    }
                }

                result = fread(reference_buffer, 1, outputBuffer.nSize1, reference_file);
                if(result != (int)outputBuffer.nSize1)
                {
                    loge("read reference_file error\n");
                    goto out;
                }

                for(i=0; i<(int)outputBuffer.nSize1; i++)
                {
                    if((outputBuffer.pData1)[i] != reference_buffer[i])
                    {
                        loge("the %d frame's data1 %d byte is not same\n", testNumber, i);
                        goto out;
                    }
                }
            }

            FreeOneBitStreamFrame(pVideoEnc, &outputBuffer);
            testNumber++;
        }

        if(encode_param.compare_flag)
        {
            encode_param.compare_result = 1;
            logd("the compare result is ok\n");
        }

        long long avr = time3/testNumber/1000;
        logd("encode costTime:avr=%0.2f ms,fps=%0.2f,min=%0.2f ms,max=%0.2f ms,cout=%d...\n",
             (float)avr,
             (float)1000/avr,
             (float)timeMin/1000,
             (float)timeMax/1000,testNumber);

        printf("output file is saved:%s\n",encode_param.output_file);

    out:

        AvTimerDestroy(pAvTimer);

            if(pRoiConfig.bEnable)
            {
                 struct ScMemOpsS *_memops = baseConfig.memops;
                 void *veOps = (void *)baseConfig.veOpsS;
                 void *pVeopsSelf = baseConfig.pVeOpsSelf;
                 CEDARC_UNUSE(_memops);
                 CEDARC_UNUSE(veOps);
                 CEDARC_UNUSE(pVeopsSelf);
                 if(pRoiConfig.pRoiYAddrVir != NULL)
                 {
                      EncAdapterMemPfree(pRoiConfig.pRoiYAddrVir);
                      pRoiConfig.pRoiYAddrVir = NULL;
                 }

                 if(pRoiConfig.pRoiCAddrVir != NULL)
                 {
                      EncAdapterMemPfree(pRoiConfig.pRoiCAddrVir);
                      pRoiConfig.pRoiCAddrVir = NULL;
                 }
            }

        if(pVideoEnc)
        {
            VideoEncDestroy(pVideoEnc);
        }
        pVideoEnc = NULL;

        if(out_file)
            fclose(out_file);
        if(in_file)
            fclose(in_file);
        if(uv_tmp_buffer)
            free(uv_tmp_buffer);
        if(baseConfig.memops)
        {
            CdcMemClose(baseConfig.memops);
        }

        releaseMb(&encode_param);

        if(encode_param.compare_flag)
        {
            if(reference_buffer)
                free(reference_buffer);
            if(reference_file)
                fclose(reference_file);

            printf("the %u cycle test, freq:%d\n", m, encode_param.frequency);

            if(encode_param.compare_result)
            {
                printf("encoder:ve_freq[%dMHz],the compare result is ok\n",encode_param.frequency);
                if(log_file)
                {
                    log_len = sprintf(log_buffer + log_len,             \
                        "encoder: ve_freq[%dMHz], the compare result is ok\n", \
                        encode_param.frequency);
                    fwrite(&logcat_buf, 1, log_len, log_file);
                    fclose(log_file);
                }
            }
            else
            {
                printf("encoder: ve_freq[%dMHz], the compare result is fail\n",\
                    encode_param.frequency);
                time_end = GetNowUs()/1000/1000;
                printf("*******demo end time[%llds] cur_cycle_time[%llds]*******\n",     \
                                                         time_end, time_end - time_start);
                if(log_file)
                {
                    log_len = sprintf(log_buffer + log_len,             \
                        "encoder: ve_freq[%dMHz], the compare result is fail\n", \
                        encode_param.frequency);
                    log_len += sprintf(log_buffer + log_len,              \
                        "*******demo end time[%llds] cur_cycle_time[%llds]*******\n",            \
                                                            time_end, time_end - time_start);
                    fwrite(&logcat_buf, 1, log_len, log_file);
                    fclose(log_file);
                }
                goto DEMO_END;
            }
        }

        time_end = GetNowUs()/1000/1000;
        printf("*******[%u]cycle demo end time[%llds] cur_cycle_time[%llds]*******\n",
                                                m, time_end, time_end - time_start);
        printf("\n");
    }

    time_end = GetNowUs()/1000/1000;

    printf("\n");
    printf("***************************************************************\n");
    printf("*******test end time[%llds] test_total_time[%llds]*******\n",
                                            time_end, time_end - time_begin);
    printf("***************************************************************\n");
DEMO_END:
    for(i=0; i<13; i++)
    {
        if(bit_map_info[i].argb_addr)
            free(bit_map_info[i].argb_addr);
    }
    return 0;
}

int main(int argc, char** argv)
{
    pthread_t tchanelEncoder[ENCODER_MAX_NUM];
    encoder_Context* pEncContext[ENCODER_MAX_NUM] = {NULL};
    int nRet = 0;
    int i = 0;
    int nEncoderNum = 1;

    //* get encoder num
    if(argc >= 2)
    {
        for(i = 1; i < argc; i += 2)
        {
            ARGUMENT_T arg;
            arg = GetArgument(argv[i]);
            switch(arg)
            {
                case HELP:
                    PrintDemoUsage();
                    exit(-1);
                case ENCODER_NUM:
                    sscanf(argv[i + 1], "%32d", &nEncoderNum);
                    break;
                default:
                    break;
            }
        }
    }
    else
    {
        logd(" we need more arguments ");
        PrintDemoUsage();
        return 0;
    }

    if(nEncoderNum <= 0)
        nEncoderNum = 1;
    else if(nEncoderNum > ENCODER_MAX_NUM)
        nEncoderNum = ENCODER_MAX_NUM;

    logd("nEncoderNum = %d",nEncoderNum);

    for(i = 0; i < nEncoderNum; i++)
    {
        pEncContext[i] = calloc(sizeof(encoder_Context), 1);
        if(pEncContext[i] == NULL)
        {
            loge("malloc for pEncContext failed");
            goto OUT;
        }
        pEncContext[i]->argc = argc;
        pEncContext[i]->argv = argv;
        pEncContext[i]->nChannel = i;
    }

    for(i = 0; i < nEncoderNum; i++)
    {
        pthread_create(&tchanelEncoder[i], NULL, ChannelThread, (void*)(pEncContext[i]));
    }

    for(i = 0; i < nEncoderNum; i++)
    {
        pthread_join(tchanelEncoder[i], (void**)&nRet);
    }

OUT:

    for(i = 0; i < nEncoderNum; i++)
    {
        if(pEncContext[i])
            free(pEncContext[i]);
    }

    return 0;
}

