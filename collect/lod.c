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

int collect_lod(json_object *jarray)
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

    json_object *tags_json = json_object_new_object();
    json_object_object_add(tags_json, "stats_type", json_object_new_string("lod"));
    json_object_object_add(tags_json, "target", json_object_new_string(typede->d_name));

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
    if (json_object_object_length(stats_json) > 0) {
      json_object_object_add(tags_json, "stats", stats_json);
      json_object_array_add(jarray, tags_json); 
    }
    else {
      json_object_put(stats_json);
      json_object_put(tags_json);
    }

  }

  rc = 0;  
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);

  return rc;
}
