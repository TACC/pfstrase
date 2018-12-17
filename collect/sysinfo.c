#include <stdlib.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <stdio.h>

float f_load = 1.f/(1 << SI_LOAD_SHIFT);

int collect_sysinfo(char **buffer)
{
  int rc = -1;
  
  char localhost[64];
  gethostname(localhost, sizeof(localhost));

  struct timespec time;
  if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto _err;
  }

  struct sysinfo info;
  if (sysinfo(&info) < 0) {
    fprintf(stderr, "cannot call sysinfo : %m\n");
    goto _err;
  }

  char *tmp = *buffer;
  if (asprintf(buffer, "\"type\": \"sysinfo\", \"host\": \"%s\", \"time\": %llu.%llu, \"load average\": {\"1m\": %.2f, \"5m\": %.2f, \"15m\": %.2f \"freeram\": %lu, \"bufferram\": %lu, \"totalram\": %lu }", 
	       localhost, time.tv_sec, time.tv_nsec, 
	       f_load*info.loads[0], f_load*info.loads[1], f_load*info.loads[2],
	       info.freeram*info.mem_unit, info.bufferram*info.mem_unit, info.totalram*info.mem_unit) < 0 ) {
    rc = -1;
    fprintf(stderr, "Write to buffer failed for sysinfo\n'");
    goto _err;
  }
  if (tmp != NULL) free(tmp);
  
  rc = 0;
 _err:
  return rc;

}
