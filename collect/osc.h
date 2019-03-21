#ifndef _OSC_H_
#define _OSC_H_
#include "lfs_utils.h"

#define PROCFS_BUF_SIZE 4096
int collect_osc(struct device_info *info, char **buffer);

#endif
