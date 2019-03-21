#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"
#include "lod.h"

#define TYPEPATH "/proc/fs/lustre/lod"
#define PROCFS_BUF_SIZE 4096

#define SINGLE	       \
  X(activeobd),	       \
    X(blocksize),      \
    X(dom_stripesize), \
    X(filesfree),      \
    X(filestotal),     \
    X(kbytesavail),    \
    X(kbytesfree),     \
    X(kbytestotal),    \
    X(lmv_failout),    \
    X(numobd),	       \
    X(stripecount),    \
    X(stripesize),     \
    X(stripetype)

int collect_lod(struct device_info *info, char **buffer)
{
  int rc = -1;

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
    char *tmp = *buffer;
    asprintf(buffer, "\"type\": \"lod\", \"dev\": \"%s\", \"host\": \"%s\", \"time\": %llu.%llu",
	     typede->d_name, info->hostname, info->time.tv_sec, info->time.tv_nsec);       
    if (tmp != NULL) free(tmp);

#define X(k,r...)						           \
    ({								           \
      char filepath[256];						   \
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, #k);	   \
      if (collect_single(filepath, buffer, #k) < 0)			   \
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", #k, filepath); \
    })
    SINGLE;
#undef X
  }

  rc = 0;
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);
  
  return rc;
}
