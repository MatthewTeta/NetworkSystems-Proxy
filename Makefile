# Author: Matthew Teta
# Date: 04/14/2023
# Build the proxy server

CC = gcc
CFLAGS = -Wall -Werror -g -DDEBUG -O0

SRCDIR = src
OBJDIR = obj
LIBDIR = libraries

SOURCES = $(SRCDIR)/md5.c $(SRCDIR)/blocklist.c $(SRCDIR)/connection.c $(SRCDIR)/IP.c $(SRCDIR)/http.c $(SRCDIR)/request.c $(SRCDIR)/response.c $(SRCDIR)/main.c
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
EXECUTABLE = main

all: clean mkdirs $(EXECUTABLE)

mkdirs:
	mkdir -p $(OBJDIR) $(LIBDIR) $(SRCDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXECUTABLE)

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)
