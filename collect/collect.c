#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lfs_utils.h"
#include "exports.h"
#include "lod.h"
#include "osc.h"
#include "llite.h"
#include "sysinfo.h"


#define PROCFS_BUF_SIZE 4096

void collect_devices(char **buffer)
{
  struct device_info *info = get_dev_data();

  // Get  time
  if (clock_gettime(CLOCK_REALTIME, &info->time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
  }

  if (asprintf(buffer, "\"host\": \"%s\", \"obdclass\": \"%s\", \"nid\": \"%s\", \"jid\": \"%s\", \"user\": \"%s\", \"time\": %llu.%llu, \"stats\": {", info->hostname, info->class_str, info->nid, info->jid, info->user, info->time.tv_sec, info->time.tv_nsec) < 0) {
    fprintf(stderr, "Write to buffer failed for device info");
  }
  
  // Exports
  if (info->class == MDS || info->class == OSS)
    if (collect_exports(info, buffer) < 0)
      fprintf(stderr, "export collection failed\n");
  
  // LOD
  if (info->class == MDS)
    if (collect_lod(info, buffer) < 0)
      fprintf(stderr, "lod collection failed\n");
  
  if (info->class == OSC) {
    // LLITE
    if (collect_llite(info, buffer) < 0)
      fprintf(stderr, "llite collection failed\n");
    // OSC
    if (collect_osc(info, buffer) < 0)
      fprintf(stderr, "osc collection failed\n");
  }

  // SYSINFO  
  if (collect_sysinfo(info, buffer) < 0)
    fprintf(stderr, "sysinfo collection failed\n");

  char *tmp = *buffer;
  asprintf(buffer, "%s}", *buffer);
  if (tmp != NULL) free(tmp);
}

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
    char *key = strsep(&line, " :\t\n\v\f\r");	
    if (key == NULL || line == NULL)
      continue;
    if (strcmp(key, "snapshot_time") == 0) {
      continue;
    }
    
    int n = sscanf(line, "%llu samples %*s %*u %*u %llu", &count, &sum);
    if (n == 1)
      val = count;
    if (n == 2)
      val = sum;
    char *tmp = *buffer;
    if (asprintf(buffer, "%s\"%s\": %llu,", *buffer, key, val) < 0) {
      rc = -1;
      fprintf(stderr, "Write to buffer failed for %s `%s\n'", path, key);
    }               
    if (tmp != NULL) free(tmp);
  }
  if (line_buf != NULL) 
    free(line_buf);

  char *p = *buffer + strlen(*buffer) - 1;
  if (*p == ',') *p = '\0';

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
  if (fscanf(fd, "%llu", &val) != 1) {
    rc = -1;
    goto devde_err;
  }      
  char *tmp = *buffer;
  if (asprintf(buffer, "%s\"%s\": %llu,", *buffer, key, val) < 0 ) {
    rc = -1;
    fprintf(stderr, "Write to buffer failed for `%s\n'", key);
  }
  if (tmp != NULL) free(tmp);
 devde_err:
  if (fd != NULL)
    fclose(fd);  	  
  return rc;
}

int collect_string(const char *filepath, char **buffer, char *key)
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
  char val[32];
  if (fscanf(fd, "%s", val) != 1) {
    rc = -1;
    goto devde_err;
  }      
  char *tmp = *buffer;
  if (asprintf(buffer, "%s\"%s\": \"%s\",", *buffer, key, val) < 0 ) {
    rc = -1;
    fprintf(stderr, "Write to buffer failed for `%s\n'", key);
  }
  if (tmp != NULL) free(tmp);
 devde_err:
  if (fd != NULL)
    fclose(fd);  	  
  return rc;
}
