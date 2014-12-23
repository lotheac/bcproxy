#ifndef room_h
#define room_h

struct room {
	char *	id;
	char *	direction;
	char *	shortdesc;
	char *	longdesc;
	char *	area;
	char *	exits;
	int	indoors;
};

struct room *	room_new(const char *);
void		room_free(struct room *);

#endif /* room_h */
