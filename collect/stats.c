#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include "stats.h"


__attribute__((constructor))
static void map_init(void) {
  host_map = json_object_new_object();
  nid_map = json_object_new_object();
  server_tag_map = json_object_new_object();
  server_tag_sum = json_object_new_object();
}
__attribute__((destructor))
static void map_kill(void) {
  json_object_put(host_map);
  json_object_put(nid_map);
  json_object_put(server_tag_map);
  json_object_put(server_tag_sum);
}

#define printf_json(json) printf(">>> %s\n", json_object_get_string(json));

static void print_exports_map() {
  int arraylen;
  int j;
  json_object *obd_tag;
  json_object *client, *jid, *uid, *stats_type, *target;
  json_object *data_array, *data_json, *stats;
  printf("---------------Exports map--------------------\n");
  printf("%20s : %10s %10s %10s %10s %20s\n", "server", "client", "jobid", "user", "stats_type", "target");
  json_object_object_foreach(host_map, key, host_entry) {    
    if (json_object_object_get_ex(host_map, "obdclass", &obd_tag))
      if ((strcmp("mds", json_object_get_string(obd_tag)) != 0) &&	\
	  (strcmp("oss", json_object_get_string(obd_tag)) != 0))
	continue;
    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;

    arraylen = json_object_array_length(data_array);
    for (j = 0; j < arraylen; j++) {
      data_json = json_object_array_get_idx(data_array, j);
      if ((!json_object_object_get_ex(data_json, "client", &client)) ||
	  (!json_object_object_get_ex(data_json, "jid", &jid)) ||	   
	  (!json_object_object_get_ex(data_json, "uid", &uid)))
	continue;

      json_object_object_get_ex(data_json, "stats_type", &stats_type);
      json_object_object_get_ex(data_json, "target", &target);
      json_object_object_get_ex(data_json, "stats", &stats);
      printf("%20s : %10s %10s %10s %10s %20s %s\n", key, json_object_get_string(client), 
	     json_object_get_string(jid), json_object_get_string(uid), 
	     json_object_get_string(stats_type), json_object_get_string(target),json_object_get_string(stats));
    }
  }
}

static void print_nid_map() {
  printf("--------------nids map------------------------\n");
  printf("%20s : %10s %10s %10s\n", "nid", "host", "jid", "uid");
  json_object *hid, *jid, *uid;
  json_object_object_foreach(nid_map, key, val) {
    if ((json_object_object_get_ex(val, "hid", &hid)) && (json_object_object_get_ex(val, "jid", &jid)) && (json_object_object_get_ex(val, "uid", &uid)))
      printf("%20s : %10s %10s %10s\n", key, json_object_get_string(hid), 
	     json_object_get_string(jid), json_object_get_string(uid)); 
  }
}

void print_tag_sum() {  
  json_object *iops, *bytes;
  printf("---------------tag sum map--------------------\n");
  printf("%10s : %16s %16s[MB]\n", "server", "iops", "bytes");
  json_object_object_foreach(server_tag_sum, servername, server_entry) {    
    json_object_object_foreach(server_entry, tags, tags_entry) {    
      if (json_object_object_get_ex(tags_entry, "iops", &iops) && \
	  json_object_object_get_ex(tags_entry, "bytes", &bytes)) {
	printf("%10s : %20s %16lu %16.1f\n", servername, tags, json_object_get_int64(iops), 
	       ((double)json_object_get_int64(bytes))/(1024*1024));
      }
    }
  }
}

void print_tag_map() { 
  
  printf("---------------server tag map--------------------\n");
  printf("%10s : %20s\n", "server", "data");
  json_object_object_foreach(server_tag_map, servername, server_entry) {    
    json_object_object_foreach(server_entry, tags, tags_entry) {    
      enum json_tokener_error error = json_tokener_success;
      json_object * tags_json = json_tokener_parse_verbose(tags, &error);  
      if (error != json_tokener_success) {
	fprintf(stderr, "tags format incorrect `%s': %s\n", tags, json_tokener_error_desc(error));
	continue;
      }
      /*
      int nt = 0;
      json_object_object_foreach(tags_json, tag, tag_val) {    
	
	nt++;
      }
      */
      printf("%10s : %20s %20s\n", servername, tags, json_object_get_string(tags_entry));
    }
  }
}

