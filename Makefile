CC=gcc
FLAGS=-Wall -Wextra -pedantic -Ofast

.PHONY: clean

cian: cian.c
	$(CC) *.c -o cian $(FLAGS)

clean:
	rm -rf *.o cian
