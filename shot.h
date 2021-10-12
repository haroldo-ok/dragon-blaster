#ifndef SHOT_H
#define SHOT_H

#include "actor.h"

#define PLAYER_SHOT_TYPE_COUNT (4)

typedef struct _shot_info {
	char base_tile, frame_count;
	char life_time, firing_delay;
	char length;
	path *paths;
} shot_info;

extern const shot_info player_shot_infos[];

#endif /* SHOT_H */