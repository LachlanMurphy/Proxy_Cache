# Make file to compile web proxy code
# Lachlan Murphy 2025

CC = gcc

CFLAGS = -g -Wall -pthread -lpthread -lcrypto -lssl

default: all

all: proxy

proxy: proxy.c
	$(CC) -o proxy proxy.c array.c dynamic_array.c $(CFLAGS)

clean:
	rm proxy