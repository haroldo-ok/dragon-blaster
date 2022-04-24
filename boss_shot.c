#include "boss_shot.h"

const path_step boss_lightining_path[] = {
	{0, 4},
	{-128, -128}
};

const path_step boss_fire_path[] = {
	{0, 2},
	{4, 2},
	{4, 2},
	{3, 2},
	{2, 2},
	{0, 2},
	{-2, 2},
	{-3, 2},
	{-4, 2},
	{-4, 2},
	{-4, 2},
	{-4, 2},
	{-3, 2},
	{-2, 2},
	{0, 2},
	{2, 2},
	{3, 2},
	{4, 2},
	{4, 2},
	{0, 2},
	{-128, -128}
};

const path boss_lightining_paths[] = {
	{0, 0, 0, boss_lightining_path},
};

const path boss_fire_paths[] = {
	{0, 0, 0, boss_fire_path}
};

const shot_info boss_shot_infos[BOSS_SHOT_TYPE_COUNT] = {
	{26, 3, 45, 4, 1, boss_lightining_paths}, // 0
	{92, 4, 40, 8, 1, boss_fire_paths}, // 1
};
