#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// connection port
#define PORT 8085

// max buffer size messages
#define BUF_SIZE 1024

// max number of clients that can be connected at once
#define MAX_CONNS 4

// current number of clients connected
atomic_int curr_conns = 0;

// array of connected client file descriptors
static struct pollfd clients[MAX_CONNS] = {0};

// static function declarations
static int new_socket();
static void setsocketopts(const int srvrfd);
static inline void setsockaddr_in(struct sockaddr_in *addr);
static void bind_socket(const int srvrfd, struct sockaddr_in *addr);
static void listen_socket(const int srvrfd);
static int setup_socket(struct sockaddr_in *addr);
static int add_client(const int clientfd);
static void accept_client_conns(const int srvrfd, struct sockaddr_in *addr);
static void rd_from_client(const int clientfd, const int index, char *buf);
static void rd_write_clients();
void write_to_client(const int clientfd, const char *msg);
static void write_to_clients(const char *buf, const int sender);
static void close_all_fds();


int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int sockfd;
	pthread_t iothrd;

	sockfd = setup_socket(&addr);

	if (pthread_create(&iothrd, NULL, rd_write_clients, NULL) != 0) {
		printf("main() failed to create thread");
		exit(-1);
	}

	accept_client_conns(sockfd, &addr);

	// clean up
	pthread_exit(iothrd);
	close_all_fds();

	return 0;
}

static int new_socket()
{
	int srvrfd;

	if ((srvrfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

static inline void setsockaddr_in(struct sockaddr_in *addr)
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
		if (clients[i].fd == 0) {
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

	clients[index].fd = 0;
	curr_conns--;
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

		if ((clientfd = accept(srvrfd, (struct sockaddr*)addr,
				       &addrsz)) < 0) {
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

static void rd_from_client(const int clientfd, const int index, char *buf)
{
	if (read(clientfd, buf, BUF_SIZE) < 1) {
		remove_client(index); // client must have disconnected
		printf("rd_from_client() read error\n");
	}

	buf[strlen(buf) - 1] = '\0'; // remove \n from the end
}

static void rd_write_clients()
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

		for (int i = 0; i < MAX_CONNS && num > 0; ++i) {
			if (clients[i].revents & POLLIN) {
				rd_from_client(clients[i].fd, i, buf);
				write_to_clients(buf, i);
				num--;
			}
		}
	}
}

void write_to_client(const int clientfd, const char *msg)
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

static void close_all_fds()
{
	for (int i = 0; i < MAX_CONNS && curr_conns > 0; ++i) {
		if (clients[i].fd > 2) {
			close(clients[i].fd);
			curr_conns--;
		}
	}
}


