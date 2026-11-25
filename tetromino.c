#include "tetromino.h"
#include "game.h"

#include <stdlib.h>
#include <string.h>

// Colors based on the Tetris Guideline:
//   https://tetris.wiki/Tetris_Guideline
const Color TETRO_COLOR[8] = {
	{0, 0, 0, 255},       // 0: empty
	{0, 210, 240, 255},   // 1: I (Cyan)
	{245, 220, 0, 255},   // 2: O (Yellow)
	{175, 65, 235, 255},  // 3: T (Purple)
	{50, 215, 75, 255},   // 4: S (Green)
	{235, 50, 60, 255},   // 5: Z (Red)
	{45, 115, 245, 255},  // 6: J (Blue)
	{255, 150, 25, 255},  // 7: L (Orange)
};

// Standard SRS spawn orientations (DIR_NORTH / state 0).
const Tetromino TETROMINOS[8] = {
	{0},
	// 1: I (4x4 matrix)
	{
		.id        = 1,
		.size      = 4,
		.direction = DIR_NORTH,
		.shape     = {
			{0, 0, 0, 0},
			{1, 1, 1, 1},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
	},
	// 2: O (2x2 matrix)
	{
		.id        = 2,
		.size      = 2,
		.direction = DIR_NORTH,
		.shape     = {
			{2, 2, 0, 0},
			{2, 2, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
	},
	// 3: T (3x3 matrix)
	{
		.id        = 3,
		.size      = 3,
		.direction = DIR_NORTH,
		.shape     = {
			{0, 3, 0, 0},
			{3, 3, 3, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
	},
	// 4: S (3x3 matrix)
	{
		.id        = 4,
		.size      = 3,
		.direction = DIR_NORTH,
		.shape     = {
			{0, 4, 4, 0},
			{4, 4, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
	},
	// 5: Z (3x3 matrix)
	{
		.id        = 5,
		.size      = 3,
		.direction = DIR_NORTH,
		.shape     = {
			{5, 5, 0, 0},
			{0, 5, 5, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
	},
	// 6: J (3x3 matrix)
	{
		.id        = 6,
		.size      = 3,
		.direction = DIR_NORTH,
		.shape     = {
			{6, 0, 0, 0},
			{6, 6, 6, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
	},
	// 7: L (3x3 matrix)
	{
		.id        = 7,
		.size      = 3,
		.direction = DIR_NORTH,
		.shape     = {
			{0, 0, 7, 0},
			{7, 7, 7, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		},
	},
};

/*
Super Rotation System (SRS) Wall Kick Tables

Standard SRS offsets are specified with +x right and +y UP.
When applied to board coordinates (+x right, +y DOWN), the y component is negated.

Clockwise transitions indexed by starting Direction:
  0 (North->East / 0->R)
  1 (East->South / R->2)
  2 (South->West / 2->L)
  3 (West->North / L->0)

Counter-clockwise transitions are exact negatives of the corresponding CW transition
indexed by destination Direction (e.g. East->North = -(North->East)).
*/
const int JLSTZ_WALL_KICK_CW[4][5][2] = {
	{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},  // 0->R
	{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},      // R->2
	{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     // 2->L
	{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},   // L->0
};

const int I_WALL_KICK_CW[4][5][2] = {
	{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}},    // 0->R
	{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}},    // R->2
	{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}},    // 2->L
	{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}},    // L->0
};

void bag_refill(Piece_Bag *bag) {
	for (int i = 0; i < 7; i++) {
		bag->pieces[i] = i + 1;
	}
	// Fisher-Yates shuffle
	for (int i = 6; i > 0; i--) {
		int j = rand() % (i + 1);
		int tmp = bag->pieces[i];
		bag->pieces[i] = bag->pieces[j];
		bag->pieces[j] = tmp;
	}
	bag->index = 0;
}

int bag_next(Piece_Bag *bag) {
	if (bag->index >= 7 || bag->pieces[0] == 0) {
		bag_refill(bag);
	}
	int id = bag->pieces[bag->index];
	bag->index++;
	return id;
}

// Rotates a tetromino 90 degrees clockwise in place.
void rotate_cw(Tetromino *t) {
	if (t->size <= 2) {
		return;
	}

	t->direction = (Direction)(((int)t->direction + 1) % 4);

	int32_t rotated[4][4] = {0};
	for (int r = 0; r < t->size; r++) {
		for (int c = 0; c < t->size; c++) {
			rotated[r][c] = t->shape[t->size - 1 - c][r];
		}
	}
	memcpy(t->shape, rotated, sizeof(rotated));
}

// Rotates a tetromino 90 degrees counter-clockwise in place.
void rotate_ccw(Tetromino *t) {
	if (t->size <= 2) {
		return;
	}

	t->direction = (Direction)(((int)t->direction + 3) % 4);

	int32_t rotated[4][4] = {0};
	for (int r = 0; r < t->size; r++) {
		for (int c = 0; c < t->size; c++) {
			rotated[r][c] = t->shape[c][t->size - 1 - r];
		}
	}
	memcpy(t->shape, rotated, sizeof(rotated));
}

// Rotates the active tetromino using SRS wall kicks.
// Returns whether rotation succeeded and optionally writes the kick test index used (0..4, or -1).
bool rotate_with_kick(Game *g, bool clockwise, int *out_kick_index) {
	Tetromino *t = &g->tetro;
	if (t->size <= 2) {
		if (out_kick_index) {
			*out_kick_index = -1;
		}
		return false;
	}

	Tetromino original = *t;
	if (clockwise) {
		rotate_cw(t);
	} else {
		rotate_ccw(t);
	}

	const int (*kicks)[5][2] = (t->size == 4) ? I_WALL_KICK_CW : JLSTZ_WALL_KICK_CW;
	int state = clockwise ? (int)original.direction : (int)t->direction;

	for (int test_idx = 0; test_idx < 5; test_idx++) {
		int ox = kicks[state][test_idx][0];
		int oy = kicks[state][test_idx][1];
		int dx = clockwise ? ox : -ox;
		int dy = clockwise ? -oy : oy; // Board +y is down

		t->x = original.x + dx;
		t->y = original.y + dy;

		if (!collide(g, *t)) {
			g->last_action_rotate = true;
			g->last_kick_index = test_idx;
			on_piece_manipulated(g);
			update_ghost(g);
			if (out_kick_index) {
				*out_kick_index = test_idx;
			}
			return true;
		}
	}

	*t = original;
	if (out_kick_index) {
		*out_kick_index = -1;
	}
	return false;
}

// Positions a tetromino at its Guideline spawn location at the top center of the visible board.
void set_tetromino_start(Tetromino *t) {
	t->x = (GAME_WIDTH - t->size) / 2;
	if (t->size == 4) {
		// I-piece occupies row 1 of its 4x4 grid; place row 1 at the top visible row
		t->y = HIDDEN_ROWS - 1;
	} else {
		t->y = HIDDEN_ROWS;
	}
}

// Sets a tetromino to a specific shape (1=I, 2=O, 3=T, 4=S, 5=Z, 6=J, 7=L) at its spawn position.
void set_tetromino(Tetromino *t, int n) {
	int idx = n;
	if (idx < 0) {
		idx = 0;
	} else if (idx > 7) {
		idx = 7;
	}
	*t = TETROMINOS[idx];
	t->id = idx;
	set_tetromino_start(t);
}
