#include "color.h"
#include "session.h"
#include "httpRequest.h"

#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <features.h>
#include <fcntl.h>

#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <netinet/tcp.h>
#include <netinet/in.h>

#include <arpa/inet.h>

int socketfd, clientfd;

void sigrouting(int);
void serveFile();

int main(int argc, char *argv[]) {
	signal(SIGINT, sigrouting);
	signal(SIGPIPE, SIG_IGN);
	session *ss;
	initSession(&ss);

	char readbuffer[1024];
	HTTPrequest *req = malloc(sizeof(HTTPrequest));

	struct sockaddr_in addr_in;
	socketfd = socket(PF_INET, SOCK_STREAM, 0);
	if(socketfd == -1) {
		puts("cannot open socket");
		exit(0);
	}

	int on = 1;
	setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
	//setsockopt(socketfd, SOL_SOCKET, TCP_CORK, (char*)&on, sizeof(on));

	bzero(&addr_in, sizeof(addr_in)); //clear buffer
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(80);
	addr_in.sin_addr.s_addr = INADDR_ANY;
	
	int result = bind(socketfd, (struct sockaddr*)&addr_in, sizeof(addr_in));
	if(result == -1) perror("bind");

	listen(socketfd, 10);

	while(1) {
		struct sockaddr_in client_addr;
		socklen_t addrlen = sizeof(client_addr);
		
		fprintf(stderr, RED"Waiting for connection\n");
		clientfd = accept(socketfd, (struct sockaddr*)&client_addr, &addrlen);
		fprintf(stderr, "Connection established!\n"NONE);

		bzero(readbuffer, sizeof(readbuffer));
		int rec = recv(clientfd, readbuffer, sizeof(readbuffer), 0);
		if(rec == -1) perror("recv from socket");

		printf(YELLOW"Client:\n%s\n"NONE, readbuffer);

		parseRequest(readbuffer, req);
		
		serveFile(req, &ss);
		
		close(clientfd);
	}
	
	return 0;
}

void sigrouting(int Sig) {
	puts("SIGINT detected");
	/* do something postSolve tasks*/
	close(socketfd);
	exit(0);
}

void serveFile(HTTPrequest *req, session **S) {
	HTTPresponse *resp = malloc(sizeof(HTTPresponse));
	resp->body = NULL;
	resp->content_length = 0;
	resp->status = 200;


	session *ss = *S;
	int filefd;
	int iscommand = 0;
	char target[256] = ".";
	char result[256] = {'\0'};
	printf(CYAN"File to serve: %s\n", strcat(target, req->file));

	/* ====== login ====== */
	if(!strcmp(req->file, "/login")) {
		initSession(S);
		ss = *S;

		sscanf(req->body, "%s%s", ss->username, ss->password);

		int success = ss->shells[0]->login(&(ss->shells[0]), ss->username, ss->password, ss->token);
		if(success) {
			strcpy(result, "success\r\n");
			strcat(result, ss->token);
		}
		else {
			strcpy(result, "failed");
		}
	}

	/* ====== shell ====== */
	else if(!strcmp(req->file, "/shell")) {
		if(strcmp(ss->token, req->loginToken)) {
			fprintf(stderr, "%s\n", "error: token timeout");
			strcpy(result, "session timeout");
		}
		else {
			int shellnum = -1;
			for(int i = 1;i < 4;i++) {
				if(!ss->shells[i]->isGranted) {
					shellnum = i;
					break;
				}
			}
			if(shellnum == -1) {
				strcpy(result, "no shell available");
			}
			else {
				if(ss->shells[shellnum]->login(&(ss->shells[shellnum]), ss->username, ss->password, NULL)) {
					sprintf(result, "success\r\n%d\r\n%s", shellnum, ss->shells[shellnum]->user_host);
				}
			}
		}
	}
	else if(!strcmp(req->file, "/command")) {
		if(strcmp(ss->token, req->loginToken)) {
			strcpy(result, "session timeout");
		}
		else {
			int shellnum;
			sscanf(req->body, "%d", &shellnum);
			
			if(!ss->shells[shellnum]->isGranted) {
				strcpy(result, "shell unavailable");
			}
			else {
				int res = 0;
				resp->body = ss->shells[shellnum]->execute(ss->shells[shellnum], req->body + 3, &res, 0);
				iscommand = 1;
			}
		}
	}
	else if(!strcmp(req->file, "/open")) {
		if(strcmp(ss->token, req->loginToken)) {
			strcpy(result, "session timeout");
		}
		else {
			char name[256];
			char cmd[256];
			int res = 0;
			sscanf(req->body, "%s", name);
			sprintf(cmd, "cd %s", name);
			char *output = ss->shells[0]->execute(ss->shells[0], cmd, &res, 0);
			if(output == NULL) {//cd success
				output = ss->shells[0]->execute(ss->shells[0], "ls -l", &res, 0);
			}
			else {//is not a directory
				filefd = open(name, O_RDONLY);
				if(filefd == -1) {
					fprintf(stderr, "/open failed\n");
					strcpy(result, "");
				}
			}
		}
	}
	else {
		filefd = open(target, O_RDONLY);

		if(filefd == -1) {
			printf(RED"Request file \"%s\" not found.\n"NONE, target);
			resp->status = 204;
			resp->content_length = 0;
			resp->filefd = -1;
		}
		else {
			resp->status = 200;
			resp->content_length = lseek(filefd, 0, SEEK_END);
			lseek(filefd, 0, SEEK_SET);
			resp->filefd = filefd;
		}
	}
	if(!iscommand) resp->body = result;
	fprintf(stderr, GREEN"BODY:\n%s\n"NONE, resp->body);

	sendResponse(resp, clientfd);

	free(resp);

	return;
}
