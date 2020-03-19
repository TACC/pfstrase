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
#define printf_json(json) printf(">>> %s\n", json_object_get_string(json));

/* Map client nids of servers to jids and uids */
static int map_stats() {
  int rc = -1;
  int arraylen;
  int j;
  json_object *obd_tag;
      
  printf("Updating export map\n");
  json_object_object_foreach(host_map, key, host_entry) {    
    if (json_object_object_get_ex(host_map, "obdclass", &obd_tag))
      if ((strcmp("mds", json_object_get_string(obd_tag)) != 0) &&	\
	  (strcmp("oss", json_object_get_string(obd_tag)) != 0))
	continue;
  
    json_object *data_array;
    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;

    arraylen = json_object_array_length(data_array);
    json_object *data_json;
    json_object *client_nid;
    json_object *hostname_tag;	
    for (j = 0; j < arraylen; j++) {
      data_json = json_object_array_get_idx(data_array, j);
      if (!json_object_object_get_ex(data_json, "client_nid", &client_nid))
	  continue;
      if (!json_object_object_get_ex(nid_map, json_object_get_string(client_nid), &hostname_tag))
	continue;

      json_object_object_add(data_json, "client", json_object_get(hostname_tag));
      json_object *entry_json;
      if (json_object_object_get_ex(host_map, json_object_get_string(hostname_tag), &entry_json)) {
	json_object *jid, *uid;
	if (json_object_object_get_ex(entry_json, "jid", &jid))
	  json_object_object_add(data_json, "jid", json_object_get(jid));
	if (json_object_object_get_ex(entry_json, "uid", &uid))
	  json_object_object_add(data_json, "uid", json_object_get(uid));
      }
    }
  }
  printf("Updating export map done\n");
  rc = 1;
 out:
  return rc;
}

static int print_exports_map() {
  int rc = -1;
  int arraylen;
  int j;
  json_object *obd_tag;
  json_object *client, *jid, *uid, *stats_type, *target;
  
  printf("---------------Exports map--------------------\n");
  printf("%20s : %10s %10s %10s %10s %20s\n", "server", "client", "jobid", "user", "stats_type", "target");
  json_object_object_foreach(host_map, key, host_entry) {    
    if (json_object_object_get_ex(host_map, "obdclass", &obd_tag))
      if ((strcmp("mds", json_object_get_string(obd_tag)) != 0) &&	\
	  (strcmp("oss", json_object_get_string(obd_tag)) != 0))
	continue;
    json_object *data_array;
    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;

    arraylen = json_object_array_length(data_array);
    for (j = 0; j < arraylen; j++) {
      json_object *data_json = json_object_array_get_idx(data_array, j);
      if ((!json_object_object_get_ex(data_json, "client", &client)) ||
	  (!json_object_object_get_ex(data_json, "jid", &jid)) ||	   
	  (!json_object_object_get_ex(data_json, "uid", &uid)))
	continue;

      json_object_object_get_ex(data_json, "stats_type", &stats_type);
      json_object_object_get_ex(data_json, "target", &target);

      printf("%20s : %10s %10s %10s %10s %20s\n", key, json_object_get_string(client), 
	     json_object_get_string(jid), json_object_get_string(uid), 
	     json_object_get_string(stats_type), json_object_get_string(target));
    }
  }
  rc = 1;
 out:
  return rc;
}

static void print_nid_map() {
  printf("--------------nids map------------------------\n");
  printf("%20s : %10s\n", "nid", "host");
  json_object_object_foreach(nid_map, key, val)
    printf("%20s : %10s\n", key, json_object_get_string(val)); 
}

/* Process RPC */
static int process_rpc(char *rpc)
{
  int rc = -1;
  json_object *rpc_json = NULL;
  char hostname[32];
  char nid[32];

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

  json_object *hostname_tag;
  if (json_object_object_get_ex(rpc_json, "hostname", &hostname_tag)) {
    snprintf(hostname, sizeof(hostname), "%s", json_object_get_string(hostname_tag));  

    json_object *nid_tag;
    if (json_object_object_get_ex(rpc_json, "nid", &nid_tag))
      json_object_object_add(nid_map, json_object_get_string(nid_tag), json_object_new_string(hostname));

    if (!json_object_object_get_ex(host_map, hostname, &hostname_tag)) {
      json_object *entry_json = json_object_new_object();
      json_object_object_add(entry_json, "jid", json_object_new_string("-"));
      json_object_object_add(entry_json, "uid", json_object_new_string("-"));
      json_object_object_add(host_map, hostname, entry_json);
    }
  }
  else
    goto out;

  printf_json(nid_map);

  json_object *entry_json;
  if (json_object_object_get_ex(host_map, hostname, &entry_json)) {

    json_object *jid, *uid, *obdclass, *data;
    if (json_object_object_get_ex(rpc_json, "jid", &jid))
      json_object_object_add(entry_json, "jid", json_object_get(jid));

    if (json_object_object_get_ex(rpc_json, "uid", &uid))
      json_object_object_add(entry_json, "uid", json_object_get(uid));

    if (json_object_object_get_ex(rpc_json, "obdclass", &obdclass))
      json_object_object_add(entry_json, "obdclass", json_object_get(obdclass));

    if (json_object_object_get_ex(rpc_json, "data", &data))
      json_object_object_add(entry_json, "data", json_object_get(data));

    map_stats();
  }
  //json_object_put(entry_json);
  print_nid_map();
  print_exports_map();
  //print_hosts_stats("mds");
  rc = 1;
  
 out:
  if (rpc_json)
    json_object_put(rpc_json);

  return rc;
}

static void map_destroy() {
  if (host_map)
    json_object_put(host_map);
  if (nid_map)
    json_object_put(nid_map);
}

int socket_destroy() {
  close(sockfd);
  map_destroy();
  return 0;
}

/* Socket is used by map_server to receive job data */
int socket_listen(const char *port) 
{
  int opt = 1;
  int backlog = 10;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int fd = -1;

  host_map = json_object_new_object();
  nid_map = json_object_new_object();

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

  freeaddrinfo(result);
 err:
  return;
}

