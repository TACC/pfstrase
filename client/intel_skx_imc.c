/*! 
 \file intel_skx_imc.c
 \author Todd Evans 
 \brief Performance Monitoring Counters for Intel Knights Landing DRAM IMC
*/
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "pci.h"
#include "collect.h"
#include "lfs_utils.h"
#include "intel_skx_imc.h"

static uint32_t imc_dclk_dids[] = {0x2042, 0x2046, 0x204a};
const char *path = "/dev/mem";
const uint64_t mmconfig_base = 0x80000000;
const uint64_t mmconfig_size = 0x10000000;

static json_object *events_array;
static uint32_t events[] = {
  CAS_READS, CAS_WRITES, ACT_COUNT, PRE_COUNT_MISS,
};
static char *events_str[] = {
  "CAS_READS", "CAS_WRITES", "ACT_COUNT", "PRE_COUNT_MISS",
};

static int intel_skx_imc_begin_dev(uint32_t bus, uint32_t dev, uint32_t fun, uint32_t *map_dev, int nr_events)
{
  int i;
  uint32_t ctl  = 0x0UL;
  size_t n = 4;

  char msr_path[80];
  int msr_fd = -1;
  uint64_t global_ctr_ctrl;
  uint32_t local_ctr_ctrl;
  uint32_t pci = pci_cfg_address(bus, dev, fun);

  events_array = json_object_new_array();

  snprintf(msr_path, sizeof(msr_path), "/dev/cpu/0/msr");
  msr_fd = open(msr_path, O_RDWR);
  if (msr_fd < 0) {
    fprintf(stderr, "cannot open `%s': %m\n", msr_path);
    goto out;
  }

  /* Enable uncore counters globally. */
  global_ctr_ctrl = 1ULL << 61;
  if (pwrite(msr_fd, &global_ctr_ctrl, sizeof(global_ctr_ctrl), U_MSR_PMON_GLOBAL_CTL) < 0) {
    fprintf(stderr, "cannot enable uncore performance counters: %m\n");
    goto out;
  }

  map_dev[index(pci, DCLK_PMON_UNIT_CTL_REG)] = ctl;
  map_dev[index(pci, DCLK_PMON_UNIT_STATUS_REG)] = ctl;

  for (i=0; i < nr_events; i++) {
    json_object *eid = json_object_new_object();      
    json_object_object_add(eid, events_str[i], json_object_new_int64((uint64_t)events[i]));
    json_object_array_add(events_array, eid);
    map_dev[index(pci, (DCLK_PMON_CTRCTL0_REG + 4*i))] = events[i];
  }
 out:
  if (msr_fd >= 0)
    close(msr_fd);

  return 0;
}

static void intel_skx_imc_collect_dev(json_object * imc_stats, uint32_t bus, uint32_t dev, uint32_t fun,   uint32_t *map_dev)
{
  uint32_t pci = pci_cfg_address(bus, dev, fun);  
  int i;
  uint64_t ev, val;
  for (i=0; i<4; i++) {
    ev  = (uint64_t) map_dev[index(pci, (DCLK_PMON_CTRCTL0_REG + 4*i))];
    val = (uint64_t) (map_dev[index(pci, (DCLK_PMON_CTR0_HIGH_REG + 8*i))]) << 32 | (uint64_t) (map_dev[index(pci, (DCLK_PMON_CTR0_LOW_REG + 8*i))]);
    json_object *eid = json_object_array_get_idx(events_array, i);
    json_object_object_foreach(eid, key, ctl) {
      if (json_object_get_int64(ctl) == ev)
	json_object_object_add(imc_stats, key, json_object_new_int64(val));
      else
	json_object_object_add(imc_stats, "ERROR", json_object_new_int64(val));
    }
  }
}


int begin_intel_skx_imc()
{
  int nr = 0;
  if (get_dev_data()->processor != SKYLAKE) goto out;

  uint32_t *mmconfig_ptr;

  int fd = open(path, O_RDWR);    // first check to see if file can be opened with read permission
  if (fd < 0) {
    fprintf(stderr, "cannot open /dev/mem\n");
    goto out;
  }
  mmconfig_ptr = mmap(NULL, mmconfig_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mmconfig_base);
  if (mmconfig_ptr == MAP_FAILED) {
    fprintf(stderr, "cannot mmap `%s': %m\n", path);
    goto out;
  }

  char **dev_paths = NULL;
  int nr_devs;
  int nr_events = 4;

  if (pci_map_create(&dev_paths, &nr_devs, imc_dclk_dids, 3) < 0) {
    fprintf(stderr,"Failed to identify pci devices");
    goto out;
  }
  // MC: DCLK
  // Devices: 0x10 0x10 0x11 (Controllers)
  // Functions: 0x02 0x06 0x02 (Channels)
  int i;  
  for (i = 0; i < nr_devs; i++) {
    uint32_t bus = strtol(strsep(&dev_paths[i], "/"), NULL, 16);
    uint32_t dev = strtol(strsep(&dev_paths[i], "."), NULL, 16);
    uint32_t fun = strtol(dev_paths[i], NULL, 16);
      
    if (intel_skx_imc_begin_dev(bus, dev, fun, mmconfig_ptr, nr_events) == 0)
      nr++;      
  }
  munmap(mmconfig_ptr, mmconfig_size);
  //pci_map_destroy(&dev_paths, nr_devs);

 out:
  if (fd >= 0)
    close(fd);

  return nr > 0 ? 0 : -1;  
}

int collect_intel_skx_imc(json_object *jarray)
{
  int rc = -1;

  uint32_t *mmconfig_ptr;

  int fd = open(path, O_RDWR);    // first check to see if file can be opened with read permission
  if (fd < 0) {
    fprintf(stderr, "cannot open /dev/mem\n");
    goto out;
  }
  mmconfig_ptr = mmap(NULL, mmconfig_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mmconfig_base);
  if (mmconfig_ptr == MAP_FAILED) {
    fprintf(stderr, "cannot mmap `%s': %m\n", path);
    goto out;
  }

  char **dev_paths = NULL;
  int nr_devs;
  if (pci_map_create(&dev_paths, &nr_devs, imc_dclk_dids, 3) < 0) {
    fprintf(stderr,"Failed to identify pci devices");
    goto out;
  }

  int i;
  for (i = 0; i < nr_devs; i++) {
    char channel[80];
    snprintf(channel, sizeof(channel), "%d", i);

    uint32_t bus = strtol(strsep(&dev_paths[i], "/"), NULL, 16);
    uint32_t dev = strtol(strsep(&dev_paths[i], "."), NULL, 16);
    uint32_t fun = strtol(dev_paths[i], NULL, 16);
    //printf("%d %d %d\n", bus,dev,fun);
    json_object *tags_json = json_object_new_object();
    json_object_object_add(tags_json, "stats_type", json_object_new_string("intel_imc"));
    json_object_object_add(tags_json, "device", json_object_new_string(channel));

    json_object *stats_json = json_object_new_object();
    intel_skx_imc_collect_dev(stats_json, bus, dev, fun, mmconfig_ptr);

    if (json_object_object_length(stats_json) > 0) {
      json_object_object_add(tags_json, "stats", stats_json);
      json_object_array_add(jarray, tags_json);
    }
    else {
      json_object_put(stats_json);
      json_object_put(tags_json);
    }
  }  

  munmap(mmconfig_ptr, mmconfig_size);
  //pci_map_destroy(&dev_paths, nr_devs);
  rc = 0;
 out:
  if (fd >= 0)
    close(fd);
}

