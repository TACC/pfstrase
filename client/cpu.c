#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "cpu.h"

/* The /proc manpage says units are units of 1/sysconf(_SC_CLK_TCK)
   seconds.  sysconf(_SC_CLK_TCK) seems to always be 100. */

/* We ignore steal and guest. */

int collect_cpu(json_object *jarray)
{
  const char *path = "/proc/stat";
  int rc = -1;

  FILE *file = NULL;
  char file_buf[4096];
  char *line = NULL;
  size_t line_size = 0;

  file = fopen(path, "r");
  if (file == NULL) {
    fprintf(stderr, "cannot open `%s': %m\n", path);
    goto out;
  }
  setvbuf(file, file_buf, _IOFBF, sizeof(file_buf));

  unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq= 0, softirq = 0;
  while (getline(&line, &line_size, file) >= 0) {
    char *rest = line;
    if (strncmp(rest, "cpu ", 4) != 0)
      continue;

    char *cpu = strsep(&rest, " :\t\n\v\f\r");
    if (cpu == NULL || rest == NULL)
      continue;

    int n = sscanf(rest, "%llu %llu %llu %llu %llu %llu %llu %*llu %*llu %*llu", 
		   &user, &nice, &system, &idle, &iowait, &irq, &softirq);
    break;
  }

  json_object *tags_json = json_object_new_object();
  json_object_object_add(tags_json, "stats_type", json_object_new_string("cpu"));

  json_object *stats_json = json_object_new_object();
  json_object_object_add(stats_json, "user", json_object_new_int64(user));
  json_object_object_add(stats_json, "nice", json_object_new_int64(nice));
  json_object_object_add(stats_json, "system", json_object_new_int64(system));
  json_object_object_add(stats_json, "idle", json_object_new_int64(idle));
  json_object_object_add(stats_json, "iowait", json_object_new_int64(iowait));
  json_object_object_add(stats_json, "irq", json_object_new_int64(irq));
  json_object_object_add(stats_json, "softirq", json_object_new_int64(softirq));
  
  if (json_object_object_length(stats_json) > 0) {
    json_object_object_add(tags_json, "stats", stats_json);
    json_object_array_add(jarray, tags_json); 
  }
  else {
    json_object_put(stats_json);
    json_object_put(tags_json);
  }

  rc = 0;
 out:
  if (line != NULL)
    free(line);
  if (file != NULL)
    fclose(file);
  return rc;
}

