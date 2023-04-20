CC = gcc
CFLAGS = -Wall -Wextra -I../src -DDEBUG -g
LDFLAGS = -pthread
OBJDIR = ../obj
BINDIR = ../bin

SRCS = $(wildcard ../src/*.h)
OBJS = $(SRCS:../src/%.h=$(OBJDIR)/%.o)

TEST_SRCS = $(wildcard *.test.c)
TEST_OBJS = $(TEST_SRCS:%.c=$(OBJDIR)/%.o)
TEST_BINS = $(TEST_SRCS:%.c=$(BINDIR)/%)

.PHONY: all clean
all: mkdirs $(TEST_BINS)

mkdirs:
	mkdir -p $(OBJDIR) $(BINDIR)

$(BINDIR)/%: $(OBJS) $(OBJDIR)/%.o
	$(CC) $(LDFLAGS) $^ -o $@

# $(OBJDIR)/%.o: ../src/%.c
# 	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.test.o: %.test.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TEST_OBJS) $(TEST_BINS)

#  gcc -g -Wall -Wextra -o IP.test -I../src -DDEBUG -pthread IP.test.c ../obj/IP.o