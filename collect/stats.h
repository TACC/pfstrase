#ifndef _STATS_H_
#define _STATS_H_
#include <json-c/json.h>

int update_host_map(char *rpc);
void group_statsbytag(const char *tag);
void print_server_tag_sum(const char *tag);
void print_server_tag_map(const char *tag);

/* Files that shared memory data structures will be mapped to */
#define SEM_MUTEX_NAME "/tmp/sem-mutex"
#define GROUP_TAGS_FILE "/tmp/group_tags"
#define SERVER_TAG_RATE_MAP_FILE "/tmp/export_stats"
#define SERVER_TAG_SUM_FULE "/tmp/export_stats_summary"

json_object *host_map;
json_object *nid_map;
json_object *server_tag_map;
json_object *server_tag_rate_map;
json_object *server_tag_sum;

int groupby;
json_object *group_tags;

#endif
