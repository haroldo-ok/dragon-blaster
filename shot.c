#include "shot.h"

const path_step lightining_path[] = {
	{0, -8},
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

const path_step hellfire_path[] = {
	{0, -2},
	{5, -2},
	{5, -2},
	{4, -2},
	{3, -2},
	{0, -2},
	{-3, -2},
	{-4, -2},
	{-5, -2},
	{-5, -2},
	{-5, -2},
	{-5, -2},
	{-4, -2},
	{-3, -2},
	{0, -2},
	{3, -2},
	{4, -2},
	{5, -2},
	{5, -2},
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

const path_step wind_path3[] = {
	{-1, -4},
	{0, -4},
	{-128, -128}
};

const path_step wind_path4[] = {
	{1, -4},
	{0, -4},
	{-128, -128}
};

const path_step firebolt_path[] = {
	{0, -4},
	{3, -4},
	{3, -4},
	{2, -4},
	{1, -4},
	{0, -4},
	{-1, -4},
	{-2, -4},
	{-3, -4},
	{-3, -4},
	{-3, -4},
	{-3, -4},
	{-2, -4},
	{-1, -4},
	{0, -4},
	{1, -4},
	{2, -4},
	{3, -4},
	{3, -4},
	{0, -4},
	{-128, -128}
};

const path_step ball_lightining_path[] = {
	{-4, 0},
	{-4, 0},
	{-4, -2},
	{-4, -2},
	{-3, -4},
	{-2, -4},
	{0, -4},
	{0, -4},
	{0, -4},
	{0, -4},
	{0, -4},
	{2, -4},
	{3, -4},
	{4, -4},
	{4, -2},
	{4, -2},
	{4, 0},
	{4, 0},
	{4, 0},
	{4, 2},
	{4, 2},
	{4, 4},
	{3, 4},
	{2, 4},
	{0, 4},
	{0, 4},
	{0, 4},
	{0, 4},
	{0, 4},
	{-2, 4},
	{-3, 4},
	{-4, 2},
	{-4, 2},
	{4, 0},
	{4, 0},
	{-128, -128}
};

const path lightining_paths[] = {
	{8, -8, 0, lightining_path}
};

const path fire_paths[] = {
	{8, -8, 0, fire_path}
};

const path hellfire_paths[] = {
	{8, -8, 0, hellfire_path}
};

const path wind_paths[] = {
	{6, -6, 0, wind_path0},
	{8, -8, 0, wind_path1},
	{10, -6, 0, wind_path2}
};

const path tempest_paths[] = {
	{6, -6, 0, wind_path0},
	{7, -8, 0, wind_path3},
	{9, -8, 0, wind_path4},
	{10, -6, 0, wind_path2}
};

const path firebolt_paths[] = {
	{8, -8, 0, firebolt_path}
};

const path thunderstorm_paths[] = {
	{6, -6, 0, wind_path3},
	{8, -8, 0, wind_path1},
	{10, -6, 0, wind_path4}
};

const path ball_lightining_paths[] = {
	{8, -8, 0, ball_lightining_path}
};


/*
 * The nine breath weapons: three elements, and every combination of two.
 * base tile, animation frames, life time, firing delay, projectiles, paths, damage
 */
const shot_info player_shot_infos[PLAYER_SHOT_TYPE_COUNT] = {
	{26, 3, 26, 5, 1, lightining_paths, 1}, // 0: Lightning
	{32, 4, 44, 8, 1, fire_paths, 3}, // 1: Fire
	{40, 2, 45, 12, 3, wind_paths, 1}, // 2: Wind
	{44, 3, 26, 5, 1, lightining_paths, 2}, // 3: Thunderstrike (lightning + lightning)
	{32, 4, 55, 8, 1, hellfire_paths, 5}, // 4: Hellfire (fire + fire)
	{40, 2, 45, 12, 4, tempest_paths, 2}, // 5: Tempest (wind + wind)
	{50, 2, 45, 5, 1, firebolt_paths, 2}, // 6: Firebolt (lightning + fire)
	{54, 2, 45, 10, 3, thunderstorm_paths, 2}, // 7: Thunderstorm (lightning + wind)
	{32, 2, 35, 9, 1, ball_lightining_paths, 4}, // 8: Firestorm (fire + wind)
};
