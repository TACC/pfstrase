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
  sock_rpc();
}
static void sock_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  printf("collect and send data based on timer\n");
  sock_send_data();
}

/* using amqp sockets */
static void amqp_rpc_cb(EV_P_ ev_io *w, int revents)
{
  printf("collect and send data based on rpc\n");
  amqp_rpc();
}
static void amqp_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  printf("collect and send data based on timer\n");
  amqp_send_data();
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

  int fd;
  ev_timer timer;
  ev_io watcher;
  ev_io sock_watcher;  

  fd = amqp_setup_connection(port, host);
  ev_timer_init(&timer, amqp_timer_cb, 0.0, 200);   
  ev_io_init(&watcher, amqp_rpc_cb, fd, EV_READ);

  fd = sock_setup_connection("8888");  
  ev_io_init(&sock_watcher, sock_rpc_cb, fd, EV_READ);

  ev_timer_start(EV_DEFAULT, &timer);
  ev_io_start(EV_DEFAULT, &watcher);    
  ev_io_start(EV_DEFAULT, &sock_watcher);    
  ev_run(EV_DEFAULT, 0);
  
  if (pidfile_path != NULL)
    unlink(pidfile_path);
    
  amqp_kill_connection();

  if(fd)
    close(fd);

  return 0;
}
