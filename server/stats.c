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
#include <fcntl.h>
#include <libpq-fe.h>

#include "stats.h"
#include "pq.h"

__attribute__((constructor))
static void map_init(void) {
  host_map = json_object_new_object();
  server_tag_map = json_object_new_object();
  server_tag_rate_map = json_object_new_object();
  group_tags = json_object_new_object();
  screen_map = json_object_new_object();
}
__attribute__((destructor))
static void map_kill(void) {
  json_object_put(host_map);
  json_object_put(server_tag_map);
  json_object_put(server_tag_rate_map);
  json_object_put(group_tags);
  json_object_put(screen_map);
}

/* Test if device type (mds/oss/osc) */
int is_class(json_object *he, const char *class) {
  int rc = -1;
  json_object *tag;
  if (json_object_object_get_ex(he, "obdclass", &tag)) {
    char tag_str[32];
    snprintf(tag_str, sizeof(tag_str), json_object_get_string(tag));
    if (strcmp(class, tag_str) == 0)
      rc = 1;
  }
  return rc;    
}

/* Accumulate events for each host entry */
static void derived_events(json_object *server_entry) {
  double total_iops = 0.0;
  double total_bytes = 0.0;
  const double cf = 1.0/(1024*1024);
  json_object_object_foreach(server_entry, tags, tag_entry) {
    if (strcmp(tags, "time") == 0) continue;

    double cpu = 0;
    double sum_reqs = 0;
    double sum_bytes = 0;
    double sum_sp_flops = 0;
    double sum_dp_flops = 0;
    double sum_mbw = 0;
    double sum_ins = 0;
    double sum_cyc = 0;
    double sum_ref = 0;
    double sum_ibw = 0;
    double load = 0;
    json_object_object_foreach(tag_entry, eventname, value) {    
      if (strcmp(eventname, "nhosts") == 0 || strcmp(eventname, "bytes") == 0  || strcmp(eventname, "iops") == 0) continue;

      if (strcmp(eventname, "read_bytes") == 0 || strcmp(eventname, "write_bytes") == 0) {
	sum_bytes += json_object_get_double(value);
	json_object_object_add(tag_entry, eventname, json_object_new_double(json_object_get_double(value)*cf));
      }
      else if (strcmp(eventname, "scalar_single") == 0)
	sum_sp_flops += json_object_get_double(value);
      else if (strcmp(eventname, "128b_single") == 0)
	sum_sp_flops += 4*json_object_get_double(value);
      else if (strcmp(eventname, "256b_single") == 0)
	sum_sp_flops += 8*json_object_get_double(value);
      else if (strcmp(eventname, "512b_single") == 0)
	sum_sp_flops += 16*json_object_get_double(value);
      else if (strcmp(eventname, "scalar_double") == 0)
	sum_dp_flops += json_object_get_double(value);
      else if (strcmp(eventname, "128b_double") == 0)
	sum_dp_flops += 2*json_object_get_double(value);
      else if (strcmp(eventname, "256b_double") == 0)
	sum_dp_flops += 4*json_object_get_double(value);
      else if (strcmp(eventname, "512b_double") == 0)
	sum_dp_flops += 8*json_object_get_double(value);
      else if (strcmp(eventname, "CAS_READS") == 0)
	sum_mbw += json_object_get_double(value);
      else if (strcmp(eventname, "CAS_WRITES") == 0)
	sum_mbw += json_object_get_double(value);
      else if (strcmp(eventname, "tx_bytes") == 0)
	sum_ibw += json_object_get_double(value);
      else if (strcmp(eventname, "rx_bytes") == 0)
	sum_ibw += json_object_get_double(value);
      else if (strcmp(eventname, "tx_packets") == 0)
	continue;
      else if (strcmp(eventname, "rx_packets") == 0)
	continue;
      else if (strcmp(eventname, "ACT_COUNT") == 0)
	continue;
      else if (strcmp(eventname, "PRE_COUNT_MISS") == 0)
	continue;
      else if (strcmp(eventname, "user") == 0)
        load += json_object_get_double(value);
      else if (strcmp(eventname, "system") == 0)
	load += json_object_get_double(value);
      else if (strcmp(eventname, "idle") == 0)
	continue;
      else if (strcmp(eventname, "freeram") == 0)
	continue;
      else if (strcmp(eventname, "instructions_retired") == 0)
	sum_ins += json_object_get_double(value);
      else if (strcmp(eventname, "core_cycles") == 0)
	sum_cyc += json_object_get_double(value);
      else if (strcmp(eventname, "ref_cycles") == 0)
	sum_ref += json_object_get_double(value);
      else
	sum_reqs += json_object_get_double(value);
    }

    sum_ibw = sum_ibw*4.0*cf;
    sum_mbw = sum_mbw*64/(1024.0*1024.0*1024.0);
    sum_dp_flops = sum_dp_flops/1000000000;
    sum_sp_flops = sum_dp_flops/1000000000;

    json_object_object_add(tag_entry, "load", json_object_new_double(load*0.01));
    json_object_object_add(tag_entry, "iops", json_object_new_double(sum_reqs));
    json_object_object_add(tag_entry, "bytes", json_object_new_double(sum_bytes*cf));
    json_object_object_add(tag_entry, "sp_flops", json_object_new_double(sum_sp_flops));
    json_object_object_add(tag_entry, "dp_flops", json_object_new_double(sum_dp_flops));
    json_object_object_add(tag_entry, "mbw", json_object_new_double(sum_mbw));
    json_object_object_add(tag_entry, "ibw", json_object_new_double(sum_ibw));
    json_object_object_add(tag_entry, "cpi", json_object_new_double(sum_ins/sum_cyc));
    json_object_object_add(tag_entry, "freq", json_object_new_double(2.7*sum_cyc/sum_ref));
  }
}

