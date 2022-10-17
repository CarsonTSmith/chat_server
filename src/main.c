#define GNU_SOURCE

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"

// connection port
#define PORT 8085

// each new msg sends the sz of the msg
// in the first 8 bytes
#define HEADERSZ 8 

#define SERVER_FULL "Server is full"

#define SEND_TO_ALL -1

// current number of clients connected
atomic_int num_clients = 0;

struct pollfd p_clients[MAX_CLIENTS];
struct client clients[MAX_CLIENTS];


static int new_socket()
{
	int srvrfd;

	if ((srvrfd = socket(AF_INET,
			     SOCK_STREAM | SOCK_NONBLOCK,
			     0)) < 0) {
		printf("socket failed");
		exit(-1);
	}

	return srvrfd;
}

static void setsocketopts(const int srvrfd)
{
	int opt = 1;

	if (setsockopt(srvrfd, SOL_SOCKET, SO_REUSEADDR,
		       &opt, sizeof(opt)) < 0) {
		printf("setsockopt");
		exit(-1);
	}
}

static void setsockaddr_in(struct sockaddr_in *addr)
{
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;
	addr->sin_port = htons(PORT);
}

static void bind_socket(const int srvrfd, struct sockaddr_in *addr)
{
	if (bind(srvrfd, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
		printf("bind failed");
		exit(errno);
	}
}

static void listen_socket(const int srvrfd)
{
	if (listen(srvrfd, MAX_CLIENTS) < 0) {
		printf("listen failed");
		exit(-1);
	}
}

static int setup_socket(struct sockaddr_in *addr)
{
	int sockfd;

	sockfd = new_socket();
	setsocketopts(sockfd);
	setsockaddr_in(addr);
	bind_socket(sockfd, addr);
	listen_socket(sockfd);

	printf("Server Successfully Created\n");

	return sockfd;
}

// returns - 0 on success, -1 otherwise
static int add_client(const int clientfd)
{
	if (num_clients >= MAX_CLIENTS) // server full
		return -1;

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (p_clients[i].fd == -1) {
			p_clients[i].fd = clientfd;
			p_clients[i].events = POLLIN | POLLPRI;
			num_clients++;
			break;
		}
	}

	return 0;
}

static void remove_client(const int index)
{
	if (index < 0)
		return;

	p_clients[index].fd = -1;
	num_clients--;
}

static void server_send_msg(const int clientfd, const char *msg)
{
	if((write(clientfd, msg, strlen(msg))) < 1)
		printf("server_send_msg() write failed\n");	
}

static void server_send_msg_all(const char *msg)
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (p_clients[i].fd > 2)
			server_send_msg(p_clients[i].fd, msg);
	}
}

/*
 * resets the client struct to 0
 */
static void reset_client(const int index)
{
	memset(clients[index].buf, 0, clients[index].bytesrd);
	clients[index].msgsz = 0;
	clients[index].bytesrd = 0;
	clients[index].msg_in_proc = MSG_NOT_IN_PROC;
}

static void write_to_client(const int clientfd, const struct client *sender)
{
	if ((write(clientfd, sender->buf, sender->msgsz)) < 1)
		printf("Couldn't write in write_to_client()\n");
}

static void write_to_clients(const int sender)
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if ((p_clients[i].fd > 2) && (i != sender))
			write_to_client(p_clients[i].fd, &clients[sender]);
	}

	reset_client(sender);
}

static void accept_client_conns(const int srvrfd, struct sockaddr_in *addr)
{
	struct pollfd pfd;
	socklen_t addrsz;
	int clientfd;

	addrsz = sizeof(*addr);

	pfd.fd = srvrfd;
	pfd.events = POLLIN | POLLPRI;

	while (1) {
		if (poll(&pfd, 1, -1) < 0) {
			printf("accept_client_conns() Poll failed\n");
			exit(-1);
		}

		if ((clientfd = accept(srvrfd, (struct sockaddr *)addr,
				       &addrsz)) < 0) {
			printf("accept_client_conns() accept failed");
			exit(-1);
		}

		if (add_client(clientfd) < 0) { // if failed to add client
			server_send_msg(clientfd, SERVER_FULL);
			close(clientfd);
		} else {
			server_send_msg_all("Client Connected");
		}
	}
}

