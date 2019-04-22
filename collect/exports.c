#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"
#include "lfs_utils.h"

int collect_exports(struct device_info *info)
{
  int rc = -1;

  char *type;
  char *typepath;
  if (info->class == MDS) {
    typepath = "/proc/fs/lustre/mdt";
    type = "mdt";
  }
  if (info->class == OSS) {
    typepath = "/proc/fs/lustre/obdfilter";
    type = "obdfilter";
  }

  DIR *typedir = NULL;
  typedir = opendir(typepath);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", typepath);
    goto typedir_err;
  }
  json_object *type_json = json_object_new_object();

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {      
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;

    DIR *exportdir = NULL;
    char exportpath[256];
    snprintf(exportpath, sizeof(exportpath), 
	     "%s/%s/exports", typepath, typede->d_name);
   
    exportdir = opendir(exportpath); 
    if(exportdir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", exportpath);
      goto exportdir_err;
    }

    json_object *nid_json = json_object_new_object();
    struct dirent *nidde;
    while ((nidde = readdir(exportdir)) != NULL) {
      if (nidde->d_type != DT_DIR || nidde->d_name[0] == '.')
	continue;

      char statspath[256];
      snprintf(statspath, sizeof(statspath), "%s/%s/stats", 
	       exportpath, nidde->d_name);

      json_object *stats_json = json_object_new_object();
      if (collect_stats(statspath, stats_json) < 0)
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", nidde->d_name, statspath);
      json_object_object_add(nid_json, nidde->d_name, stats_json);      
    }
    json_object_object_add(type_json, "exports", nid_json);

  exportdir_err:
    if (exportdir != NULL)
      closedir(exportdir);  

  }

  json_object_object_add(info->jobj, type, type_json);
  rc = 0;  
  
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);   
  
  return rc;
}
