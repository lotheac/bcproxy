#ifndef POSTGRES_H
#define POSTGRES_H
#include <libpq-fe.h>
#include "room.h"

void *postgres_init(void);
void postgres_free(void *);
int postgres_add_room(void *, struct room *);
int postgres_add_exit(void *, struct room *, struct room *);

#endif /* POSTGRES_H */
