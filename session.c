#include "session.h"
#include "SubBash.h"

#include <stdlib.h>
#include <string.h>

void initSession(session** self) {
	*self = malloc(sizeof(session));
	memset((*self)->token, '\0', 33);
	memset((*self)->username, '\0', 100);
	memset((*self)->password, '\0', 100);

	for(int i = 0;i < 4;i++) {
		initSubBash(&((*self)->shells[i]));
	}
}
