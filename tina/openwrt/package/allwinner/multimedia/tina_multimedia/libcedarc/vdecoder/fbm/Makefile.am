include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libfbm.la

libfbm_la_SOURCES = fbm.c

libfbm_la_CFLAGS = $(CFLAGS_CDCG)

LOCAL_INCLUDE = -I$(top_srcdir)/include \
                -I$(top_srcdir)/base/include \
                -I$(top_srcdir)/vdecoder/include

libfbm_la_CFLAGS += $(LOCAL_INCLUDE)

libfbm_la_LDFLAGS = $(LDFLAGS_CDCG)

libfbm_la_LIBADD = $(top_srcdir)/memory/libMemAdapter.la \
                   $(top_srcdir)/base/libcdc_base.la

LOCAL_LIB = -ldl -lpthread -lvideoengine
libfbm_la_LDFLAGS += $(LOCAL_LIB)