/* Calculate the rates from two tag maps */
static void calc_rates(json_object *otags_map, json_object *tags_map, json_object *tags_rate_map) {

  json_object *curr_t, *prev_t;
  if (!json_object_object_get_ex(otags_map, "time", &prev_t) || 
      !json_object_object_get_ex(tags_map, "time", &curr_t))
    return;
  double dt = json_object_get_double(curr_t) - json_object_get_double(prev_t);
  
  json_object_object_foreach(tags_map, tags, tag_entry) {    
    if (strcmp(tags, "time") == 0) continue;
      
    /* Check if any previous entry for these tags exists. If not then pass */
    json_object *otags_entry;
    if (!json_object_object_get_ex(otags_map, tags, &otags_entry))
      continue;

    json_object *rates = json_object_new_object();
        
    json_object_object_foreach(tag_entry, eventname, value) {
      if (strcmp(eventname, "nhosts") == 0) json_object_object_add(rates, eventname, json_object_new_double(json_object_get_int64(value)));
      json_object *ovalue;
      if (!json_object_object_get_ex(otags_entry, eventname, &ovalue))
	continue;
      double delta = (double)((json_object_get_int64(value) - json_object_get_int64(ovalue)))/dt;

      if (delta > 0)
	json_object_object_add(rates, eventname, json_object_new_double(delta));
    }

    json_object_object_add(tags_rate_map, tags, json_object_get(rates));
    json_object_put(rates);
  }
}

/* Test if an entry is newer than what a server map contains */
static int is_new_entry(char *servername, json_object *host_entry, json_object **cur_time) {
  int rc = -1;
  json_object *he, *hid, *pre_time;    
  

  if (!json_object_object_get_ex(host_entry, "time", cur_time))
    goto out;

  if (!json_object_object_get_ex(server_tag_map, servername, &he)) {
    rc = 1;
    goto out;
  }

  if (json_object_object_get_ex(he, "time", &pre_time) &&
      (json_object_get_double(*cur_time) > json_object_get_double(pre_time)))
    rc = 1;

 out:
  return rc;
}

