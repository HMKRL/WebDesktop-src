#include "httpRequest.h"
#include "color.h"

#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

void parseRequest(char *request, HTTPrequest *req) {
	char tk[128];
	char *saveptr;
	char *temp;
	req->content_length = 0;
	req->body = NULL;

	sscanf(request, "%s", tk);
	if(!strcmp(tk, "GET")) req->method = GET;
	else req->method = POST;
	
	request += strlen(tk) + 1;
	
	sscanf(request, "%s", req->file);
	if(!strcmp(req->file, "/")) strcpy(req->file, "/index.html");
	
	request += strlen(req->file) + 1;
	sscanf(request, "%s", req->protocal);
	
	temp = strtok_r(request, "\r\n", &saveptr);
	while((temp = strtok_r(NULL, "\r\n", &saveptr))) {
		if(saveptr[1] == '\r' && req->content_length) {
			req->body = calloc(req->content_length, 1);
			strncpy(req->body, saveptr + 3, req->content_length);
			return;
		}
		sscanf(temp, "%s", tk);

		if(!strcmp(tk, "Accept:")) {
			sscanf(temp, "%*s%s", req->accept);
		}
		if(!strcmp(tk, "Host:")) {
			sscanf(temp, "%*s%s", req->host);
		}
		if(!strcmp(tk, "Content-Length:")) {
			sscanf(temp, "%*s%d", &req->content_length);
		}
		if(!strcmp(tk, "Cookie:")) {
			strncpy(req->loginToken, strstr(temp, "loginToken=") + 11, 32);
		}

	}
	
	return;
}

void sendResponse(HTTPresponse *resp, int socketfd) {
	char buffer[256];
	char status[32];
	int bodylength = 0;

	if(resp->body) {
		bodylength = strlen(resp->body);
	}

	if(resp->filefd == -1 && !bodylength) { //no file to send
		strcpy(buffer, "HTTP/1.1 204 No Content\r\n\r\n");
		send(socketfd, buffer, strlen(buffer), 0);

		return;
	}

	bzero(buffer, sizeof(buffer));

	switch(resp->status) {
		case 200:
			strcpy(status, "OK");
			break;
		case 204:
			strcpy(status, "No Content");
			break;
		default:
			/* todo: other status code */
			break;
	}
	sprintf(buffer, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n", resp->status, status, resp->content_length + bodylength + 2);

	send(socketfd, buffer, strlen(buffer), 0);

	if(resp->body) send(socketfd, resp->body, bodylength, 0);
	if(resp->content_length) sendfile(socketfd, resp->filefd, NULL, resp->content_length);

	send(socketfd, "\r\n\r\n", 4, 0);

	return;
}
