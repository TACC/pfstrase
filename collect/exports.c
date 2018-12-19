#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"

static const char *type[] = { 
  [0] = "mdt",
  [1] = "obdfilter",
};

#define basepath "/proc/fs/lustre"

int collect_exports(char **buffer)
{
  int rc = -1;

  char localhost[64];
  gethostname(localhost, sizeof(localhost));

  struct timespec time;
  if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto typedir_err;
  }

  size_t i;
  for (i = 0; i < sizeof(type)/sizeof(type[0]); i++) {
    char typepath[256];
    snprintf(typepath, sizeof(typepath), "%s/%s", basepath, type[i]);

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

      DIR *exportdir = NULL;
      char exportpath[256];
      snprintf(exportpath, sizeof(exportpath), 
	       "%s/%s/exports", typepath, typede->d_name);

      char *tmp = *buffer;
      asprintf(buffer, "\"type\": \"%s\", \"dev\": \"%s\", \"host\": \"%s\", \"time\": %llu.%llu, \"exports\": [",
	       type[i], typede->d_name, localhost, time.tv_sec, time.tv_nsec);       
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
    }   
  typedir_err:
    if (typedir != NULL)
      closedir(typedir);
  }    
  
  rc = 0;
  return rc;
}
