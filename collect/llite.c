#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"

#define TYPEPATH "/proc/fs/lustre/llite"

int collect_llite(char **buffer)
{
  int rc = -1;
  char localhost[64];
  gethostname(localhost, sizeof(localhost));

  struct timespec time;
  if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto typedir_err;
  }

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
    DIR *devdir = NULL;
    char devpath[256];
    snprintf(devpath, sizeof(devpath), "%s/%s", TYPEPATH, typede->d_name);

    asprintf(buffer, "type: llite dev: %s host: %s time: %llu.%llu",
	     typede->d_name, localhost, time.tv_sec, time.tv_nsec);       

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
      if (strcmp(devde->d_name, "stats") == 0) {
	if (collect_stats(filepath, buffer) < 0)
	  continue;
      }
      else {
	if(collect_single(filepath, buffer, devde->d_name) < 0)
	  continue;
      }
    }
  }

  rc = 0;
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);
  
  return rc;
}
