package game

import (
	"slices"
	"testing"

	"github.com/sspencer/texas/internal/tetromino"
)

func TestBoardOccupiedBounds(t *testing.T) {
	var b Board
	for _, pt := range []tetromino.Point{{X: -1, Y: 0}, {X: Width, Y: 0}, {X: 0, Y: -1}, {X: 0, Y: Height}} {
		if !b.Occupied(pt.X, pt.Y) {
			t.Errorf("Occupied(%d,%d) = false, want true outside board", pt.X, pt.Y)
		}
	}
	if b.Occupied(0, 0) {
		t.Error("empty cell reported occupied")
	}
}

func TestBoardClearFullRows(t *testing.T) {
	var b Board
	fill := func(y int) {
		for x := range Width {
			b[y][x] = tetromino.L
		}
	}
	fill(Height - 1)
	fill(Height - 2)
	fill(Height - 4)
	b[Height-3][0] = tetromino.T // survivor between full rows
	b[Height-5][9] = tetromino.S // survivor above

	if got, want := b.FullRows(nil), []int{Height - 4, Height - 2, Height - 1}; !slices.Equal(got, want) {
		t.Fatalf("FullRows = %v, want %v", got, want)
	}
	if n := b.ClearFullRows(); n != 3 {
		t.Fatalf("ClearFullRows = %d, want 3", n)
	}
	if b[Height-1][0] != tetromino.T || b[Height-2][9] != tetromino.S {
		t.Error("surviving rows not shifted down correctly")
	}
	if len(b.FullRows(nil)) != 0 {
		t.Error("full rows remain after clear")
	}
}

func TestBoardPlaceLockOut(t *testing.T) {
	var b Board
	p := tetromino.New(tetromino.O)
	p.Y = HiddenRows - 2
	if b.Place(p) {
		t.Error("piece entirely in hidden rows reported visible")
	}
	p.Y = HiddenRows - 1
	if !b.Place(p) {
		t.Error("piece reaching visible rows reported hidden")
	}
}

func TestGarbageRowsClamp(t *testing.T) {
	if GarbageRows(-1) != 0 || GarbageRows(99) != 12 || GarbageRows(2) != 5 {
		t.Error("GarbageRows clamp/lookup wrong")
	}
}
