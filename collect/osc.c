#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"
#include "osc.h"

#define STATEPATH "/sys/fs/lustre/osc"
//#define STATSPATH "/proc/fs/lustre/osc"
#define STATSPATH "/sys/kernel/debug/lustre/osc"

#define STATS		 \
    X(stats)

int collect_osc(json_object *jarray)
{
  int rc = -1;

  DIR *typedir = NULL;
  typedir = opendir(STATEPATH);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", STATEPATH);
    goto typedir_err;
  }

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {  
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;

    char osspath[256];
    snprintf(osspath, sizeof(osspath), "%s/%s/ost_conn_uuid", STATEPATH, typede->d_name);    

    char devpath[256];
    snprintf(devpath, sizeof(devpath), "%s/%s", STATSPATH, typede->d_name);    

    if (strlen(typede->d_name) < 16) {
      fprintf(stderr, "invalid obd name `%s'\n", typede->d_name);
      continue;
    }
    char *p = typede->d_name + strlen(typede->d_name) - 20 - 1;
    *p = '\0'; 

    json_object *tags_json = json_object_new_object();
    json_object_object_add(tags_json, "stats_type", json_object_new_string("osc"));
    json_object_object_add(tags_json, "target", json_object_new_string(typede->d_name));
    if (collect_string(osspath, tags_json, "server_nid") < 0)			       
      fprintf(stderr, "cannot read `%s' from `%s': %m\n", "ost_conn_uuid", osspath);

    json_object *stats_json = json_object_new_object();

#define X(k,r...)							\
    ({									\
      char filepath[256];						\
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, #k);	\
      if (collect_stats(filepath, stats_json) < 0)			\
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", #k, filepath); \
    })
    STATS;
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
