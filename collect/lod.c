#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"

#define TYPEPATH "/proc/fs/lustre/lod"
#define PROCFS_BUF_SIZE 4096

int collect_lod(char **buffer)
{
  int rc = -1;

  char localhost[64];
  gethostname(localhost, sizeof(localhost));

  struct timespec time;
  if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto typedir_err;
  }

  asprintf(buffer, "type: lod host: %s time: %llu.%llu",
	   localhost, time.tv_sec, time.tv_nsec);       

  DIR *typedir = NULL;
  typedir = opendir(TYPEPATH);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", TYPEPATH);
    goto typedir_err;
  }

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {  
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;

    char devpath[256];
    snprintf(devpath, sizeof(devpath), "%s/%s", TYPEPATH, typede->d_name);

    DIR *devdir = NULL;
    devdir = opendir(devpath);
    if(devdir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", devpath);
      goto typedir_err;
    }

    struct dirent *devde;
    while ((devde = readdir(devdir)) != NULL) {  
      if (devde->d_type == DT_DIR || devde->d_name[0] == '.')
	continue;

      char filepath[256];
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, devde->d_name);
      if (collect_single(filepath, buffer, devde->d_name) < 0)
	continue;
    }
  }
  
  rc = 0;
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);
  
  return rc;
}
