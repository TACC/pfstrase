#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <stdio.h>
#include "sysinfo.h"

float f_load = 1.f/(1 << SI_LOAD_SHIFT);

int collect_sysinfo(json_object *jarray)
{
  int rc = -1;
  
  struct sysinfo sinfo;

  if (sysinfo(&sinfo) < 0) {
    fprintf(stderr, "cannot call sysinfo : %m\n");
    goto _err;
  }

  json_object *tags_json = json_object_new_object();
  json_object_object_add(tags_json, "stats_type", json_object_new_string("sysinfo"));

  json_object *stats_json = json_object_new_object();
  json_object_object_add(stats_json, "loadavg1m", json_object_new_double(f_load*sinfo.loads[0]));
  json_object_object_add(stats_json, "loadavg5m", json_object_new_double(f_load*sinfo.loads[1]));
  json_object_object_add(stats_json, "loadavg15m", json_object_new_double(f_load*sinfo.loads[2]));
  json_object_object_add(stats_json, "loadavg15m", json_object_new_double(f_load*sinfo.loads[2]));
  json_object_object_add(stats_json, "freeram", json_object_new_int64(sinfo.mem_unit*sinfo.freeram));
  json_object_object_add(stats_json, "bufferram", json_object_new_int64(sinfo.mem_unit*sinfo.bufferram));
  json_object_object_add(stats_json, "totalram", json_object_new_int64(sinfo.mem_unit*sinfo.totalram));

  if (json_object_object_length(stats_json) > 0) {
    json_object_object_add(tags_json, "stats", stats_json);
    json_object_array_add(jarray, tags_json); 
  }
  else {
    json_object_put(stats_json);
    json_object_put(tags_json);
  }

  rc = 0;
 _err:
  return rc;

}
