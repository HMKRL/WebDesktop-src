#include "SubBash.h"
#include "color.h"

/* for pts */
#define _XOPEN_SOURCE 700

/* for cfmakeraw */
#define _BSD_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <termios.h>
#include <time.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

int login(SubBash** Self, char* username, char* password, char* token) {
	char buffer[10240] = {'\0'};
	int size = 0;
	int rc;
	fd_set fd_in;

	initSubBash(Self);
	SubBash *self = *Self;

	strcpy(self->user_host, username);
	strcat(self->user_host, "@");

	while(1) {
		FD_ZERO(&fd_in);
		FD_SET(self->masterfd, &fd_in);

		rc = select(self->masterfd + 1, &fd_in, NULL, NULL, NULL);
		if(rc == -1) perror("select");
		else {
			if(FD_ISSET(self->masterfd, &fd_in)) {
				size = read(self->masterfd, buffer, sizeof(buffer));
				buffer[size] = '\0';
//				printf("===%s", buffer);
				if(strstr(buffer, "login:") && !strstr(buffer, "pts")) {
					char hostname[256];
					sscanf(buffer, "%s", hostname);
					strcat(self->user_host, hostname);
					
//					printf("%s\n", self->user_host);
					write(self->masterfd, username, strlen(username));
					write(self->masterfd, "\n", 1);
				}
				if(strstr(buffer, "Password:")) {
					write(self->masterfd, password, strlen(password));
					write(self->masterfd, "\n", 1);
				}
				if(strstr(buffer, self->user_host)) {
					char id_time[64] = "";
					char *pre_hash;
					strcat(id_time, username);
					self->isGranted = 1;
					time_t timer;
					time(&timer);
					strcat(id_time, ctime(&timer));
					id_time[strlen(id_time) - 1] = '\0';

					if(token) {
						char command[100];
						bzero(command, sizeof(command));
						sprintf(command, "echo '%s' | md5sum", id_time);
						int tmp = 0; //for execute parameter
						pre_hash = self->execute(self, command, &tmp, 0);
						sscanf(pre_hash, "%s", token);
						strcpy(self->token, token);
						free(pre_hash);
					}
					fprintf(stderr, "%s\n", "login: success");

					return 1;
				}
				if(strstr(buffer, "Login incorrect")) {
					self->isGranted = 0;
					fprintf(stderr, "%s\n", "login: failed");
					return 0;
				}
			}
		}
	}
	return 0;
}

char* execute(SubBash* self, char* command, int* result_size, int echo) {
	if(!self->isGranted) return NULL;

	int size = 0;
	int readed = 0;
	int cur_max = 256;
	*result_size = 0;
	char buffer[256];
	char* output;

	/* result will be reallocate*/
	output = calloc(cur_max, 1);

	/* allocation failed */
	if(output == NULL) return NULL;

	if(!strcmp("exit", command)) {
		write(self->masterfd, command, strlen(command));
		write(self->masterfd, "\n", 1);
		*result_size = 0;
		self->isGranted = 0;
		return NULL;
	}

	write(self->masterfd, command, strlen(command));
	write(self->masterfd, "\n", 1);

	fd_set fd_in;

	while(1) {
		FD_ZERO(&fd_in);
		FD_SET(self->masterfd, &fd_in);

		int rc = select(self->masterfd + 1, &fd_in, NULL, NULL, NULL);
		if(rc == -1) perror("select");
		else {
			if(FD_ISSET(self->masterfd, &fd_in)) {

				size = read(self->masterfd, buffer, sizeof(buffer) - 1);
				if(size == -1) perror("read");
				buffer[size] = '\0';

				if(strstr(buffer, self->user_host)) {
					if(echo) {
						*result_size = readed;
						return output;
					}
					else {
						char *trimmed = calloc(strlen(output), 1);
						strcpy(trimmed, output + strlen(command) + 1);
						free(output);
						
						*result_size = readed - strlen(command) - 1;
						return trimmed;
					}
				}

				readed += size;

				if(cur_max <= readed) {
					cur_max *= 2;
					char *newout = realloc(output, cur_max);
					if(newout == NULL) fprintf(stderr, "%s\n", "realloc failed.");
					else output = newout;
				}
				strcat(output, buffer);
			}
		}
	}
}

void initSubBash(SubBash** self) {
	*self = malloc(sizeof(SubBash));
	(*self)->isGranted = 0;
	bzero((*self)->user_host, sizeof((*self)->user_host));

	(*self)->login = login;
	(*self)->execute = execute;

	/* create pts master */
	(*self)->masterfd = posix_openpt(O_RDWR);
	if((*self)->masterfd == -1) perror("posix_openpt");
	grantpt((*self)->masterfd);
	unlockpt((*self)->masterfd);

	/* create pts slave */
	(*self)->slavefd = open(ptsname((*self)->masterfd), O_RDWR);

	pid_t pid = fork();

	/* on fork failed */
	if(pid == -1) perror("fork");

	/* child process */
	if(pid == 0) {
		struct termios newSettings, origSettings;
		tcgetattr((*self)->slavefd, &origSettings);
		newSettings = origSettings;
		cfmakeraw(&newSettings);
		tcsetattr((*self)->slavefd, TCSANOW, &newSettings);

		close((*self)->masterfd);

		dup2((*self)->slavefd, STDIN_FILENO);
		dup2((*self)->slavefd, STDOUT_FILENO);
		dup2((*self)->slavefd, STDERR_FILENO);

		close((*self)->slavefd);

		/* set child process as session leader */
		setsid();
		if(ioctl(0, TIOCSCTTY, 1) == -1) perror("ioctl");

		execl("/bin/login", "login", (char*)NULL);
	}
	/* parent process */
	else {
		close((*self)->slavefd);
	}
}
