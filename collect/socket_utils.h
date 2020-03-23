#ifndef _SOCKET_LISTENER_H_
#define _SOCKET_LISTENER_H_
#include <json-c12/json.h>
#define SOCKET_BUFFERSIZE 655336

int socket_destroy();
int socket_listen(const char *port);
void sock_rpc();
void sock_send_data(const char *addr, const char *port);

json_object *host_map;
json_object *nid_map; 
json_object *server_client_map; 
json_object *server_client_sum; 
json_object *server_jid_map; 
json_object *server_uid_map; 

#endif
