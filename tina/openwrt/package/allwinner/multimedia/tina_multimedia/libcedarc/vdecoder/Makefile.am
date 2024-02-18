include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libvdecoder.la

libvdecoder_la_SOURCES = pixel_format.c \
                         vdecoder.c

libvdecoder_la_CFLAGS = $(CFLAGS_CDCG)
LOCAL_INCLUDE = -I./ \
                -I./videoengine \
                -I./include \
                -I$(top_srcdir)/include \
                -I$(top_srcdir)/base/include \
                -I$(top_srcdir)/vdecoder/aftertreatment

libvdecoder_la_CFLAGS += $(LOCAL_INCLUDE)

libvdecoder_la_LDFLAGS = $(LDFLAGS_CDCG)

libvdecoder_la_LIBADD = $(top_srcdir)/base/libcdc_base.la \
                        $(top_srcdir)/memory/libMemAdapter.la \
                        $(top_srcdir)/vdecoder/fbm/libfbm.la \
                        $(top_srcdir)/vdecoder/sbm/libsbm.la \
                        $(top_srcdir)/vdecoder/aftertreatment/libaftertreatment.la

LOCAL_LIB = -ldl -lpthread -lVE -lvideoengine
libvdecoder_la_LDFLAGS += $(LOCAL_LIB)
