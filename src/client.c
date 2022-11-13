#include "client.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HEADERSZ 8
#define DISCONNECT_FROM_SERVER_MSG "Disconnecting from server"

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
	char *endptr;
	int ret;

	ret = strtol(buf, &endptr, 10);
	if (ret >= 0)
		return ret;

	return 0;
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
static void rd_header(const int clientfd, const int index)
{
	int msgsz;

	read(clientfd, clients[index].buf, HEADERSZ);
	msgsz = rd_msg_len(clients[index].buf);
	clients[index].msgsz = msgsz > BUFSZ ? BUFSZ : msgsz;
	clients[index].msg_in_proc = MSG_IN_PROC;
}

// caller must free this memory
static char *prepend_header(const char *msg)
{
	char lenstr[HEADERSZ + 1];
	char *formatted_str;
	int len;

	len = strlen(msg) + 1;
	snprintf(lenstr, HEADERSZ + 1, "%08i", len);
	formatted_str = (char *)malloc(sizeof(char) * (HEADERSZ + len));
	snprintf(formatted_str, HEADERSZ + len + 1, "%s%s", lenstr, msg);
	return formatted_str;
}

static void reset_client_fd(const int index)
{
	p_clients[index].fd = -1;
}

void clients_init()
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		p_clients[i].fd = -1;
		clients[i].msg_in_proc = MSG_NOT_IN_PROC;
	}
}

void reset_client_msg(const int index)
{
	memset(clients[index].buf, '\0', HEADERSZ + clients[index].bytesrd);
	clients[index].msgsz = 0;
	clients[index].bytesrd = 0;
	clients[index].msg_in_proc = MSG_NOT_IN_PROC;
}

void write_to_client(const int receiver_index, const int sender_index)
{
	if ((write(p_clients[receiver_index].fd, clients[sender_index].buf,
			   HEADERSZ + clients[sender_index].msgsz)) < 1) {
		if (EPIPE == errno) {
			reset_client_msg(receiver_index);
			reset_client_fd(receiver_index);
		}
	}
}

void write_to_clients(const int sender_index)
{
	int num_sent = 0;

	for (int i = 0; (i < MAX_CLIENTS) && (num_sent < num_clients); ++i) {
		if ((p_clients[i].fd > 2)) {
			write_to_client(i, sender_index);
			++num_sent;
		}
	}

	reset_client_msg(sender_index);
}

/*
 * function reads from client socket and puts the data
 * in the clients msg buffer
 *
 * returns - 1, on success and the entire msg has been received
 *           0 on success but only a partial msg has been received,
 *           -1 otherwise, disconnect or socket error
 * 
 */
int rd_from_client(const int index)
{
	int bytesrd, bytesleft, ret;

	if (MSG_NOT_IN_PROC == clients[index].msg_in_proc)
		rd_header(p_clients[index].fd, index);

	while (1) {
		bytesleft = clients[index].msgsz - clients[index].bytesrd;
		bytesrd = read(p_clients[index].fd, clients[index].buf + HEADERSZ + 
					   clients[index].bytesrd, bytesleft);
		clients[index].bytesrd += bytesrd;
		if (bytesrd > 0) {
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

	if (ret == -1) {
		reset_client_msg(index);
		reset_client_fd(index);
	}

	return ret;
}

void rd_write_clients(int num_fds)
{
	for (int i = 0; i < MAX_CLIENTS && num_fds > 0; ++i) {
		if (p_clients[i].revents & POLLIN) {
			if (rd_from_client(i) == 1)
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
	int count = 0;
	char *formatted_msg;

	formatted_msg = prepend_header(msg);
	for (int i = 0; (i < MAX_CLIENTS) && (count < num_clients); ++i) {
		if (p_clients[i].fd > 2) {
			server_send_msg(p_clients[i].fd, formatted_msg);
			++count;
		}
	}

	free(formatted_msg);
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