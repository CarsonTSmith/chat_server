#include "client.h"

void clients_init()
{
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		p_clients[i].fd = -1; 
		clients[i].pfd = &p_clients[i];
		clients[i].msg_in_proc = MSG_NOT_IN_PROC;
	} 
}
