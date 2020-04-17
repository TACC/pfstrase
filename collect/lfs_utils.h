#ifndef _LFS_UTILS_H_
#define _LFS_UTILS_H_

#include <time.h>
#include <json-c/json.h>

enum device_class{MDS, OSS, OSC};

struct device_info
{
  struct timespec time;
  char hostname[64];
  char nid[32];
  char jid[32];
  char uid[32];
  char llite_path[128];
  char osc_path[128];
  char oss_nid_path[128];
  char class_str[16];
  enum device_class class;   
};

struct device_info *get_dev_data();

#endif
