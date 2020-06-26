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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "stats.h"
#include "shmmap.h"

#define mm_size 100*1024*1024

sem_t *mutex_sem = NULL;
int fd_shm = NULL;
caddr_t *mm_ptr = NULL;

void shmmap_server_init(void) {
  umask(S_IXGRP | S_IXOTH);
  sem_unlink(SEM_MUTEX_NAME);
  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, O_CREAT, 0666, 0)) == SEM_FAILED) {
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));
    exit(1);
  }
  if ((fd_shm = shm_open (SERVER_TAG_RATE_MAP_FILE, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1) {
    fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
    exit(1);
  }
  if (ftruncate (fd_shm, mm_size) == -1) {
    fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));  
    exit(1);
  }
  if ((mm_ptr = mmap (NULL, mm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %s\n", strerror(errno));
    exit(1);
  }
  if(mlock(mm_ptr, mm_size) != 0){
    fprintf(stderr, "mlock failure");
    exit(1);
  }
  if (sem_post (mutex_sem) == -1) {
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno)); 
    exit(1);
  }
}

void shmmap_server_kill(void) {
  if (munmap (mm_ptr, mm_size) == -1)
    fprintf(stderr, "munmap failed: %s\n", strerror(errno));
  if (fd_shm)
    close(fd_shm);
  if(mutex_sem)
    sem_close(mutex_sem);
  sem_unlink(SEM_MUTEX_NAME);
  shm_unlink(SERVER_TAG_RATE_MAP_FILE);
}

void shmmap_client_init(void) {
  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, 0)) == SEM_FAILED) {
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));
    exit(1);
  }
  if ((fd_shm = shm_open (SERVER_TAG_RATE_MAP_FILE, O_RDWR, 0)) == -1) {
    fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
    exit(1);
  }
  if ((mm_ptr = mmap (NULL, mm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %s\n", strerror(errno));
    exit(1);
  }  
}

void shmmap_client_kill(void) {
  sem_wait(mutex_sem);
  if (sem_post(mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));

  if (munmap (mm_ptr, mm_size) == -1)
    fprintf(stderr, "munmap failed: %s\n", strerror(errno));
  if (fd_shm)
    close(fd_shm);
}


void set_shm_map() {

  sem_wait(mutex_sem);
  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 

  tag_stats();    
  group_statsbytags(5, "fid", "server", "client", "jid", "uid");
  
  const char *buffer = json_object_to_json_string(server_tag_rate_map);
  //printf(buffer);
  memcpy(mm_ptr, buffer, strlen(buffer));

  if (sem_post (mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));  

  //gettimeofday(&te, NULL);   
  //printf("time for cpy %f\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000. );
}

void get_shm_map() {

  json_object *server_map = NULL;
  int val;

  sem_wait(mutex_sem);

  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 
  
  enum json_tokener_error error = json_tokener_success;    
  server_map = json_tokener_parse_verbose((char *)mm_ptr, &error);
  if (error != json_tokener_success) {
    //fprintf(stderr, "mm_ptr parsing failed `%s': %s\n", json_object_to_json_string(server_map), 
    //	    json_tokener_error_desc(error));
    goto out;
  }
  json_object_put(server_tag_rate_map);
  server_tag_rate_map = json_object_get(server_map);

  //host_map = (json_object *)mm_ptr;
  //printf("here %s\n", json_object_to_json_string(host_map));
  if (sem_post(mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));

  //gettimeofday(&te, NULL);   
  //printf("time for cpy %f\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000. );


 out:
  if (server_map)
    json_object_put(server_map);
}

