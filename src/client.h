#ifndef _CLIENT_H
#define _CLIENT_H

#include <poll.h>
#include <stdatomic.h>

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

extern atomic_int num_clients;

void clients_init();
void reset_client(const int index);
void write_to_client(const int receiver_index, const int sender_index);
void write_to_clients(const int sender_index);
void server_send_msg(const int clientfd, const char *msg);
void server_send_msg_all(const char *msg);
int rd_from_client(const int clientfd, const int index);
void rd_write_clients(int num_fds);
int add_client(const int fd);
void close_all_fds(const int sockfd);

#endif
