#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"
#include "lod.h"

#define TYPEPATH "/sys/fs/lustre/lod"
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

int collect_lod(struct device_info *info)
{
  int rc = -1;

  DIR *typedir = NULL;
  typedir = opendir(TYPEPATH);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", TYPEPATH);
    goto typedir_err;
  }

  json_object *mdt = json_object_new_object();

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {  
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;

    char devpath[256];
    snprintf(devpath, sizeof(devpath), "%s/%s", TYPEPATH, typede->d_name);

    json_object *stats_json = json_object_new_object();
#define X(k,r...)						           \
    ({								           \
      char filepath[256];						   \
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, #k);	   \
      if (collect_single(filepath, stats_json, #k) < 0)			   \
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", #k, filepath); \
    })
    SINGLE;
#undef X
    json_object_object_add(mdt, typede->d_name, stats_json);
  }
  json_object_object_add(info->jobj, "lod", mdt);

  rc = 0;  
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);

  return rc;
}
