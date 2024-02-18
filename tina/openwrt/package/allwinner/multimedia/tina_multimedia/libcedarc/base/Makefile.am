
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdc_base.la

libcdc_base_la_SOURCES = CdcLog.c \
                         CdcSysinfo.c \
                         CdcMessageQueue.c \
                         CdcIonUtil.c \
                         CdcMalloc.c \
                         cdcIniparser/cdcDictionary.c   \
                         cdcIniparser/cdcIniparserapi.c \
                         cdcIniparser/cdcIniparser.c \
                         CdcSinkInterface.c \
                         filesink/CdcBSSink.c \
                         filesink/CdcPicSink.c \
                         filesink/SinkMd5.c \
                         CdcTimeUtil.c

libcdc_base_la_CFLAGS = $(CFLAGS_CDCG)
LOCAL_INCLUDE = -I./include \
                -I./filesink/include \
                -I$(top_srcdir)/include

libcdc_base_la_CFLAGS += $(LOCAL_INCLUDE)
libcdc_base_la_CFLAGS += -DCONF_USE_IOMMU

libcdc_base_la_LDFLAGS = $(LDFLAGS_CDCG)
LOCAL_LIB = -lpthread -lrt
libcdc_base_la_LDFLAGS += $(LOCAL_LIB)

