#include <stdlib.h>
#include <string.h>
#include "room.h"

struct room *room_new(const char *mapmsg) {
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
        goto err;\
    strncpy(target, cur, len - 1);\
    target[len-1] = '\0';\
    cur = end + strlen(SEP);\
} while(0)

    SKIP(); /* BAT_MAPPER */
    NEXT(room->area);
    NEXT(room->id);
    NEXT(room->direction);
    NEXT(room->unknown); /* unknown flag */
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

void room_free(struct room *room) {
    if (room) {
        free(room->area);
        free(room->id);
        free(room->direction);
        free(room->unknown);
        free(room->shortdesc);
        free(room->longdesc);
        free(room->exits);
        free(room);
    }
}
