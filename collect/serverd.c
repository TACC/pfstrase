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
  printf("collect and send data based on sock rpc\n");
  sock_rpc();
}
static void sock_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  printf("collect and send data based on sock timer\n");
  sock_send_data();
}

/* using amqp sockets */
static void amqp_rpc_cb(EV_P_ ev_io *w, int revents)
{
  printf("collect and send data based on amqp rpc\n");
  amqp_rpc();
}
static void amqp_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  printf("collect and send data based on amqp timer\n");
  amqp_send_data();
}

int main(int argc, char *argv[])
{
  const char *host = "tacc-stats03.tacc.utexas.edu";
  const char *port = "5672";

  int pidfile_fd = -1;
  const char *pidfile_path = "/var/run/serverd.lock";

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

  int amqp_fd;
  int sock_fd;
  ev_timer timer;
  ev_io amqp_watcher;
  ev_io sock_watcher;  

  amqp_fd = amqp_setup_connection(port, host);
  ev_timer_init(&timer, amqp_timer_cb, 0.0, 300);   
  ev_io_init(&amqp_watcher, amqp_rpc_cb, amqp_fd, EV_READ);

  sock_fd = sock_setup_connection("8888");  
  ev_io_init(&sock_watcher, sock_rpc_cb, sock_fd, EV_READ);

  ev_timer_start(EV_DEFAULT, &timer);
  ev_io_start(EV_DEFAULT, &amqp_watcher);    
  ev_io_start(EV_DEFAULT, &sock_watcher);    
  ev_run(EV_DEFAULT, 0);
  
  if (pidfile_path != NULL)
    unlink(pidfile_path);
    
  amqp_kill_connection();

  if(amqp_fd)
    close(amqp_fd);
  if(sock_fd)
    close(sock_fd);


  return 0;
}
