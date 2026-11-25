package tetromino

import (
	"fmt"
	"math/rand/v2"
	"slices"
	"testing"
)

func countBlocks(p Piece) int {
	n := 0
	for range p.Blocks() {
		n++
	}
	return n
}

func TestSpawnShapes(t *testing.T) {
	// J has its hook at the top-left, L at the top-right.
	j, l := New(J).Shape, New(L).Shape
	if j[0][0] != J || j[0][2] != None || j[1] != [4]Kind{J, J, J, None} {
		t.Errorf("J spawn shape = %v", j)
	}
	if l[0][0] != None || l[0][2] != L || l[1] != [4]Kind{L, L, L, None} {
		t.Errorf("L spawn shape = %v", l)
	}

	for k := I; k <= L; k++ {
		p := New(k)
		if p.Dir != North {
			t.Errorf("New(%v).Dir = %v, want North", k, p.Dir)
		}
		if n := countBlocks(p); n != 4 {
			t.Errorf("New(%v) has %d blocks, want 4", k, n)
		}
		for pt := range p.Blocks() {
			if got := p.Shape[pt.Y][pt.X]; got != k {
				t.Errorf("New(%v) cell %v = %v, want %v", k, pt, got, k)
			}
		}
	}
}

func TestNewInvalidKind(t *testing.T) {
	for _, k := range []Kind{None, L + 1} {
		if got := New(k); got != (Piece{}) {
			t.Errorf("New(%v) = %+v, want zero Piece", k, got)
		}
	}
}

func TestRotationRoundTrip(t *testing.T) {
	for k := I; k <= L; k++ {
		orig := New(k)
		p := orig
		for range 4 {
			p = p.RotatedCW()
		}
		if p != orig {
			t.Errorf("%v: four CW turns = %+v, want %+v", k, p, orig)
		}
		if got := orig.RotatedCW().RotatedCCW(); got != orig {
			t.Errorf("%v: CW then CCW = %+v, want %+v", k, got, orig)
		}
	}
}

func TestORotationIsNoOp(t *testing.T) {
	o := New(O)
	if got, kick, ok := o.Rotate(true, func(Piece) bool { return true }); ok || kick != -1 || got != o {
		t.Errorf("O.Rotate = (%+v, %d, %v), want unchanged, -1, false", got, kick, ok)
	}
}

// wallFits reports whether every block of p lies in columns >= 0.
func wallFits(p Piece) bool {
	for pt := range p.Blocks() {
		if pt.X < 0 {
			return false
		}
	}
	return true
}

func TestRotateWallKick(t *testing.T) {
	// A T pointing East with its stem flush against the left wall must kick
	// one cell right (test 1) to rotate into South.
	p := New(T).RotatedCW()
	p.X = -1
	if !wallFits(p) {
		t.Fatalf("setup piece does not fit")
	}

	got, kick, ok := p.Rotate(true, wallFits)
	if !ok {
		t.Fatalf("Rotate failed")
	}
	if kick != 1 || got.X != 0 || got.Dir != South {
		t.Errorf("Rotate = kick %d, X %d, Dir %v; want kick 1, X 0, Dir South", kick, got.X, got.Dir)
	}
}

func TestRotateNoFit(t *testing.T) {
	p := New(T)
	got, kick, ok := p.Rotate(false, func(Piece) bool { return false })
	if ok || kick != -1 || got != p {
		t.Errorf("Rotate = (%+v, %d, %v), want unchanged, -1, false", got, kick, ok)
	}
}

func TestKickOffsetsCCWNegatesCW(t *testing.T) {
	for _, k := range []Kind{I, T} {
		for d := North; d <= West; d++ {
			cw := kickOffsets(k, d, true)
			ccw := kickOffsets(k, d.CW(), false)
			for i := range cw {
				if ccw[i].X != -cw[i].X || ccw[i].Y != -cw[i].Y {
					t.Errorf("%v %v: ccw[%d] = %v, want -%v", k, d, i, ccw[i], cw[i])
				}
			}
		}
	}
}

func TestBagDealsEachKindOncePerSeven(t *testing.T) {
	bag := NewBag(rand.New(rand.NewPCG(1, 2)))
	for round := range 10 {
		got := make([]Kind, 0, Count)
		for range Count {
			got = append(got, bag.Next())
		}
		slices.Sort(got)
		if want := []Kind{I, O, T, S, Z, J, L}; !slices.Equal(got, want) {
			t.Errorf("round %d: bag dealt %v, want a permutation of %v", round, got, want)
		}
	}
}

func FuzzRotate(f *testing.F) {
	f.Add(uint8(T), []byte{1, 0, 1, 1})
	f.Add(uint8(I), []byte{0, 0, 0})
	f.Fuzz(func(t *testing.T, kind uint8, turns []byte) {
		k := Kind(kind%Count) + I
		p := New(k)
		var cw, ccw int
		for _, b := range turns {
			if b&1 == 1 {
				p = p.RotatedCW()
				cw++
			} else {
				p = p.RotatedCCW()
				ccw++
			}
			if n := countBlocks(p); n != 4 {
				t.Fatalf("%v has %d blocks after rotation, want 4", k, n)
			}
		}
		if k != O {
			if want := Direction(((cw-ccw)%4 + 4) % 4); p.Dir != want {
				t.Fatalf("%v: Dir = %v, want %v", k, p.Dir, want)
			}
		}
	})
}

func ExamplePiece_Rotate() {
	// Rotate a T piece on an open 10-wide field.
	inBounds := func(p Piece) bool {
		for pt := range p.Blocks() {
			if pt.X < 0 || pt.X >= 10 || pt.Y < 0 {
				return false
			}
		}
		return true
	}
	p, kick, ok := New(T).Moved(3, 0).Rotate(true, inBounds)
	fmt.Println(p.Dir, kick, ok)
	// Output: East 0 true
}
