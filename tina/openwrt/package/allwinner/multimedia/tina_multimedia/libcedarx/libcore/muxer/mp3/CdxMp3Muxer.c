/*
 * Copyright (c) 2008-2022 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMp3Muxer.c
 * Description : Allwinner MP3 Muxer Definition
 * History :
 *   Author  : chengweipeng <chengweipeng@allwinnertech.com>
 *   Date    : 2022/07/20
 *   Comment : 创建初始版
 *
 */

#include "CdxMp3Muxer.h"

static int __Mp3MuxerWriteExtraData(CdxMuxerT *mux, unsigned char *data, int len, int idx)
{
	return 0;
}

static int __Mp3MuxerWriteHeader(CdxMuxerT *mux)
{
	Mp3MuxContext *impl = (Mp3MuxContext *)mux;
	CdxFsWriterInfo *p_fs;

#if FS_WRITER
	p_fs = &impl->fs_writer_info;

	if (p_fs->mp_fs_writer) {
		loge("(f:%s, l:%d) fatal error! why mov->mp_fs_writer[%p]!=NULL",
		     __FUNCTION__, __LINE__, p_fs->mp_fs_writer);
		return -1;
	}
	if (impl->stream_writer) {
		cdx_int8 *p_cache = NULL;
		cdx_uint32 n_cache_size = 0;
		FSWRITEMODE mode = impl->mFsWriteMode;

		if (FSWRITEMODE_CACHETHREAD == mode) {
			if (p_fs->m_cache_mem_info.m_cache_size > 0 && p_fs->m_cache_mem_info.mp_cache != NULL) {
				mode = FSWRITEMODE_CACHETHREAD;
				p_cache = (cdx_int8 *)p_fs->m_cache_mem_info.mp_cache;
				n_cache_size = p_fs->m_cache_mem_info.m_cache_size;
			} else {
				logw("fatal error! not set cacheMemory but set mode FSWRITEMODE_CACHETHREAD! use FSWRITEMODE_DIRECT.");
				mode = FSWRITEMODE_DIRECT;
			}
		} else if (FSWRITEMODE_SIMPLECACHE == mode) {
			logd("FSWRITEMODE_SIMPLECACHE == mode\n");
			p_cache = NULL;
			n_cache_size = p_fs->m_fs_simple_cache_size;
		}
		p_fs->mp_fs_writer = CreateFsWriter(mode, impl->stream_writer, (cdx_uint8 *)p_cache,
						    n_cache_size, 0);
		if (NULL == p_fs->mp_fs_writer) {
			loge("fatal error! create FsWriter() fail!");
			return -1;
		}
	}
#endif

	return Mp3WriteHeader(impl);
}

static int __Mp3MuxerWritePacket(CdxMuxerT *mux, CdxMuxerPacketT *packet)
{
	Mp3MuxContext *impl = (Mp3MuxContext *)mux;
	int ret;

	ret = CdxWriterWrite(impl->stream_writer, packet->buf, packet->buflen);
	if (ret < packet->buflen) {
		logd("=== ret: %d, packet->buflen: %d", ret, packet->buflen);
		return -1;
	}
	return 0;
}

static int __Mp3MuxerSetMediaInfo(CdxMuxerT *mux, CdxMuxerMediaInfoT *pMediaInfo)
{
	Mp3MuxContext *impl = (Mp3MuxContext *)mux;

	return 0;
}

static int __Mp3MuxerWriteTrailer(CdxMuxerT *mux)
{
	Mp3MuxContext *impl = (Mp3MuxContext *)mux;

	return Mp3WriteTrailer(impl);
}

static int __Mp3MuxerControl(CdxMuxerT *mux, int uCmd, void *pParam)
{
	Mp3MuxContext *impl = (Mp3MuxContext *)mux;

	switch (uCmd) {
	case SET_FS_WRITE_MODE: {
		logd("SET_FS_WRITE_MODE\n");
		impl->fs_writer_info.m_fs_writer_mode = *((FSWRITEMODE *)pParam);
		break;
	}
	case SET_CACHE_MEM: {
		impl->fs_writer_info.m_cache_mem_info = *((CdxFsCacheMemInfo *)pParam);
		break;
	}
	case SET_FS_SIMPLE_CACHE_SIZE: {
		impl->fs_writer_info.m_fs_simple_cache_size = *((cdx_int32 *)pParam);
		break;
	}
	}
	return 0;
}

static int __Mp3MuxerClose(CdxMuxerT *mux)
{
	Mp3MuxContext *impl = (Mp3MuxContext *)mux;

	if (impl) {
		if (impl->stream_writer) {
			impl->stream_writer = NULL;
		}
		free(impl);
	}

	return 0;
}

static struct CdxMuxerOpsS mp3MuxerOps = {
	.writeExtraData =  __Mp3MuxerWriteExtraData,
	.writeHeader     = __Mp3MuxerWriteHeader,
	.writePacket     = __Mp3MuxerWritePacket,
	.writeTrailer    = __Mp3MuxerWriteTrailer,
	.control         = __Mp3MuxerControl,
	.setMediaInfo    = __Mp3MuxerSetMediaInfo,
	.close           = __Mp3MuxerClose
};


CdxMuxerT *__CdxMp3MuxerOpen(CdxWriterT *stream)
{
	Mp3MuxContext *mp3Mux;
	logd("__CdxMp3MuxerOpen");

	mp3Mux = malloc(sizeof(Mp3MuxContext));
	if (!mp3Mux) {
		return NULL;
	}
	memset(mp3Mux, 0x00, sizeof(Mp3MuxContext));

	mp3Mux->stream_writer = stream;
	mp3Mux->muxInfo.ops = &mp3MuxerOps;

	return &mp3Mux->muxInfo;
}


CdxMuxerCreatorT mp3MuxerCtor = {
	.create = __CdxMp3MuxerOpen
};
