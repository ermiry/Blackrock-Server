src = 	$(wildcard src/*.c) \
		$(wildcard src/blackrock/*.c) \
		$(wildcard src/utils/*.c)

objs = $(src:.c=.o)

IDIR = ./include/

CC = gcc
CFLAGS = -I $(IDIR) -l pthread -l sqlite3 $(DEFINES) $(RUN_MAKE) $(DEBUG) 

# for debugging...
DEBUG = -g

# print additional information
DEFINES = -D CERVER_DEBUG -D DEBUG

# run from parent folder
RUN_BIN = -D RUN_FROM_BIN

# run from bin folder
RUN_MAKE = -D RUN_FROM_MAKE

server: $(objs)
	$(CC) $^ $(CFLAGS) -o ./bin/server

run:
	./bin/server

clean:
	rm ./bin/server 