#define the build commands
CC=gcc
CFLAGS=-c -Wall

SRCDIR=./src

APP_NAME=timing
APP_LIBFLAGS1=
APP_LIBFLAGS2=-lrt
APP_OBJS=./src/workload_timing.o
APP_BINDIR=./bin

PeSoRTADIR=..
PeSoRTA_LIBDIR=$(PeSoRTADIR)/lib
PeSoRTA_INCDIR=$(PeSoRTADIR)/include

all: PeSoRTA_apps

$(SRCDIR)/workload_timing.o: $(SRCDIR)/workload_timing.c $(PeSoRTA_INCDIR)/PeSoRTA.h
	$(CC) $(CFLAGS) -I $(PeSoRTA_INCDIR) -o $(SRCDIR)/workload_timing.o \
	$(SRCDIR)/workload_timing.c

include $(PeSoRTADIR)/PeSoRTA_APP.mk

clean: PeSoRTA_apps_clean
	rm -rf $(SRCDIR)/workload_timing.o

