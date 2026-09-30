CC = gcc --std=c99
CFLAGS = -Wall -Wextra -g -fsanitize=address

chadcserver: main.c
	$(CC) $(CFLAGS) main.c -o chadcserver

clean:
	rm -f chadcserver
