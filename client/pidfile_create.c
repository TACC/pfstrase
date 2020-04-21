#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

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
