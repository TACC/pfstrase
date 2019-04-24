#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lfs_utils.h"
#include "exports.h"
#include "lod.h"
#include "osc.h"
#include "llite.h"
#include "sysinfo.h"
#include "collect.h"

#define PROCFS_BUF_SIZE 4096

void collect_devices(json_object *jobj)
{
  struct device_info *info = get_dev_data();

  // Get  time
  if (clock_gettime(CLOCK_REALTIME, &info->time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
  }
 
  json_object_object_add(jobj, "host", json_object_new_string(info->hostname));
  json_object_object_add(jobj, "obdclass", json_object_new_string(info->class_str));
  json_object_object_add(jobj, "nid", json_object_new_string(info->nid));
  json_object_object_add(jobj, "jid", json_object_new_string(info->jid));
  json_object_object_add(jobj, "user", json_object_new_string(info->user));
  json_object_object_add(jobj, "time", json_object_new_double(info->time.tv_sec + 1e-9*info->time.tv_nsec));

  json_object *type_json = json_object_new_object();
  // Exports
  if (info->class == MDS || info->class == OSS)
    if (collect_exports(type_json) < 0)
      fprintf(stderr, "export collection failed\n");

  // LOD
  if (info->class == MDS)
    if (collect_lod(type_json) < 0)
      fprintf(stderr, "lod collection failed\n");

  
  if (info->class == OSC) {
    // LLITE
    if (collect_llite(type_json) < 0)
      fprintf(stderr, "llite collection failed\n");
    // OSC
    if (collect_osc(type_json) < 0)
      fprintf(stderr, "osc collection failed\n");
  }
 
  // SYSINFO  
  if (collect_sysinfo(type_json) < 0)
    fprintf(stderr, "sysinfo collection failed\n");

  json_object_object_add(jobj, "stats", type_json);
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
    
    int n = sscanf(line, "%llu samples %*s %*u %*u %llu", &count, &sum);
    if (n == 1)
      val = count;
    if (n == 2)
      val = sum;
    json_object_object_add(stats, key, json_object_new_int(val)); 
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
  json_object_object_add(stats, key, json_object_new_int(val));
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
