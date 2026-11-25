package main

import "core:math/rand"
import rl "vendor:raylib"

Direction :: enum {
	North,
	East,
	South,
	West,
}

// Colors based on the Tetris Guideline:
//   https://tetris.wiki/Tetris_Guideline
Tetro_Color := [8]rl.Color{
	rl.BLACK, // 0: empty
	rl.Color{0, 210, 240, 255}, // 1: I (Cyan)
	rl.Color{245, 220, 0, 255}, // 2: O (Yellow)
	rl.Color{175, 65, 235, 255}, // 3: T (Purple)
	rl.Color{50, 215, 75, 255}, // 4: S (Green)
	rl.Color{235, 50, 60, 255}, // 5: Z (Red)
	rl.Color{45, 115, 245, 255}, // 6: J (Blue)
	rl.Color{255, 150, 25, 255}, // 7: L (Orange)
}

Tetromino :: struct {
	id:        int,
	x:         int,
	y:         int,
	direction: Direction,
	size:      int,
	shape:     matrix[4, 4]i32, // Indexed as shape[row, col]
}

// Standard SRS spawn orientations (Direction.North / state 0).
tetrominos: [8]Tetromino = {
	Tetromino{},
	// 1: I (4x4 matrix)
	Tetromino{
		id = 1,
		size = 4,
		direction = .North,
		shape = matrix[4, 4]i32{
			0, 0, 0, 0,
			1, 1, 1, 1,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
	// 2: O (2x2 matrix)
	Tetromino{
		id = 2,
		size = 2,
		direction = .North,
		shape = matrix[4, 4]i32{
			2, 2, 0, 0,
			2, 2, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
	// 3: T (3x3 matrix)
	Tetromino{
		id = 3,
		size = 3,
		direction = .North,
		shape = matrix[4, 4]i32{
			0, 3, 0, 0,
			3, 3, 3, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
	// 4: S (3x3 matrix)
	Tetromino{
		id = 4,
		size = 3,
		direction = .North,
		shape = matrix[4, 4]i32{
			0, 4, 4, 0,
			4, 4, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
	// 5: Z (3x3 matrix)
	Tetromino{
		id = 5,
		size = 3,
		direction = .North,
		shape = matrix[4, 4]i32{
			5, 5, 0, 0,
			0, 5, 5, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
	// 6: J (3x3 matrix)
	Tetromino{
		id = 6,
		size = 3,
		direction = .North,
		shape = matrix[4, 4]i32{
			6, 0, 0, 0,
			6, 6, 6, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
	// 7: L (3x3 matrix)
	Tetromino{
		id = 7,
		size = 3,
		direction = .North,
		shape = matrix[4, 4]i32{
			0, 0, 7, 0,
			7, 7, 7, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
}

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

References:
- https://tetris.wiki/Super_Rotation_System
- https://harddrop.com/wiki/SRS
*/
JLSTZ_WALL_KICK_CW := [4][5][2]int{
	{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}, // 0->R
	{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}, // R->2
	{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}, // 2->L
	{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}, // L->0
}

I_WALL_KICK_CW := [4][5][2]int{
	{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}, // 0->R
	{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}, // R->2
	{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}, // 2->L
	{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}, // L->0
}

// 7-Bag Random Generator (Tetris Guideline)
Piece_Bag :: struct {
	pieces: [7]int,
	index:  int,
}

bag_refill :: proc(bag: ^Piece_Bag) {
	bag.pieces = {1, 2, 3, 4, 5, 6, 7}
	// Fisher-Yates shuffle
	for i := 6; i > 0; i -= 1 {
		j := rand.int_max(i + 1)
		bag.pieces[i], bag.pieces[j] = bag.pieces[j], bag.pieces[i]
	}
	bag.index = 0
}

bag_next :: proc(bag: ^Piece_Bag) -> int {
	if bag.index >= len(bag.pieces) || bag.pieces[0] == 0 {
		bag_refill(bag)
	}
	id := bag.pieces[bag.index]
	bag.index += 1
	return id
}

// Rotates a tetromino 90 degrees clockwise in place.
rotate_cw :: proc(t: ^Tetromino) {
	if t.size <= 2 {
		return
	}

	t.direction = Direction((int(t.direction) + 1) % 4)

	rotated: matrix[4, 4]i32
	for r in 0 ..< t.size {
		for c in 0 ..< t.size {
			rotated[r, c] = t.shape[t.size - 1 - c, r]
		}
	}
	t.shape = rotated
}

// Rotates a tetromino 90 degrees counter-clockwise in place.
rotate_ccw :: proc(t: ^Tetromino) {
	if t.size <= 2 {
		return
	}

	t.direction = Direction((int(t.direction) + 3) % 4)

	rotated: matrix[4, 4]i32
	for r in 0 ..< t.size {
		for c in 0 ..< t.size {
			rotated[r, c] = t.shape[c, t.size - 1 - r]
		}
	}
	t.shape = rotated
}

// Rotates the active tetromino using SRS wall kicks.
// Returns whether rotation succeeded and the kick test index used (0..4).
rotate_with_kick :: proc(g: ^Game, clockwise: bool) -> (bool, int) {
	t := &g.tetro
	if t.size <= 2 {
		return false, -1
	}

	original := t^
	if clockwise {
		rotate_cw(t)
	} else {
		rotate_ccw(t)
	}

	kicks := I_WALL_KICK_CW if t.size == 4 else JLSTZ_WALL_KICK_CW
	state := int(original.direction) if clockwise else int(t.direction)

	for test_idx in 0 ..< 5 {
		offset := kicks[state][test_idx]
		dx: int
		dy: int
		if clockwise {
			dx = offset[0]
			dy = -offset[1] // Board +y is down
		} else {
			dx = -offset[0]
			dy = offset[1] // Negated kick + negated y = +offset[1]
		}

		t.x = original.x + dx
		t.y = original.y + dy

		if !collide(g, t^) {
			g.last_action_rotate = true
			g.last_kick_index = test_idx
			on_piece_manipulated(g)
			update_ghost(g)
			return true, test_idx
		}
	}

	t^ = original
	return false, -1
}

// Positions a tetromino at its Guideline spawn location at the top center of the visible board.
set_tetromino_start :: proc(t: ^Tetromino) {
	t.x = (GAME_WIDTH - t.size) / 2
	if t.size == 4 {
		// I-piece occupies row 1 of its 4x4 grid; place row 1 at the top visible row
		t.y = HIDDEN_ROWS - 1
	} else {
		t.y = HIDDEN_ROWS
	}
}

// Sets a tetromino to a specific shape (1=I, 2=O, 3=T, 4=S, 5=Z, 6=J, 7=L) at its spawn position.
set_tetromino :: proc(t: ^Tetromino, n: int) {
	idx := clamp(n, 0, len(tetrominos) - 1)
	t^ = tetrominos[idx]
	t.id = idx
	set_tetromino_start(t)
}
