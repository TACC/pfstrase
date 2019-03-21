#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"
#include "lfs_utils.h"

int collect_exports(struct device_info *info, char **buffer)
{
  int rc = -1;

  DIR *typedir = NULL;
  typedir = opendir(info->typepath);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", info->typepath);
    goto typedir_err;
  }

  char *tmp = *buffer;
  asprintf(buffer, "\"type\": \"%s\", \"host\": \"%s\", \"time\": %llu.%llu, \"dev\": [", info->type, info->hostname, info->time.tv_sec, info->time.tv_nsec);       
  if (tmp != NULL) free(tmp);

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {      
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;

    DIR *exportdir = NULL;
    char exportpath[256];
    snprintf(exportpath, sizeof(exportpath), 
	     "%s/%s/exports", info->typepath, typede->d_name);
   
    tmp = *buffer;
    asprintf(buffer, "%s{\"%s\", \"exports\": [", *buffer, typede->d_name);
    if (tmp != NULL) free(tmp);

    exportdir = opendir(exportpath); 
    if(exportdir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", exportpath);
      goto exportdir_err;
    }

    struct dirent *nidde;
    while ((nidde = readdir(exportdir)) != NULL) {
      if (nidde->d_type != DT_DIR || nidde->d_name[0] == '.')
	continue;

      char statspath[256];
      snprintf(statspath, sizeof(statspath), "%s/%s/stats", 
	       exportpath, nidde->d_name);

      tmp = *buffer;
      asprintf(buffer, "%s{\"nid\": \"%s\"", *buffer, nidde->d_name);
      if (tmp != NULL) free(tmp);
	
      if (collect_stats(statspath, buffer) < 0)
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", nidde->d_name, statspath);
      
      tmp = *buffer;
      asprintf(buffer, "%s},", *buffer);
      if (tmp != NULL) free(tmp);
    }
            
    char *p = *buffer;
    p = *buffer + strlen(*buffer) - 1;
    *p = ']';
  
  exportdir_err:
    if (exportdir != NULL)
      closedir(exportdir);  
      tmp = *buffer;
      asprintf(buffer, "%s},", *buffer);
      if (tmp != NULL) free(tmp);
    }
            
    char *p = *buffer;
    p = *buffer + strlen(*buffer) - 1;
    *p = ']';
   
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);   
  
  rc = 0;
  return rc;
}
