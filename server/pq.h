#ifndef _PQ_H_
#define _PQ_H_

#include <json/json.h>
#include <libpq-fe.h>

PGconn *conn;

int pq_connect(char *pq_server, char *dbname, char *dbuser);
int pq_insert();
int pq_select();
void pq_finish();

#endif
