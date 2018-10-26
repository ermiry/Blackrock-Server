CC = gcc
CFLAGS = -I $(IDIR) -l pthread -D CERVER_DEBUG -D DEBUG

IDIR = ./include/
SRCDIR = ./src/

SOURCES = $(SRCDIR)*.c \
		  $(SRCDIR)/game/*.c \
		  $(SRCDIR)/utils/*.c

all: server #run #clean

server: $(SOURCES)
	$(CC) $(SOURCES) $(CFLAGS) -o ./bin/server

run:
	./bin/server

clean:
	rm ./bin/server 