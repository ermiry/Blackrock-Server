SRCDIR = src

SOURCES = 	$(wildcard $(SRCDIR)/*.c) \
			$(wildcard $(SRCDIR)/blackrock/*.c) \
			$(wildcard $(SRCDIR)/utils/*.c)

objs = $(SOURCES:.c=.o)

IDIR = ./include/

CC = gcc
CFLAGS = -I $(IDIR) $(DEPENDENCIES) $(DEFINES) $(RUN_MAKE) $(DEBUG) 

SQLITE = -l sqlite3 
PTHREAD = -l pthread

DEPENDENCIES = $(SQLITE) $(PTHREAD)

# for debugging...
DEBUG = -g

# print additional information
DEFINES = -D CERVER_DEBUG -D DEBUG

# run from parent folder
RUN_BIN = -D RUN_FROM_BIN

# run from bin folder
RUN_MAKE = -D RUN_FROM_MAKE

OUTPUT = -o ./bin/server

server: $(objs)
	$(CC) $^ $(CFLAGS) $(OUTPUT)

run:
	./bin/server

clean:
	rm ./bin/server 