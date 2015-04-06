CC=gcc
CFLAGS=-Wall -g -c
LDFLAGS=-lm
MAKEFLAGS += -r
#CFLAGS= -Wall  -fmudflap  -g -c 
#LDFLAGS=-lmudflap -rdynamic


SOURCES=allocator.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLES=milestone3

#this makes sure that typing 'make' will execute 'make all'
.default: all

%.o: %.c
	$(CC) $(CFLAGS)  $< 



milestone3: allocator.o
	$(CC) $(LDFLAGS)  -o $@ $+


all: $(OBJECTS) $(EXECUTABLES)

clean:
	rm *.o $(EXECUTABLES)
