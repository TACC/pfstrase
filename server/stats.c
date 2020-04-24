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
#include <semaphore.h>
#include <sys/mman.h>
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

  pq_connect();
}
__attribute__((destructor))
static void map_kill(void) {
  json_object_put(host_map);
  json_object_put(nid_map);
  json_object_put(server_tag_map);
  json_object_put(server_tag_rate_map);
  json_object_put(group_tags);

  pq_finish();
}

#define printf_json(json) printf(">>> %s\n", json_object_get_string(json));

/* Map data structures to shared memory */
static size_t mm_size = 1024*1024;;

static void set_shm_map() {

  caddr_t *mm_ptr;
  sem_t *mutex_sem;
  int fd_shm;

  size_t json_size = strlen(json_object_to_json_string(server_tag_rate_map))*sizeof(char);
  //mm_size = strlen(json_object_to_json_string(server_tag_rate_map))*sizeof(char);
  //  mutual exclusion semaphore, mutex_sem with an initial value 0.
  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, O_CREAT, 0660, 0)) == SEM_FAILED)
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));

  if ((fd_shm = shm_open (SERVER_TAG_RATE_MAP_FILE, O_RDWR | O_CREAT, 0660)) == -1)
    fprintf(stderr, "shm_open failed: %s\n", strerror(errno));

  if (ftruncate (fd_shm, mm_size) == -1)
    fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));

  if ((mm_ptr = mmap (NULL, mm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED)
    fprintf(stderr, "mmap failed: %s\n", strerror(errno));

  memcpy((void *)mm_ptr, (const void*)server_tag_rate_map, json_size);
  
  if (sem_post (mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));

  //if (munmap (mm_ptr, mm_size) == -1)
  //fprintf(stderr, "munmap failed: %s\n", strerror(errno));
  //printf("before\n");
  //close(fd_shm);
  //sem_close(mutex_sem);
  
}


static void get_shm_map() {

  caddr_t *mm_ptr;
  sem_t *mutex_sem;
  int fd_shm;

  //size_t mm_size = 12;

  //  mutual exclusion semaphore, mutex_sem with an initial value 0.
  if ((mutex_sem = sem_open (SEM_MUTEX_NAME, 0, 0, 0)) == SEM_FAILED)
    fprintf(stderr, "sem_open failed: %s\n", strerror(errno));

  if (sem_wait(mutex_sem) == -1)
    fprintf(stderr, "sem_wait failed: %s\n", strerror(errno));

  if ((fd_shm = shm_open (SERVER_TAG_RATE_MAP_FILE, O_RDWR, 0)) == -1)
    fprintf(stderr, "shm_open failed: %s\n", strerror(errno));

  if ((mm_ptr = mmap (NULL, mm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED)
    fprintf(stderr, "mmap failed: %s\n", strerror(errno));

  printf("%s\n", json_object_to_json_string((json_object *)mm_ptr));
  
  if (sem_post (mutex_sem) == -1)
    fprintf(stderr, "sem_post failed: %s\n", strerror(errno));

  if (munmap (mm_ptr, mm_size) == -1)
    fprintf(stderr, "munmap failed: %s\n", strerror(errno));

  close(fd_shm);
  sem_close(mutex_sem);

}


/* Add new stat values to old stat values */
static void add_stats(json_object *old_stats, json_object *new_stats) {
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
static void accumulate_events(json_object *se) {
    json_object *tags_sum_map = json_object_new_object();
    json_object_object_foreach(se, tags, tag_entry) {
      if (strcmp(tags, "time") == 0) continue;

      double cpu = 0;
      double sum_reqs = 0;
      double sum_bytes = 0;
      json_object_object_foreach(tag_entry, eventname, value) {    
	if (strcmp(eventname, "read_bytes") == 0 || strcmp(eventname, "write_bytes") == 0) 
	  sum_bytes += json_object_get_double(value);
	else if (strcmp(eventname, "load") == 0)
	  cpu = json_object_get_double(value);
	else
	  sum_reqs += json_object_get_double(value);
      }

      json_object_object_add(tag_entry, "load", json_object_new_double(cpu));
      json_object_object_add(tag_entry, "iops", json_object_new_double(sum_reqs));
      json_object_object_add(tag_entry, "bytes", json_object_new_double(sum_bytes));
    }
}

/* Calculate the rates from two tag maps */
static void calc_rates(json_object *otags_map, json_object *tags_map, json_object *tags_rate_map) {

  json_object *curr_t, *prev_t;
  if (!json_object_object_get_ex(otags_map, "time", &prev_t))
    return;
  if (!json_object_object_get_ex(tags_map, "time", &curr_t))
    return;
  double dt = json_object_get_double(curr_t) - json_object_get_double(prev_t);

  json_object_object_foreach(tags_map, tags, tag_entry) {    
    if (strcmp(tags, "time") == 0) continue;
    json_object *otags_entry;
    json_object *rates = json_object_new_object();

    /* Check if any entry for these tags exists. If not, initialize with 0 */
    if (!json_object_object_get_ex(otags_map, tags, &otags_entry)) {      
      json_object_object_foreach(tag_entry, eventname, value) {
	json_object_object_add(rates, eventname, json_object_new_double(0));
      }
    }
    else {      
      json_object_object_foreach(tag_entry, eventname, value) {
	json_object *ovalue;
	double delta;
	/* Check if any entry for this event exists and calc rate. If not initialize rate with 0 */
	if (!json_object_object_get_ex(otags_entry, eventname, &ovalue))
	  delta = 0;
	else
	  delta = (double)((json_object_get_int64(value) - json_object_get_int64(ovalue)))/dt;
	json_object_object_add(rates, eventname, json_object_new_double(delta));
      }
    }
    json_object_object_add(tags_rate_map, tags, rates);
  }
}

/* Test if an entry is newer than what a server map contains */
static int is_new_entry(json_object *server_map, char *servername, json_object *host_entry) {
  int rc = 1;
  json_object *current_time, *se, *prev_time;    
  if (json_object_object_get_ex(host_entry, "time", &current_time) &&
      json_object_object_get_ex(server_map, servername, &se) &&
      json_object_object_get_ex(se, "time", &prev_time) &&             
      (json_object_get_double(current_time) <= json_object_get_double(prev_time)))
    rc = -1;

  return rc;
}

/* Aggregate a stats type over tags */
void aggregate_stat(json_object *host_entry, json_object *tag_tuple, char *stat_type, json_object *accum_data) {
  int i;
  int arraylen;
  json_object *da, *de, *tid, *accum_stats, *stats;
  if (!json_object_object_get_ex(host_entry, "data", &da) || 
      ((arraylen = json_object_array_length(da)) == 0))
    return;
  
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

    if (!json_object_object_get_ex(accum_data, tags_str, &accum_stats)) {
      accum_stats = json_object_new_object();
      json_object_object_add(accum_data, tags_str, accum_stats);
    }
    add_stats(accum_stats, stats);
    json_object_object_add(accum_data, tags_str, json_object_get(accum_stats));
  }
}

/* Group stats by given tag tuple */
void group_statsbytags(int nt, ...) {
  int arraylen;
  int i, j;
  va_list args;
  json_object *data_array, *data_entry, *tid;
  json_object *tag_map, *tags, *stats_json, *tag_stats;
  json_object *cpu_map, *cpu_rate_map;

  va_start(args, nt);
  group_tags = json_object_new_object();
  for (j = 0; j < nt; j++) {
    const char *str = va_arg(args, const char *);
    json_object_object_add(group_tags, str, json_object_new_string(""));
  } 
  va_end(args);
  
  json_object_object_foreach(host_map, servername, host_entry) {    
    /* Only update if data is new */
    if (is_new_entry(server_tag_map, servername, host_entry) < 0)
      continue;

    json_object *current_time;
    if (!json_object_object_get_ex(host_entry, "time", &current_time))
      continue;

    tag_map = json_object_new_object();
    cpu_map = json_object_new_object();
    if (is_class(host_entry, "mds") > 0) {
      aggregate_stat(host_entry, group_tags, "mds", tag_map);
    }
    if (is_class(host_entry, "oss") > 0) {
      aggregate_stat(host_entry, group_tags, "oss", tag_map);
    }

    /* Get cpu load due to kernel as well */
    aggregate_stat(host_entry, group_tags, "cpu", cpu_map);
    json_object_object_foreach(cpu_map, s, cpu_entry) { 
      json_object *system;
      json_object_object_get_ex(cpu_entry, "system", &system);
      json_object_object_foreach(tag_map, tag, tag_entry) {
	json_object_object_add(tag_entry, "load", json_object_get(system));
      }
    }
    json_object *prev_tag_map, *tag_rate_map;
    tag_rate_map = json_object_new_object();

    json_object_object_add(tag_map, "time", json_object_get(current_time));
    json_object_object_add(tag_rate_map, "time", json_object_get(current_time));

    accumulate_events(tag_map);
    if (json_object_object_get_ex(server_tag_map, servername, &prev_tag_map)) {
      calc_rates(prev_tag_map, tag_map, tag_rate_map);
      json_object_object_add(server_tag_rate_map, servername, json_object_get(tag_rate_map));
    }
    json_object_put(tag_rate_map);
    json_object_object_add(server_tag_map, servername, json_object_get(tag_map));   
    json_object_put(tag_map);
  }  
}

/* Tag servers exports with client names, jids, uids, and filesystem */
static void tag_stats() {
  int arraylen;
  int j;
  json_object *data_array, *data_entry, *nid_entry;
  json_object *tag;

  json_object_object_foreach(host_map, key, host_entry) {    
    if (is_class(host_entry, "mds") < 0 && is_class(host_entry, "oss") < 0)
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
      }     

      json_object_object_add(data_entry, "server", json_object_new_string(key));      
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
  json_object *entry_json, *tag;
  if (json_object_object_get_ex(host_map, hostname, &entry_json)) {
    
    if (json_object_object_get_ex(rpc_json, "time", &tag))
      json_object_object_add(entry_json, "time", json_object_get(tag));

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

    //pq_insert(entry_json);
    //pq_select();
  }
  switch(groupby) {
  case 4:
    group_statsbytags(5, "fid", "server", "client", "jid", "uid");
    break;
  case 3:
    group_statsbytags(4, "fid", "server", "jid", "uid");
    break;
  case 2:
    group_statsbytags(3, "fid", "server", "uid");
    break;
  case 1:
    group_statsbytags(2, "fid", "server");
    break;
  case 5:
    group_statsbytags(1, "server");
    break;
  default:
    group_statsbytags(5, "fid", "server", "client", "jid", "uid");
    break;
  }

  //set_shm_map();
  //get_shm_map();
  rc = 1;
 out:
  if (rpc_json)
    json_object_put(rpc_json);
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
  return rc;
}
