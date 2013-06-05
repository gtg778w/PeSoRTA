#define the build commands
CC=gcc
CFLAGS=-c -Wall
AR=ar
ARFLAGS= -rsv

SOURCES= PeSoRTA_config.c PeSoRTA_string.c PeSoRTA_vector.c
HEADERS= PeSoRTA.h PeSoRTA_helper.h
OBJECTS=$(SOURCES:.c=.o)

all: $(OBJECTS)

.c.o: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS)

