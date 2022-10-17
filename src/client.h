#ifndef _CLIENT_H
#define _CLIENT_H

#include <poll.h>

#define BUFSZ 8192
#define MAX_CLIENTS 1024

#define MSG_IN_PROC 'y'
#define MSG_NOT_IN_PROC 'n'

struct client {
	struct pollfd *pfd; // reference to p_clients at same index
	char buf[BUFSZ];
	char msg_in_proc; // header has been read, msg is proc of being rd
	int bytesrd;
	int msgsz;
};

extern struct pollfd p_clients[MAX_CLIENTS];
extern struct client clients[MAX_CLIENTS];

void clients_init(); 

#endif
