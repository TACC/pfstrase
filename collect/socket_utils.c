#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#include "utils.h"
#include "lfs_utils.h"
#include "socket_utils.h"
#include "exports.h"
#include "lod.h"
#include "osc.h"
#include "llite.h"
#include "sysinfo.h"

char const *exchange = "amq.direct";

int amqp_setup_connection(amqp_connection_state_t *conn, 
			  const char *port, const char *hostname)
{
  amqp_socket_t *socket = NULL;
  amqp_bytes_t request_queue;
  amqp_bytes_t response_queue;
  int status = -1;
  char localhost[64];

  gethostname(localhost, sizeof(localhost)); 

  *conn = amqp_new_connection();

  socket = amqp_tcp_socket_new(*conn);
  if (!socket) {
    die("creating TCP socket");
  }

  status = amqp_socket_open(socket, hostname, atoi(port));
  if (status) {
    die("opening TCP socket");
  }

  die_on_amqp_error(amqp_login(*conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
			       "guest", "guest"), "Logging in");
  amqp_channel_open(*conn, 1);
  die_on_amqp_error(amqp_get_rpc_reply(*conn), "Opening channel");
  
  {
    amqp_queue_declare_ok_t *r = amqp_queue_declare(*conn, 1, amqp_cstring_bytes(localhost), 
						    0, 0, 1, 1, amqp_empty_table);
    die_on_amqp_error(amqp_get_rpc_reply(*conn), "Declaring request queue");
    request_queue = amqp_bytes_malloc_dup(r->queue);
    if (request_queue.bytes == NULL) {
      fprintf(stderr, "Out of memory while copying queue name");
      return -1;
    }
  }
  {
    amqp_queue_declare_ok_t *r = amqp_queue_declare(*conn, 1, amqp_cstring_bytes("response"), 
						    0, 1, 0, 0, amqp_empty_table);
    die_on_amqp_error(amqp_get_rpc_reply(*conn), "Declaring response queue");
    response_queue = amqp_bytes_malloc_dup(r->queue);
    if (response_queue.bytes == NULL) {
      fprintf(stderr, "Out of memory while copying queue name");
      return -1;
    }
  }
  
  amqp_queue_bind(*conn, 1, request_queue, amqp_cstring_bytes(exchange),
                  amqp_cstring_bytes("request"), amqp_empty_table);
  die_on_amqp_error(amqp_get_rpc_reply(*conn), "Binding queue");

  amqp_queue_bind(*conn, 1, response_queue, amqp_cstring_bytes(exchange),
                  amqp_cstring_bytes("response"), amqp_empty_table);
  die_on_amqp_error(amqp_get_rpc_reply(*conn), "Binding queue");

  amqp_basic_consume(*conn, 1, request_queue, amqp_empty_bytes, 0, 1, 0,
                     amqp_empty_table);
  die_on_amqp_error(amqp_get_rpc_reply(*conn), "Consuming");
  
  //return 1;
  return amqp_socket_get_sockfd(socket);
}

void amqp_rpc(amqp_connection_state_t conn)
{

  char localhost[64];
  gethostname(localhost, sizeof(localhost)); 

  amqp_rpc_reply_t res;
  amqp_envelope_t envelope;
  
  amqp_maybe_release_buffers(conn);
  res = amqp_consume_message(conn, &envelope, NULL, 0);
  
  if (AMQP_RESPONSE_NORMAL != res.reply_type) {
    return;
  }
  
  printf("Delivery %u, exchange %.*s routingkey %.*s\n",
	 (unsigned)envelope.delivery_tag, (int)envelope.exchange.len,
	 (char *)envelope.exchange.bytes, (int)envelope.routing_key.len,
	 (char *)envelope.routing_key.bytes);
  
  if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
    printf("Content-type: %.*s\n",
	   (int)envelope.message.properties.content_type.len,
	   (char *)envelope.message.properties.content_type.bytes);
  }
  printf("----\n");    
  amqp_dump(envelope.message.body.bytes, envelope.message.body.len);    
  amqp_destroy_envelope(&envelope);
  
  amqp_send_data(conn);
}

