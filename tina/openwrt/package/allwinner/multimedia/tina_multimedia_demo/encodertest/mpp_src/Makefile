TARGET := encodertest
INCLUDES +=	-I$(STAGING_DIR)/usr/include/allwinner/include -I$(STAGING_DIR)/usr/include/allwinner
LIBS += -lvencoder -lMemAdapter -lvenc_base -lcdc_base -lpthread -ldl -lm

SRCS = encodertest.c \
       avtimer.c

OBJS = $(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -g $(INCLUDES) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)

all:$(TARGET)


clean:
	rm -rf $(TARGET) *.o *.a *~
	cd common && rm -f *.o *.a *.bak *~ .depend
