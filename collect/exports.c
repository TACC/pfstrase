#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"

static const char *typepath[] = { 
  [0] = "/proc/fs/lustre/mdt",
  [1] = "/proc/fs/lustre/obdfilter",
};

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
  printf("%s\n", "here");
  asprintf(buffer, "type: exports host: %s time: %llu.%llu",
	   localhost, time.tv_sec, time.tv_nsec);       

  size_t i;
  for (i = 0; i < sizeof(typepath)/sizeof(typepath[0]); i++) {
    DIR *typedir = NULL;
    typedir = opendir(typepath[i]);
    if(typedir == NULL) {
      fprintf(stderr, "cannot open `%s' : %m\n", typepath[i]);
      goto typedir_err;
    }

    struct dirent *typede;
    while ((typede = readdir(typedir)) != NULL) {      
      if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
	continue;

      DIR *exportdir = NULL;
      char exportpath[256];
      snprintf(exportpath, sizeof(exportpath), 
	       "%s/%s/exports", typepath[i], typede->d_name);

      exportdir = opendir(exportpath); 
      if(exportdir == NULL) {
	fprintf(stderr, "cannot open `%s' : %m\n", exportpath);
	goto exportdir_err;
      }

      struct dirent *nidde;
      while ((nidde = readdir(exportdir)) != NULL) {
	if (nidde->d_type != DT_DIR || nidde->d_name[0] == '.')
	  continue;

	FILE *fd = NULL;
	char statspath[256];
	snprintf(statspath, sizeof(statspath), "%s/%s/stats", exportpath, nidde->d_name);

	asprintf(buffer, "%s nid: %s", *buffer, nidde->d_name);
	if (collect_stats(statspath, buffer) < 0)
	  continue;

      statpath_err:
	if (fd != NULL)
	  fclose(fd);  	
      }      
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