static void add_stats(json_object *tag_map, char *tags, json_object *stats) {
  
  json_object *pre_stats, *pre_val;
  if (json_object_object_get_ex(tag_map, tags, &pre_stats)) {

    json_object_object_foreach(stats, event, new_val) {
      if (json_object_object_get_ex(pre_stats, event, &pre_val)) 
	json_object_object_add(pre_stats, event, json_object_new_int64(json_object_get_int64(new_val) + json_object_get_int64(pre_val)));
      else
	json_object_object_add(pre_stats, event, json_object_get(new_val));
    }

    json_object_object_add(tag_map, tags, json_object_get(pre_stats));
  }
  else
    json_object_object_add(tag_map, tags, json_object_get(stats));
  
}

/* Aggregate a stats type over tags */
static void aggregate_stat(json_object *host_entry, json_object *tag_tuple, json_object *tag_map) {
  int i;
  int arraylen;
  json_object *da, *de, *tid, *stats;

  if (!json_object_object_get_ex(host_entry, "data", &da) || 
      ((arraylen = json_object_array_length(da)) == 0))
    return;

  json_object *clients = json_object_new_object();

  for (i = 0; i < arraylen; i++) {
    de = json_object_array_get_idx(da, i);
    if (!json_object_object_get_ex(de, "stats", &stats)) 
      continue;
    
    json_object *tags = json_object_new_object();
    json_object_object_foreach(tag_tuple, tag, t) {    
      if (json_object_object_get_ex(de, tag, &tid))
	json_object_object_add(tags, tag, json_object_get(tid));
    }
    char tags_str[256];
    snprintf(tags_str, sizeof(tags_str), json_object_to_json_string(tags));
    json_object_put(tags);

    json_object *empty;
    json_object_object_add(stats, "nhosts", json_object_new_int64(1));
    if (json_object_object_get_ex(de, "host", &tid) && json_object_object_get_ex(clients, json_object_get_string(tid), &empty))
      json_object_object_add(stats, "nhosts", json_object_new_int64(0));
    else
      json_object_object_add(clients, json_object_get_string(tid), json_object_new_string(""));
    	      
    add_stats(tag_map, tags_str, stats);
  }

  json_object_put(clients);
}

void filter(json_object *tags, char *value, char *tag) {
  json_object *tid;
  if (value && json_object_object_get_ex(tags, tag, &tid) && strcmp(json_object_get_string(tid), value) != 0)
    json_object_object_add(tags, tag, json_object_new_string("*"));	   	
  return;
}

void group_ratesbytags(int nt, ...) {
  int i;
  va_list args;
  enum json_tokener_error error = json_tokener_success;

  va_start(args, nt);

  json_object_put(group_tags);
  group_tags = json_object_new_object();
  for (i = 0; i < nt; i++) {
    const char *str = va_arg(args, const char *);
    json_object_object_add(group_tags, str, json_object_new_string(""));
  } 
  va_end(args);


  json_object_put(screen_map);
  screen_map = json_object_new_object();

  json_object *tid;
  json_object_object_foreach(server_tag_rate_map, s, se) {   
    json_object *time;
    if (!json_object_object_get_ex(se, "time", &time)) return;
    json_object_object_foreach(se, t, rates) {    
      json_object *pre_tags = NULL;
      if (strcmp(t, "time") == 0)  {
	json_object_object_add(screen_map, "time", json_object_get(time));
	continue;      
      }

      pre_tags = json_tokener_parse_verbose(t, &error);
      if (error != json_tokener_success) {
	fprintf(stderr, "tags format incorrect `%s': %s\n", t, json_tokener_error_desc(error));
	continue;
      }

      json_object *tags = json_object_new_object();
      json_object_object_foreach(group_tags, tag, t) {
	if (json_object_object_get_ex(pre_tags, tag, &tid))
	  json_object_object_add(tags, tag, json_object_get(tid));
      }
      filter(tags, filter_user, "uid");
      filter(tags, filter_job, "jid");
      filter(tags, filter_host, "host");

      char tags_str[256];
      snprintf(tags_str, sizeof(tags_str), json_object_to_json_string(tags));
      add_stats(screen_map, tags_str, rates);
      json_object_put(pre_tags);
      json_object_put(tags);
    }
  }
  derived_events(screen_map);
}

