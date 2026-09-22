CC=gcc --std=c99

CFLAGS=-Wall -Wextra

SERVERNAME=chadcserver

http_server: main.c
	$(CC) $(CFLAGS) main.c -o $(SERVERNAME)

