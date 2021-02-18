#include "pq.h"
#include "stats.h"
#include <string.h>

int pq_connect(char *pq_server, char *dbname, char *dbuser) {
  int rc = -1;
  
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
  printf("here: pq_insert\n");
  struct timeval ts,te;
  gettimeofday(&ts, NULL); 
  int total_size = 0;  
  tag_stats();
  //group_ratesbytags(5, "fid", "server", "client", "jid", "uid");
  group_ratesbytags(4, "fid", "server", "jid", "uid");
  json_object_object_foreach(screen_map, s, se) {
    char query[256000] = "insert into stats (time, hostname, fid, jid, uid, client, event_name, value) values ";
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

      char tag_str[256];
      char *cur = tag_str, * const end = tag_str + sizeof(tag_str);
      cur += snprintf(cur, end - cur, "to_timestamp(%f), ", json_object_get_double(time)); /* time */
      cur += snprintf(cur, end - cur, "'%s', ", s); /* hostname */

      json_object *tid;
      if (json_object_object_get_ex(tags, "fid", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'-', ");
      if (json_object_object_get_ex(tags, "jid", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'-', ");
      if (json_object_object_get_ex(tags, "uid", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'-', ");
      if (json_object_object_get_ex(tags, "client", &tid))
	cur += snprintf(cur, end - cur, "'%s', ", json_object_get_string(tid));
      else
	cur += snprintf(cur, end - cur, "'-', ");

      json_object_object_foreach(te, event, val) {
	if (json_object_get_double(val) > 0)
	  qcur += snprintf(qcur, qend - qcur, "(%s '%s', %f), ", tag_str, event, json_object_get_double(val));
      }

    end:
      if(tags)
        json_object_put(tags);
    }

    int query_len = strlen(query);
    total_size += query_len;
    if (query_len > empty_len) {
      query[strlen(query) - 2] = ';';
    printf("%s\n",query);
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

  res = PQexec(conn, "select * from stats order by time desc limit 10");
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
      fprintf(stderr, "select * from stats failed: %s", PQerrorMessage(conn));
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
