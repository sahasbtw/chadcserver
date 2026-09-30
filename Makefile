CC = gcc --std=c99
CFLAGS = -Wall -Wextra

chadcserver: main.c
	$(CC) $(CFLAGS) main.c -o chadcserver

clean:
	rm -f chadcserver
