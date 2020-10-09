#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "lfs_utils.h"

#define PROCFS_BUF_SIZE 4096

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

struct device_info info;

__attribute__((constructor))
static void devices_discover(void) {

  snprintf(info.nid, sizeof(info.nid), "-");
  snprintf(info.jid, sizeof(info.jid), "-");  
  snprintf(info.uid, sizeof(info.uid), "-");

  info.nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  info.processor = signature(&info.n_pmcs);
  begin_intel_pmc3();
  begin_intel_skx_imc();

  // Get hostname, device class, and time
  if (clock_gettime(CLOCK_REALTIME, &info.time) != 0) {
    fprintf(stderr, "cannot clock_gettime(): %m\n");
    goto err;
  }
  gethostname(info.hostname, sizeof(info.hostname));
  fprintf(stdout, "discovering information for host %s\n", info.hostname);

  char procfs_buf[PROCFS_BUF_SIZE];
  char *line_buf = NULL;
  size_t line_buf_size = 0;    
  unsigned long long num;
  char state[32];
  char type[32];
  char detail[64];
  char uuid[64];
  unsigned long long u;
  char devices_path[128];
  char nid_path[128];
  char peers_path[128];

  FILE *fd = NULL;
  
  if (fd = fopen("/sys/kernel/debug/lustre/devices", "r"))
    strcpy(devices_path,      "/sys/kernel/debug/lustre/devices");    
  else if (fd = fopen("/proc/fs/lustre/devices", "r"))
    strcpy(devices_path,      "/proc/fs/lustre/devices");
  else {    
    fprintf(stderr, "cannot open %s: %m\n", "devices file");
    goto err;
  }

  struct stat dir_stat;
  stat("/sys/kernel/debug/lustre/llite", &dir_stat);
  if (S_ISDIR(dir_stat.st_mode))
    strcpy(info.llite_path, "/sys/kernel/debug/lustre/llite");
  else
    strcpy(info.llite_path, "/proc/fs/lustre/llite");

  stat("/sys/kernel/debug/lustre/osc", &dir_stat);
  if (S_ISDIR(dir_stat.st_mode))
    strcpy(info.osc_path, "/sys/kernel/debug/lustre/osc");
  else
    strcpy(info.osc_path,     "/proc/fs/lustre/osc");

  stat("/sys/fs/lustre/osc", &dir_stat);
  if (S_ISDIR(dir_stat.st_mode))
    strcpy(info.oss_nid_path, "/sys/fs/lustre/osc");
  else
    strcpy(info.oss_nid_path, "/proc/fs/lustre/osc");

  setvbuf(fd, procfs_buf, _IOFBF, sizeof(procfs_buf));
  while(getline(&line_buf, &line_buf_size, fd) >= 0) {
    char *line = line_buf;
    int n = sscanf(line, "%llu %s %s %s %s %llu", 
		   &num, &state, &type, &detail, &uuid, &u);
    if (n != 6) 
      continue;
    if (strcmp(type, "mdt") == 0) {
      info.class = MDS;      
      snprintf(info.class_str, sizeof(info.class_str), "mds");
      break;
    }
    if (strcmp(type, "obdfilter") == 0) {
      info.class = OSS;
      snprintf(info.class_str, sizeof(info.class_str), "oss");
      break;
    }

    if (strcmp(type, "osc") == 0) {
      info.class = OSC;
      snprintf(info.class_str, sizeof(info.class_str), "osc");
      break;
    }
  }

  fclose(fd);
  fd = NULL;
  
  // Get local nid
  
  if (fd = fopen("/sys/kernel/debug/lnet/nis", "r")) {
    strcpy(nid_path, "/sys/kernel/debug/lnet/nis");
    strcpy(peers_path, "/sys/kernel/debug/lnet/peers");
  }
  else if (fd = fopen("/proc/sys/lnet/nis", "r")) {
    strcpy(nid_path, "/proc/sys/lnet/nis");  
    strcpy(peers_path, "/proc/sys/lnet/peers");  
  }
  else {
    fprintf(stderr, "cannot open %s: %m\n", nid_path);
    goto err;
  }
  
  setvbuf(fd, procfs_buf, _IOFBF, sizeof(procfs_buf));
  char nid[32];
  while(getline(&line_buf, &line_buf_size, fd) >= 0) {
    char *line = line_buf;
    int n = sscanf(line, "%s %*s", &nid);
    if (strstr(nid, "@o2ib") != NULL) { 
      snprintf(info.nid, sizeof(info.nid), nid);
      break;
    }
  }

  fclose(fd);
  fd = NULL;
 
 err:
  if (line_buf != NULL)
    free(line_buf);

  if (fd != NULL)
    fclose(fd);  	
}

struct device_info *get_dev_data(void) {
  return &info;
}
