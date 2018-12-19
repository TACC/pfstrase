#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <ev.h>
#include <string.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>

#include "pidfile_create.h"
#include "utils.h"
#include "socket_utils.h"

static void signal_cb(EV_P_ ev_signal *sigterm, int revents)
{
  ev_break (EV_A_ EVBREAK_ALL);
}

/* using bare sockets */
static void sock_rpc_cb(EV_P_ ev_io *w, int revents)
{
  printf("collect and send data based on rpc\n");
  sock_rpc(w->fd);
}
static void sock_send_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  printf("collect and send data based on timer\n");
  sock_send_data(*((int *)w->data));
}

/* using amqp sockets */
static void amqp_rpc_cb(EV_P_ ev_io *w, int revents)
{
  printf("collect and send data based on rpc\n");
  amqp_rpc(*((amqp_connection_state_t *)w->data));
}
static void amqp_send_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  printf("collect and send data based on timer\n");
  amqp_send_data(*((amqp_connection_state_t *)w->data));
}

int main(int argc, char *argv[])
{
  const char *host = "tacc-stats03.tacc.utexas.edu";
  const char *port = "5672";

  int pidfile_fd = -1;
  //const char *pidfile_path = "/var/run/serverd.lock";
  const char *pidfile_path = "serverd.lock";

  int amqp_mode = 1;
  int sock_mode = 0;
  /*
  if (daemon(0, 0) < 0) {
    fprintf(stderr, "failed to daemonize %m\n");
    exit(1);
  }
  */

  if (pidfile_path != NULL) {
    pidfile_fd = pidfile_create(pidfile_path);
    if (pidfile_fd < 0)      
      exit(1);
  }
    
  signal(SIGPIPE, SIG_IGN);
  static struct ev_signal sigterm;
  ev_signal_init(&sigterm, signal_cb, SIGTERM);
  ev_signal_start(EV_DEFAULT, &sigterm);

  amqp_connection_state_t conn;
  int fd;
  ev_timer timer;
  ev_io watcher;
  
  if(amqp_mode) {
    fd = amqp_setup_connection(&conn, port, host);
    timer.data = (void *)&conn;
    ev_timer_init(&timer, amqp_send_cb, 0.0, 1);   
    watcher.data = (void *)&conn;
    ev_io_init(&watcher, amqp_rpc_cb, fd, EV_READ);

  }   
  else if (sock_mode) {
    fd = sock_setup_connection(port);  
    timer.data = (void *)&fd;
    ev_timer_init(&timer, sock_send_cb, 0.0, 1);  
    ev_io_init(&watcher, sock_rpc_cb, fd, EV_READ);
  }
  else {
    fprintf(stderr, "Error: connection mode not supported\n");
    exit(1);
  }  

  ev_timer_start(EV_DEFAULT, &timer);
  ev_io_start(EV_DEFAULT, &watcher);    
  ev_run(EV_DEFAULT, 0);
  
  if (pidfile_path != NULL)
    unlink(pidfile_path);
    
  if (amqp_mode) {
    die_on_amqp_error(amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS),
		      "Closing channel");
    die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS),
		      "Closing connection");
    die_on_error(amqp_destroy_connection(conn), "Ending connection");
  }

  close(fd);

  return 0;
}
