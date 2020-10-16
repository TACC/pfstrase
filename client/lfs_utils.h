#ifndef _LFS_UTILS_H_
#define _LFS_UTILS_H_

#include <time.h>
#include <json/json.h>
#include "cpuid.h"
#include "intel_skx_imc.h"

enum device_class{MDS, OSS, OSC};

struct device_info
{
  struct timespec time;
  char hostname[64];
  char nid[32];
  char llite_path[128];
  char osc_path[128];
  char oss_nid_path[128];
  char class_str[16];
  enum device_class class;   
  int nr_cpus;
  int n_pmcs;
  processor_t processor;
};

struct device_info *get_dev_data();

#endif
