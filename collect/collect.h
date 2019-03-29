#ifndef _COLLECT_H_
#define _COLLECT_H_
#include "lfs_utils.h"
void collect_devices(struct device_info *info, char **buffer);
int collect_stats(const char *path, char **buffer);
int collect_single(const char *path, char **buffer, char *key);
int collect_string(const char *path, char **buffer, char *key);

#endif
