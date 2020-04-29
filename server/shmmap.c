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

#define mm_size 1024*1024*1024


int fd_shm = NULL;
caddr_t *mm_ptr = NULL;

__attribute__((constructor))
static void shmmap_init(void) {
  //shm_unlink(SERVER_TAG_RATE_MAP_FILE);
  if ((fd_shm = shm_open (SERVER_TAG_RATE_MAP_FILE, O_RDWR | O_CREAT, 0644)) == -1) {
    fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
  }
  if (ftruncate (fd_shm, mm_size) == -1) {
    fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));  
  }
  if ((mm_ptr = mmap (NULL, mm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %s\n", strerror(errno));
  }
}
__attribute__((destructor))
static void shmmap_kill(void) {
  if (munmap (mm_ptr, mm_size) == -1)
    fprintf(stderr, "munmap failed: %s\n", strerror(errno));
  if (fd_shm)
    close(fd_shm);
  //shm_unlink(SEM_MUTEX_NAME);
  //shm_unlink(SERVER_TAG_RATE_MAP_FILE);
}

void set_shm_map() {

  sem_t *mutex_sem = NULL;
  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 

  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, O_CREAT, 0660, 0)) == SEM_FAILED) {
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));
    goto out;
  }

  const char *buffer = json_object_to_json_string(host_map);
  memcpy(mm_ptr, buffer, strlen(buffer));
  //memcpy(mm_ptr, host_map, strlen(buffer)*sizeof(char));

  if (sem_post (mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));  

 out:
  if(mutex_sem)
    sem_close(mutex_sem);

  //gettimeofday(&te, NULL); 
  //printf("time for cpy %f\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000. );


}

void get_shm_map() {

  sem_t *mutex_sem = NULL;
  json_object *server_map = NULL;
  struct timeval ts,te;
  gettimeofday(&ts, NULL); 

  //  mutual exclusion semaphore, mutex_sem with an initial value 0.  
  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, 0)) == SEM_FAILED) {
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));
    goto out;
  }

  sem_wait(mutex_sem);

  enum json_tokener_error error = json_tokener_success;
    
  server_map = json_tokener_parse_verbose((char *)mm_ptr, &error);
  gettimeofday(&te, NULL); 
  //printf("time for cpy %f\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000. );

  if (error != json_tokener_success) {
    fprintf(stderr, "mm_ptr parsing failed `%s': %s\n", json_object_to_json_string(server_map), 
	    json_tokener_error_desc(error));
    goto out;
  }

  if (sem_post(mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));
  json_object_put(host_map);
  host_map = json_object_get(server_map);

 out:
  if (server_map)
    json_object_put(server_map);
  if (mutex_sem)
    sem_close(mutex_sem);


}

