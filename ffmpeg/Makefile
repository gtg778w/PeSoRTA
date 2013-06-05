#define the build commands
CC=gcc
CFLAGS=-c -Wall -Wmissing-prototypes
AR=ar
ARFLAGS= -rsv

PeSoRTADIR=..
PeSoRTAINC=$(PeSoRTADIR)/include

HELPERDIR=$(PeSoRTADIR)/PeSoRTAhelper
HELPEROBJS=$(HELPERDIR)/PeSoRTA_config.o $(HELPERDIR)/PeSoRTA_string.o

SRCDIR=./src

FW_HEADERS=$(SRCDIR)/ffmpegwrapper.h

FW_SRC=$(SRCDIR)/fw_encoder.c $(SRCDIR)/fw_decoder.c $(SRCDIR)/fw_video.c \
$(SRCDIR)/fw_audio.c
FW_OBJ=$(FW_SRC:.c=.o)

OUTLIBDIR=.
TARGET=$(OUTLIBDIR)/libPeSoRTA_ffmpeg.a

all: $(TARGET)

helperobjs:
	$(MAKE) -C $(HELPERDIR)

$(SRCDIR)/PeSoRTA_ffmpeg.o: $(SRCDIR)/PeSoRTA_ffmpeg.c $(FW_HEADERS) $(HELPERDIR)/PeSoRTA_helper.h
	$(CC) -I $(PeSoRTAINC) -I $(HELPERDIR) $(CFLAGS) $< -o $@

.c.o: $(FW_HEADERS)
	$(CC) $(CFLAGS) $< -o $@

$(TARGET): $(SRCDIR)/PeSoRTA_ffmpeg.o $(FW_OBJ) helperobjs
	$(AR) $(ARFLAGS) $(TARGET) $(SRCDIR)/*.o $(HELPEROBJS)

helperclean:
	$(MAKE) -C $(HELPERDIR) clean

clean: helperclean
	rm -f $(TARGET) $(SRCDIR)/*.o

