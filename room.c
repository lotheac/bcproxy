#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "config.h"
#include "room.h"

struct room *
room_new(const char *mapmsg)
{
	struct room *room;
	const char *cur;
	size_t len;
	const char *end;

	room = calloc(1, sizeof(struct room));
	if (!room)
		goto err;
	cur = mapmsg;

#define SEP ";;"
#define SKIP() do {\
	cur = strstr(cur, SEP);\
	if (!cur)\
		goto err;\
	cur += strlen(SEP);\
} while(0)
#define NEXT(target) do {\
	end = strstr(cur, SEP);\
	if (!end)\
		goto err;\
	len = end - cur + 1;\
	target = malloc(len);\
	if (!target)\
		err(1, "room_new: malloc");\
	strlcpy(target, cur, len);\
	cur = end + strlen(SEP);\
} while(0)

	SKIP(); /* BAT_MAPPER */
	NEXT(room->area);
	NEXT(room->id);
	NEXT(room->direction);
	sscanf(cur, "%d" SEP, &room->indoors);
	SKIP();
	NEXT(room->shortdesc);
	NEXT(room->longdesc);
	NEXT(room->exits);

#undef SEP
#undef NEXT

	return room;
err:
	room_free(room);
	return NULL;
}

void
room_free(struct room *room)
{
	if (room) {
		free(room->area);
		free(room->id);
		free(room->direction);
		free(room->shortdesc);
		free(room->longdesc);
		free(room->exits);
		free(room);
	}
}
