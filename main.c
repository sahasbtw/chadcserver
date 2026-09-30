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
#include <sys/types.h>
#include <dirent.h>

#define SERVERNAME "chadcserver"

#define MAXCONN	3
#define MAXBUFF	1024

const char *current_path = ".";
/* Longest method name should be the last of the array */
const char *http_methods[] = {"GET","HEAD","POST"};
const char log_separator = '='; 

int port = 8000;
int default_index = 1;

/* TODO : Fix this crap */
char *headers = "HTTP/1.1 200 OK\r\nServer: chadcserver\r\n\n";
char *method_not_allowed = "HTTP/1.1 405 METHOD NOT ALLOWED\r\nServer: chadcserver\r\n\n";
char host_path[128];
char index_file[128];
char index_filename[] = "index.html";

/* Check if a string is a digit */
int isint(const char *s)
{
	while (*s) {
		char c = *s++;
		if (((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
			return 1;
	}
	return 0;
}

/* Perror with exit */
void perrexit(char *dbugmsg)
{
	int errnum = errno;			/* Making sure to get the errno right after */
	perror(dbugmsg);
	exit(errnum);				/* Exits with the same errno code */
}

/* Exit with more user friendly custom error message */
void usrerrexit(char *dbugmsg)
{
	fprintf(stderr, "%s\n", dbugmsg);
	exit(EXIT_FAILURE);
}

/* Returns content of a file to buffer */
char *readfile(char *filename)
{
	FILE *fp; 
	fp = fopen(filename, "r");
	if (fp == NULL)
		perrexit("readfile -> fopen");

	fseek(fp, 0, SEEK_END);
	long eof = ftell(fp);
	rewind(fp);

	char *buff = malloc(eof + 1);
	size_t ret = fread(buff, 1, eof, fp);
	fclose(fp);

	if (ret < (size_t)eof)
		perrexit("readfile() fread");

	buff[eof] = '\0';
	return buff;
}

/* Returns a concatenated string of headers & body to a buffer */
char *craftresp(char *headers, char *body)
{
	size_t h_len = strlen(headers);
	size_t b_len = strlen(body);

	char *buff = malloc(h_len + b_len + 1);

	strcpy(buff, headers);
	strcat(buff, body);
	buff[h_len + b_len] = '\0';
	return buff;
}

int returnmethod(char *req)
{
	size_t i = 0;
	/* Some arbitrary number; i.e 9, to just detect an unsupported method */
	size_t method_index = 9;
	char *method = malloc(strlen(http_methods[2]) + 1);

	for (i = 0; req[i] != ' '; i++)
		method[i] = req[i];
	method[i] = '\0';

	for (i = 0; i < countof(http_methods); i++)
		if (strcmp(method, http_methods[i]) == 0)
			method_index = i;
	free(method);   /* Be sure to always free it*/
	return method_index;
}

int validdir(const char *dirname)
{
	DIR *dirp = opendir(dirname);
	if (dirp == NULL) {
		perror("opendir");
		return 1;
	} else
		return 0;
}

/* See if a file exists in a given directory */
int fileindir(const char *dirname, const char *filename)
{
	DIR *dir_p = opendir(dirname);
	if (dir_p == NULL)
		perrexit("fileindir -> opendir");

	struct dirent *de;
	while ((de = readdir(dir_p)) != NULL) {
            if (strcmp(de->d_name,filename) == 0) {
				closedir(dir_p);
				return 0;
			}
	}

	return 1;
}

void getresp(int fd, char *headers, char *path)
{
	char *body = readfile(path); /* free this */
	char *resp = craftresp(headers, body); /* free this */

	if (send(fd, resp, strlen(resp), 0) < 0)
		perrexit("FAILED: Sending GET resp to client");

	free(resp); /* Freed it */
	free(body); /* Freed it */
}

void headresp(int fd, char *headers)
{
	if (send(fd, headers, strlen(headers), 0) < 0)
		perrexit("FAILED: Sending HEAD resp to client");
}


void unknownresp(int fd, char *headers)
{
	if (send(fd, headers, strlen(headers), 0) < 0)
		perrexit("FAILED: Sending 405 resp to client");
}

void arghandling(int argc, char *argv[])
{
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			/* Checking directory path */
			if (strcmp(argv[i], "-h") == 0) { 
				printf("USAGE: %s -d DIR -p PORT\n", argv[0]);
				exit(0);
			} else if (strcmp(argv[i], "-d") == 0) { 
				if (++i < argc) {
					if (validdir(argv[i]) != 0)
						perrexit("Invalid Directory Path");
					else {
						memset(host_path, 0, sizeof(host_path));
						strcpy(host_path, argv[i]);
						default_index = 0;
					}
				} else usrerrexit("Invalid Directory Name");

			/* Checking port if given, defaults to 8000 if not */
			} else if (strcmp(argv[i], "-p") == 0)
			{
				if (++i < argc) {
					if (isint(argv[i]) == 1)
						usrerrexit("Invalid Port Number");
					else 
						port = atoi(argv[i]);
				} else usrerrexit("Invalid Port Number");
			}
		}
	}

}

int main(int argc, char *argv[])
{
	strcpy(host_path, current_path);
	arghandling(argc, argv);

	if (fileindir(host_path, index_filename) == 0) {
		for (size_t i = 0; i < strlen(host_path) ; i++)
			if (host_path[i] == '/' && host_path[i+1] == '\0') {
				strcpy(index_file, host_path);
				strcat(index_file, index_filename);
			} else {
				strcpy(index_file, host_path);
				strcat(index_file, "/");
				strcat(index_file, index_filename);
			}
	} else
		printf("No index file detected, using default %s landing page\n", SERVERNAME);

	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0)
		perrexit("Initialising socket failed..!");

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);                    /*  8000 - uint16_t    */
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 - uint32_t */

	/* Eliminate Address already in use error */
	int yes = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes)) < 0)
		perrexit("Failed at setting options..!");

	socklen_t socket_len = sizeof(serv_addr);
	if (bind(socket_fd, &serv_addr, socket_len) < 0)
		perrexit("Binding failed..!");

	printf("============\n%s\n============\n", SERVERNAME);
	printf("Hosting directory : %s\n", host_path);
	printf("PORT : %d\n\n", port);

	while (listen(socket_fd, MAXCONN) == 0) {
		/* Setting up a new socket fd to receive and send data */
		int new_socket_fd = accept(socket_fd, &serv_addr, &socket_len);
		if (new_socket_fd < 0)
			perrexit("Creating a new socket failed..!");

		/* Receiving a message */ 
		char msg_buff[MAXBUFF];
		ssize_t recvd_data = recv(new_socket_fd, &msg_buff, sizeof(msg_buff), 0);

		/* Responding depending on the request type */
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

	perrexit("Listening failed..!");

	if (shutdown(socket_fd, SHUT_RDWR) < 0)
		perrexit("Couldn't close the socket");

	return 0;
}
