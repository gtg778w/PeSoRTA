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
DATDIR=./data

OUTLIBDIR=.
TARGET=$(OUTLIBDIR)/libPeSoRTA_membound.a

all: $(TARGET) $(DATDIR)/membound_input.dat

helperobjs:
	$(MAKE) -C $(HELPERDIR)

$(DATDIR)/membound_input.dat: $(SRCDIR)/generatedatafile.m
	octave $(SRCDIR)/generatedatafile.m $(DATDIR)/membound_input.dat

$(SRCDIR)/PeSoRTA_membound.o: $(SRCDIR)/PeSoRTA_membound.c $(SRCDIR)/membound.h $(HELPERDIR)/PeSoRTA_helper.h
	$(CC) -I $(PeSoRTAINC) -I $(HELPERDIR) $(CFLAGS) $(SRCDIR)/PeSoRTA_membound.c \
	-o $(SRCDIR)/PeSoRTA_membound.o

$(SRCDIR)/membound.o: $(SRCDIR)/membound.o $(SRCDIR)/membound.h
	$(CC) $(CFLAGS) $(SRCDIR)/membound.c -o $(SRCDIR)/membound.o

$(TARGET): $(SRCDIR)/PeSoRTA_membound.o $(SRCDIR)/membound.o helperobjs
	$(AR) $(ARFLAGS) $(TARGET) $(SRCDIR)/PeSoRTA_membound.o $(SRCDIR)/membound.o $(HELPEROBJS)

helperclean:
	$(MAKE) -C $(HELPERDIR) clean

clean: helperclean
	rm -f $(TARGET) $(SRCDIR)/PeSoRTA_membound.o $(SRCDIR)/membound.o $(DATDIR)/membound_input.dat

