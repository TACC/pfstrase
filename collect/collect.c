#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PROCFS_BUF_SIZE 4096

int collect_stats(const char *path, char **buffer)
{
  int rc = 0;

  FILE *fd = NULL;
  fd = fopen(path, "r");
  if (fd == NULL) {
    fprintf(stderr, "cannot open %s: %m\n", path);
    rc = -1;
    goto statpath_err;
  }

  char procfs_buf[PROCFS_BUF_SIZE];
  setvbuf(fd, procfs_buf, _IOFBF, sizeof(procfs_buf));
  char *line_buf = NULL;
  size_t line_buf_size = 0;    
  unsigned long long count = 0, sum = 0, val = 0;
  while(getline(&line_buf, &line_buf_size, fd) >= 0) {
    char *line = line_buf;
    char *key = strsep(&line, " \t\n\v\f\r");	
    if (key == NULL || line == NULL)
      continue;
    if (strcmp(key, "snapshot_time") == 0) {
      //sscanf(line, "%llu.%llu secs.nsecs", &secs, &nsecs);
      continue;
    }
    
    int n = sscanf(line, "%llu samples %*s %*u %*u %llu", &count, &sum);
    if (n == 1)
      val = count;
    if (n == 2)
      val = sum;
    char *tmp = *buffer;
    if (asprintf(buffer, "%s key: %s val: %llu", *buffer, key, val) < 0) {
      rc = -1;
      fprintf(stderr, "Write to buffer failed for %s `%s\n'", path, key);
    }               
    if (tmp != NULL) free(tmp);
  }
  if (line_buf != NULL) 
    free(line_buf);
  
 statpath_err:
  if (fd != NULL)
    fclose(fd);  	
  return rc;
}      

int collect_single(const char *filepath, char **buffer, char *key)
{
  int rc = 0;

  FILE *fd = NULL;
  fd = fopen(filepath, "r");
  if (fd == NULL) {
    fprintf(stderr, "cannot open %s: %m\n", filepath);
    rc = -1;
   goto devde_err;
  }

  char procfs_buf[PROCFS_BUF_SIZE];
  setvbuf(fd, procfs_buf, _IOFBF, sizeof(procfs_buf));
  unsigned long long val;
  if (fscanf(fd, "%llu", &val) == 1) {
    rc = -1;
  }      
  char *tmp = *buffer;
  if (asprintf(buffer, "%s key: %s val: %llu", *buffer, key, val) < 0 ) {
    rc = -1;
    fprintf(stderr, "Write to buffer failed for `%s\n'", key);
  }
  if (tmp != NULL) free(tmp);
 devde_err:
  if (fd != NULL)
    fclose(fd);  	  
  return rc;
}
