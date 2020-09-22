#ifndef ROOM_H
#define ROOM_H

struct room {
	char		*id;
	char		*direction;
	char		*shortdesc;
	char		*longdesc;
	char		*area;
	char		*exits;
	unsigned	spec_map_count;
	int		indoors;
};

struct room *	room_new(const char *);
void		room_free(struct room *);

#endif /* ROOM_H */
