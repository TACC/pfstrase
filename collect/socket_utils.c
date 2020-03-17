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
#include "lfs_utils.h"
#include "collect.h"
#include "socket_utils.h"

static int port;

int sockfd;

#define OOM() fprintf(stderr, "cannot allocate memory\n");

/* Get stat of stats_type for host */
static int stats_get(json_object *h_json, const char *stats_type, json_object **stats_json)
{
  int rc = 0;
  int arraylen;
  int j;
  json_object *data;

  if (json_object_object_get_ex(h_json, "data", &data)) {
    arraylen = json_object_array_length(data);
    for (j = 0; j < arraylen; j++) {
      json_object *data_json = json_object_array_get_idx(data, j);
      json_object *stats_type_json;
      if (json_object_object_get_ex(data_json, "stats_type", &stats_type_json) && \
	  (strcmp(json_object_get_string(stats_type_json), stats_type) == 0))
	if (json_object_object_get_ex(data_json, "stats", stats_json))
	  rc = 1;             
    }
  }  

  return rc;
}

/* Process RPC */
static int process_rpc(char *rpc)
{
  int rc = -1;
  json_object *rpc_json = NULL;
  char hostname[32];

  //fprintf(stderr, "RPC %s\n", rpc);
  if (rpc[0] != '{') {
    fprintf(stderr, "RPC `%s': json must start with `{'\n", rpc);
    goto out;
  }

  enum json_tokener_error error = json_tokener_success;
  rpc_json = json_tokener_parse_verbose(rpc, &error);  
  if (error != json_tokener_success) {
    fprintf(stderr, "RPC `%s': %s\n", rpc, json_tokener_error_desc(error));
    goto out;
  }

  json_object *tmp;
  if (json_object_object_get_ex(rpc_json, "hostname", &tmp)) {
    snprintf(hostname, sizeof(hostname), "%s", json_object_get_string(tmp)); 
    json_object_object_del(rpc_json, "hostname");   
    json_object_object_add(host_json, hostname, rpc_json);
  }
  else 
    goto out;

  printf("%10s %10s %10s %20s\n", "CLIENT", "JOBID", "USER", "SYSINFO");
  /* Iterate through hosts in the json */
  json_object_object_foreach(host_json, key, val) {
    json_object *jid;
    json_object *uid;
    
    if (!json_object_object_get_ex(val, "jid", &jid)) {
      json_object_object_add(val, "jid", json_object_new_string("-"));
      json_object_object_get_ex(val, "jid", &jid);
    }
    if (!json_object_object_get_ex(val, "uid", &uid)) {
      json_object_object_add(val, "uid", json_object_new_string("-"));
      json_object_object_get_ex(val, "uid", &uid);
    }

    /* Get sysinfo for host */
    json_object *sysinfo_json; 
    if (stats_get(val, "sysinfo", &sysinfo_json))
	printf("%10s %10s %10s %20s\n", key, json_object_get_string(jid), 
	       json_object_get_string(uid), json_object_get_string(sysinfo_json));
    else
      printf("%10s %10s %10s\n", key, json_object_get_string(jid), json_object_get_string(uid));
  }

  rc = 1;

 out:
  //if (rpc_json)
  //json_object_put(rpc_json);
  //printf("after %s\n", json_object_to_json_string_ext(host_json, JSON_C_TO_STRING_PRETTY));
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

  host_json = json_object_new_object();

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
  //fprintf(stderr, "The json object created: %s\n",
  //	  json_object_to_json_string(message_json));
  int rv = send(server_socket, json_object_to_json_string(message_json), 
		strlen(json_object_to_json_string(message_json)), 0);
  json_object_put(message_json);

  freeaddrinfo(result);
 err:
  return;
}

