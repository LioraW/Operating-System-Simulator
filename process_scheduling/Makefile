CC = gcc
CFLAGS = -Wall
HEADER_FILES =  clock.h messenger.h pcb_table.h queue.h oss.h
C_FILES = oss.c userprocess.c clock.c messenger.c pcb_table.c queue.c
DEPS = C_FILES HEADER_FILES
OBJ = oss.o userprocess.o clock.o messenger.o pcb_table.o queue.o

all: oss userprocess

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

oss: oss.o clock.o  messenger.o pcb_table.o queue.o
	$(CC) $(CFLAGS) -o $@ $^ -lm -lpthread -lrt

userprocess: userprocess.o messenger.o
	$(CC) $(CFLAGS) -o $@ $^ -lm -lpthread -lrt
clean:
	rm -f *.o *.a oss userprocess


