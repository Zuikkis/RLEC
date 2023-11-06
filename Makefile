CC = gcc
CFLAGS = -Wall -g -O0

all: rlec rlecid

rlec: rlec.c
	    $(CC) $(CFLAGS) -o $@ $^

rlecid: rlecid.c
	    $(CC) $(CFLAGS) -o $@ $^

