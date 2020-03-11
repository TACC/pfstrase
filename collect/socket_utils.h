#ifndef _SOCKET_LISTENER_H_
#define _SOCKET_LISTENER_H_

#define SOCKET_BUFFERSIZE 655336

int sock_setup_connection(const char *port);
void sock_rpc();
void sock_send_data();
 
#endif
