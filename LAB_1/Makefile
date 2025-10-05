CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror  

all: parent child

parent: parent.c general.h
	$(CC) $(CFLAGS) -o parent parent.c

child: child.c general.h
	$(CC) $(CFLAGS) -o child child.c

clean:
	rm -f parent child

test: all
	./parent

.PHONY: all clean test