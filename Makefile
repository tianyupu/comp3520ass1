CC = gcc
CFLAGS = -Wall -g

BINARIES = strtokeg

.PHONY: all clean

all:
	$(CC) strtokeg.c -o strtokeg $(CFLAGS)

clean:
	-rm -f $(BINARIES)
