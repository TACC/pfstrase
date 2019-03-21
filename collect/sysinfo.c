#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <stdio.h>
#include "sysinfo.h"

float f_load = 1.f/(1 << SI_LOAD_SHIFT);

int collect_sysinfo(struct device_info *info, char **buffer)
{
  int rc = -1;
  
  struct sysinfo sinfo;
  if (sysinfo(&sinfo) < 0) {
    fprintf(stderr, "cannot call sysinfo : %m\n");
    goto _err;
  }

  char *tmp = *buffer;
  if (asprintf(buffer, "\"type\": \"sysinfo\", \"host\": \"%s\", \"time\": %llu.%llu, \"load average\": {\"1m\": %.2f, \"5m\": %.2f, \"15m\": %.2f, \"freeram\": %lu, \"bufferram\": %lu, \"totalram\": %lu }", 
	       info->hostname, info->time.tv_sec, info->time.tv_nsec, 
	       f_load*sinfo.loads[0], f_load*sinfo.loads[1], f_load*sinfo.loads[2],
	       sinfo.freeram*sinfo.mem_unit, sinfo.bufferram*sinfo.mem_unit, sinfo.totalram*sinfo.mem_unit) < 0 ) {
    rc = -1;
    fprintf(stderr, "Write to buffer failed for sysinfo\n'");
    goto _err;
  }
  if (tmp != NULL) free(tmp);
  
  rc = 0;
 _err:
  return rc;

}
