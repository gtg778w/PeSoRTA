#define the build commands
CC=gcc
CFLAGS=-c -Wall -Wmissing-prototypes

SPHINX_HDIR=/usr/local/include
SPHINX_IFLAGS=-I $(SPHINX_HDIR)/sphinxbase/ -I $(SPHINX_HDIR)/pocketsphinx/

AR=ar
ARFLAGS= -rsv

PeSoRTADIR=..
PeSoRTAINC=$(PeSoRTADIR)/include

HELPERDIR=$(PeSoRTADIR)/PeSoRTAhelper
HELPEROBJS=$(HELPERDIR)/PeSoRTA_config.o $(HELPERDIR)/PeSoRTA_string.o

SRCDIR=./src

SW_HEADERS=$(SRCDIR)/sphinxwrapper.h $(SRCDIR)/sw_wav.h

SW_SRC=$(SRCDIR)/sphinxwrapper.c
SW_OBJ=$(SW_SRC:.c=.o)

OUTLIBDIR=.
TARGET=$(OUTLIBDIR)/libPeSoRTA_cmusphinx.a

all: $(TARGET)

helperobjs:
	$(MAKE) -C $(HELPERDIR)

$(SRCDIR)/PeSoRTA_cmusphinx.o: $(SRCDIR)/PeSoRTA_cmusphinx.c $(SW_HEADERS) \
$(HELPERDIR)/PeSoRTA_helper.h 
	$(CC) $(SPHINX_IFLAGS) -I $(PeSoRTAINC) -I $(HELPERDIR) $(CFLAGS) $< -o $@

.c.o: $(SW_HEADERS)
	$(CC) $(SPHINX_IFLAGS) $(CFLAGS) $< -o $@

$(TARGET): $(SRCDIR)/PeSoRTA_cmusphinx.o $(SW_OBJ) helperobjs
	$(AR) $(ARFLAGS) $(TARGET) $(SRCDIR)/*.o $(HELPEROBJS)

helperclean:
	$(MAKE) -C $(HELPERDIR) clean

clean: helperclean
	rm -f $(TARGET) $(SRCDIR)/*.o

