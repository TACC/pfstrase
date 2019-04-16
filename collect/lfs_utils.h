#ifndef _LFS_UTILS_H_
#define _LFS_UTILS_H_

#include <time.h>
#include "dict.h"

enum device_class{MDS, OSS, OSC};

struct device_info
{
  struct timespec time;
  char hostname[64];
  char nid[32];
  char jid[32];
  char user[32];
  char typepath[64];
  char class_str[16];
  enum device_class class;   
  struct dict nid_jid_dict;
};

struct device_info *get_dev_data();

#endif
