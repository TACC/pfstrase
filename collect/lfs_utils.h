#ifndef _LFS_UTILS_H_
#define _LFS_UTILS_H_

#include <time.h>

enum device_class{MDS, OSS, OSC};

struct device_info
{
  struct timespec time;
  char hostname[64];
  char typepath[64];
  char type[16];
  enum device_class class;   
};

int devices_discover();

#endif
