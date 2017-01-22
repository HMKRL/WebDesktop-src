#ifndef SESSION_H
#define SESSION_H

#include "SubBash.h"

typedef struct {
	char token[33];
	SubBash* shells[4];
	char username[100];
	char password[100];
} session;

void initSession(session** self);

#endif
