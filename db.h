#ifndef db_h
#define db_h
#include <libpq-fe.h>
#include "room.h"
int	db_init(PGconn *);
int	db_add_room(PGconn *, struct room *);
int	db_add_exit(PGconn *, struct room *, struct room *);
#endif /* db_h */
