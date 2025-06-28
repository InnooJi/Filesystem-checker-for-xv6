K=kernel

CC = gcc
.PHONY : clean

all: chkfs

chkfs: chkfs.c $K/fs.h $K/types.h
	gcc -Wall -I. -o chkfs chkfs.c

clean:
	rm -f chkfs

