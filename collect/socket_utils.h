#ifndef _SOCKET_LISTENER_H_
#define _SOCKET_LISTENER_H_
#include <json/json.h>
#define SOCKET_BUFFERSIZE 655336

int socket_listen(const char *port);
void sock_rpc();
void sock_send_data(const char *addr, const char *port);

json_object *host_json;
 
#endif