/* Group stats by given tag tuple */
void group_statsbytag(int nt, ...) {

  int i;
  va_list args;

  va_start(args, nt);
  json_object_put(group_tags);
  group_tags = json_object_new_object();
  for (i = 0; i < nt; i++) {
    const char *str = va_arg(args, const char *);
    json_object_object_add(group_tags, str, json_object_new_string(""));
  } 
  va_end(args);

  int ctr = 0;
  json_object_object_foreach(host_map, servername, host_entry) {    
    /* Only update if data is new */
    json_object *new_time;
    ctr += 1;
    if (is_new_entry(servername, host_entry, &new_time) < 0)
      continue;
    
    json_object *tag_map = json_object_new_object();
    aggregate_stat(host_entry, group_tags, tag_map);
    json_object_object_add(tag_map, "time", json_object_get(new_time));

    json_object *pre_tag_map;
    if (json_object_object_get_ex(server_tag_map, servername, &pre_tag_map)) {
      json_object *tag_rate_map = json_object_new_object();
      json_object_object_add(tag_rate_map, "time", json_object_get(new_time));    
      calc_rates(pre_tag_map, tag_map, tag_rate_map);
      json_object_object_add(server_tag_rate_map, servername, json_object_get(tag_rate_map));
      json_object_put(tag_rate_map);
    }
    json_object_object_add(server_tag_map, servername, json_object_get(tag_map));   
    json_object_put(tag_map);
  }  
}


/* Tag servers exports with client names, jids, uids, and filesystem */
void tag_stats() {
  int arraylen;
  int j;
  json_object *data_array, *data_entry;
  json_object *tag;

  json_object_object_foreach(host_map, key, host_entry) {    
    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;
    arraylen = json_object_array_length(data_array);
    for (j = 0; j < arraylen; j++) {
      
      data_entry = json_object_array_get_idx(data_array, j);

      if (json_object_object_get_ex(host_entry, "hid", &tag))
	json_object_object_add(data_entry, "host", json_object_get(tag));
      if (json_object_object_get_ex(host_entry, "jid", &tag)) 
	json_object_object_add(data_entry, "jid", json_object_get(tag));
      if (json_object_object_get_ex(host_entry, "uid", &tag))
	json_object_object_add(data_entry, "uid", json_object_get(tag));     
    }
  }  
}

static int update_host_entry(json_object *rpc_json) {
  int rc = -1;
  char hostname[32];

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
  json_object *host_entry, *tag;
  if (json_object_object_get_ex(host_map, hostname, &host_entry)) {
    
    if (json_object_object_get_ex(rpc_json, "time", &tag))
      json_object_object_add(host_entry, "time", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "hostname", &tag))
      json_object_object_add(host_entry, "hid", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "jid", &tag))
      json_object_object_add(host_entry, "jid", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "uid", &tag))
      json_object_object_add(host_entry, "uid", json_object_get(tag));
    
    if (json_object_object_get_ex(rpc_json, "data", &tag))
      json_object_object_add(host_entry, "data", json_object_get(tag));
  }
  rc = 1;
  //printf("%s\n", json_object_to_json_string(host_map));
 out:
  return rc;
}


int update_host_map(char *rpc) {

  int rc = -1;
  json_object *rpc_json = NULL;

  enum json_tokener_error error = json_tokener_success;
  rpc_json = json_tokener_parse_verbose(rpc, &error);  
  if (error != json_tokener_success) {
    fprintf(stderr, "RPC `%s': %s\n", rpc, json_tokener_error_desc(error));
    goto out;
  }
  
  int i;
  if (json_object_is_type(rpc_json, json_type_array)) {
    int arraylen = json_object_array_length(rpc_json);
    for (i = 0; i < arraylen; i++)
      update_host_entry(json_object_array_get_idx(rpc_json, i));
  }
  else
    update_host_entry(rpc_json);
  rc = 1;

 out:
  if (rpc_json)
    json_object_put(rpc_json);
  return rc;
}
