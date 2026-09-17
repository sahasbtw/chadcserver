CC=gcc --std=c99

CFLAGS=-Wall -Wextra

http_server: main.c
	$(CC) $(CFLAGS) main.c -o http_server

