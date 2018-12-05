#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

#define PROCFS_BUF_SIZE 4096
#define TYPEPATH "/proc/fs/lustre/lod"

int collect_lod(char *buffer, int size_buffer)
{
  DIR *typedir = NULL;
  int rc = -1;
  int pos = 0;

  char localhost[64];
  gethostname(localhost, sizeof(localhost));

  struct timespec time;
  if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto typedir_err;
  }

  typedir = opendir(TYPEPATH);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", TYPEPATH);
    goto typedir_err;
  }

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {  
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;
    DIR *devdir = NULL;
    char devpath[256];
    snprintf(devpath, sizeof(devpath), "%s/%s", TYPEPATH, typede->d_name);

    devdir = opendir(devpath);
    if(devdir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", devpath);
      goto typedir_err;
    }

    struct dirent *devde;
    while ((devde = readdir(devdir)) != NULL) {  
      if (devde->d_type == DT_DIR || devde->d_name[0] == '.')
	continue;
      FILE *fd = NULL;
      char filepath[256];
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, devde->d_name);

      fd = fopen(filepath, "r");
      if (fd == NULL) {
	fprintf(stderr, "cannot open %s: %m\n", filepath);
	goto devde_err;
      }
    
      char procfs_buf[PROCFS_BUF_SIZE];
      setvbuf(fd, procfs_buf, _IOFBF, sizeof(procfs_buf));
      
      char *line_buf = NULL;
      size_t line_buf_size = 0;    

      while(getline(&line_buf, &line_buf_size, fd) >= 0) {
	char *line = line_buf;
	pos += snprintf(buffer + pos, size_buffer, 
			"time: %llu.%llu host: %s key: %s val: %s",  
			time.tv_sec, time.tv_nsec, localhost, 
			devde->d_name, line);
      }
      if (line_buf != NULL) 
	free(line_buf);
    devde_err:
      if (fd != NULL)
	fclose(fd);  	
    }
  }

    rc = 0;
  typedir_err:
    if (typedir != NULL)
      closedir(typedir);

    return rc;
  }
