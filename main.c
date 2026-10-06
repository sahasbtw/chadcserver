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
#include <sys/types.h>
#include <dirent.h>

#define SERVERNAME "chadcserver"
#define MAXCONN	3
#define MAXBUFF	1024
#define INDEX_HTML "index.html"
#define METHOD_MAX 17
#define HTTP_V_SIZE sizeof("HTTP/1.1")
#define LOG_SEPRTR '-'

/* Globals */
int port = 8000;
char host_path[128] = ".";
char index_file[128] = "www/index.html";
/* TODO : Fix this crap */
char *headers = "HTTP/1.1 200 OK\r\nServer: chadcserver\r\n\n";
char *method_not_allowed = "HTTP/1.1 405 METHOD NOT ALLOWED\r\nServer: chadcserver\r\n\n";

const char *http_methods[] = {"GET","HEAD","POST"};  /* Longest method name should be the last */

typedef struct {
	char method[METHOD_MAX];
	char res[MAXBUFF];
	char http_v[HTTP_V_SIZE];
} Response;

Response parse_resp(char *req) {
	int j = 0;
	int req_elemnt = 0;
	char buff[MAXBUFF] = {0};
	Response resp;

	for (int i = 0; req_elemnt < 3; i++) {
		switch(req[i]) {
			case ' ':
				req_elemnt++;
				buff[j] = '\0';

				if (req_elemnt == 1)
					strncpy(resp.method, buff, METHOD_MAX);
				else if (req_elemnt == 2)
					strncpy(resp.res, buff, MAXBUFF);

				j = 0;
				memset(buff, 0, strlen(buff));
				break;
			case '\r': case '\n':
				req_elemnt++;
				buff[j] = '\0';
				strncpy(resp.http_v, buff, HTTP_V_SIZE);
				break;
			default:
				buff[j] = req[i];
				j++;
				break;
		}
	}

	return resp;
}

/* Perror with exit */
void perrexit(char *dbugmsg)
{
	int errnum = errno;         /* Making sure to get the errno right after */
	perror(dbugmsg);
	exit(errnum);               /* Exits with the same errno code */
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


int returnmethod(char *method)
{
	size_t i = 0;
	/* Some arbitrary number; i.e 9, to just detect an unsupported method */
	size_t method_index = 9;

	for (i = 0; i < countof(http_methods); i++)
		if (strcmp(method, http_methods[i]) == 0)
			method_index = i;

	return method_index;
}

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

void craftpath(char *file, char *dir, char *filename)
{
	for (size_t i = 0; i < strlen(dir); i++)
		if (dir[i] == '/' && dir[i+1] == '\0') {
			strcpy(file, dir);
			strcat(file, filename);
		} else {
			strcpy(file, dir);
			strcat(file, "/");
			strcat(file, filename);
		}
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
					}
				} else
					usrerrexit("Invalid Directory Name");

			/* Checking port if given, defaults to 8000 if not */
			} else if (strcmp(argv[i], "-p") == 0)
			{
				if (++i < argc) {
					if (isint(argv[i]) == 1)
						usrerrexit("Invalid Port Number");
					else 
						port = atoi(argv[i]);
				} else
					usrerrexit("Invalid Port Number");
			}
		}
	}

}

void print_header()
{
	printf("---------------\n| %s |\n---------------\n", SERVERNAME);
	printf("Hosting directory : %s\n", host_path);
	printf("PORT : %d\n\n", port);
}

void print_logs(const size_t content_size, char *content, char log_separator)
{
	for (int i = 0; i < 80; i++)
		printf("%c", log_separator);
	printf("\n");
	printf("|  | Content Received : %3zd |\n", content_size);
	printf("-----------------------------\n\n");
	printf("%s\n", content);
}


int main(int argc, char *argv[])
{
	arghandling(argc, argv);

	if (fileindir(host_path, INDEX_HTML) == 0) {
		memset(index_file, 0, sizeof(index_file));
		craftpath(index_file, host_path, INDEX_HTML);
	} else
		printf("Using default %s landing page at www/index.html\n", SERVERNAME);

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

	print_header();

	char msg_recvd[MAXBUFF];

	while (listen(socket_fd, MAXCONN) == 0) {
		/* Setting up a new socket fd to receive and send data */
		int new_socket_fd = accept(socket_fd, &serv_addr, &socket_len);
		if (new_socket_fd < 0)
			perrexit("Creating a new socket failed..!");

		/* Receiving a message */ 
		ssize_t recvd_data = recv(new_socket_fd, &msg_recvd, sizeof(msg_recvd), 0);

		Response resp_parsd = parse_resp(msg_recvd);
		/* Responding depending on the request type */
		switch (returnmethod(resp_parsd.method)) {
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
		print_logs(recvd_data, msg_recvd, LOG_SEPRTR);

		/* Clean the array of received data length */
		memset(msg_recvd, 0, (size_t)recvd_data);
	}

	perrexit("Listening failed..!");

	if (shutdown(socket_fd, SHUT_RDWR) < 0)
		perrexit("Couldn't close the socket");

	return 0;
}
