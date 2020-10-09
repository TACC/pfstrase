#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include "collect.h"
#include "lfs_utils.h"
#include "intel_pmc3.h"

json_object *events_array;
uint64_t *events;  
char **events_str;

//! Configure and start counters for a pmc3 cpu counters
static int intel_pmc3_begin_cpu(char *cpu)
{
  int rc = -1;
  char msr_path[80];
  int msr_fd = -1;
  uint64_t global_ctr_ctrl, fixed_ctr_ctrl;
  
  events_array = json_object_new_array();
  
  snprintf(msr_path, sizeof(msr_path), "/dev/cpu/%s/msr", cpu);
  msr_fd = open(msr_path, O_RDWR);
  if (msr_fd < 0) {
    fprintf(stderr, "cannot open `%s': %m\n", msr_path);
    goto out;
  }
  /* Disable counters globally. */
  global_ctr_ctrl = 0x0ULL;
  if (pwrite(msr_fd, &global_ctr_ctrl, sizeof(global_ctr_ctrl), IA32_PERF_GLOBAL_CTRL) < 0) {
    fprintf(stderr, "cannot disable performance counters: %m\n");
    goto out;
  }
  int i;
  for (i = 0; i < get_dev_data()->n_pmcs; i++) {    
    json_object *eid = json_object_new_object();    
    json_object_object_add(eid, events_str[i], json_object_new_int64(events[i]));
    json_object_array_add(events_array, eid);
    if (pwrite(msr_fd, &events[i], sizeof(events[i]), IA32_CTL0 + i) < 0) {
      fprintf(stderr, "cannot write event %016llX to MSR %08X through `%s': %m\n",
            (unsigned long long) events[i],
            (unsigned) IA32_CTL0 + i,
            msr_path);
      goto out;
    }
  }
  
  rc = 0;
  /* Enable fixed counters.  Three 4 bit blocks, enable OS, User, Turn off any thread. */
  fixed_ctr_ctrl = 0x333UL;

  if (pwrite(msr_fd, &fixed_ctr_ctrl, sizeof(fixed_ctr_ctrl), IA32_FIXED_CTR_CTRL) < 0)
    fprintf(stderr, "cannot enable fixed counters: %m\n");

  /* Enable counters globally, n_pmcs PMC and 3 fixed. */
  global_ctr_ctrl = BIT_MASK(get_dev_data()->n_pmcs) | (0x7ULL << 32);
  if (pwrite(msr_fd, &global_ctr_ctrl, sizeof(global_ctr_ctrl), IA32_PERF_GLOBAL_CTRL) < 0)
    fprintf(stderr, "cannot enable performance counters: %m\n");

 out:
  if (msr_fd >= 0)
    close(msr_fd);

  return rc;
}

//! Collect values in counters for cpu
static void intel_pmc3_collect_cpu(json_object *core_stats, char *cpu)
{
  char msr_path[80];
  int msr_fd = -1;

  snprintf(msr_path, sizeof(msr_path), "/dev/cpu/%s/msr", cpu);
  msr_fd = open(msr_path, O_RDONLY);
  if (msr_fd < 0) {
    fprintf(stderr, "cannot open `%s': %m\n", msr_path);
    goto out;
  }

  int i;
  uint64_t ev, val;
  for (i = 0; i < get_dev_data()->n_pmcs; i++) {
    if (pread(msr_fd, &ev, sizeof(ev), IA32_CTL0 + i) && 
	pread(msr_fd, &val, sizeof(val), IA32_CTR0 + i)) {      
      json_object *eid = json_object_array_get_idx(events_array, i);
      json_object_object_foreach(eid, key, ctl) {
	if (json_object_get_int64(ctl) == ev)
	json_object_object_add(core_stats, key, json_object_new_int64(val));
      else
	json_object_object_add(core_stats, "ERROR", json_object_new_int64(val));
      }
    }
  }

  if (pread(msr_fd, &val, sizeof(val), IA32_FIXED_CTR0))
    json_object_object_add(core_stats, "instructions_retired", json_object_new_int64(val));
  if (pread(msr_fd, &val, sizeof(val), IA32_FIXED_CTR1))
    json_object_object_add(core_stats, "core_cycles", json_object_new_int64(val));
  if (pread(msr_fd, &val, sizeof(val), IA32_FIXED_CTR2))
    json_object_object_add(core_stats, "ref_cycles", json_object_new_int64(val));

 out:
    if (msr_fd >= 0)
      close(msr_fd);
}

int begin_intel_pmc3()
{
  switch(get_dev_data()->processor) {
  case NEHALEM:
    events = nhm_events; break;
  case WESTMERE:
    events = nhm_events; break;
  case SANDYBRIDGE:
    events = snb_events; break;
  case IVYBRIDGE:
    events = snb_events; break;
  case HASWELL:
    events = hsw_events; break;
  case BROADWELL:
    events = hsw_events; break;
  case KNL:
    events = knl_events; break;
  case SKYLAKE:
    events = skx_events; 
    events_str = skx_events_str; 
    break;
  default:
    fprintf(stderr, "Processor model/family not supported: %m\n");
    goto out;
  }
 
  int nr = 0;
  int i;
  for (i = 0; i < get_dev_data()->nr_cpus; i++) {
    char cpu[80];
    snprintf(cpu, sizeof(cpu), "%d", i);    
    if (intel_pmc3_begin_cpu(cpu) == 0)
      nr++;
  }  
 out:
  return nr > 0 ? 0 : -1;
}

int collect_intel_pmc3(json_object *jarray)
{
  int rc = -1;

  int i;
  for (i = 0; i < get_dev_data()->nr_cpus; i++) {    
    char cpu[80];
    snprintf(cpu, sizeof(cpu), "%d", i);

    json_object *tags_json = json_object_new_object();
    json_object_object_add(tags_json, "stats_type", json_object_new_string("intel_pmc3"));
    json_object_object_add(tags_json, "device", json_object_new_string(cpu));

    json_object *stats_json = json_object_new_object();
    intel_pmc3_collect_cpu(stats_json, cpu);
    if (json_object_object_length(stats_json) > 0) {
      json_object_object_add(tags_json, "stats", stats_json);
      json_object_array_add(jarray, tags_json);
    }
    else {
      json_object_put(stats_json);
      json_object_put(tags_json);
    }
  }

  rc = 0;
 out:
  return rc;
}

