package game

import (
	"slices"

	"github.com/sspencer/texas/internal/tetromino"
)

// Board is the playfield grid, indexed as Board[row][col] with row 0 at the
// top of the hidden buffer. Each cell holds the kind of the piece that filled
// it, or tetromino.None when empty.
type Board [Height][Width]tetromino.Kind

// Occupied reports whether the cell at (x, y) is filled or lies outside the
// playfield.
func (b *Board) Occupied(x, y int) bool {
	if x < 0 || x >= Width || y < 0 || y >= Height {
		return true
	}
	return b[y][x] != tetromino.None
}

// Fits reports whether p can occupy its position without overlapping filled
// cells or leaving the playfield.
func (b *Board) Fits(p tetromino.Piece) bool {
	for pt := range p.Blocks() {
		if b.Occupied(pt.X, pt.Y) {
			return false
		}
	}
	return true
}

// Place writes p's blocks into the board. It reports whether any block landed
// in the visible area; a piece locked entirely in the hidden rows is a
// Guideline "lock out".
func (b *Board) Place(p tetromino.Piece) (visible bool) {
	for pt := range p.Blocks() {
		if pt.Y >= HiddenRows {
			visible = true
		}
		if pt.X >= 0 && pt.X < Width && pt.Y >= 0 && pt.Y < Height {
			b[pt.Y][pt.X] = p.Kind
		}
	}
	return visible
}

func (b *Board) rowFull(y int) bool {
	return !slices.Contains(b[y][:], tetromino.None)
}

// FullRows appends the indices of all completely filled rows, top to bottom,
// to dst and returns the extended slice.
func (b *Board) FullRows(dst []int) []int {
	for y := range Height {
		if b.rowFull(y) {
			dst = append(dst, y)
		}
	}
	return dst
}

// ClearFullRows removes every filled row, shifting the rows above down, and
// returns the number of rows removed.
func (b *Board) ClearFullRows() int {
	cleared := 0
	for y := Height - 1; y >= 0; {
		if !b.rowFull(y) {
			y--
			continue
		}
		copy(b[1:y+1], b[:y])
		b[0] = [Width]tetromino.Kind{}
		cleared++
	}
	return cleared
}

// Reset empties the board.
func (b *Board) Reset() {
	*b = Board{}
}
