#ifndef SHOT_H
#define SHOT_H

#include "actor.h"

typedef struct _shot_info {
	char base_tile, frame_count;
	char length;
	path *paths;
} shot_info;

extern shot_info player_shot_infos[];

#endif /* SHOT_H */