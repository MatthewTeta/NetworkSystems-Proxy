# Author: Matthew Teta
# Date: 04/14/2023
# Build the proxy server

CC = gcc
CFLAGS = -Wall -Werror -g -pthread -DDEBUG

SRCDIR = src
OBJDIR = obj
LIBDIR = libraries
TESTDIR = tests

SOURCES = $(SRCDIR)/md5.c $(SRCDIR)/cache.c $(SRCDIR)/blocklist.c $(SRCDIR)/IP.c $(SRCDIR)/http.c $(SRCDIR)/request.c $(SRCDIR)/response.c $(SRCDIR)/pid_list.c $(SRCDIR)/server.c $(SRCDIR)/proxy.c $(SRCDIR)/main.c
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
EXECUTABLE = main

all: mkdirs $(EXECUTABLE)

mkdirs:
	mkdir -p $(OBJDIR) $(LIBDIR) $(SRCDIR)

# Call the Makefile in the tests directory
# tests:
# 	$(MAKE) -C $(TESTDIR)

# cpmd5:
# 	cp $(LIBDIR)/md5-c/md5.h $(SRCDIR)
# 	cp $(LIBDIR)/md5-c/md5.c $(SRCDIR)

# $(OBJDIR)/md5.o: $(SRCDIR)/md5.c
# 	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXECUTABLE)

clean:
	rm -rf $(OBJDIR) $(EXECUTABLE)
