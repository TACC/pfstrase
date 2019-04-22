#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <stdio.h>
#include "sysinfo.h"

float f_load = 1.f/(1 << SI_LOAD_SHIFT);

int collect_sysinfo(struct device_info *info)
{
  int rc = -1;
  
  struct sysinfo sinfo;
  if (sysinfo(&sinfo) < 0) {
    fprintf(stderr, "cannot call sysinfo : %m\n");
    goto _err;
  }

  json_object *stats = json_object_new_object();
  json_object_object_add(stats, "loadavg1m", json_object_new_double(f_load*sinfo.loads[0]));
  json_object_object_add(stats, "loadavg5m", json_object_new_double(f_load*sinfo.loads[1]));
  json_object_object_add(stats, "loadavg15m", json_object_new_double(f_load*sinfo.loads[2]));
  json_object_object_add(stats, "loadavg15m", json_object_new_double(f_load*sinfo.loads[2]));
  json_object_object_add(stats, "freeram", json_object_new_double(sinfo.mem_unit*sinfo.freeram));
  json_object_object_add(stats, "bufferram", json_object_new_double(sinfo.mem_unit*sinfo.bufferram));
  json_object_object_add(stats, "totalram", json_object_new_double(sinfo.mem_unit*sinfo.totalram));

  json_object_object_add(info->jobj, "sysinfo", stats);

  rc = 0;
 _err:
  return rc;

}
