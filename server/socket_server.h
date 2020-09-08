#ifndef _SOCKET_UTILS_H_
#define _SOCKET_UTILS_H_
#define SOCKET_BUFFERSIZE 1048576*10

int socket_destroy();
int socket_listen(const char *port);
void sock_rpc();
void sock_send_data(const char *addr, const char *port);

#endif
