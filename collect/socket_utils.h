#ifndef _SOCKET_LISTENER_H_
#define _SOCKET_LISTENER_H_

#define SOCKET_BUFFERSIZE 655336

int amqp_setup_connection(amqp_connection_state_t *conn, const char *port, const char *hostname);
void amqp_rpc(amqp_connection_state_t conn);
void amqp_send_data(amqp_connection_state_t conn);

int sock_setup_connection(const char *port);
void sock_rpc(int fd);
void sock_send_data(int fd);
 
#endif
