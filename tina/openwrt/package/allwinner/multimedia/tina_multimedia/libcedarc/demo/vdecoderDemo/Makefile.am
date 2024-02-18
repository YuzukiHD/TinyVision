include $(top_srcdir)/Makefile.inc

bin_PROGRAMS = vdecoderdemo

vdecoderdemo_SOURCES =  \
                        rawStreamParser.c \
                        vdecoderDemo.c

vdecoderdemo_CFLAGS = $(CFLAGS_CDXG)
#jpegdemo_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
                -I$(top_srcdir)/include \
                -I$(top_srcdir)/base/include

vdecoderdemo_CFLAGS += $(LOCAL_INCLUDE)

vdecoderdemo_LDFLAGS = $(LDFLAGS_CDXG)

LOCAL_LIB = -lpthread -lm -lrt -ldl -lVE -lscaledown -lvideoengine

vdecoderdemo_LDFLAGS += $(top_srcdir)/base/libcdc_base.la \
                            $(top_srcdir)/memory/libMemAdapter.la \
                            $(top_srcdir)/vdecoder/libvdecoder.la \
                            $(top_srcdir)/vdecoder/fbm/libfbm.la \
                            $(top_srcdir)/vdecoder/sbm/libsbm.la \
                            $(top_srcdir)/vdecoder/aftertreatment/libaftertreatment.la

vdecoderdemo_LDFLAGS += $(LOCAL_LIB)
