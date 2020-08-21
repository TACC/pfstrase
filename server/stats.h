#ifndef _STATS_H_
#define _STATS_H_
#include <json/json.h>

int update_host_map(char *rpc);
void group_statsbytag(const char *tag);
void tag_stats();

json_object *host_map;
json_object *nid_map;
json_object *server_tag_map;
json_object *server_tag_rate_map;
json_object *screen_map;

char *filter_user;
char *filter_server;
char *filter_client;
char *filter_job;

int groupby;
json_object *group_tags;

int is_class(json_object *he, const char *class);

#endif
