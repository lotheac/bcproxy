#include <stdlib.h>
#include "db.h"

void
db_init(struct db *db)
{
	if (db->dbp_init)
		db->dbp = db->dbp_init();
}

void
db_free(struct db *db)
{
	if (db->dbp_free)
		db->dbp_free(db->dbp);
	db->dbp = NULL;
}

int
db_add_room(struct db *db, struct room *room)
{
	if (db->add_room)
		return db->add_room(db->dbp, room);
	return 0;
}

int
db_add_exit(struct db *db, struct room *a, struct room *b)
{
	if (db->add_exit)
		return db->add_exit(db->dbp, a, b);
	return 0;
}
