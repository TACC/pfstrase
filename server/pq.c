#include "pq.h"

int pq_connect() {
  int rc = -1;
  
  const char *conninfo;
  conninfo = "dbname=pfstrase_db1 user=postgres";
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

int pq_insert(json_object *host_entry) {

  int rc = -1;
  PGresult   *res;
  int nrows, ncols;
  int i;

  char tags[256];
  json_object *time, *fid, *hid, *oid, *jid, *uid, *da;
  int arraylen;

  if (is_class(host_entry, "mds") < 0 && is_class(host_entry, "oss") < 0)
    goto out;
  if (!json_object_object_get_ex(host_entry, "data", &da) || 
      ((arraylen = json_object_array_length(da)) == 0))
    goto out;

  if (json_object_object_get_ex(host_entry, "time", &time) &&
      json_object_object_get_ex(host_entry, "hid", &hid) &&
      json_object_object_get_ex(host_entry, "obdclass", &oid) &&

      json_object_object_get_ex(host_entry, "uid", &uid)) {   
    snprintf(tags, sizeof(tags), "to_timestamp(%f), '%s', '%s'", json_object_get_double(time), 
	     json_object_get_string(hid), json_object_get_string(oid));
  }
  else
    goto out;

  json_object *de, *target, *client, *sid, *stats;  
  for (i = 0; i < arraylen; i++) {
    de = json_object_array_get_idx(da, i);
    
    if (!json_object_object_get_ex(de, "target", &target) ||
	!json_object_object_get_ex(de, "fid", &fid) ||
	!json_object_object_get_ex(de, "jid", &jid) ||
	!json_object_object_get_ex(de, "uid", &uid) ||
	!json_object_object_get_ex(de, "client", &client) ||
	!json_object_object_get_ex(de, "stats_type", &sid) ||
	!json_object_object_get_ex(de, "stats", &stats))
      continue;
    
    json_object_object_foreach(stats, eventname, value) {    
      char query[512];
      snprintf(query, sizeof(query), "insert into stats (time, hostname, obdclass, jid, uid, "
	       "fid, target, client, stats_type, event_name, value) " 
	       "values (%s, '%s', '%s', '%s', '%s', '%s', '%s', '%s', %lu);", 
	       tags, json_object_get_string(jid), json_object_get_string(uid), json_object_get_string(fid), 
	       json_object_get_string(target), json_object_get_string(client), json_object_get_string(sid), 
	       eventname, value);
      res = PQexec(conn, query);
      PQclear(res);
    }
  }  

  rc = 1;

 out:
  return rc;
}

