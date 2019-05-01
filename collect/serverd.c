#include <sys/types.h>
#include <getopt.h>
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

static void usage(void)
{
  fprintf(stderr,
          "Usage: %s [OPTION]... [TYPE]...\n"
          "Collect statistics.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -h, --help         display this help and exit\n"
          "  -s [SERVER] or --server [SERVER]       Server to send data.\n"
          "  -q [QUEUE] or --queue [QUEUE]      Queue to route data to on RMQ server. \n"
          "  -p [PORT] or --port [PORT]         Port to use (5672 is the default).\n"
          "  -f [FREQUENCY] or --frequency [FREQUENCY]  Frequency to sample (600 seconds is the default).\n"
          ,
          program_invocation_short_name);
}

int main(int argc, char *argv[])
{
  char *server = NULL;
  char *port   = "5672";
  double freq  = 300;
  int daemonmode = 0;

  struct option opts[] = {
    { "help",   0, 0, 'h' },
    { "daemon", 0, 0, 'd' },
    { "server", 0, 0, 's' },
    { "port",   0, 0, 'p' },
    { "freq ",  0, 0, 'f' },
    { NULL,     0, 0, 0 },
  };

  int c;
  while ((c = getopt_long(argc, argv, "hds:p:f:", opts, 0)) != -1) {
    switch (c) {
    case 'h':
      usage();
      exit(0);
    case 'd':
      daemonmode = 1;
      continue;
    case 's':
      server = optarg;
      continue;
    case 'p':
      port = optarg;
      continue;
    case 'f':
      freq = atof(optarg);
      continue;
    case '?':
      fprintf(stderr, "Try `%s --help' for more information.\n", program_invocation_short_name);
      exit(1);
    }
  }

  if (server == NULL) {
    fprintf(stderr, "Must specify a RMQ server with -s [--server] argument.\n");
    exit(0);
  }

  int pidfile_fd = -1;
  const char *pidfile_path = "/run/pfstrase.pid";

  if (daemonmode)
    if (daemon(0, 0) < 0) {
      fprintf(stderr, "failed to daemonize %m\n");
      exit(1);
    }
  
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

  while (amqp_fd = amqp_setup_connection(port, server) < 0) sleep(60);

  ev_timer_init(&timer, amqp_timer_cb, 0.0, freq);   
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
