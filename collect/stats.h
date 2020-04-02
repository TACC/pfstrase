#ifndef _STATS_H_
#define _STATS_H_
#include <json-c12/json.h>

int update_host_map(char *rpc);
void group_statsbytag(const char *tag);
void print_server_tag_sum(const char *tag);
void print_server_tag_map(const char *tag);

json_object *host_map;
json_object *nid_map;
json_object *server_tag_map;
json_object *server_tag_rate_map;
json_object *server_tag_sum;

int groupby;
json_object *group_tags;

#endif
