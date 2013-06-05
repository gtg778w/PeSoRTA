#define the build commands
CC=gcc
CFLAGS=-c -Wall
AR=ar
ARFLAGS= -rsv

PeSoRTADIR=..
PeSoRTAINC=$(PeSoRTADIR)/include

HELPERDIR=$(PeSoRTADIR)/PeSoRTAhelper
HELPEROBJS=$(HELPERDIR)/PeSoRTA_config.o $(HELPERDIR)/PeSoRTA_string.o

SRCDIR=./src

OUTLIBDIR=.
TARGET=$(OUTLIBDIR)/libPeSoRTA_sqrwav.a

all: $(TARGET)

helperobjs:
	$(MAKE) -C $(HELPERDIR)
	
$(SRCDIR)/PeSoRTA_sqrwav.o: $(SRCDIR)/PeSoRTA_sqrwav.c $(HELPERDIR)/PeSoRTA_helper.h
	$(CC) -I $(PeSoRTAINC) -I $(HELPERDIR) $(CFLAGS) $(SRCDIR)/PeSoRTA_sqrwav.c \
	-o $(SRCDIR)/PeSoRTA_sqrwav.o
	
$(TARGET): $(SRCDIR)/PeSoRTA_sqrwav.o helperobjs
	$(AR) $(ARFLAGS) $(TARGET) $(SRCDIR)/PeSoRTA_sqrwav.o $(HELPEROBJS)

helperclean:
	$(MAKE) -C $(HELPERDIR) clean

clean: helperclean
	rm -f $(TARGET) PeSoRTA_sqrwav.o
	
