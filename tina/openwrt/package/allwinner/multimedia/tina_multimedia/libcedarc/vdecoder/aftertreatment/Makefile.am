include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libaftertreatment.la

libaftertreatment_la_SOURCES = aftertreatment.c

libaftertreatment_la_CFLAGS = $(CFLAGS_CDCG)
LOCAL_INCLUDE = -I./ \
                -I./scaledown \
                -I$(top_srcdir)/include \
                -I$(top_srcdir)/base/include \
                -I$(top_srcdir)/vdecoder/include \
                -I$(top_srcdir)/vdecoder/aftertreatment/include

libaftertreatment_la_CFLAGS += $(LOCAL_INCLUDE)


libaftertreatment_la_LDFLAGS = $(LDFLAGS_CDCG)

libaftertreatment_la_LIBADD = $(top_srcdir)/base/libcdc_base.la \
                        $(top_srcdir)/memory/libMemAdapter.la \
                        $(top_srcdir)/vdecoder/fbm/libfbm.la

LOCAL_LIB = -ldl -lpthread -lscaledown -lVE
libaftertreatment_la_LDFLAGS += $(LOCAL_LIB)
