#include <sys/types.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <ev.h>
#include <string.h>
#include "daemonize.h"
#include "pq.h"

static char *app_name = NULL;
static char *conf_file_name = NULL;
static FILE *log_stream = NULL;

static char *dbserver = NULL;
static char *dbname = NULL;
static char *dbuser = NULL;
static char *port = "5433";
static double db_interval = 30;

static ev_timer pq_timer;

/* Read conf file in to set server, port, and frequency of collection*/
int read_conf_file()
{
  FILE *conf_file_fd = NULL;
  int ret = -1;

  if (conf_file_name == NULL) return 0;

  conf_file_fd = fopen(conf_file_name, "r");

  if (conf_file_fd == NULL) {
    fprintf(log_stream, "Can not open config file: %s, error: %s",
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
    if (strcmp(key, "port") == 0) {
      line[strlen(line) - 1] = '\0';
      port = strdup(line);
      fprintf(log_stream, "%s: Setting server port to %s based on file %s\n",
	      app_name, port, conf_file_name);
    }
    if (strcmp(key, "dbserver") == 0) {  
      line[strlen(line) - 1] = '\0';
      dbserver = strdup(line);
      fprintf(log_stream, "%s: Setting database server to %s based on file %s\n",
	      app_name, dbserver, conf_file_name);
    }
    if (strcmp(key, "dbname") == 0) {  
      line[strlen(line) - 1] = '\0';
      dbname = strdup(line);
      fprintf(log_stream, "%s: Setting database name to %s based on file %s\n",
	      app_name, dbname, conf_file_name);
    }
    if (strcmp(key, "dbuser") == 0) {  
      line[strlen(line) - 1] = '\0';
      dbuser = strdup(line);
      fprintf(log_stream, "%s: Setting database user to %s based on file %s\n",
	      app_name, dbuser, conf_file_name);
    }
    if (strcmp(key, "db_interval") == 0) {  
      if (sscanf(line, "%lf", &db_interval) == 1)
	fprintf(log_stream, "%s: Setting database update interval to %f based on file %s\n",
	       app_name, db_interval, conf_file_name);
    }
  }
  if (line_buf)
    free(line_buf);
  fclose(conf_file_fd);
  return ret;
}

/* Ssend data to postgres based on ev pg_timer interval */
static void pq_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  //fprintf(log_stream, "Sending data to postgres server\n");
  pq_insert();
}

/* Signal Callbacks for SIGINT (terminate) and SIGHUP (reload conf file) */
static void signal_cb_int(EV_P_ ev_signal *sig, int revents)
{
    fprintf(log_stream, "Stopping pfsql\n");
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
  fprintf(log_stream, "Reloading pfsql config file\n");
  read_conf_file();    
  pq_timer.repeat = db_interval; 
  ev_timer_again(EV_DEFAULT, &pq_timer);
}

static void usage(void)
{
  fprintf(stderr,
          "Usage: %s [OPTION]... [TYPE]...\n"
          "Statistics collector for PFSTRASE (Parallel FileSystem TRacing and Analysis SErvice).\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -h, --help                 display this help and exit.\n"
	        "  -d --daemon                Run in daemon mode.\n"
          "  -i --interval  [INTERVAL]  Interval to update shared memory.\n"
	  "  -c --conf_file [FILENAME]  Read configuration from the file.\n"
	  "  -p --port      [PORT]      Port to listen on.\n"
          ,
          program_invocation_short_name);
}

int main(int argc, char *argv[])
{
  int daemonmode = 0;
  char *log_file_name = NULL;
  char *pid_file_name = NULL;
  app_name = argv[0];

  struct option opts[] = {
    { "help",   no_argument, 0, 'h' },
    { "daemon", no_argument, 0, 'd' },
    { "interval ",  required_argument, 0, 'i' },
    { "conf_file", required_argument, 0, 'c' },
    { "port", required_argument, 0, 'p' },
    { NULL,     0, 0, 0 },
  };

  int c;
  while ((c = getopt_long(argc, argv, "hds:f:c:l:p:", opts, 0)) != -1) {
    switch (c) {
    case 'd':
      daemonmode = 1;
      break;
    case 'c':
      conf_file_name = strdup(optarg);
      break;
    case 'p':
      port = strdup(optarg);
      break;
    case 'h':
      usage();
      exit(0);
    case '?':
      fprintf(stderr, "Try '%s --help' for more information.\n", program_invocation_short_name);
      exit(1);
    }
  }

  if (daemonmode) {
    if (pid_file_name == NULL) 
      pid_file_name = strdup("/var/run/pfsql.pid");
    daemonize();
  }
  log_stream = stderr;  
  fprintf(log_stream, "Started %s\n", app_name);

  /* Setup signal callbacks to stop pfsql or reload conf file */
  signal(SIGPIPE, SIG_IGN);
  static struct ev_signal sigint;
  ev_signal_init(&sigint, signal_cb_int, SIGINT);
  ev_signal_start(EV_DEFAULT, &sigint);

  static struct ev_signal sighup;
  ev_signal_init(&sighup, signal_cb_hup, SIGHUP);
  ev_signal_start(EV_DEFAULT, &sighup);


  read_conf_file(0);

  shmmap_client_init();

  if (pq_connect(dbserver, dbname, dbuser) < 0)
    goto out;
  ev_timer_init(&pq_timer, pq_timer_cb, 0.0, db_interval);   
  ev_timer_start(EV_DEFAULT, &pq_timer);

  ev_run(EV_DEFAULT, 0);

  pq_finish();

 out:

  /* Close log file, when it is used. */
  if (log_stream != stderr) {
    fclose(log_stream);
  }

  /* Write system log and close it. */
  fprintf(log_stream, "Stopped %s", app_name);

  /* Free up names of files */
  if (conf_file_name != NULL) free(conf_file_name);
  if (log_file_name != NULL) free(log_file_name);
  if (pid_file_name != NULL) free(pid_file_name);

  //shmmap_client_kill();

  return EXIT_SUCCESS;
}
