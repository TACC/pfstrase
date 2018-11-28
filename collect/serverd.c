#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>

#include <ev.h>
#include <dirent.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PROCFS_BUF_SIZE 4096
#define SOCKET_BUFFERSIZE 655336

int pidfile_create(const char *path) {
    pid_t pid = getpid();
    int fd = -1;
    struct flock fl = {
      .l_type = F_WRLCK,
      .l_whence = SEEK_SET,
    };
    char buf[80];
    int len;

    fd = open(path, O_RDWR|O_CREAT|O_CLOEXEC, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
    if (fd < 0) {
      fprintf(stderr, "cannot open pidfile `%s': %s\n", path, strerror(errno));
      goto err;
    }
    if (fcntl(fd, F_SETLK, &fl) < 0) {
      if (errno == EAGAIN || errno == EACCES)
	fprintf(stderr, "cannot lock pidfile `%s': another instance may be running\n", path);
      else
	fprintf(stderr, "cannot lock pidfile `%s': %s\n", path, strerror(errno));
      goto err;
    }
    if (ftruncate(fd, 0) < 0) {
      fprintf(stderr, "cannot truncate pidfile `%s': %s\n", path, strerror(errno));
      goto err;
    }

    len = snprintf(buf, sizeof(buf), "%ld\n", (long) pid);
    if (write(fd, buf, len) != len) {
      fprintf(stderr, "cannot write to pidfile `%s': %s\n", path, strerror(errno));
      goto err;
    }
    return fd;
  err:
    if (!(fd < 0))
      close(fd);
    return -1;
}

static int collect_exports(const char *type_path, char *stats_buffer, int size_stats_buffer)
{
  DIR *type_dir = NULL;
  int rc = -1;
  int sb_pos = 0;

  type_dir = opendir(type_path);
  if(type_dir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", type_path);
    goto type_dir_err;
  }

  struct dirent *de;
  while ((de = readdir(type_dir)) != NULL) {  
    DIR *export_dir = NULL;
    char export_dir_path[256];
    
    if (de->d_type != DT_DIR || de->d_name[0] == '.')
      continue;
    
    snprintf(export_dir_path, sizeof(export_dir_path), 
	     "%s/%s/exports", type_path, de->d_name);

    export_dir = opendir(export_dir_path); 
    if(export_dir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", export_dir_path);
      goto export_dir_err;
    }

    struct dirent *nid_de;
    while ((nid_de = readdir(export_dir)) != NULL) {
      char stats_path[256];
      char procfs_buf[PROCFS_BUF_SIZE];
      FILE *fd = NULL;

      if (nid_de->d_type != DT_DIR || nid_de->d_name[0] == '.')
	continue;
      snprintf(stats_path, sizeof(stats_path), "%s/%s/stats", 
	       export_dir_path, nid_de->d_name);

      fd = fopen(stats_path, "r");
      if (fd == NULL) {
	fprintf(stderr, "cannot open %s: %m\n", stats_path);
	goto stat_path_err;
      }

      setvbuf(fd, procfs_buf, _IOFBF, sizeof(procfs_buf));

      char *line_buf = NULL;
      size_t line_buf_size = 0;    
      unsigned long long secs = 0, nsecs = 0, count = 0, sum = 0, value = 0;
      while(getline(&line_buf, &line_buf_size, fd) >= 0) {
	char *line = line_buf;
	char *key = strsep(&line, " \t\n\v\f\r");	
	if (key == NULL || line == NULL)
	  continue;
	if (strcmp(key, "snapshot_time") == 0) {
	  sscanf(line, "%llu.%llu secs.nsecs", &secs, &nsecs);
	  continue;
	}

	int n = sscanf(line, "%llu samples %*s %*u %*u %llu", &count, &sum);
	if (n == 1)
	  value = count;
	if (n == 20)
	  value = sum;

	sb_pos += snprintf(stats_buffer + sb_pos, size_stats_buffer, 
			   "time: %llu.%llu host: %s key: %s val: %llu\n", 
			   secs, nsecs, nid_de->d_name, key, value);	
      }
      if (line_buf != NULL) 
	free(line_buf);
    stat_path_err:
      if (fd != NULL)
	fclose(fd);  	
    }
  
  export_dir_err:
    if (export_dir != NULL)
      closedir(export_dir);  
  }

  rc = 0;
  printf(stats_buffer);

 type_dir_err:
  if (type_dir != NULL)
    closedir(type_dir);

  return rc;
}

static void timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  char stats_buffer[SOCKET_BUFFERSIZE];
  const char *type_path = "/proc/fs/lustre/mdt";
  collect_exports(type_path, stats_buffer, sizeof(stats_buffer));
}

static void signal_cb(EV_P_ ev_signal *sigterm, int revents)
{
  ev_break (EV_A_ EVBREAK_ALL);
}

static int get_listener_socket(char *port) 
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

  return sockfd;
}

static void fd_cb(EV_P_ ev_io *w, int revents)
{
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int cfd = -1;
  
  if ((cfd = accept(w->fd, (struct sockaddr *)&addr, &addrlen)) < 0)
    if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED))
      fprintf(stderr, "cannot accept connections: %s\n", strerror(errno));
    
  char request[SOCKET_BUFFERSIZE];
  int bytes_recvd = recv(cfd, request, sizeof(request), 0);
  if (bytes_recvd < 0)
    fprintf(stderr, "cannot recv: %s\n", strerror(errno));

  printf("request: %s\n", request);
  
  char stats_buffer[SOCKET_BUFFERSIZE];
  const char *type_path = "/proc/fs/lustre/mdt";
  collect_exports(type_path, stats_buffer, sizeof(stats_buffer));

  char response[SOCKET_BUFFERSIZE];
  snprintf(response, sizeof(response), "%s\n", stats_buffer);
  int rv = send(cfd, response, sizeof(response), 0);
  close(cfd);
  ev_break(EV_A_ EVBREAK_ALL);
}

int main(int argc, char *argv[])
{

  const char *host = "mds";
  const char *port = "9169";
  
  int pidfile_fd = -1;
  const char *pidfile_path = NULL;

  pidfile_path = "/var/run/serverd.lock";

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

  int lfd;
  if ((lfd = get_listener_socket("9980")) < 0) {
    fprintf(stderr, "server: fatal error getting listening socket\n");
    exit(1);
  }

  signal(SIGPIPE, SIG_IGN);
  ev_timer timer;
  static struct ev_signal sigterm;
  ev_signal_init(&sigterm, signal_cb, SIGTERM);
  ev_signal_start(EV_DEFAULT, &sigterm);
  /*
  ev_timer_init(&timer, timer_cb, 0.0, 5.0);
  ev_timer_start(EV_DEFAULT, &timer);
  ev_run(EV_DEFAULT, 0);
  */
  ev_io socket_watcher;
  ev_io_init(&socket_watcher, fd_cb, lfd, EV_READ);
  ev_io_start(EV_DEFAULT, &socket_watcher);
  while(1)
    ev_run(EV_DEFAULT, 0);

  if (pidfile_path != NULL)
    unlink(pidfile_path);
  close(lfd);
  return 0;
}
