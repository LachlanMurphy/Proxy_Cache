# Make file to compile web proxy code
# Lachlan Murphy 2025

CC = gcc

CFLAGS = -g -Wall -pthread -lpthread

default: all

all: proxy

proxy: proxy.c
	$(CC) $(CFLAGS) -o proxy proxy.c array.c

clean:
	rm proxy