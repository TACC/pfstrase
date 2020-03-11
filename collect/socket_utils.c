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

//#include "collect.h"
#include "lfs_utils.h"
#include "socket_utils.h"
#include "dict.h"

static int port;
static char *hostname;

int sockfd;
struct dict *jid_map;
struct dict *uid_map;

/* Process RPC */
static int process_rpc(char *rpc)
{
  int rc = -1;

  fprintf(stderr, "RPC %s\n", rpc);
  if (rpc[0] != '{') {
    fprintf(stderr, "RPC `%s': json must start with `{'\n", rpc);
    goto out;
  }

  json_object *rpc_json = NULL;
  enum json_tokener_error error = json_tokener_success;
  rpc_json = json_tokener_parse_verbose(rpc, &error);  

  if (error != json_tokener_success) {
    fprintf(stderr, "RPC `%s': %s\n", rpc, json_tokener_error_desc(error));
    goto out;
  }
  char host_name[32];
  char jid[32];
  char uid[32];
  json_object_object_foreach(rpc_json, key, val) {
    if (strcmp(key, "hostname") == 0)
      snprintf(hostname, sizeof(hostname), json_object_get_string(val));
    if (strcmp(key, "jid") == 0)
      snprintf(jid, sizeof(jid), json_object_get_string(val));
    if (strcmp(key, "user") == 0)
      snprintf(uid, sizeof(uid), json_object_get_string(val));
  }
  printf("%s %s %s\n", hostname, jid, uid);
  rc = 1;
 out:
  if (rpc_json)
    json_object_put(rpc_json);

  return rc;
}

/* Socket is used by job_map_server to recieve job data */
int sock_setup_connection(const char *port) 
{
  int opt = 1;
  int backlog = 10;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int fd = -1;

  //if (dict_init(jid_map, 5) < 0)
  //printf("failed to initialize jobid-to-node map;");
  //if (dict_init(uid_map, 5) < 0)
  //printf("failed to initialize user-to-node map;");

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(atoi(port));
  //syslog(LOG_INFO, "Initializing listen on local port: %s", port);
  printf("Initializing listen on local port: %s\n", port);

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
  //syslog(LOG_INFO, "Established listen on local port: %s", port);
  printf("Established listen on local port: %s\n", port);
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

  char *p = request + strlen(request) - 1;
  *p = '\0';

  if (process_rpc(request) < 0)
    fprintf(stderr, "rpc processing failed: %s\n", strerror(errno));
}


void sock_send_data()
{
  json_object *message_json = json_object_new_object();
  //collect_devices(message_json);
  fprintf(stderr, "The json object created: %s\n",json_object_to_json_string(message_json));
  int rv = send(sockfd, json_object_to_json_string(message_json), strlen(json_object_to_json_string(message_json)), 0);
  json_object_put(message_json);
}

