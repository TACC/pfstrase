#ifndef _STATS_H_
#define _STATS_H_
#include <json/json.h>

int update_host_map(char *rpc);
void group_statsbytag(const char *tag);

/* Files that shared memory data structures will be mapped to */
#define SEM_MUTEX_NAME "/sem-mutex"
#define GROUP_TAGS_FILE "/group_tags"
#define SERVER_TAG_RATE_MAP_FILE "/export_stats"
#define SERVER_TAG_SUM_FULE "/export_stats_summary"

json_object *host_map;
json_object *nid_map;
json_object *server_tag_map;
json_object *server_tag_rate_map;

int groupby;
json_object *group_tags;

int is_class(json_object *he, const char *class);

#endif
