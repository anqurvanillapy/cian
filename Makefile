CC=gcc
FLAGS=-Wall -Wextra -pedantic -Ofast

.PHONY: clean

cian: cian.c
	$(CC) -c *.c $(FLAGS)
	$(CC) *.o -o cian

clean:
	rm -rf *.o cian
