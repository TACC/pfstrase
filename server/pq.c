#include "pq.h"
#include "stats.h"
#include "shmmap.h"
#include <string.h>

int pq_connect(char *pq_server, char *dbname, char *dbuser) {
  int rc = -1;
  PGresult *res;
  char conninfo[256];

  snprintf(conninfo, sizeof(conninfo), "dbname=%s user=%s host=%s port=5433", dbname, dbuser, pq_server);
  conn = PQconnectdb(conninfo);
  /* Check to see that the backend connection was successfully made */
  if (PQstatus(conn) != CONNECTION_OK) {
    fprintf(stderr, "Connection to database failed: %s",
	    PQerrorMessage(conn));
    PQfinish(conn);
    goto out;
  }  
  rc = 1;

  /* Create host_data hypertable if it does not exist */  
  res = PQexec(conn, "CREATE TABLE IF NOT EXISTS host_data (time  TIMESTAMPTZ NOT NULL,host  text,system text,fid  text,jid   text,uid   text,client   text,event text,value   real);");
  PQclear(res);
  
  res = PQexec(conn, "CREATE EXTENSION IF NOT EXISTS timescaledb CASCADE; SELECT create_hypertable('host_data', 'time', if_not_exists => TRUE, chunk_time_interval => INTERVAL '1 day'); CREATE INDEX IF NOT EXISTS host_data_host_time_idx ON host_data (host, time DESC); CREATE INDEX IF NOT EXISTS host_data_system_time_idx ON host_data (system, time DESC); CREATE INDEX IF NOT EXISTS host_data_uid_time_idx ON host_data (uid, time DESC); CREATE INDEX IF NOT EXISTS host_data_jid_time_idx ON host_data (jid, time DESC); CREATE INDEX IF NOT EXISTS host_data_event_time_idx ON host_data (event, time DESC);");
  PQclear(res);

  res = PQexec(conn, "ALTER TABLE host_data SET (timescaledb.compress, timescaledb.compress_orderby = 'time DESC', timescaledb.compress_segmentby = 'system,uid,jid,host,event'); SELECT add_compression_policy('host_data', INTERVAL '12h');");
  PQclear(res);
    
 out:
  return rc;
}

void pq_finish() {
  PQfinish(conn);
}


enum json_tokener_error error = json_tokener_success;
int pq_insert() {

  int rc = -1;
  PGresult *res;
  int i;

  struct timeval ts,te;
  gettimeofday(&ts, NULL); 
  int total_size = 0;  
  get_shm_map();
  group_ratesbytags(5, "system", "fid", "server", "jid", "uid");

  json_object_object_foreach(screen_map, s, se) {
    char query[128000] = "insert into host_data (time, host, system, fid, jid, uid, client, event, value) values ";    
    int empty_len = strlen(query);
    char *qcur = query + empty_len, * const qend = query + sizeof(query);
    json_object *time;
    if (!json_object_object_get_ex(se, "time", &time)) continue;
   
    json_object_object_foreach(se, t, te) {

      if (strcmp(t, "time") == 0) continue;     

      json_object *tags = NULL;      
      tags = json_tokener_parse_verbose(t, &error);
      if (error != json_tokener_success) {
        fprintf(stderr, "tags format incorrect `%s': %s\n", t, json_tokener_error_desc(error));
        goto end;
      }

      char tag_str[1024];
      char *cur = tag_str, * const end = tag_str + sizeof(tag_str);
      cur += snprintf(cur, end - cur, "to_timestamp(%f), ", json_object_get_double(time)); /* time */
      cur += snprintf(cur, end - cur, "'%s', ", s); /* hostname */
      
      json_object *tid;
      if (json_object_object_get_ex(tags, "system", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'*', ");
      if (json_object_object_get_ex(tags, "fid", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'*', ");
      if (json_object_object_get_ex(tags, "jid", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'*', ");
      if (json_object_object_get_ex(tags, "uid", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'*', ");
      if (json_object_object_get_ex(tags, "client", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'*', ");
     
      char record_str[8192];
      char *rcur = record_str, * const rend = record_str + sizeof(record_str);
      json_object_object_foreach(te, event, val) {
	if (json_object_get_double(val) > 0)
	  rcur += snprintf(rcur, rend - rcur, "(%s '%s', %f), ", tag_str, event, json_object_get_double(val));
      }

      //printf("cur %s %zu %zu\n", record_str, strlen(record_str), strlen(query));

      if ((qcur + strlen(record_str)) > qend) {
	int query_len = strlen(query);
	  total_size += query_len;
	  if (query_len > empty_len) {
	    query[query_len - 2] = ';';
	    res = PQexec(conn, query);
	    PQclear(res);
	  }
	  qcur = query + empty_len;	  
      }

      qcur += snprintf(qcur, qend - qcur, "%s", record_str);

    end:
      if(tags)
        json_object_put(tags);
    }
    /*
      json_object_object_foreach(te, event, val) {
      if (json_object_get_double(val) > 0)
      //qcur += snprintf(qcur, qend - qcur, "(%s '%s', %f), ", tag_str, event, json_object_get_double(val));
      }
    */
    
    int query_len = strlen(query);
    total_size += query_len;
    if (query_len > empty_len) {
      printf("query total\n");
      query[query_len - 2] = ';';
      res = PQexec(conn, query);
      PQclear(res);
    }
  }
  gettimeofday(&te, NULL);   
  printf("time for insert %f of size %d\n", (double)(te.tv_sec - ts.tv_sec) + (double)(te.tv_usec - ts.tv_usec)/1000000., total_size);

  rc = 1;

 out:
  return rc;
}


int pq_select() {
  int rc = -1;
  
  PGresult   *res;
  int nrows, ncols;
  int i,j;

  res = PQexec(conn, "select * from host_data order by time desc limit 10");
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
      fprintf(stderr, "select * from host_data failed: %s", PQerrorMessage(conn));
      goto out;     
  }
  
  ncols = PQnfields(res);
  nrows = PQntuples(res);
  
  for(i = 0; i < ncols; i++) {
    char *name = PQfname(res, i);
    printf(" %-15s", name); 
  }     
  printf("\n");
    
  for (j = 0; j < nrows; j++)
    for (i = 0; i < ncols; i++)
      printf("%-15s", PQgetvalue(res, j, i));
  printf("\n\n");

  rc = 1;

 out:
  PQclear(res);
  return rc;
}
