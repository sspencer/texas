#ifndef TEXAS_TETROMINO_H
#define TEXAS_TETROMINO_H

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

typedef struct Game Game;

typedef enum Direction {
	DIR_NORTH = 0,
	DIR_EAST  = 1,
	DIR_SOUTH = 2,
	DIR_WEST  = 3,
} Direction;

typedef struct Tetromino {
	int       id;
	int       x;
	int       y;
	Direction direction;
	int       size;
	int32_t   shape[4][4]; // Indexed as shape[row][col]
} Tetromino;

// 7-Bag Random Generator (Tetris Guideline)
typedef struct Piece_Bag {
	int pieces[7];
	int index;
} Piece_Bag;

extern const Color TETRO_COLOR[8];
extern const Tetromino TETROMINOS[8];
extern const int JLSTZ_WALL_KICK_CW[4][5][2];
extern const int I_WALL_KICK_CW[4][5][2];

void bag_refill(Piece_Bag *bag);
int  bag_next(Piece_Bag *bag);

void rotate_cw(Tetromino *t);
void rotate_ccw(Tetromino *t);
bool rotate_with_kick(Game *g, bool clockwise, int *out_kick_index);

void set_tetromino_start(Tetromino *t);
void set_tetromino(Tetromino *t, int n);

#endif // TEXAS_TETROMINO_H
