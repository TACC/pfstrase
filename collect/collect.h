#ifndef _COLLECT_H_
#define _COLLECT_H_
#include "lfs_utils.h"
#include "json/json.h"

void collect_devices();
int collect_stats(const char *path, json_object *stats);
int collect_single(const char *path, json_object *stats, char *key);
int collect_string(const char *path, json_object *stats, char *key);

#endif
