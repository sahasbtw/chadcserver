#define _GNU_SOURCE		/* Needs to be the first line */

/* 
 * Resources:
 *
 * Source - https://stackoverflow.com/a/37241328 
 * Posted by Vlad from Moscow, modified by community. See post 'Timeline' for change history
 * Retrieved 2026-09-20, License - CC BY-SA 3.0 
 *
 * https://pythonexamples.org/c/how-to-check-if-string-ends-with-specific-suffix
 *
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdcountof.h>

#define SERVERNAME "chadcserver"

#define MAXCONN	3
#define MAXBUFF	1024

int port = 8000;
char *headers = "HTTP/1.1 200 OK\r\nServer: chadcserver\r\n\n";
char *method_not_allowed = "HTTP/1.1 405 METHOD NOT ALLOWED\r\nServer: chadcserver\r\n\n";
const char *http_methods[] = {"GET","HEAD","POST"};
const char log_separator = '='; 

void
errhandling(char *dbugmsg)
{
	int errnum = errno;			/* Making sure to get the errno right after */
	const char *errname = strerrorname_np(errnum);
	const char *errdesc = strerrordesc_np(errnum);
	printf("ERROR: %s\n", dbugmsg);	/* Custom messages for ez debuging */
	printf("DESC :  %d %s - %s\n", errnum, errname, errdesc);
	exit(errnum);				/* Exits with the same errno code */
}

/* Joins header str with content of a file */
char
*craftresp(char *path, char *headers)
{
	size_t h_len = strlen(headers);
	FILE *fp; 
	fp = fopen(path, "r");
	if (fp == NULL) errhandling("Can't open file to read..!");

	fseek(fp, 0, SEEK_END);
	long eof = ftell(fp);
	rewind(fp);

	char *buff = malloc(h_len + eof + 1);

	memcpy(buff, headers, h_len);
	fread(buff + h_len, 1, eof, fp);
	buff[h_len + eof] = '\0';
	fclose(fp);

	return buff;
}

int
returnmethod(char *req)
{
	size_t i = 0;
	size_t method_index = 9; /* Some arbitrary number to just detect an unsupported method */
	char *longest_method = (char *)(&http_methods + 1) - 1; /* Longest method is the last of the array */
	char *method = malloc(strlen(longest_method) + 1);

	for (i = 0; req[i] != ' '; i++)
		method[i] = req[i];
	method[i] = '\0';

	for (i = 0; i < countof(http_methods); i++)
		if (strcmp(method, http_methods[i]) == 0)
			method_index = i;
	free(method);   /* Be sure to always free it*/
	return method_index;
}

void
getresp(int fd, char *headers, char *path)
{
	char *resp = craftresp(path, headers); /* free this */

	if (send(fd, resp, strlen(resp), 0) < 0)
		errhandling("FAILED: Sending GET resp to client");

	free(resp); /* Freed it */
}

void
headresp(int fd, char *headers)
{
	if (send(fd, headers, strlen(headers), 0) < 0)
		errhandling("FAILED: Sending HEAD resp to client");
}


void
unknownresp(int fd, char *headers)
{
	if (send(fd, headers, strlen(headers), 0) < 0)
		errhandling("FAILED: Sending 405 resp to client");
}

int
main(int argc, char *argv[])
{
	char index_file[128];
	char index_filename[] = "index.html";

	if (argc == 1) {
		printf("USAGE: %s -d DIR -p PORT\n", argv[--argc]);
		exit(1);
	} else
	{
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-d") == 0) {
				if (++i < argc) {
					FILE *fp; 
					char dir_suffix[] = "/";
					strcpy(index_file, argv[i]);    
					/* Check it the path provided has a forward slash at the end, if not add one */
					char *suffix_provided = strstr(argv[i], dir_suffix);
					if (suffix_provided == NULL && suffix_provided != argv[i] + strlen(argv[i]) - strlen(dir_suffix))
						strcat(index_file, "/");
					strcat(index_file, index_filename); 
					/* Check it the path provided has a forward slash at the end, if not add one */
					fp = fopen(index_file, "r");
					if (fp == NULL) errhandling("Can't open file to read..!");
				} else {
					printf("ERROR: No directory given after -d\n");
					exit(1);
				}
			} else if (strcmp(argv[i], "-p") == 0) {
				if (++i < argc) port = atoi(argv[i]);
				else {
					printf("ERROR: empty port number after -p\n");
					exit(1);
				}
			}
		}
	}

	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) errhandling("Initialising socket failed..!");

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);                    /*  8000 - uint16_t    */
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 - uint32_t */

	int yes = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0)
		errhandling("Failed at setting options..!");

	socklen_t socket_len = sizeof(serv_addr);
	if (bind(socket_fd, &serv_addr, socket_len) < 0)
		errhandling("Binding failed..!");

	printf("============\n%s\n============\n", SERVERNAME);
	printf("Hosting directory : %s\n", index_file);
	printf("PORT : %d\n\n", port);

	while (listen(socket_fd, MAXCONN) == 0) {
		/* Setting up a new socket fd to receive and send data */
		int new_socket_fd = accept(socket_fd, &serv_addr, &socket_len);
		if (new_socket_fd < 0) errhandling("Creating a new socket failed..!");

		/* Receiving a message */ 
		char msg_buff[MAXBUFF];
		ssize_t recvd_data = recv(new_socket_fd, &msg_buff, sizeof(msg_buff), 0);

		/* Sending a message depending on the request type */
		switch (returnmethod(msg_buff)) {
			case 0:
				getresp(new_socket_fd, headers, index_file);
				break;
			case 1:
				headresp(new_socket_fd, headers);
				break;
			default:
				unknownresp(new_socket_fd, method_not_allowed);
				break;
		}

		shutdown(new_socket_fd, SHUT_RDWR);
		printf("Content Received : %zd \n%s\n", recvd_data, msg_buff);
		for (int i = 0; i < 50; i++)
			printf("%c", log_separator);
		printf("\n");

		/* Clean the array of received data length */
		memset(msg_buff, '\0', recvd_data);
	}

	errhandling("Listening failed..!");
	if (shutdown(socket_fd, SHUT_RDWR) < 0) errhandling("Couldn't close the socket");
	return 0;
}
