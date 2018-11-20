#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>

#include <ev.h>
#include <dirent.h>

#define PROCFS_BUF_SIZE 4096

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

static int collect(const char *type_path)
{
  DIR *type_dir = NULL;
  int rc = -1;

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
      char buf[PROCFS_BUF_SIZE];
      FILE *fd = NULL;

      if (nid_de->d_type != DT_DIR || nid_de->d_name[0] == '.')
	continue;
      snprintf(stats_path, sizeof(stats_path), "%s/%s/stats", 
	       export_dir_path, nid_de->d_name);
      printf("%s\n", nid_de->d_name);
      fd = fopen(stats_path, "r");
      if (fd == NULL) {
	fprintf(stderr, "cannot open %s: %m\n", stats_path);
	goto stat_path_err;
      }

      char *line_buf = NULL;
      size_t line_buf_size = 0;
      setvbuf(fd, buf, _IOFBF, sizeof(buf));

      while(getline(&line_buf, &line_buf_size, fd) >= 0) {
	char *line = line_buf;
	printf(line);
      }

    stat_path_err:
      if (line_buf != NULL) 
	free(line_buf);
      if (fd != NULL)
	fclose(fd);  	
    }
  
  export_dir_err:
    if (export_dir != NULL)
      closedir(export_dir);  
  }

  rc = 0;

 type_dir_err:
  if (type_dir != NULL)
    closedir(type_dir);

  return rc;
}

static void timer_cb(struct ev_loop *loop, ev_timer *w, int revents) 
{
  const char *type_path = "/proc/fs/lustre/mdt";
  collect(type_path);
}

static void signal_cb(EV_P_ ev_signal *sigterm, int revents)
{
  ev_break (EV_A_ EVBREAK_ALL);
}

int main(int argc, char *argv[])
{

  const char *host = "mds";
  const char *port = "9169";
  
  int pidfile_fd = -1;
  const char *pidfile_path = NULL;

  ev_timer timer;

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

  signal(SIGPIPE, SIG_IGN);
  static struct ev_signal sigterm;
  ev_signal_init(&sigterm, signal_cb, SIGTERM);
  ev_signal_start(EV_DEFAULT, &sigterm);

  ev_timer_init(&timer, timer_cb, 0.0, 5.0);
  ev_timer_start(EV_DEFAULT, &timer);
  ev_run(EV_DEFAULT, 0);

  if (pidfile_path != NULL)
    unlink(pidfile_path);

  return 0;
}
