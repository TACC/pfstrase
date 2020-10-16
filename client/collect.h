#ifndef _COLLECT_H_
#define _COLLECT_H_
#include <stdio.h>
#include <stdarg.h>

#include "lfs_utils.h"
#include "json/json.h"

void collect_devices(json_object *json);
int collect_stats(const char *path, json_object *stats);
int collect_single(const char *path, json_object *stats, char *key);
int collect_string(const char *path, json_object *stats, char *key);

__attribute__((format(scanf, 2, 3)))
  static inline int pscanf(const char *path, const char *fmt, ...)
{
  int rc = -1;
  FILE *file = NULL;
  char file_buf[4096];
  va_list arg_list;
  va_start(arg_list, fmt);

  file = fopen(path, "r");
  if (file == NULL)
    goto out;
  setvbuf(file, file_buf, _IOFBF, sizeof(file_buf));

  rc = vfscanf(file, fmt, arg_list);

 out:
  if (file != NULL)
    fclose(file);
  va_end(arg_list);
  return rc;
}

#endif
