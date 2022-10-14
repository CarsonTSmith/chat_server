CFLAGS = -g
CC = gcc

OBJS = $(BUILDDIR)/main.o
SRCS = $(SRCDIR)/main.c

SRCDIR = src
BUILDDIR = ./build
EXECNAME = $(BUILDDIR)/server

all: $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) -o $(EXECNAME)
	@echo "Project Built"

$(BUILDDIR)/main.o: $(SRCDIR)/main.c
	@mkdir -p $(BUILDDIR)
	@$(CC) -c $(CFLAGS) $(SRCDIR)/main.c -o $(BUILDDIR)/main.o

clean:
	@rm -f $(EXECNAME) $(BUILDDIR)/*.o *.d
	@echo "Project Cleaned"
