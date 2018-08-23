#ifndef DB_H
#define DB_H
#include "room.h"

struct db {
	void *dbp;
	void *(*dbp_init)(void);
	void (*dbp_free)(void *);
	int (*add_room)(void *, struct room *);
	int (*add_exit)(void *, struct room *, struct room *);
};

void db_init(struct db *);
void db_free(struct db *);
int db_add_room(struct db *, struct room *);
int db_add_exit(struct db *, struct room *, struct room *);

#endif /* DB_H */
