#include <libpq-fe.h>
#include <err.h>
#include "db.h"

int
db_init(PGconn *db)
{
	int status = 0;
	/* XXX just a prototype db format for now */
	PGresult *res = PQexec(db,
	    "CREATE TABLE IF NOT EXISTS room ("
	    /*
	     * These things look a little like apr1 hashes but
	     * aren't, they contain characters not legal for base64:
	     *    $apr1$dF!!_X#W$zUxMycg35omZ3p973Tllm1
	     * Just store as text for now.
	     */
	    "id TEXT PRIMARY KEY,"
	    "shortdesc TEXT NOT NULL,"
	    "longdesc TEXT NOT NULL,"
	    "area TEXT,"
	    "indoors BOOLEAN,"
	    "exits TEXT);"
	    "CREATE TABLE IF NOT EXISTS exit ("
	    "direction TEXT,"
	    "source TEXT,"
	    "destination TEXT,"
	    "FOREIGN KEY(source) REFERENCES room(id),"
	    "FOREIGN KEY(destination) REFERENCES room(id),"
	    "PRIMARY KEY(direction, source, destination));");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		warnx("db_init: %s", PQerrorMessage(db));
		status = -1;
	}
	PQclear(res);
	return status;
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