void amqp_send_data(amqp_connection_state_t conn)      
{
  
  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
  props.content_type = amqp_cstring_bytes("text/plain");
  props.delivery_mode = 2; /* persistent delivery mode */

  // Gt basic device info
  struct device_info info;
  devices_discover(&info);
  char *buf = NULL;
  asprintf(&buf, "\"host\": \"%s\", \"nid\": \"%s\",\"time\": %llu.%llu, \"stats\": [",
	   info.hostname, info.nid, info.time.tv_sec, info.time.tv_nsec);

  // Exports
  if (info.class == MDS || info.class == OSS) {
    if (collect_exports(&info, &buf) == 0) {
      char *tmp = buf;
      asprintf(&buf, "%s},", buf);
      if (tmp != NULL) free(tmp);
    } 
    else
      fprintf(stderr, "export collection failed\n");
  }

  // LOD
  if (info.class == MDS) {
    if (collect_lod(&info, &buf) == 0) {    
      char *tmp = buf;
      asprintf(&buf, "%s},", buf);
      if (tmp != NULL) free(tmp);
    }
    else
      fprintf(stderr, "lod collection failed\n");    
  }

  if (info.class == OSC) {
    // LLITE
    if (collect_llite(&info, &buf) == 0) {
      char *tmp = buf;
      asprintf(&buf, "%s},", buf);
      if (tmp != NULL) free(tmp);
    }
    else
      fprintf(stderr, "llite collection failed\n");
    // OSC
    if (collect_osc(&info, &buf) == 0) {
      char *tmp = buf;
      asprintf(&buf, "%s},", buf);
      if (tmp != NULL) free(tmp);
    }
    else
      fprintf(stderr, "osc collection failed\n");
  }

  // SYSINFO
  if (collect_sysinfo(&info, &buf) == 0) {
      char *tmp = buf;
      asprintf(&buf, "%s},", buf);
      if (tmp != NULL) free(tmp);
  }
  else
    fprintf(stderr, "sysinfo collection failed\n");

  char *p = buf;
  p = buf + strlen(buf) - 1;
  *p = ']';

  // Send data
  if (buf) {
    printf("%s\n", buf);
    amqp_basic_publish(conn, 1,
		       amqp_cstring_bytes(exchange),
		       amqp_cstring_bytes("response"),
		       0, 0, &props,
		       amqp_cstring_bytes(buf));          
    free(buf);
  }
}

int sock_setup_connection(const char *port) 
{
  int sockfd;
  int rc;
  int opt = 1;
  int backlog = 10;
  struct addrinfo *servinfo, *p;
  struct addrinfo hints = {
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_PASSIVE, // use my IP
  };

  if ((rc = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
    return -1;
  }

  for(p = servinfo; p != NULL; p = p->ai_next) {

    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      continue;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
      fprintf(stderr, "cannot set SO_REUSEADDR: %s\n", strerror(errno));
      freeaddrinfo(servinfo);
    }
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      fprintf(stderr, "cannot bind: %s\n", strerror(errno));
      close(sockfd);
      continue;
    }
    break;
  }
  
  freeaddrinfo(servinfo);
  if (listen(sockfd, backlog) == -1) {
    fprintf(stderr, "cannot listen: %s\n", strerror(errno));
    close(sockfd);
    return -1;
  }

  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int fd = -1;
  
  if ((fd = accept(sockfd, (struct sockaddr *)&addr, &addrlen)) < 0)
    if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED))
      fprintf(stderr, "cannot accept connections: %s\n", strerror(errno));
  
  return fd;
}

void sock_rpc(int fd) 
{
  char request[SOCKET_BUFFERSIZE];
  int bytes_recvd = recv(fd, request, sizeof(request), 0);
  if (bytes_recvd < 0)
    fprintf(stderr, "cannot recv: %s\n", strerror(errno));
  printf(request);    
  sock_send_data(fd);
}

void sock_send_data(int fd)
{

  struct device_info info;
  devices_discover(&info);  

  {
    char *buf = NULL;
    collect_exports(&info, &buf);
    if (buf) {
      int rv = send(fd, buf, strlen(buf), 0);
      free(buf);
    }
  }
  {
    char *buf = NULL;
    collect_lod(&info, &buf);
    if (buf) {
      int rv = send(fd, buf, strlen(buf), 0);
      free(buf);
    }
  }
  {
    char *buf = NULL;
    collect_llite(&info, &buf);
    if (buf) {
      int rv = send(fd, buf, strlen(buf), 0);
      free(buf);
    }
  }
}
