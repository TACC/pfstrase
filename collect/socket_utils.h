#ifndef _SOCKET_LISTENER_H_
#define _SOCKET_LISTENER_H_

#define SOCKET_BUFFERSIZE 655336

int amqp_setup_connection(const char *port, const char *hostname);
void amqp_kill_connection();
void amqp_rpc();
void amqp_send_data();

int sock_setup_connection(const char *port);
void sock_rpc();
void sock_send_data();
 
#endif
