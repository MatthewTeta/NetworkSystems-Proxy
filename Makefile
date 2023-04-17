# Author: Matthew Teta
# Date: 04/14/2023
# Build the proxy server

CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -std=c99 -pedantic

all: mkdist proxy

mkdist:
	mkdir -p dist

proxy: proxy.o
	$(CC) $(CFLAGS) -o proxy proxy.o

proxy.o: proxy.c
	$(CC) $(CFLAGS) -c proxy.c

clean:
	rm -rf dist
