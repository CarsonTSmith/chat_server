CFLAGS = -g
CC = gcc

OBJS = $(BUILDDIR)/main.o
SRCS = main.c

SRCDIR = src
BUILDDIR = ./build
EXECNAME = $(BUILDDIR)/server

all: $(OBJS)
	$(CC) $(OBJS) -o $(EXECNAME)

#%.o: %.c
#	@$(CC.c) -MD -o $@ $<
#	@sed -i 's,\($*\.o\)[ :]*\(.*\),$@ : $$\(wildcard \2\)\n\1 : \2,g' $*.d

$(BUILDDIR)/main.o: main.c
	@mkdir -p $(BUILDDIR)
	$(CC) -c $(CFLAGS) main.c -o $(BUILDDIR)/main.o

clean:
	rm -f $(EXECNAME) $(BUILDDIR)/*.o *.d

-include $(SRCS:.c=.d)
