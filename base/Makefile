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

OUTLIBDIR=.

TARGET=$(OUTLIBDIR)/libPeSoRTA_base.a

all: $(TARGET)

helperobjs:
	$(MAKE) -C $(HELPERDIR) 

$(SRCDIR)/PeSoRTA_base.o: $(SRCDIR)/PeSoRTA_base.c $(HELPERDIR)/PeSoRTA_helper.h 
	$(CC) -I $(PeSoRTAINC) -I $(HELPERDIR) $(CFLAGS) $< -o $@

$(TARGET): $(SRCDIR)/PeSoRTA_base.o helperobjs
	$(AR) $(ARFLAGS) $(TARGET) $(SRCDIR)/*.o $(HELPEROBJS)

helperclean:
	$(MAKE) -C $(HELPERDIR) clean

clean: helperclean
	rm -f $(TARGET) $(SRCDIR)/*.o