/*
 * If this is the first data being read for this msg
 * then the first HEADERSZ bytes will be the message length.
 * We will extract the message length and return it
 * as an int.
 *
 * returns - if successful, an int > -1, 0 otherwise
 */
static int rd_header(const char *buf)
{
	char msgsz[HEADERSZ + 1];
	char *endptr;
	int ret;

	memcpy(msgsz, buf, HEADERSZ);
	msgsz[HEADERSZ] = '\0';
	ret = strtol(msgsz, &endptr, 10);
	if (ret >= 0)
		return ret;

	return 0;
}

/*
 * Concatenates buf to the client's (given by index) buf 
 */
static void cat_client_buf(const char *buf, const int index, const int bytesrd)
{
	int total_bytesrd;
	
	total_bytesrd = bytesrd + clients[index].bytesrd;
	strncat(clients[index].buf + clients[index].bytesrd,
		buf, total_bytesrd > BUFSZ ?
		BUFSZ - clients[index].bytesrd : bytesrd);
	clients[index].bytesrd += bytesrd;
}

/*
 * function reads from client socket
 * and places the data in buf.
 *
 * returns - 1, on success and the entire msg has been received
 *           0 on success but only a partial msg has been received,
 *           -1 otherwise, disconnect or socket error
 */
static int rd_from_client(const int clientfd, const int index)
{
	char buf[BUFSZ] = {0};
	int bytesrd, msgsz, ret, bytesleft;

	// this read needs to be put in a loop unfortunately...	
	if (MSG_NOT_IN_PROC == clients[index].msg_in_proc) {
		read(clientfd, buf, HEADERSZ);
		buf[HEADERSZ] = '\0';	
		msgsz = rd_header(buf);
		clients[index].msgsz = msgsz > BUFSZ ? BUFSZ : msgsz; 
		clients[index].msg_in_proc = MSG_IN_PROC;
	}

	while (1) {
		bytesleft = clients[index].msgsz - clients[index].bytesrd;
		bytesrd = read(clientfd, buf, bytesleft);
		if (bytesrd > 0) {
			cat_client_buf(buf, index, bytesrd);	
			if (clients[index].bytesrd < clients[index].msgsz) {
				continue;
			} else {
				ret = 1;
				break; // got all the bytes
			}	
		} else if (bytesrd == 0) {
			ret = 1;
			break;	
			//remove_client(index); // client disconnected
			//ret = -1;
			//break;
		} else if (bytesrd == -1) {
			if ((EWOULDBLOCK == errno) || (EAGAIN == errno)) {
				ret = 0; // go back to polling	
				break;
			} else {
				ret = -1;
				remove_client(index);
				server_send_msg(index, 
                                                "Disconnecting from Server");
				break;
			}
		}
	}
	
	if (ret == -1)
		reset_client(index);
	
	return ret;
}

static void rd_write_clients(int num_fds)
{
	for (int i = 0; i < MAX_CLIENTS && num_fds > 0; ++i) {
		if (p_clients[i].revents & POLLIN) {
			if (rd_from_client(p_clients[i].fd, i) == 1)
				write_to_clients(i);
			num_fds--;
		}
	}
}

static void *process_messages(void *arg)
{
	int num;

	while (1) {
		num = poll(p_clients, MAX_CLIENTS, 500); // 0.5s timeout

		if (num < 0) { // poll error
			printf("rd_from_clients() poll error\n");
			exit(-1);
		} else if (num == 0) { // no data sent yet, poll again
			continue;
		}

		rd_write_clients(num);
	}

	return NULL;
}

static void close_all_fds(const int sockfd)
{
	close(sockfd);

	for (int i = 0; i < MAX_CLIENTS && num_clients > 0; ++i) {
		if (p_clients[i].fd > 2) {
			close(p_clients[i].fd);
			num_clients--;
		}
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int sockfd;
	pthread_t iothrd;

	sockfd = setup_socket(&addr);
	clients_init();

	if (pthread_create(&iothrd, NULL, &process_messages, NULL) != 0) {
		printf("main() failed to create thread");
		exit(-1);
	}

	accept_client_conns(sockfd, &addr);

	// clean up
	pthread_exit(&iothrd);
	close_all_fds(sockfd);

	return 0;
}
