package tetromino

// SRS wall-kick tables.
//
// Offsets are written exactly as published, with +x right and +y UP. They are
// converted to playfield coordinates (+y down) by kickOffsets.
//
// Each row is the clockwise transition starting from that direction:
// 0->R, R->2, 2->L, L->0. A counter-clockwise transition is the negation of
// the clockwise transition that starts at its destination (for example
// R->0 = -(0->R)).
//
// References:
//   - https://tetris.wiki/Super_Rotation_System
//   - https://harddrop.com/wiki/SRS
var (
	jlstzKicksCW = [4][5]Point{
		{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}, // 0->R
		{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},     // R->2
		{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},    // 2->L
		{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},  // L->0
	}
	iKicksCW = [4][5]Point{
		{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}, // 0->R
		{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}, // R->2
		{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}, // 2->L
		{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}, // L->0
	}
)

// kickOffsets returns the five SRS kick tests, in playfield coordinates, for
// rotating a piece of kind k out of direction from.
func kickOffsets(k Kind, from Direction, clockwise bool) [5]Point {
	table := &jlstzKicksCW
	if k == I {
		table = &iKicksCW
	}

	var out [5]Point
	if clockwise {
		for i, off := range table[from] {
			out[i] = Point{X: off.X, Y: -off.Y}
		}
		return out
	}
	for i, off := range table[from.CCW()] {
		out[i] = Point{X: -off.X, Y: off.Y}
	}
	return out
}
