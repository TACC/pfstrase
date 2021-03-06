#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"
#include "target.h"

#define SINGLE	       \
    X(filesfree),      \
    X(filestotal),     \
    X(kbytesavail),    \
    X(kbytesfree),     \
    X(kbytestotal)

int collect_target(json_object *jarray)
{
  int rc = -1;

  char *type;
  char *typepath;
  typepath = "/proc/fs/lustre/osd-ldiskfs";
  if (get_dev_data()->class == MDS) {
    type = "mdt";
  }
  if (get_dev_data()->class == OSS) {
    type = "ost";
  }

  DIR *typedir = NULL;
  typedir = opendir(typepath);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", typepath);
    goto typedir_err;
  }

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {  
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;

    char devpath[256];
    snprintf(devpath, sizeof(devpath), "%s/%s", typepath, typede->d_name);

    json_object *tags_json = json_object_new_object();
    json_object_object_add(tags_json, "stats_type", json_object_new_string(type));
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
