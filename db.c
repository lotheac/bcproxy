#include <libpq-fe.h>
#include <err.h>
#include "db.h"

PGconn *
db_init(void)
{
	PGconn *db = NULL;

	db = PQconnectdb("dbname=batmud");
	if (PQstatus(db) != CONNECTION_OK)
		errx(1, "db_init: %s", PQerrorMessage(db));
	return db;
}

int
db_add_room(PGconn *db, struct room *room)
{
	int status = 0;
	const char *paramValues[] = {
		room->id, room->shortdesc, room->longdesc, room->area,
		room->exits,
		room->indoors ? "1" : "0",
	};
	PGresult *res = PQexecParams(db, "INSERT INTO room(id, shortdesc, "
	    "longdesc, area, exits, indoors) SELECT $1, $2, $3, $4, $5, $6"
	    "WHERE NOT EXISTS (SELECT 1 FROM room WHERE id=$1)",
	    6, NULL, paramValues, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		warnx("db_add_room: %s", PQerrorMessage(db));
		status = -1;
	}
	PQclear(res);
	return status;
}

int
db_add_exit(PGconn *db, struct room *src, struct room *dest)
{
	int status = 0;
	const char *paramValues[] = {
		dest->direction, src->id, dest->id
	};
	PGresult *res = PQexecParams(db, "INSERT INTO exit(direction, source,"
	    "destination) SELECT $1, $2, $3 WHERE NOT EXISTS (SELECT 1 FROM "
	    "exit WHERE source=$2 AND destination=$3)", 3, NULL, paramValues,
	    NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		warnx("db_add_exit: %s", PQerrorMessage(db));
		status = -1;
	}
	PQclear(res);
	return status;
}
