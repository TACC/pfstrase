#ifndef _SHMMAP_H_
#define _SHMMAP_H_

#include <json/json.h>

#define SEM_MUTEX_NAME "/sem-mutex"
#define GROUP_TAGS_FILE "/group_tags"
#define SERVER_TAG_RATE_MAP_FILE "/export_stats"
#define SERVER_TAG_SUM_FULE "/export_stats_summary"

void set_shm_map();
void get_shm_map();

#endif
