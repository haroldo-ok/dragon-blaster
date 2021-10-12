#include "shot.h"

const path_step lightining_path[] = {
	{0, -4},
	{-128, -128}
};

const path_step fire_path[] = {
	{0, -2},
	{4, -2},
	{4, -2},
	{3, -2},
	{2, -2},
	{0, -2},
	{-2, -2},
	{-3, -2},
	{-4, -2},
	{-4, -2},
	{-4, -2},
	{-4, -2},
	{-3, -2},
	{-2, -2},
	{0, -2},
	{2, -2},
	{3, -2},
	{4, -2},
	{4, -2},
	{0, -2},
	{-128, -128}
};

const path_step wind_path0[] = {
	{-2, -3},
	{-128, -128}
};

const path_step wind_path1[] = {
	{0, -4},
	{-128, -128}
};

const path_step wind_path2[] = {
	{2, -3},
	{-128, -128}
};

const path lightining_paths[] = {
	{8, -8, 0, lightining_path}
};

const path fire_paths[] = {
	{8, -8, 0, fire_path}
};

const path wind_paths[] = {
	{6, -6, 0, wind_path0},
	{8, -8, 0, wind_path1},
	{10, -6, 0, wind_path2}
};

const shot_info player_shot_infos[PLAYER_SHOT_TYPE_COUNT] = {
	{26, 3, 45, 4, 1, lightining_paths},
	{32, 4, 45, 8, 1, fire_paths},
	{40, 2, 45, 12, 3, wind_paths}
};
