#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define PROCFS_BUF_SIZE 4096

static const char *typepath[] = { 
  [0] = "/proc/fs/lustre/mdt",
  [1] = "/proc/fs/lustre/obdfilter",
};

int collect_exports(char *stats_buffer, int size_stats_buffer)
{
  int rc = -1;
  int pos = 0;

  char localhost[64];
  gethostname(localhost, sizeof(localhost));

  size_t i;
  for (i = 0; i < sizeof(typepath)/sizeof(typepath[0]); i++) {
    DIR *typedir = NULL;
    typedir = opendir(typepath[i]);
    if(typedir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", typepath);
      goto typedir_err;
    }

    struct dirent *typede;
    while ((typede = readdir(typedir)) != NULL) {      
      if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
	continue;

      DIR *exportdir = NULL;
      char exportpath[256];
      snprintf(exportpath, sizeof(exportpath), 
	       "%s/%s/exports", typepath[i], typede->d_name);

      exportdir = opendir(exportpath); 
      if(exportdir == NULL) {
	fprintf(stderr, "cannot open `%s' : %m\n", exportpath);
	goto exportdir_err;
      }

      struct dirent *nidde;
      while ((nidde = readdir(exportdir)) != NULL) {
	char statspath[256];
	char procfs_buf[PROCFS_BUF_SIZE];
	FILE *fd = NULL;

	if (nidde->d_type != DT_DIR || nidde->d_name[0] == '.')
	  continue;
	snprintf(statspath, sizeof(statspath), "%s/%s/stats", 
		 exportpath, nidde->d_name);

	fd = fopen(statspath, "r");
	if (fd == NULL) {
	  fprintf(stderr, "cannot open %s: %m\n", statspath);
	  goto statpath_err;
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

	  pos += snprintf(stats_buffer + pos, size_stats_buffer, 
			  "host: %s time: %llu.%llu nid: %s key: %s val: %llu\n", 
			  localhost, secs, nsecs, nidde->d_name, key, value);	
	}
	if (line_buf != NULL) 
	  free(line_buf);
      statpath_err:
	if (fd != NULL)
	  fclose(fd);  	
      }      
    exportdir_err:
      if (exportdir != NULL)
	closedir(exportdir);  
    }   
  typedir_err:
    if (typedir != NULL)
      closedir(typedir);
  }    
  rc = 0;
  return rc;
}
