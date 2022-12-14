#include "client.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HEADERSZ 8
#define DISCONNECT_FROM_SERVER_MSG "00000026Disconnecting from server"

/*
 * If this is the first data being read for this msg
 * then the first HEADERSZ bytes will be the message length.
 * We will extract the message length and return it
 * as an int.
 *
 * returns - if successful, an int > -1, 0 otherwise
 */
static int rd_msg_len(const char *buf)
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
static void cat_client_buf(const char *buf, const int index, 
						   const int bytesrd)
{
	int total_bytesrd;

	total_bytesrd = bytesrd + clients[index].bytesrd;
	strncat(clients[index].buf + clients[index].bytesrd, buf,
			total_bytesrd > BUFSZ ? BUFSZ - clients[index].bytesrd : bytesrd);
	clients[index].bytesrd += bytesrd;
}

static void remove_client(const int index)
{
	if (index < 0)
		return;

	p_clients[index].fd = -1;
	num_clients--;
}

/*
 * Reads the msg header and gets the msg size.
 * Prepends a copy of the msg length to the
 * client msg buffer.
 */
static void rd_header(char *buf, const int clientfd, const int index)
{
	int msgsz;

	read(clientfd, buf, HEADERSZ);
	buf[HEADERSZ] = '\0';
	msgsz = rd_msg_len(buf);
	memcpy(clients[index].buf, buf, HEADERSZ);
	clients[index].msgsz = msgsz > BUFSZ ? BUFSZ : msgsz;
	clients[index].msg_in_proc = MSG_IN_PROC;
}

void clients_init()
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		p_clients[i].fd = -1;
		clients[i].pfd = &p_clients[i];
		clients[i].msg_in_proc = MSG_NOT_IN_PROC;
	}
}

void reset_client(const int index)
{
	memset(clients[index].buf, 0, clients[index].bytesrd);
	clients[index].msgsz = 0;
	clients[index].bytesrd = 0;
	clients[index].msg_in_proc = MSG_NOT_IN_PROC;
}

void write_to_client(const int clientfd, const struct client *sender)
{
	if ((write(clientfd, sender->buf, HEADERSZ + sender->msgsz)) < 1)
		printf("Couldn't write in write_to_client()\n");
}

void write_to_clients(const int sender_index)
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if ((p_clients[i].fd > 2))
			write_to_client(p_clients[i].fd, &clients[sender_index]);
	}

	reset_client(sender_index);
}

/*
 * function reads from client socket
 * and places the data in buf.
 *
 * returns - 1, on success and the entire msg has been received
 *           0 on success but only a partial msg has been received,
 *           -1 otherwise, disconnect or socket error
 */
int rd_from_client(const int clientfd, const int index)
{
	char buf[BUFSZ] = {0};
	int bytesrd, bytesleft, ret;

	if (MSG_NOT_IN_PROC == clients[index].msg_in_proc)
		rd_header(buf, clientfd, index);

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
		} else if (bytesrd == -1) {
			if ((EWOULDBLOCK == errno) || (EAGAIN == errno)) {
				ret = 0; // go back to polling
				break;
			} else {
				ret = -1;
				remove_client(index);
				server_send_msg(index, DISCONNECT_FROM_SERVER_MSG);
				break;
			}
		}
	}

	if (ret == -1)
		reset_client(index);

	return ret;
}

void rd_write_clients(int num_fds)
{
	for (int i = 0; i < MAX_CLIENTS && num_fds > 0; ++i) {
		if (p_clients[i].revents & POLLIN) {
			if (rd_from_client(p_clients[i].fd, i) == 1)
				write_to_clients(i);
			num_fds--;
		}
	}
}

// returns - 0 on success, -1 otherwise
int add_client(const int clientfd)
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

void server_send_msg(const int clientfd, const char *msg)
{
	if ((write(clientfd, msg, strlen(msg) + 1)) < 1)
		printf("server_send_msg() write failed\n");
}

void server_send_msg_all(const char *msg)
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (p_clients[i].fd > 2)
			server_send_msg(p_clients[i].fd, msg);
	}
}

void close_all_fds(const int sockfd)
{
	close(sockfd);

	for (int i = 0; i < MAX_CLIENTS && num_clients > 0; ++i) {
		if (p_clients[i].fd > 2) {
			close(p_clients[i].fd);
			num_clients--;
		}
	}
}
