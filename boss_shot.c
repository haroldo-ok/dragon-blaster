#include "boss_shot.h"

const path_step boss_lightining_path[] = {
	{0, 4},
	{-128, -128}
};

const path boss_lightining_paths[] = {
	{0, 0, 0, boss_lightining_path}
};

const shot_info boss_shot_infos[BOSS_SHOT_TYPE_COUNT] = {
	{26, 3, 45, 4, 1, boss_lightining_paths}, // 0
};
