#ifndef _PQ_H_
#define _PQ_H_

#include <json/json.h>
#include <libpq-fe.h>

PGconn *conn;

int pq_connect();
void pq_finish();
int pq_insert(json_object *host_entry);
int pq_select();

#endif
