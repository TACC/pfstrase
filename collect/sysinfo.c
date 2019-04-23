#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <stdio.h>
#include "sysinfo.h"

float f_load = 1.f/(1 << SI_LOAD_SHIFT);

int collect_sysinfo(json_object *type_json)
{
  int rc = -1;
  
  struct sysinfo sinfo;
  if (sysinfo(&sinfo) < 0) {
    fprintf(stderr, "cannot call sysinfo : %m\n");
    goto _err;
  }

  json_object *stats = json_object_new_object();
  json_object *si = json_object_new_object();
  json_object_object_add(si, "loadavg1m", json_object_new_double(f_load*sinfo.loads[0]));
  json_object_object_add(si, "loadavg5m", json_object_new_double(f_load*sinfo.loads[1]));
  json_object_object_add(si, "loadavg15m", json_object_new_double(f_load*sinfo.loads[2]));
  json_object_object_add(si, "loadavg15m", json_object_new_double(f_load*sinfo.loads[2]));
  json_object_object_add(si, "freeram", json_object_new_double(sinfo.mem_unit*sinfo.freeram));
  json_object_object_add(si, "bufferram", json_object_new_double(sinfo.mem_unit*sinfo.bufferram));
  json_object_object_add(si, "totalram", json_object_new_double(sinfo.mem_unit*sinfo.totalram));

  json_object_object_add(type_json, "sysinfo", si);


  rc = 0;
 _err:
  return rc;

}
