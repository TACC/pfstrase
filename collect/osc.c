#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"

//#define TYPEPATH "/sys/fs/lustre/osc"
#define TYPEPATH "/sys/kernel/debug/lustre/osc"

#define STATS		 \
    X(stats)

int collect_osc(char **buffer)
{
  int rc = -1;
  char localhost[64];
  gethostname(localhost, sizeof(localhost));
  
  struct timespec time;
  if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto typedir_err;
  }

  DIR *typedir = NULL;
  typedir = opendir(TYPEPATH);
  if(typedir == NULL) {
    fprintf(stderr, "cannot open `%s' : %m\n", TYPEPATH);
    goto typedir_err;
  }

  char *tmp = *buffer;
  asprintf(buffer, "\"type\": \"osc\", \"host\": \"%s\", \"time\": %llu.%llu, \"osts\": [",
	   localhost, time.tv_sec, time.tv_nsec);       
  if (tmp != NULL) free(tmp);

  struct dirent *typede;
  while ((typede = readdir(typedir)) != NULL) {  
    if (typede->d_type != DT_DIR || typede->d_name[0] == '.')
      continue;

    char devpath[256];
    snprintf(devpath, sizeof(devpath), "%s/%s", TYPEPATH, typede->d_name);    

    if (strlen(typede->d_name) < 16) {
      fprintf(stderr, "invalid obd name `%s'\n", typede->d_name);
      continue;
    }
    char *p = typede->d_name + strlen(typede->d_name) - 20 - 1;
    *p = '\0'; 
    
    tmp = *buffer;
    asprintf(buffer, "%s{\"idx\": \"%s\"", *buffer, typede->d_name);
    if (tmp != NULL) free(tmp);
    
#define X(k,r...)							\
    ({									\
      char filepath[256];						\
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, #k);	\
      if (collect_stats(filepath, buffer) < 0)				\
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", #k, filepath); \
    })
    STATS;
#undef X
    
    tmp = *buffer;
    asprintf(buffer, "%s},", *buffer);
    if (tmp != NULL) free(tmp);
    
  }
  char *p = *buffer;
  p = *buffer + strlen(*buffer) - 1;
  *p = ']';


  rc = 0;
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);
  
  return rc;
}
/*
struct stats_type osc_stats_type = {
  .st_name = "osc",
  .st_collect = &osc_collect,
#define X SCHEMA_DEF
  .st_schema_def = JOIN(KEYS),
#undef X
};
*/
