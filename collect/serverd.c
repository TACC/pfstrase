#include <sys/types.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <ev.h>
#include <string.h>
#include <syslog.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#include "utils.h"
#include "socket_utils.h"
#include "daemonize.h"

static char *app_name = NULL;
static char *conf_file_name = NULL;
static FILE *log_stream = NULL;

static char *server = NULL;
static port = 5672;
static double freq = 300;

static ev_timer timer;

/* Read conf file in to set server, port, and frequency of collection*/
int read_conf_file()
{
  FILE *conf_file_fd = NULL;
  int ret = -1;

  if (conf_file_name == NULL) return 0;

  conf_file_fd = fopen(conf_file_name, "r");

  if (conf_file_fd == NULL) {
    syslog(LOG_ERR, "Can not open config file: %s, error: %s",
	   conf_file_name, strerror(errno));
    return -1;
  }

  char *line_buf = NULL;
  size_t line_buf_size = 0;
  while(getline(&line_buf, &line_buf_size, conf_file_fd) >= 0) {
    char *line = line_buf;
    char *key = strsep(&line, " :\t=");	
    if (key == NULL || line == NULL)
      continue;
    if (strcmp(key, "server") == 0) { 
      line[strlen(line) - 1] = '\0';
      server = strdup(line);
      syslog(LOG_ERR, "%s: Setting server to %s based on file %s\n",
	      app_name, server, conf_file_name);
    }
    if (strcmp(key, "port") == 0) {
      line[strlen(line) - 1] = '\0';
      port = atoi(line);
      syslog(LOG_ERR, "%s: Setting server port to %d based on file %s\n",
	      app_name, port, conf_file_name);
    }
    if (strcmp(key, "frequency") == 0) {  
      if (sscanf(line, "%lf", &freq) == 1)
	syslog(LOG_ERR, "%s: Setting frequency to %f based on file %s\n",
	       app_name, freq, conf_file_name);
    }
  }

  fclose(conf_file_fd);

  return ret;
}

/* using bare sockets */
static void sock_rpc_cb(EV_P_ ev_io *w, int revents)
{
  //fprintf(log_stream, "collect and send data based on sock rpc\n");
  syslog(LOG_INFO, "collect and send data based on socket rpc\n");
  sock_rpc();
}
static void sock_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  fprintf(log_stream, "collect and send data based on socket timer\n");
  sock_send_data();
}

/* using amqp sockets */
static void amqp_rpc_cb(EV_P_ ev_io *w, int revents)
{
  //fprintf(log_stream, "collect and send data based on amqp rpc\n");
  syslog(LOG_INFO, "collect and send data based on amqp rpc\n");
  amqp_rpc();
}
static void amqp_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  fprintf(log_stream, "collect and send data based on amqp timer\n");
  amqp_send_data();
}
/* Signal Callbacks for SIGINT (terminate) and SIGHUP (reload conf file) */
static void signal_cb_int(EV_P_ ev_signal *sig, int revents)
{
    fprintf(log_stream, "Stopping pfstrased\n");
    if (pid_fd != -1) {
      lockf(pid_fd, F_ULOCK, 0);
      close(pid_fd);
    }
    if (pid_file_name != NULL) {
      unlink(pid_file_name);
    }
    ev_break (EV_A_ EVBREAK_ALL);
}
static void signal_cb_hup(EV_P_ ev_signal *sig, int revents) 
{
  fprintf(log_stream, "Reloading pfstrase config file\n");
  read_conf_file();    
  timer.repeat = freq; 
  ev_timer_again(EV_DEFAULT, &timer);
}

static void usage(void)
{
  fprintf(stderr,
          "Usage: %s [OPTION]... [TYPE]...\n"
          "Collect statistics.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -h, --help                 display this help and exit\n"
	  "  -d --daemon                Run in daemon mode\n"
          "  -s --server    [SERVER]    Server to send data.\n"
          "  -f --frequency [FREQUENCY] Frequency to sample.\n"
	  "  -c --conf_file [FILENAME]  Read configuration from the file\n"
	  "  -l --log_file  [FILENAME]  Write logs to the file\n"
	  "  -p --pid_file  [FILENAME]  PID file used in daemon mode.\n"
          ,
          program_invocation_short_name);
}

