#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

enum  {GET, POST};

typedef struct HTTPrequest {
	int method;
	int content_length;
	char file[128];
	char protocal[16];
	char host[32];
	char accept[128];
	char loginToken[33];
	char *body;
} HTTPrequest;

typedef struct HTTPresponse {
	int status; 			//200 OK/204 No Content
	int content_length; 	//Body content
	char* body;				//response content
	int filefd; 			//-1 means no file
} HTTPresponse;

//parse original http request, save into req
void parseRequest(char* request, HTTPrequest *req);

//send response to socketfd
void sendResponse(HTTPresponse* resp, int socketfd);

#endif
