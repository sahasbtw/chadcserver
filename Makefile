CC = gcc --std=c99
CFLAGS = -Wall -Wextra

chadcserver: main.c
	$(CC) $(CFLAGS) main.c -o chadserver

clean:
	rm -f ${EXECUTABLE}