int main(int argc, char *argv[])
{
  int daemonmode = 0;
  char *log_file_name = NULL;
  app_name = argv[0];

  struct option opts[] = {
    { "help",   no_argument, 0, 'h' },
    { "daemon", no_argument, 0, 'd' },
    { "server", required_argument, 0, 's' },
    { "freq ",  required_argument, 0, 'f' },
    {"conf_file", required_argument, 0, 'c'},
    {"log_file", required_argument, 0, 'l'},
    {"pid_file", required_argument, 0, 'p'},
    { NULL,     0, 0, 0 },
  };

  int c;
  while ((c = getopt_long(argc, argv, "hds:f:c:l:p:", opts, 0)) != -1) {
    switch (c) {
    case 'd':
      daemonmode = 1;
      break;
    case 's':
      server = strdup(optarg);
      break;
    case 'f':
      freq = atof(optarg);
      break;
    case 'c':
      conf_file_name = strdup(optarg);
      break;
    case 'l':
      log_file_name = strdup(optarg);
      break;
    case 'p':
      pid_file_name = strdup(optarg);
      break;
    case 'h':
      usage();
      exit(0);
    case '?':
      fprintf(stderr, "Try `%s --help' for more information.\n", program_invocation_short_name);
      exit(1);
    }
  }

  if (daemonmode) {
    if (pid_file_name == NULL) 
      pid_file_name = strdup("/var/run/pfstrased.pid");
    daemonize();
  }
  
  openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
  syslog(LOG_INFO, "Started %s", app_name);

  /* Setup signal callbacks to stop pfstrased or reload conf file */
  signal(SIGPIPE, SIG_IGN);
  static struct ev_signal sigint;
  ev_signal_init(&sigint, signal_cb_int, SIGINT);
  ev_signal_start(EV_DEFAULT, &sigint);
  //static struct ev_signal sigterm;
  //ev_signal_init(&sigint, signal_cb_int, SIGTERM);
  //ev_signal_start(EV_DEFAULT, &sigterm);
  static struct ev_signal sighup;
  ev_signal_init(&sighup, signal_cb_hup, SIGHUP);
  ev_signal_start(EV_DEFAULT, &sighup);
  log_stream = stderr;
  /* Try to open log file to this daemon */
  /*
  if (log_file_name != NULL) {
    log_stream = fopen(log_file_name, "a+");
    if (log_stream == NULL) {
      syslog(LOG_ERR, "Can not open log file: %s, error: %s",
	     log_file_name, strerror(errno));
      log_stream = stderr;
    }
  } else {
    log_stream = stderr;
  }

  int ret = fprintf(log_stream, "Debug: %d\n", 1);
  if (ret < 0) {
    syslog(LOG_ERR, "Can not write to log stream: %s, error: %s",
	   (log_stream == stderr) ? "stderr" : log_file_name, strerror(errno));
  }
  ret = fflush(log_stream);
  if (ret != 0) {
    syslog(LOG_ERR, "Can not fflush() log stream: %s, error: %s",
	   (log_stream == stderr) ? "stderr" : log_file_name, strerror(errno));
  }
  */
  /* Read configuration from config file */
  read_conf_file(0);
  
  if (server == NULL) {
    fprintf(log_stream, "Must specify a server to send data to with -s [--server] argument.\n");
    exit(0);
  } else {
    fprintf(log_stream, "Sending data to server %s.\n", server);
  }
  
  int amqp_fd;
  int sock_fd;

  //ev_io amqp_watcher;
  ev_io sock_watcher;  

  /*  Setup persistent AMQP connection to RMQ server */
  while (amqp_fd = amqp_setup_connection(port, server) < 0) sleep(60);
  /* Setup persistent local listener to accept RPCs to socket */
  sock_fd = sock_setup_connection("8888");  

  /* Initialize timer routine to collect and send data */
  ev_timer_init(&timer, amqp_timer_cb, 0.0, freq);   
  /* Initialize callback to respond to RPCs sent through RMQ connections */
  //ev_io_init(&amqp_watcher, amqp_rpc_cb, amqp_fd, EV_READ);
  /* Initialize callback to respond to RPCs send to socekt */
  ev_io_init(&sock_watcher, sock_rpc_cb, sock_fd, EV_READ);

  ev_timer_start(EV_DEFAULT, &timer);
  //ev_io_start(EV_DEFAULT, &amqp_watcher);    
  ev_io_start(EV_DEFAULT, &sock_watcher);    
  syslog(LOG_ERR, "Starting pfstrased with collection frequency %fs\n", freq);
  ev_run(EV_DEFAULT, 0);

  /* Clean up sockets and connections */
  amqp_kill_connection();

  if(amqp_fd)
    close(amqp_fd);
  if(sock_fd)
    close(sock_fd);

  /* Close log file, when it is used. */
  if (log_stream != stderr) {
    fclose(log_stream);
  }

  /* Write system log and close it. */
  syslog(LOG_INFO, "Stopped %s", app_name);
  closelog();
  /* Free up names of files */
  if (conf_file_name != NULL) free(conf_file_name);
  if (log_file_name != NULL) free(log_file_name);
  if (pid_file_name != NULL) free(pid_file_name);

  return EXIT_SUCCESS;
}
