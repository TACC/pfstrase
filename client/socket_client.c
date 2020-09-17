#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <json/json.h>

#define SOCKET_BUFFERSIZE 1048576*10
char request[SOCKET_BUFFERSIZE];

void sock_send_data(const char *dn, const char *port)
{
  
  int server_socket;

  struct addrinfo *result;
  struct addrinfo hints; 
  struct sockaddr_in *saddr_in;    

  memset(&hints, 0, sizeof(struct addrinfo));  
  hints.ai_family = AF_INET;

  if(getaddrinfo(dn, NULL, &hints, &result) != 0) {
    syslog(LOG_INFO, "getaddrinfo:: %s\n", strerror(errno));
    goto err;
  }

  saddr_in = (struct sockaddr_in *) result->ai_addr;
  saddr_in->sin_port = htons(atoi(port));

  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    syslog(LOG_INFO, "cannot intialize socket for send: %s\n", 
	   strerror(errno));
    goto err;
  }

  if (connect(server_socket, (struct sockaddr *)saddr_in, sizeof(*saddr_in)) == -1) {
    syslog(LOG_INFO, "cannot connect to address %s port %s for send: %s\n", dn, port, 
	   strerror(errno));
  }
  
  json_object *message_json = json_object_new_object();
  collect_devices(message_json);

  memset(request, 0, sizeof(request));
  snprintf(request, sizeof(request), json_object_to_json_string(message_json));

  ssize_t bytes_sent;
  ssize_t p = 0;
  while ((bytes_sent = send(server_socket, request + p, sizeof(request) - p*sizeof(char), 0)) > 0)
    p += bytes_sent;

  if (bytes_sent < 0)
    fprintf(stderr, "cannot send: %s\n", strerror(errno));

  json_object_put(message_json);
  close(server_socket);
  freeaddrinfo(result);
 err:
  return;
}
