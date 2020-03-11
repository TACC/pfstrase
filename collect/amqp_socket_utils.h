#ifndef _SOCKET_LISTENER_H_
#define _SOCKET_LISTENER_H_

#define SOCKET_BUFFERSIZE 655336

int amqp_setup_connection(int port, char *hostname);
void amqp_kill_connection();
void amqp_rpc();
void amqp_send_data();
 
#endif