static void add_stats(json_object *old_stats, json_object *new_stats) {
  /* Add stats values for all devices with same client/jid/uid */
  json_object *oldval;
  json_object_object_foreach(new_stats, event, newval) {
    if (json_object_object_get_ex(old_stats, event, &oldval))
      json_object_object_add(old_stats, event,  
			     json_object_new_int64(json_object_get_int64(oldval) + \
						   json_object_get_int64(newval)));
    else 
      json_object_object_add(old_stats, event, json_object_get(newval));  
  }
}

static int is_server(json_object *he) {
  int rc = -1;
  json_object *tag;
  if (json_object_object_get_ex(he, "obdclass", &tag)) {
    char tag_str[json_object_get_string_len(tag)];
    snprintf(tag_str, sizeof(tag_str), json_object_get_string(tag));
    if ((strcmp("mds", tag_str) != 0) &&
	(strcmp("oss", tag_str) != 0))
      rc = 1;
  }
  return rc;    
}

/* Aggregate all events along current tags */
static void aggregate_grouped_events() {
  json_object_object_foreach(server_tag_map, servername, server_entry) {    
    json_object *tags_sum_map = json_object_new_object();
    json_object_object_foreach(server_entry, tags, tag_entry) {    
      long long sum_reqs = 0;
      long long sum_bytes = 0;
      json_object_object_foreach(tag_entry, eventname, value) {    
	if (strcmp(eventname, "read_bytes") == 0 || strcmp(eventname, "write_bytes") == 0) 
	  sum_bytes += json_object_get_int64(value);
	else
	  sum_reqs += json_object_get_int64(value);
      }
      json_object *sum_json = json_object_new_object();
      json_object_object_add(sum_json, "iops", json_object_new_int64(sum_reqs));
      json_object_object_add(sum_json, "bytes", json_object_new_int64(sum_bytes));
      json_object_object_add(tags_sum_map, tags, sum_json); 
    }
    json_object_object_add(server_tag_sum, servername, tags_sum_map);
  }
}

/* Aggregate (group) stats by given tag (client/jid/uid are most likely) */
void group_statsbytags(int nt, ...) {
  int arraylen;
  int i, j;
  va_list args;
  json_object *data_array, *data_entry, *tid;
  json_object *tag_map, *tags, *stats_json, *tag_stats;

  json_object_object_foreach(host_map, servername, host_entry) {    
    if (is_server(host_entry) < 0)
      continue;

    if (!json_object_object_get_ex(host_entry, "data", &data_array) || 
	((arraylen = json_object_array_length(data_array)) == 0))
      continue;
    tag_map = json_object_new_object();
    for (i = 0; i < arraylen; i++) {
      data_entry = json_object_array_get_idx(data_array, i);

      if (json_object_object_get_ex(data_entry, "stats_type", &tid)) {
	if ((strcmp("mds", json_object_get_string(tid)) != 0) &&	\
	    (strcmp("oss", json_object_get_string(tid)) != 0))
	  continue;
      }
      else 
	continue;
      if (!json_object_object_get_ex(data_entry, "stats", &stats_json)) continue;

      tags = json_object_new_object();
      va_start(args, nt);     
      for (j = 0; j < nt; j++) {
	const char *str = va_arg(args, const char *);
	if (json_object_object_get_ex(data_entry, str, &tid))
	  json_object_object_add(tags, str, json_object_get(tid));
      } 
      va_end(args);
      char tags_str[256];
      snprintf(tags_str, sizeof(tags_str), json_object_to_json_string(tags));
      json_object_put(tags);
      if (!json_object_object_get_ex(tag_map, tags_str, &tag_stats)) {
	tag_stats = json_object_new_object();
	json_object_object_add(tag_map, tags_str, tag_stats);
      }
      add_stats(tag_stats, stats_json);
      json_object_object_add(tag_map, tags_str, json_object_get(tag_stats));
    }
    json_object_object_add(server_tag_map, servername, json_object_get(tag_map));   
    json_object_put(tag_map);
  }  
  aggregate_grouped_events();
}

