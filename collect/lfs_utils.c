#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lfs_utils.h"

#define devices_path "/sys/kernel/debug/lustre/devices"
#define PROCFS_BUF_SIZE 4096

struct device_info info;

 /* Discover device info from devices file 
    cat /sys/kernel/debug/lustre/devices
    0 UP osd-ldiskfs blue-OST0000-osd blue-OST0000-osd_UUID 4
    1 UP mgc MGC192.168.0.5@o2ib 5564345b-5349-fa2b-efb7-3c8fe1e649eb 4
    2 UP ost OSS OSS_uuid 2
    3 UP obdfilter blue-OST0000 blue-OST0000_UUID 4
    4 UP lwp blue-MDT0000-lwp-OST0000 blue-MDT0000-lwp-OST0000_UUID 4
    5 UP osd-ldiskfs blue-OST0001-osd blue-OST0001-osd_UUID 4
    6 UP obdfilter blue-OST0001 blue-OST0001_UUID 4
    7 UP lwp blue-MDT0000-lwp-OST0001 blue-MDT0000-lwp-OST0001_UUID 4
 */

int devices_discover(struct device_info *info) {

  int rc = -1;

  if (clock_gettime(CLOCK_REALTIME, &info->time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto disco_err;
  }

  gethostname(info->hostname, sizeof(info->hostname));
  fprintf(stdout, "discovering information for host %s\n", info->hostname);

  FILE *fd = NULL;
  fd = fopen(devices_path, "r");
  if (fd == NULL) {
    fprintf(stderr, "cannot open %s: %m\n", devices_path);
    rc = -1;
    goto disco_err;
  }
  
  char procfs_buf[PROCFS_BUF_SIZE];
  setvbuf(fd, procfs_buf, _IOFBF, sizeof(procfs_buf));
  char *line_buf = NULL;
  size_t line_buf_size = 0;    
  unsigned long long num;
  char state[32];
  char type[32];
  char detail[32];
  char uuid[32];
  unsigned long long u;

  while(getline(&line_buf, &line_buf_size, fd) >= 0) {
    char *line = line_buf;
    int n = sscanf(line, "%llu %s %s %s %s %llu", 
		   &num, &state, &type, &detail, &uuid, &u);
    if (n != 6) 
      continue;

    if (strcmp(type, "mdt") == 0) {
      info->class = MDS;      
      snprintf(info->type, sizeof(info->type), type);
      snprintf(info->typepath, sizeof(info->typepath), "/proc/fs/lustre/mdt");
    }
    if (strcmp(type, "obdfilter") == 0) {
      info->class = OSS;
      snprintf(info->type, sizeof(info->type), type);
      snprintf(info->typepath, sizeof(info->typepath), "/proc/fs/lustre/obdfilter");
    }
    if (strcmp(type, "llite") == 0) {
      info->class = OSC;
      snprintf(info->type, sizeof(info->type), type);
      snprintf(info->typepath, sizeof(info->typepath), "/sys/kernel/debug/lustre/llite");
    }
  }

  if (line_buf != NULL) 
    free(line_buf);
  fprintf(stdout, "device type/class: %s/%d\n", info->type, info->class);
  fprintf(stdout, "collect data from: %s\n", info->typepath);

 disco_err:
  if (fd != NULL)
    fclose(fd);  	
  return rc;

}