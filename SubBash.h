#ifndef SUBBASH_H
#define SUBBASH_H

typedef struct subbash {
	int masterfd, slavefd;
	int isGranted;
	char user_host[300];
	char token[33];
	int (*login)(struct subbash**, char* username, char* password, char* token);
	char* (*execute)(struct subbash*, char* command, int* result_size, int echo);
} SubBash;

void initSubBash(SubBash**);
#endif
