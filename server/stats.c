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
  nid_map = json_object_new_object();
  server_tag_map = json_object_new_object();
  server_tag_rate_map = json_object_new_object();
  group_tags = json_object_new_object();
  screen_map = json_object_new_object();
}
__attribute__((destructor))
static void map_kill(void) {
  json_object_put(host_map);
  json_object_put(nid_map);
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
    json_object_object_foreach(tag_entry, eventname, value) {    
      if (strcmp(eventname, "nclients") == 0 || strcmp(eventname, "bytes") == 0  || strcmp(eventname, "iops") == 0) continue;
      if (strcmp(eventname, "read_bytes") == 0 || strcmp(eventname, "write_bytes") == 0) {
	sum_bytes += json_object_get_double(value);
	json_object_object_add(tag_entry, eventname, json_object_new_double(json_object_get_double(value)*cf));
      }
      else if (strcmp(eventname, "load") == 0)
	cpu = json_object_get_double(value);
      else
	sum_reqs += json_object_get_double(value);
    }

    total_iops += sum_reqs;
    total_bytes += sum_bytes*cf;
    json_object_object_add(tag_entry, "load", json_object_new_double(cpu*0.01));
    json_object_object_add(tag_entry, "iops", json_object_new_double(sum_reqs));
    json_object_object_add(tag_entry, "bytes", json_object_new_double(sum_bytes*cf));
    //printf("%s\n", json_object_to_json_string(tag_entry));
  }
  double factor_iops = 1.0/total_iops;
  double factor_bytes = 1.0/total_bytes;

  json_object_object_foreach(server_entry, retags, retag_entry) {
    if (strcmp(retags, "time") == 0) continue;
    json_object *l, *i, *b;

    if (!json_object_object_get_ex(retag_entry, "load", &l)) continue;

    double l_factor = 0;
    int norm = 0;
    if (total_iops > 0 && json_object_object_get_ex(retag_entry, "iops", &i)) {
      l_factor +=json_object_get_double(i)*factor_iops;
      norm += 1;
    }
    if (total_bytes > 0 && json_object_object_get_ex(retag_entry, "bytes", &b)) {
      l_factor +=json_object_get_double(b)*factor_bytes;
      norm += 1;
    }
    if (norm > 0)
      json_object_object_add(retag_entry, "load_eff", json_object_new_double(json_object_get_double(l)*l_factor/norm));
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
      if (strcmp(eventname, "nclients") == 0) json_object_object_add(rates, eventname, json_object_new_double(json_object_get_int64(value)));
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
      if (json_object_object_get_ex(pre_stats, event, &pre_val) && strcmp(event, "load") != 0) 
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
static void aggregate_stat(json_object *host_entry, json_object *tag_tuple, char *stat_type, json_object *tag_map) {
  int i;
  int arraylen;
  json_object *da, *de, *tid, *stats;

  if (!json_object_object_get_ex(host_entry, "data", &da) || 
      ((arraylen = json_object_array_length(da)) == 0))
    return;

  json_object *clients = json_object_new_object();
  for (i = 0; i < arraylen; i++) {
    de = json_object_array_get_idx(da, i);
        
    if (!json_object_object_get_ex(de, "stats", &stats) ||
	!json_object_object_get_ex(de, "stats_type", &tid) ||
	(strcmp(stat_type, json_object_get_string(tid)) != 0))
      continue;
    
    json_object *tags = json_object_new_object();
    json_object_object_foreach(tag_tuple, tag, t) {    
      if (json_object_object_get_ex(de, tag, &tid))
	json_object_object_add(tags, tag, json_object_get(tid));
    }
    char tags_str[256];
    snprintf(tags_str, sizeof(tags_str), json_object_to_json_string(tags));
    json_object_put(tags);

    if (json_object_object_get_ex(de, "client", &tid)) {
      json_object *tmp;
      if (tid && !json_object_object_get_ex(clients, json_object_get_string(tid), &tmp)) {
	json_object_object_add(clients, json_object_get_string(tid), json_object_new_string(""));
	json_object_object_add(stats, "nclients", json_object_new_int64(1));
      }
    }

    add_stats(tag_map, tags_str, stats);
  }
  json_object_put(clients);
}

void group_ratesbytags(int nt, ...) {
  int i;
  va_list args;
  enum json_tokener_error error = json_tokener_success;

  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 
  va_start(args, nt);

  json_object_put(group_tags);
  group_tags = json_object_new_object();
  for (i = 0; i < nt; i++) {
    const char *str = va_arg(args, const char *);
    json_object_object_add(group_tags, str, json_object_new_string(""));
  } 
  va_end(args);

  json_object *tid;
  json_object_object_foreach(server_tag_rate_map, server, rate_entry) {    
    json_object *time;
    if (!json_object_object_get_ex(rate_entry, "time", &time)) continue;

    json_object *tag_rate_map = json_object_new_object();
    json_object_object_foreach(rate_entry, t, rates) {    
      json_object *pre_tags = NULL;
      if (strcmp(t, "time") == 0)  {
	json_object_object_add(tag_rate_map, "time", json_object_get(time));
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
      char tags_str[256];
      snprintf(tags_str, sizeof(tags_str), json_object_to_json_string(tags));
      add_stats(tag_rate_map, tags_str, rates);
      json_object_put(pre_tags);
      json_object_put(tags);
    }

    //printf("%s\n", json_object_to_json_string(tag_rate_map));
    derived_events(tag_rate_map);
    json_object_object_add(screen_map, server, json_object_get(tag_rate_map));
    json_object_put(tag_rate_map);
    
  }
}

/* Group stats by given tag tuple */
void group_statsbytags(int nt, ...) {

  int i;
  va_list args;

  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 
  va_start(args, nt);

  json_object_put(group_tags);
  group_tags = json_object_new_object();
  for (i = 0; i < nt; i++) {
    const char *str = va_arg(args, const char *);
    json_object_object_add(group_tags, str, json_object_new_string(""));
  } 
  va_end(args);
  
  json_object_object_foreach(host_map, servername, host_entry) {    
    /* Only update if data is new */
    json_object *new_time;
    if (is_new_entry(servername, host_entry, &new_time) < 0)
      continue;

    json_object *tag_map = json_object_new_object();

    if (is_class(host_entry, "mds") > 0) {
      aggregate_stat(host_entry, group_tags, "mds", tag_map);
    }
    if (is_class(host_entry, "oss") > 0) {
      aggregate_stat(host_entry, group_tags, "oss", tag_map);
    }
  
    /* Get cpu load due to kernel as well */
    json_object *cpu_map = json_object_new_object();
    aggregate_stat(host_entry, group_tags, "cpu", cpu_map);

    json_object_object_foreach(cpu_map, s, cpu_entry) { 
      json_object *system;
      json_object_object_get_ex(cpu_entry, "system", &system);
      
      json_object_object_foreach(tag_map, t, tag_entry) {
	json_object_object_add(tag_entry, "load", json_object_get(system));
      }
    }
    json_object_put(cpu_map);
    json_object_object_add(tag_map, "time", json_object_get(new_time));
    
    json_object *pre_tag_map;
    if (json_object_object_get_ex(server_tag_map, servername, &pre_tag_map)) {
      json_object *tag_rate_map = json_object_new_object();
      json_object_object_add(tag_rate_map, "time", json_object_get(new_time));
      
      calc_rates(pre_tag_map, tag_map, tag_rate_map);
      //derived_events(tag_rate_map);
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
  json_object *data_array, *data_entry, *nid_entry;
  json_object *tag;
  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 

  json_object_object_foreach(host_map, key, host_entry) {    
    if (is_class(host_entry, "mds") < 0 && is_class(host_entry, "oss") < 0)
      continue;
    if (!json_object_object_get_ex(host_entry, "data", &data_array))
      continue;

    json_object *server = json_object_new_string(key);
    arraylen = json_object_array_length(data_array);
    for (j = 0; j < arraylen; j++) {

      data_entry = json_object_array_get_idx(data_array, j);

      if (json_object_object_get_ex(data_entry, "client_nid", &tag)) {
	if  (json_object_object_get_ex(nid_map, json_object_get_string(tag), &nid_entry)) {
	  if (json_object_object_get_ex(nid_entry, "hid", &tag))
	    json_object_object_add(data_entry, "client", json_object_get(tag));
	  if (json_object_object_get_ex(nid_entry, "jid", &tag))
	    json_object_object_add(data_entry, "jid", json_object_get(tag));
	  if (json_object_object_get_ex(nid_entry, "uid", &tag))
	    json_object_object_add(data_entry, "uid", json_object_get(tag));
	}          
	else
	  json_object_object_add(data_entry, "client", json_object_get(tag));
      }
      
      json_object_object_add(data_entry, "server", json_object_get(server));      

      /* Tag with filesystem name */
      if (json_object_object_get_ex(data_entry, "target", &tag)) {
	char target[32];
	snprintf(target, sizeof(target), "%s", json_object_get_string(tag));  
	char *p = target + strlen(target) - 8;
	*p = '\0';
	json_object_object_add(data_entry, "fid", json_object_new_string(target));	  
      }      
    }
    json_object_put(server);
  }
  //gettimeofday(&te, NULL); 
  //printf("time for tag %f\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000. );

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

    if (json_object_object_get_ex(rpc_json, "obdclass", &tag))
      json_object_object_add(host_entry, "obdclass", json_object_get(tag));

    if (json_object_object_get_ex(rpc_json, "data", &tag))
      json_object_object_add(host_entry, "data", json_object_get(tag));

    if (is_class(host_entry, "mds") < 0 && is_class(host_entry, "oss") < 0)
      if (json_object_object_get_ex(rpc_json, "nid", &tag))	
	json_object_object_add(nid_map, json_object_get_string(tag), json_object_get(host_entry));
  
  }
  rc = 1;
 out:
  return rc;
}


int update_host_map(char *rpc) {

  int rc = -1;
  json_object *rpc_json = NULL;

  //struct timeval ts,te;
  //gettimeofday(&ts, NULL); 

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

  //gettimeofday(&te, NULL); 
  //printf("time for process %f\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000. );

  rc = 1;

 out:
  if (rpc_json)
    json_object_put(rpc_json);
  return rc;
}
