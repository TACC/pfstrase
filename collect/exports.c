#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define PROCFS_BUF_SIZE 4096
const char *type_path = "/proc/fs/lustre/mdt";

int collect_exports(char *stats_buffer, int size_stats_buffer)
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

 type_dir_err:
  if (type_dir != NULL)
    closedir(type_dir);

  return rc;
}
