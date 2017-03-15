
CC=gcc
CFLAGS=-c -Wall -Wextra -g -O0 -D_GNU_SOURCE=1

all: synclink

synclink: fileops.o stringops.o synclink.o
	$(CC) fileops.o stringops.o synclink.o -o synclink

synclink.o: synclink.c fileops.h stringops.h
	$(CC) $(CFLAGS) synclink.c

fileops.o: fileops.c fileops.h stringops.h
	$(CC) $(CFLAGS) fileops.c

stringops.o: stringops.c stringops.h fileops.h
	$(CC) $(CFLAGS) stringops.c

clean:
	rm *.o synclink
