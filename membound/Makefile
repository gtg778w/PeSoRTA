#define the build commands
CC=gcc
CFLAGS=-c -Wall

BINDIR=../../bin

MEMBOUND_SRCDIR=./
MEMBOUND_DATAFILE=membound_input.dat

all: $(BINDIR)/membound $(BINDIR)/$(MEMBOUND_DATAFILE)

$(BINDIR)/membound: $(MEMBOUND_SRCDIR)/membound.c
	gcc -Wall -O3 -o $(BINDIR)/membound $(MEMBOUND_SRCDIR)/membound.c -lrt

$(BINDIR)/$(MEMBOUND_DATAFILE): $(MEMBOUND_SRCDIR)/generatedatafile.m
	octave $(MEMBOUND_SRCDIR)/generatedatafile.m $(BINDIR)/$(MEMBOUND_DATAFILE)

clean:
	rm -rf $(BINDIR)/membound $(BINDIR)/$(MEMBOUND_DATAFILE)

