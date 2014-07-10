#ifndef room_h
#define room_h

struct room {
    char *area;
    char *id;
    char *direction;
    char *shortdesc;
    char *longdesc;
    char *exits;
    int  indoors;
};

struct room *room_new(const char *mapmsg);
void room_free(struct room *room);

#endif /* room_h */
