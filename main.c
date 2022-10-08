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

// connection port
#define PORT 8085

// max buffer size messages
#define BUF_SIZE 1024

// each msg from the client is sent with a "1" at the start
#define MSG_START_OFFSET 1

// max number of clients that can be connected at once
#define MAX_CONNS 4

// current number of clients connected
atomic_int curr_conns = 0;

// array of connected client file descriptors
struct pollfd clients[MAX_CONNS];


static void clients_init()
{
	for (int i = 0; i < MAX_CONNS; ++i) {
		clients[i].fd = -1;
		clients[i].events = 0;
		clients[i].revents = 0;
	}
}


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
	if (listen(srvrfd, MAX_CONNS) < 0) {
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
	if (curr_conns >= MAX_CONNS) // server full
		return -1;

	for (int i = 0; i < MAX_CONNS; ++i) {
		if (clients[i].fd == -1) {
			clients[i].fd = clientfd;
			clients[i].events = POLLIN | POLLPRI;
			curr_conns++;
			break;
		}
	}

	return 0;
}

static void remove_client(const int index)
{
	if (index < 0)
		return;

	clients[index].fd = -1;
	curr_conns--;
}

static void write_to_client(const int clientfd, const char *msg)
{
	if ((write(clientfd, msg, strlen(msg))) < 1)
		printf("Couldn't write in write_to_client()\n");
}

// sender = -1 to send to all
static void write_to_clients(const char *buf, const int sender)
{
	for (int i = 0; i < MAX_CONNS; ++i) {
		if ((clients[i].fd > 2) && (i != sender))
			write_to_client(clients[i].fd, buf);
	}
}

static void accept_client_conns(const int srvrfd, struct sockaddr_in *addr)
{
	struct pollfd pfd;
	int addrsz;
	int clientfd;

	addrsz = sizeof(*addr);

	pfd.fd = srvrfd;
	pfd.events = POLLIN | POLLPRI;

	while (1) {
		if (poll(&pfd, 1, -1) < 0) {
			printf("accept_client_conns() Poll failed\n");
			exit(-1);
		}

		if ((clientfd = accept4(srvrfd, (struct sockaddr*)addr,
				       &addrsz, SOCK_NONBLOCK)) < 0) {
			printf("accept_client_conns() accept failed");
			exit(-1);
		}

		if (add_client(clientfd) < 0) { // if failed to add client
			write_to_client(clientfd, "Server is full");
			close(clientfd);
		} else {
			write_to_clients("Client Connected", -1); // send to all
		}
	}
}

/*
 * function reads from client socket
 * and places the data in buf.
 *
 * returns - 0 on success, -1 otherwise
 */
static int rd_from_client(const int clientfd, const int index, char *buf)
{
	if (read(clientfd, buf, BUF_SIZE) < 1) {
		remove_client(index); // client must have disconnected
		printf("rd_from_client() read error\n");
		return -1;
	}

	if (buf[0] != '1')
		return -1;

	if (buf[strlen(buf) - 1] == '\n')
		buf[strlen(buf) - 1] = '\0';

	return 0;
}

static void rd_write_clients(char *buf, int num_fds)
{
	for (int i = 0; i < MAX_CONNS && num_fds > 0; ++i) {
		if (clients[i].revents & POLLIN) {
			if (rd_from_client(clients[i].fd, i, buf) == 0)
				write_to_clients(buf + MSG_START_OFFSET, i);
			num_fds--;
		}
	}
}

static void process_messages()
{
	char buf[BUF_SIZE];
	int num;

	while (1) {
		num = poll(clients, MAX_CONNS, 500); // 0.5s timeout

		if (num < 0) { // poll error
			printf("rd_from_clients() poll error\n");
			exit(-1);
		} else if (num == 0) { // no data sent yet, poll again
			continue;
		}

		rd_write_clients(buf, num);
	}
}

static void close_all_fds()
{
	for (int i = 0; i < MAX_CONNS && curr_conns > 0; ++i) {
		if (clients[i].fd > 2) {
			close(clients[i].fd);
			curr_conns--;
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

	if (pthread_create(&iothrd, NULL, process_messages, NULL) != 0) {
		printf("main() failed to create thread");
		exit(-1);
	}

	accept_client_conns(sockfd, &addr);

	// clean up
	pthread_exit(&iothrd);
	close_all_fds();

	return 0;
}
