CC = gcc --std=c99
CFLAGS = -Wall -Wextra

EXECUTABLE = chadcserver

main: main.c
	$(CC) $(CFLAGS) main.c -o $(EXECUTABLE)

clean:
	rm -f ${EXECUTABLE}
