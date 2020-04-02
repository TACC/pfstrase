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
#include "socket_utils.h"
#include "stats.h"

static int port;

int sockfd;

/* Process RPC */
static int process_rpc(char *rpc)
{
  int rc = -1;

  if (rpc[0] != '{') {
    fprintf(stderr, "RPC `%s': json must start with `{'\n", rpc);
    goto out;
  }

  if (update_host_map(rpc) < 0)
    goto out;

  rc = 1;
 out:
  return rc;
}

/* Socket is used by map_server to receive job data */
int socket_listen(const char *port) 
{
  int opt = 1;
  int backlog = 10;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int fd = -1;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(atoi(port));
  syslog(LOG_INFO, "Initializing map_server listen on local port: %s", 
	 port);

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {    
    syslog(LOG_INFO, "cannot initialize socket: %s\n", strerror(errno));
    goto err;
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) == -1) {
    fprintf(stderr, "cannot set SO_REUSEADDR: %s\n", strerror(errno));
    goto err; 
  }
  if (bind(sockfd, (struct sockaddr *)&addr, addrlen) == -1) {
    syslog(LOG_INFO, "cannot bind: %s\n", strerror(errno));
    close(sockfd);
    goto err;
  }
  if (listen(sockfd, backlog) == -1) {
    syslog(LOG_INFO, "cannot listen: %s\n", strerror(errno));
    close(sockfd);
    goto err;
  }
  syslog(LOG_INFO, "Established map_server listen on local port: %s", port);

 err:
  return sockfd;
}

void sock_rpc() 
{  
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int fd = -1;

  if ((fd = accept(sockfd, (struct sockaddr *)&addr, &addrlen)) < 0)
    if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED))
      fprintf(stderr, "cannot accept connections: %s\n", strerror(errno));

  char request[SOCKET_BUFFERSIZE];
  memset(request, 0, sizeof(request));
  ssize_t bytes_recvd = recv(fd, request, sizeof(request), 0);
  close(fd);

  if (bytes_recvd < 0) {
    fprintf(stderr, "cannot recv: %s\n", strerror(errno));
    return;
  }

  if (process_rpc(request) < 0)
    fprintf(stderr, "rpc processing failed: %s\n", strerror(errno));
}


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
  int rv = send(server_socket, json_object_to_json_string(message_json), 
		strlen(json_object_to_json_string(message_json)), 0);
  json_object_put(message_json);
  close(server_socket);
  freeaddrinfo(result);
 err:
  return;
}

int socket_destroy() {
  close(sockfd);
  return 0;
}
