#ifndef _LOD_H_
#define _LOD_H_
#include "lfs_utils.h"


#define PROCFS_BUF_SIZE 4096
int collect_lod(struct device_info *info, char **buffer);

#endif
