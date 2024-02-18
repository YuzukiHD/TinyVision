
include $(top_srcdir)/Makefile.inc

noinst_LTLIBRARIES = libcdx_mp3_muxer.la

## set the source files.
libcdx_mp3_muxer_la_SOURCES =  CdxMp3Muxer.c CdxMp3MuxerLib.c

libcdx_mp3_muxer_la_CFLAGS = $(CFLAGS_CDXG)
LOCAL_INCLUDE = -I../include \
		-I../../include \
		-I../../base/include \
		-I../base \
		-I$(top_srcdir)/external/include/aencoder \
		-I$(top_srcdir)/../mpp_libcedarc/include \
		-I$(top_srcdir)/../libcedarc/include \
		-I$(top_srcdir)/

libcdx_mp3_muxer_la_CFLAGS += $(LOCAL_INCLUDE)


libcdx_mp3_muxer_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_mp3_muxer_la_LIBADD = $(top_srcdir)/libcore/base/libcdx_base.la

#LOCAL_LIB = -lpthread
libcdx_mp3_muxer_la_LDFLAGS += $(LOCAL_LIB)
