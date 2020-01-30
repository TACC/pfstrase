#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"
#include "lfs_utils.h"
#include "json/json_object.h"

int collect_exports(json_object *jarray)
{
  int rc = -1;

  char *type;
  char *typepath;
  if (get_dev_data()->class == MDS) {
    typepath = "/proc/fs/lustre/mdt";
    type = "mds";
  }
  if (get_dev_data()->class == OSS) {
    typepath = "/proc/fs/lustre/obdfilter";
    type = "oss";
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

    /* Storage Target */
    DIR *exportdir = NULL;
    char exportpath[256];
    snprintf(exportpath, sizeof(exportpath), 
	     "%s/%s/exports", typepath, typede->d_name);
   
    exportdir = opendir(exportpath); 
    if(exportdir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", exportpath);
      goto exportdir_err;
    }
    
    struct dirent *nidde;
    while ((nidde = readdir(exportdir)) != NULL) {
      if (nidde->d_type != DT_DIR || nidde->d_name[0] == '.')
	continue;

      // Client nid
      char statspath[256];
      snprintf(statspath, sizeof(statspath), "%s/%s/stats", 
	       exportpath, nidde->d_name);
      printf("%s\n",statspath);
      json_object *tags_json = json_object_new_object();
      json_object_object_add(tags_json, "stats_type", json_object_new_string(type));
      json_object_object_add(tags_json, "target", json_object_new_string(typede->d_name));
      json_object_object_add(tags_json, "client_nid", json_object_new_string(nidde->d_name));
      
      json_object *stats_json = json_object_new_object();

      if (collect_stats(statspath, stats_json) < 0)
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", nidde->d_name, statspath);
      if (json_object_object_length(stats_json) > 0) {
	json_object_object_add(tags_json, "stats", stats_json);
	json_object_array_add(jarray, tags_json); 
      }
      else {
	json_object_put(stats_json);
	json_object_put(tags_json);
      }

    }

  exportdir_err:
    if (exportdir != NULL)
      closedir(exportdir);  
  }

  rc = 0;  
  
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);   

  return rc;
}
