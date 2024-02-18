include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libsbm.la

libsbm_la_SOURCES = sbmStream.c \
                    sbmHwProcess.c \
                    sbmFrameH264.c \
                    sbmFrameH265.c \
                    sbmFrameAvs2.c \
                    sbmFrameBase.c

libsbm_la_CFLAGS = $(CFLAGS_CDCG)

LOCAL_INCLUDE = -I$(top_srcdir)/include \
                -I$(top_srcdir)/base/include \
                -I$(top_srcdir)/vdecoder/include

libsbm_la_CFLAGS += $(LOCAL_INCLUDE)

libsbm_la_LDFLAGS = $(LDFLAGS_CDCG)

libsbm_la_LIBADD = $(top_srcdir)/base/libcdc_base.la \
                        $(top_srcdir)/memory/libMemAdapter.la

LOCAL_LIB = -ldl -lpthread -lVE -lvideoengine
libsbm_la_LDFLAGS += $(LOCAL_LIB)
