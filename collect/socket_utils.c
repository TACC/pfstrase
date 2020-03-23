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
      json_object *stats;
      json_object_object_get_ex(data_json, "stats", &stats);
      printf("%20s : %10s %10s %10s %10s %20s %s\n", key, json_object_get_string(client), 
	     json_object_get_string(jid), json_object_get_string(uid), 
	     json_object_get_string(stats_type), json_object_get_string(target),json_object_get_string(stats));
    }
  }
  rc = 1;
 out:
  return rc;
}

static void print_nid_map() {
  printf("--------------nids map------------------------\n");
  printf("%20s : %10s %10s %10s\n", "nid", "host", "jid", "uid");
  json_object *hid, *jid, *uid;
  json_object_object_foreach(nid_map, key, val) {
    if ((json_object_object_get_ex(val, "hid", &hid)) && (json_object_object_get_ex(val, "jid", &jid)) && (json_object_object_get_ex(val, "uid", &uid)))
      printf("%20s : %10s %10s %10s\n", key, json_object_get_string(hid), 
	     json_object_get_string(jid), json_object_get_string(uid)); 
  }
}

static void print_server_tag_sum(const char *tag) {  
  printf("---------------server %s map--------------------\n", tag);
  printf("%10s : %10s %16s %16s\n", "server", tag, "iops", "bytes");
  json_object_object_foreach(server_client_sum, servername, server_entry) {    
    json_object_object_foreach(server_entry, clientname, client_entry) {    
      json_object *iops, *bytes;
      if (json_object_object_get_ex(client_entry, "iops", &iops) && \
	  json_object_object_get_ex(client_entry, "bytes", &bytes)) 
	printf("%10s : %10s %16lu %16lu\n", servername, clientname, json_object_get_int64(iops), 
	       json_object_get_int64(bytes));
    }
  }
}

static void print_server_tag_map(const char *tag) {  
  printf("---------------server %s map--------------------\n", tag);
  printf("%10s : %10s %20s\n", "server", tag, "data");
  json_object_object_foreach(server_client_map, servername, server_entry) {    
    json_object_object_foreach(server_entry, tagname, tag_entry) {    
      printf("%10s : %10s %20s\n", servername, tagname, json_object_get_string(tag_entry));
    }
  }
}

static int aggregate_server_tag_events() {
  json_object_object_foreach(server_client_map, servername, server_entry) {    
    json_object *client_sum_entry = json_object_new_object();
    json_object_object_foreach(server_entry, clientname, client_entry) {    
      long long sum_reqs = 0;
      long long sum_bytes = 0;
      json_object_object_foreach(client_entry, eventname, value) {    
	if (strcmp(eventname, "read_bytes") == 0 || strcmp(eventname, "write_bytes") == 0) 
	  sum_bytes += json_object_get_int64(value);
	else
	  sum_reqs += json_object_get_int64(value);
      }
      json_object *sum_json = json_object_new_object();
      json_object_object_add(sum_json, "iops", json_object_new_int64(sum_reqs));
      json_object_object_add(sum_json, "bytes", json_object_new_int64(sum_bytes));
      json_object_object_add(client_sum_entry, clientname, sum_json); 
    }
    json_object_object_add(server_client_sum, servername, client_sum_entry);
  }
  print_server_tag_sum("tag");
  return 0;
}

static int groupbytag(const char *tag) {
  int rc = -1;
  int arraylen;
  int j;
  json_object *obd_tag, *data_array, *data_json;
  json_object *tag_stats;
  json_object *tid;
  json_object_object_foreach(host_map, key, host_entry) {    
    if (json_object_object_get_ex(host_entry, "obdclass", &obd_tag)) {
      if ((strcmp("mds", json_object_get_string(obd_tag)) != 0) &&	\
	  (strcmp("oss", json_object_get_string(obd_tag)) != 0))
	continue;
    }
    else
      continue;

    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;

    json_object *tag_map = json_object_new_object();

    arraylen = json_object_array_length(data_array);
    for (j = 0; j < arraylen; j++) {
      data_json = json_object_array_get_idx(data_array, j);
      json_object *stats_type;
      if (json_object_object_get_ex(data_json, "stats_type", &stats_type)) {
	if ((strcmp("mds", json_object_get_string(stats_type)) != 0) &&	\
	    (strcmp("oss", json_object_get_string(stats_type)) != 0))
	  continue;
      }
      else 
	continue;

      if (!json_object_object_get_ex(data_json, tag, &tid))
	continue;     
      json_object *stats_json;
      if (!json_object_object_get_ex(data_json, "stats", &stats_json))
	continue;

      if (!json_object_object_get_ex(tag_map, json_object_get_string(tid), &tag_stats)) {
	tag_stats = json_object_new_object();
	json_object_object_add(tag_map, json_object_get_string(tid), tag_stats);
      }
      /* Add stats values for all devices with same client/jid/uid */
      json_object_object_foreach(stats_json, event, newval) {
	  json_object *oldval;
	  if (json_object_object_get_ex(tag_stats, event, &oldval))
	    json_object_object_add(tag_stats, event,  
				   json_object_new_int64(json_object_get_int64(oldval) + \
							 json_object_get_int64(newval)));
	  else 
	    json_object_object_add(tag_stats, event, json_object_get(newval));  
      }
      json_object_object_add(tag_map, json_object_get_string(tid), json_object_get(tag_stats));
      
    }
    json_object_object_add(server_client_map, key, json_object_get(tag_map));   
    json_object_put(tag_map);
  }  
  print_server_tag_map(tag);  
  aggregate_server_tag_events();
}

