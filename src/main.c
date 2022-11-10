#define GNU_SOURCE

#include "client.h"

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// connection port
#define PORT 8085

#define SERVER_FULL "Server is full"
#define CLIENT_CONNECTED_MSG "Client Connected"

#define SEND_TO_ALL -1

// current number of clients connected
atomic_int num_clients = 0;

// array of each client's pollfd struct
struct pollfd p_clients[MAX_CLIENTS];
struct client clients[MAX_CLIENTS];

static int setup_socket(struct sockaddr_in *addr)
{
	int sockfd, opt = 1;

	if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
		printf("socket failed");
		exit(-1);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		printf("setsockopt");
		exit(-1);
	}

	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;
	addr->sin_port = htons(PORT);

	if (bind(sockfd, (struct sockaddr *)addr, sizeof(*addr)) < 0) {
		printf("bind failed");
		exit(errno);
	}

	if (listen(sockfd, MAX_CLIENTS) < 0) {
		printf("listen failed");
		exit(-1);
	}

	printf("server successfully started\n");
	return sockfd;
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
			server_send_msg_all(CLIENT_CONNECTED_MSG);
		}
	}
}

static void *process_messages(void *arg)
{
	int num_fds;

	while (1) {
		num_fds = poll(p_clients, MAX_CLIENTS, 500); // 0.5s timeout

		if (num_fds > 0) {
			rd_write_clients(num_fds);
		} else if (num_fds < 0) { // poll error
			printf("rd_from_clients() poll error\n");
			exit(-1);
		} else if (num_fds == 0) { // no data sent yet, poll again
			continue;
		}
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int sockfd;
	pthread_t iothrd;

	sockfd = setup_socket(&addr);
	sigaction(SIGPIPE, &(struct sigaction){{SIG_IGN}}, NULL);
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
