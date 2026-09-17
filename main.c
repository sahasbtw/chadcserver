#define _GNU_SOURCE		/* Needs to be the first line */

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

#define MAXCONN		3
#define MAXBUFF		1024
#define PORT		8000

char *headers = "HTTP/1.1 200 OK\n\n";
const char *http_methods[] = {"GET","HEAD","POST"};
enum HTTPMethods {
	GET,
	HEAD,
	POST,
};

const char *http_status[][2] = { 
	{"200","OK"}, 
	{"404","NOT FOUND"}
};

void
errhandling(char *dbugmsg)
{
	int errnum = errno;			/* Making sure to get the errno right after */
	const char *errname = strerrorname_np(errnum);
	const char *errdesc = strerrordesc_np(errnum);
	printf("ERROR : %s\n", dbugmsg);	/* Custom messages for ez debuging */
	printf("        %d %s - %s\n", errnum, errname, errdesc);
	exit(errnum);				/* Exits with the same errno code */
}


char
*craftresponse(char *path, char *headers)
{
	size_t h_len = strlen(headers);
	FILE *fp; 
	fp = fopen(path, "r");
	if (fp == NULL) { errhandling("Can't open file to read..!"); }

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
	size_t i;
	/* POST is the method with the longest name supported */
	char *method = malloc(strlen(http_methods[POST]) + 1);

	for (i = 0; req[i] != ' '; i++)
		method[i] = req[i];
	method[i] = '\0';

	for (i = 0; i < countof(http_methods); i++)
		if (strcmp(method, http_methods[i]) == 0)
			printf("Its %s method\n", http_methods[i]);

	free(method); /* Be sure to, always*/
	return i;
}

int
main()
{
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) errhandling("Initialising socket failed..!");

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);		     /* 8000 - uint16_t	    */
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 - uint32_t */

	int yes = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0)
		errhandling("Failed at setting options..!");

	socklen_t socket_len = sizeof(serv_addr);
	if (bind(socket_fd, &serv_addr, socket_len) < 0)
		errhandling("Binding failed..!");

	while (1) {
		if (listen(socket_fd, MAXCONN) < 0)
			errhandling("Listening failed..!");

		/* Setting up a new socket fd to receive and send data */
		int new_socket_fd = accept(socket_fd, &serv_addr, &socket_len);
		if (new_socket_fd < 0) errhandling("Creating a new socket failed..!");

		/* Receiving a message */ 
		char msgbuff[MAXBUFF];		/* for the receiving message */
		ssize_t recv_data = recv(new_socket_fd, &msgbuff, sizeof(msgbuff), 0);

		/* Sending a message */
		if (returnmethod(msgbuff)) {
			char *response = craftresponse("www/index.html", headers); /* free this */

			if (send(new_socket_fd, response, strlen(response), 0) < 0)
				errhandling("Sending a message to client failed..!");

			free(response); /* Freed it */
		}

		shutdown(new_socket_fd, SHUT_RDWR);
		printf("Content Received = %zd \n", recv_data);
		printf("Response ::\n%s \n", msgbuff);

		/* Clean the array of received data length */
		memset(msgbuff, '\0', recv_data);
	}

	if (shutdown(socket_fd, SHUT_RDWR) < 0) errhandling("Couldn't close the socket");
	return 0;
}
