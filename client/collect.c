#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include "lfs_utils.h"
#include "collect.h"
#include "cpu.h"
#include "exports.h"
#include "llite.h"
#include "target.h"
#include "sysinfo.h"

#define PROCFS_BUF_SIZE 4096

void collect_devices(json_object *jobj)
{
  struct device_info *info = get_dev_data();

  // Get  time
  if (clock_gettime(CLOCK_REALTIME, &info->time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
  }

  //json_object *tags_json = json_object_new_object();
  json_object_object_add(jobj, "time", json_object_new_double(info->time.tv_sec + \
								   1e-9*info->time.tv_nsec));
  json_object_object_add(jobj, "obdclass", json_object_new_string(info->class_str));
  json_object_object_add(jobj, "hostname", json_object_new_string(info->hostname));
  json_object_object_add(jobj, "nid", json_object_new_string(info->nid));

  if (info->class == OSC) {
    json_object_object_add(jobj, "jid", json_object_new_string(info->jid));
    json_object_object_add(jobj, "uid", json_object_new_string(info->uid));
  }

  //json_object_object_add(jobj, "tags", tags_json);  
  json_object *data_json = json_object_new_array();
  // SYSINFO    
  if (collect_sysinfo(data_json) < 0)
    fprintf(stderr, "sysinfo collection failed\n"); 
  // CPU
  if (collect_cpu(data_json) < 0)
    fprintf(stderr, "cpu collection failed\n");
 
  // Server exports and targets
  if (info->class == MDS || info->class == OSS) {
    if (collect_exports(data_json) < 0)
      fprintf(stderr, "export collection failed\n");
    if (collect_target(data_json) < 0)
      fprintf(stderr, "target collection failed\n");
  }

  // Client data
  if (info->class == OSC) {
    // LLITE
    if (collect_llite(data_json) < 0)
      fprintf(stderr, "llite collection failed\n");   
    // OSC
    if (collect_osc(data_json) < 0)
      fprintf(stderr, "osc collection failed\n");
  }
  json_object_object_add(jobj, "data", data_json);  
}

int collect_stats(const char *path, json_object *stats)
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
    
    char units[16];
    int n = sscanf(line, "%llu samples %s %*u %*u %llu", &count, units, &sum);
    if (n == 1 || n == 2) {
      json_object_object_add(stats, key, json_object_new_int64(count)); 
    }
    else if (n == 3) {
      json_object_object_add(stats, key, json_object_new_int64(count)); 
      char sum_key[32];
      snprintf(sum_key, sizeof(sum_key), "%s_%s", key, units);
      json_object_object_add(stats, sum_key, json_object_new_int64(sum)); 
    }
  }
  if (line_buf != NULL) 
    free(line_buf);

 statpath_err:
  if (fd != NULL)
    fclose(fd);  	
  return rc;
}      

int collect_single(const char *filepath, json_object *stats, char *key)
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
  json_object_object_add(stats, key, json_object_new_int64(val));
 devde_err:
  if (fd != NULL)
    fclose(fd);  	  
  return rc;
}

int collect_string(const char *filepath, json_object *stats, char *key)
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
  json_object_object_add(stats, key, json_object_new_string(val));
 devde_err:
  if (fd != NULL)
    fclose(fd);  	  
  return rc;
}
