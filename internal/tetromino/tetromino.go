// Package tetromino defines the seven Guideline tetrominoes, their Super
// Rotation System (SRS) rotation and wall-kick behavior, and the 7-bag
// randomizer.
//
// The package is purely logical: it knows nothing about the playfield beyond
// what callers tell it through a fit predicate, and nothing about rendering.
package tetromino

import "iter"

// Kind identifies a tetromino. The zero value, None, means "no piece" and is
// also used for empty playfield cells.
type Kind uint8

// The seven tetromino kinds, in Guideline order.
const (
	None Kind = iota
	I
	O
	T
	S
	Z
	J
	L
)

// Count is the number of playable kinds (I through L).
const Count = 7

var kindNames = [...]string{None: "None", I: "I", O: "O", T: "T", S: "S", Z: "Z", J: "J", L: "L"}

// String returns the single-letter name of k, or "None".
func (k Kind) String() string {
	if int(k) < len(kindNames) {
		return kindNames[k]
	}
	return "Kind(?)"
}

// Valid reports whether k is one of the seven playable kinds.
func (k Kind) Valid() bool {
	return k >= I && k <= L
}

// Size returns the edge length of the square bounding box the kind rotates
// within: 4 for I, 2 for O, 3 for the rest and 0 for None.
func (k Kind) Size() int {
	switch k {
	case None:
		return 0
	case I:
		return 4
	case O:
		return 2
	default:
		return 3
	}
}

// Direction is a piece's SRS orientation state.
type Direction uint8

// SRS orientation states. North is the spawn orientation ("0"), East is one
// clockwise turn ("R"), South two ("2"), and West three ("L").
const (
	North Direction = iota
	East
	South
	West
)

var directionNames = [...]string{"North", "East", "South", "West"}

// String returns the compass name of d.
func (d Direction) String() string {
	return directionNames[d%4]
}

// CW returns the direction one clockwise quarter turn from d.
func (d Direction) CW() Direction { return (d + 1) % 4 }

// CCW returns the direction one counter-clockwise quarter turn from d.
func (d Direction) CCW() Direction { return (d + 3) % 4 }

// Shape is a 4x4 occupancy grid indexed as Shape[row][col]. Occupied cells
// hold the piece's Kind; empty cells hold None.
type Shape [4][4]Kind

// Point is an integer cell coordinate with +x to the right and +y downward.
type Point struct {
	X, Y int
}

// Piece is a tetromino with a position and orientation. X and Y locate the
// top-left corner of its Shape grid on the playfield.
//
// Piece is a small value type: methods return modified copies rather than
// mutating the receiver.
type Piece struct {
	Kind  Kind
	X, Y  int
	Dir   Direction
	Shape Shape
}

// spawnShapes holds the standard SRS spawn (North) orientation of each kind.
var spawnShapes = [...]Shape{
	I: {
		{},
		{I, I, I, I},
	},
	O: {
		{O, O},
		{O, O},
	},
	T: {
		{None, T, None},
		{T, T, T},
	},
	S: {
		{None, S, S},
		{S, S, None},
	},
	Z: {
		{Z, Z, None},
		{None, Z, Z},
	},
	J: {
		{J, None, None},
		{J, J, J},
	},
	L: {
		{None, None, L},
		{L, L, L},
	},
}

// New returns a piece of kind k in its spawn orientation at the origin. An
// invalid kind yields the zero Piece.
func New(k Kind) Piece {
	if !k.Valid() {
		return Piece{}
	}
	return Piece{Kind: k, Dir: North, Shape: spawnShapes[k]}
}

// Size returns the edge length of the piece's rotation box.
func (p Piece) Size() int {
	return p.Kind.Size()
}

// Moved returns p translated by (dx, dy).
func (p Piece) Moved(dx, dy int) Piece {
	p.X += dx
	p.Y += dy
	return p
}

// Blocks yields the absolute playfield coordinates of each occupied cell, in
// row-major order.
func (p Piece) Blocks() iter.Seq[Point] {
	return func(yield func(Point) bool) {
		for r, row := range p.Shape {
			for c, cell := range row {
				if cell != None && !yield(Point{X: p.X + c, Y: p.Y + r}) {
					return
				}
			}
		}
	}
}

// RotatedCW returns p turned a quarter turn clockwise in place, without wall
// kicks. Pieces that do not rotate (O and None) are returned unchanged.
func (p Piece) RotatedCW() Piece {
	n := p.Size()
	if n <= 2 {
		return p
	}
	var out Shape
	for r := range n {
		for c := range n {
			out[r][c] = p.Shape[n-1-c][r]
		}
	}
	p.Shape = out
	p.Dir = p.Dir.CW()
	return p
}

// RotatedCCW returns p turned a quarter turn counter-clockwise in place,
// without wall kicks. Pieces that do not rotate (O and None) are returned
// unchanged.
func (p Piece) RotatedCCW() Piece {
	n := p.Size()
	if n <= 2 {
		return p
	}
	var out Shape
	for r := range n {
		for c := range n {
			out[r][c] = p.Shape[c][n-1-r]
		}
	}
	p.Shape = out
	p.Dir = p.Dir.CCW()
	return p
}

// Rotate turns p a quarter turn using the SRS wall-kick tests. fits reports
// whether a candidate placement is legal on the caller's playfield.
//
// On success it returns the rotated piece, the index (0-4) of the kick test
// that succeeded, and true. If the piece does not rotate or no kick test fits,
// it returns p unchanged, -1 and false.
func (p Piece) Rotate(clockwise bool, fits func(Piece) bool) (Piece, int, bool) {
	if p.Size() <= 2 {
		return p, -1, false
	}
	turned := p.RotatedCCW()
	if clockwise {
		turned = p.RotatedCW()
	}
	for i, off := range kickOffsets(p.Kind, p.Dir, clockwise) {
		candidate := turned
		candidate.X = p.X + off.X
		candidate.Y = p.Y + off.Y
		if fits(candidate) {
			return candidate, i, true
		}
	}
	return p, -1, false
}
