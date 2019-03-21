#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "collect.h"

//#define TYPEPATH "/sys/fs/lustre/llite"
#define TYPEPATH "/sys/kernel/debug/lustre/llite"

#define STATS		 \
  X(max_cached_mb),	 \
    X(read_ahead_stats), \
    X(stats),		 \
    X(statahead_stats),	 \
    X(unstable_stats)

#define SINGLE			   \
  X(max_read_ahead_whole_mb),	   \
    X(max_read_ahead_mb),	   \
    X(max_read_ahead_per_file_mb), \
    X(default_easize),		   \
    X(max_easize),		   \
    X(uuid),			   \
    X(statahead_agl),		   \
    X(statahead_max),		   \
    X(stat_blocksize),		   \
    X(kbytesavail),		   \
    X(kbytesfree),		   \
    X(kbytestotal),		   \
    X(fast_read),		   \
    X(filesfree),		   \
    X(filestotal),		   \
    X(lazystatfs)                                                                  
                  
int collect_llite(char **buffer)
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
    char *p = typede->d_name + strlen(typede->d_name) - 16 - 1;
    *p = '\0'; 
   
    char *tmp = *buffer;
    asprintf(buffer, "\"type\": \"llite\", \"dev\": \"%s\", \"host\": \"%s\", \"time\": %llu.%llu",
	     typede->d_name, localhost, time.tv_sec, time.tv_nsec);       
    if (tmp != NULL) free(tmp);
    
#define X(k,r...)							\
    ({									\
      char filepath[256];						\
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, #k); \
      if (collect_stats(filepath, buffer) < 0)				\
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", #k, filepath); \
    })
    STATS;
#undef X
#define X(k,r...)							\
    ({									\
      char filepath[256];						\
      snprintf(filepath, sizeof(filepath), "%s/%s", devpath, #k);	\
      if (collect_single(filepath, buffer, #k) < 0)			\
	fprintf(stderr, "cannot read `%s' from `%s': %m\n", #k, filepath); \
    })
    SINGLE;
#undef X
  }
  
  rc = 0;
 typedir_err:
  if (typedir != NULL)
    closedir(typedir);
  
  return rc;
}