CC = gcc
CFLAGS = -Wall -pthread

all: fryzjersemafory

fryzjersemafory: fryzjersemafory.c
	$(CC) $(CFLAGS) -o f fryzjersemafory.c

clean:
	rm -f f
