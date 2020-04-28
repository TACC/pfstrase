#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "stats.h"
#include "shmmap.h"

#define mm_size 1024*1024

__attribute__((constructor))
static void shmmap_init(void) {
}
__attribute__((destructor))
static void shmmap_kill(void) {
  //shm_unlink(SEM_MUTEX_NAME);
  //shm_unlink(SERVER_TAG_RATE_MAP_FILE);
}

void set_shm_map() {

  caddr_t *mm_ptr = NULL;
  sem_t *mutex_sem = NULL;
  int fd_shm = NULL;

  char buffer[mm_size] = "";
  size_t cur = snprintf(buffer, sizeof(buffer), "%s", json_object_to_json_string(host_map));

  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, O_CREAT, 0660, 0)) == SEM_FAILED) {
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));
    goto out;
  }
  if ((fd_shm = shm_open (SERVER_TAG_RATE_MAP_FILE, O_RDWR | O_CREAT, 0644)) == -1) {
    fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
    goto out;
  }
  if (ftruncate (fd_shm, mm_size) == -1) {
    fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));  
    goto out;
  }
  if ((mm_ptr = mmap (NULL, mm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %s\n", strerror(errno));
    goto out;
  }

  strncpy((char *)mm_ptr, buffer, cur);
  //memcpy(mm_ptr, host_map, cur);

  if (sem_post (mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));  

 out:

  if (munmap (mm_ptr, mm_size) == -1)
    fprintf(stderr, "munmap failed: %s\n", strerror(errno));
  if (fd_shm)
    close(fd_shm);
  sem_close(mutex_sem);
}

void get_shm_map() {

  caddr_t *mm_ptr = NULL;
  sem_t *mutex_sem = NULL;
  int fd_shm = NULL;
  //json_object *server_map = NULL;

  //  mutual exclusion semaphore, mutex_sem with an initial value 0.  
  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, 0)) == SEM_FAILED) {
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));
    goto out;
  }
  sem_wait(mutex_sem);
  
  if ((fd_shm = shm_open (SERVER_TAG_RATE_MAP_FILE, O_RDWR, 0644)) == -1) {
    fprintf(stderr, "shm_open get failed: %s\n", strerror(errno));
    goto out;
  }
    
  if ((mm_ptr = mmap (NULL, mm_size, PROT_READ, MAP_SHARED, fd_shm, 0)) == MAP_FAILED) {
    fprintf(stderr, "mmap get failed: %s\n", strerror(errno));
    goto out;
  }
  
  enum json_tokener_error error = json_tokener_success;
  host_map = json_tokener_parse_verbose((char *)mm_ptr, &error);
  if (error != json_tokener_success) {
    fprintf(stderr, "mm_ptr `%s': %s\n", host_map, json_tokener_error_desc(error));
    goto out;
  }

  //host_map = json_object_get(server_map);

  if (sem_post(mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));

 out:
  //if (server_map)
  //json_object_put(server_map);
  
  if (munmap (mm_ptr, mm_size) == -1)
    fprintf(stderr, "munmap failed: %s\n", strerror(errno));
  
  if (fd_shm)
    close(fd_shm);
  if (mutex_sem)
    sem_close(mutex_sem);
}

