/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMp3Muxer.h
 * Description : Allwinner AAC Muxer Definition
 * History :
 *
 */

#ifndef CDX_MP3_MUXER_H
#define CDX_MP3_MUXER_H

#include <cdx_log.h>
#include "CdxMuxer.h"
#include "CdxMuxerBaseDef.h"
#include "CdxFsWriter.h"

#define ID3v1_TAG_SIZE 128
#define ID3v1_GENRE_MAX 147

#define ID3v2_HEADER_SIZE 10

#define AV_RB32(x)                              \
    ((((const unsigned char*)(x))[0] << 24) |         \
     (((const unsigned char*)(x))[1] << 16) |         \
     (((const unsigned char*)(x))[2] <<  8) |         \
      ((const unsigned char*)(x))[3])

#define CDX_AV_METADATA_MATCH_CASE      1
#define CDX_AV_METADATA_IGNORE_SUFFIX   2
#define CDX_AV_METADATA_DONT_STRDUP_KEY 4
#define CDX_AV_METADATA_DONT_STRDUP_VAL 8
#define CDX_AV_METADATA_DONT_OVERWRITE 16

typedef struct {
	char *key;
	char *value;
} AVMetadataTag;

typedef struct AVMetadata AVMetadata;

struct AVMetadata {
	int count;
	AVMetadataTag *elems;
};

typedef struct Mp3MuxContext {
	CdxMuxerT   muxInfo;
	CdxWriterT  *stream_writer;
	CdxFsWriterInfo     fs_writer_info;
	CdxMuxerMediaInfoT mediaInfo;

	FSWRITEMODE mFsWriteMode;

	AVMetadata *metadata;
} Mp3MuxContext;

cdx_int32 Mp3WriteHeader(Mp3MuxContext *s);
cdx_int32 Mp3WriteTrailer(Mp3MuxContext *s);

#endif /* CDX_MP3_MUXER_H */