/* Map client nids of servers to jids and uids */
static int map_stats() {
  int rc = -1;
  int arraylen;
  int j;
  json_object *obd_tag, *data_array, *data_json;
  json_object *client_nid, *nid_entry;
  json_object *hid, *jid, *uid;

  json_object_object_foreach(host_map, key, host_entry) {    
    if (json_object_object_get_ex(host_entry, "obdclass", &obd_tag)) {
      if ((strcmp("mds", json_object_get_string(obd_tag)) != 0) &&	\
	  (strcmp("oss", json_object_get_string(obd_tag)) != 0))
	continue;
    }
    else 
      continue;

    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;

    arraylen = json_object_array_length(data_array);
    for (j = 0; j < arraylen; j++) {
      data_json = json_object_array_get_idx(data_array, j);
      if ((json_object_object_get_ex(data_json, "client_nid", &client_nid)) && \
	  (json_object_object_get_ex(nid_map, json_object_get_string(client_nid), &nid_entry))) {
	if (json_object_object_get_ex(nid_entry, "hid", &hid))
	  json_object_object_add(data_json, "client", json_object_get(hid));
	if (json_object_object_get_ex(nid_entry, "jid", &jid))
	  json_object_object_add(data_json, "jid", json_object_get(jid));
	if (json_object_object_get_ex(nid_entry, "uid", &uid))
	  json_object_object_add(data_json, "uid", json_object_get(uid));
      }
    }
  }
  rc = 1;
  groupbytag("client");
  groupbytag("jid");
  groupbytag("uid");
 out:
  return rc;
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

  /* If hostname is not in the rpc it is not valid so return */
  json_object *hostname_tag;
  if (json_object_object_get_ex(rpc_json, "hostname", &hostname_tag)) {
    snprintf(hostname, sizeof(hostname), "%s", json_object_get_string(hostname_tag));  
    /* init host_entry of hostmap if does not exist */
    json_object *hostname_entry;
    if (!json_object_object_get_ex(host_map, hostname, &hostname_entry)) {
      json_object *entry_json = json_object_new_object();
      json_object_object_add(entry_json, "jid", json_object_new_string("-"));
      json_object_object_add(entry_json, "uid", json_object_new_string("-"));
      json_object_object_add(host_map, hostname, entry_json);
    }
  }
  else
    goto out;

  /* update hostname_entry if tags or data are present in rpc */
  json_object *entry_json;
  if (json_object_object_get_ex(host_map, hostname, &entry_json)) {

    json_object *hid, *jid, *uid, *nid, *obdclass, *data;
    if (json_object_object_get_ex(rpc_json, "hostname", &hid))
      json_object_object_add(entry_json, "hid", json_object_get(hid));

    if (json_object_object_get_ex(rpc_json, "jid", &jid))
      json_object_object_add(entry_json, "jid", json_object_get(jid));

    if (json_object_object_get_ex(rpc_json, "uid", &uid))
      json_object_object_add(entry_json, "uid", json_object_get(uid));

    if (json_object_object_get_ex(rpc_json, "obdclass", &obdclass))
      json_object_object_add(entry_json, "obdclass", json_object_get(obdclass));

    if (json_object_object_get_ex(rpc_json, "data", &data))
      json_object_object_add(entry_json, "data", json_object_get(data));

    if (json_object_object_get_ex(rpc_json, "nid", &nid))
      json_object_object_add(nid_map, json_object_get_string(nid), json_object_get(entry_json));

    map_stats();
  }

  //print_nid_map();
  //print_exports_map();
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

  server_client_map = json_object_new_object();
  server_client_sum = json_object_new_object();
  server_jid_map = json_object_new_object();
  server_uid_map = json_object_new_object();

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