/* Tag servers exports with client names, jids, uids, and filesystem */
static void tag_stats() {
  int arraylen;
  int j;
  json_object *data_array, *data_entry, *nid_entry;
  json_object *tag;

  json_object_object_foreach(host_map, key, host_entry) {    
    if (is_server(host_entry) < 0)
      continue;
    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;

    arraylen = json_object_array_length(data_array);
    for (j = 0; j < arraylen; j++) {

      data_entry = json_object_array_get_idx(data_array, j);
      if ((json_object_object_get_ex(data_entry, "client_nid", &tag)) && \
	  (json_object_object_get_ex(nid_map, json_object_get_string(tag), &nid_entry))) {
	if (json_object_object_get_ex(nid_entry, "hid", &tag))
	  json_object_object_add(data_entry, "client", json_object_get(tag));
	if (json_object_object_get_ex(nid_entry, "jid", &tag))
	  json_object_object_add(data_entry, "jid", json_object_get(tag));
	if (json_object_object_get_ex(nid_entry, "uid", &tag))
	  json_object_object_add(data_entry, "uid", json_object_get(tag));
	/* Tag with filesystem name */
	if (json_object_object_get_ex(data_entry, "target", &tag)) {
	  char target[32];
	  snprintf(target, sizeof(target), "%s", json_object_get_string(tag));  
	  char *p = target + strlen(target) - 8;
	  *p = '\0';
	  json_object_object_add(data_entry, "fid", json_object_new_string(target));	  
	}
      }
    }
  }
}

int update_host_map(char *rpc) {

  int rc = -1;
  json_object *rpc_json = NULL;
  char hostname[32];

  enum json_tokener_error error = json_tokener_success;
  rpc_json = json_tokener_parse_verbose(rpc, &error);  
  if (error != json_tokener_success) {
    fprintf(stderr, "RPC `%s': %s\n", rpc, json_tokener_error_desc(error));
    goto out;
  }

  /* If hostname is not in the rpc it is not valid so return */
  json_object *hostname_tag;
  if (json_object_object_get_ex(rpc_json, "hostname", &hostname_tag)) {
    snprintf(hostname, sizeof(hostname), "%s", json_object_get_string(hostname_tag));  
    /* init host_entry of hostmap if does not exist */
    json_object *hostname_entry;
    if (!json_object_object_get_ex(host_map, hostname, &hostname_entry)) {
      json_object *entry_json = json_object_new_object();
      json_object_object_add(entry_json, "jid", json_object_new_string("-"));
      json_object_object_add(entry_json, "uid", json_object_new_string("-"));
      json_object_object_add(host_map, hostname, entry_json);
    }
  }
  else {
    fprintf(stderr, "%s\n", "RPC does not contain a hostname to update server data");
    goto out;
  }

  /* update hostname_entry if tags or data are present in rpc */
  json_object *entry_json, *tag;
  if (json_object_object_get_ex(host_map, hostname, &entry_json)) {

    if (json_object_object_get_ex(rpc_json, "hostname", &tag))
      json_object_object_add(entry_json, "hid", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "jid", &tag))
      json_object_object_add(entry_json, "jid", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "uid", &tag))
      json_object_object_add(entry_json, "uid", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "obdclass", &tag))
      json_object_object_add(entry_json, "obdclass", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "data", &tag))
      json_object_object_add(entry_json, "data", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "nid", &tag))
      json_object_object_add(nid_map, json_object_get_string(tag), json_object_get(entry_json));

    tag_stats();
  }
  rc = 1;

  group_statsbytags(4, "fid", "client", "jid", "uid");

 out:
  if (rpc_json)
    json_object_put(rpc_json);
}

void map_destroy() {
  if (host_map)
    json_object_put(host_map);
  if (nid_map)
    json_object_put(nid_map);
}
