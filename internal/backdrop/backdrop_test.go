package backdrop

import (
	"image/color"
	"math"
	"math/rand/v2"
	"testing"
	"time"

	"github.com/sspencer/texas/internal/geom"
)

func newTestBackdrop(t *testing.T) *Backdrop {
	t.Helper()
	return New(rand.New(rand.NewPCG(7, 11)))
}

func isFinite(v float32) bool {
	f := float64(v)
	return !math.IsNaN(f) && !math.IsInf(f, 0)
}

func TestStylesStayMutedAndBounded(t *testing.T) {
	const lo, hi = -wrapMargin - 0.001, 1 + wrapMargin + 0.001
	inBounds := func(v float32) bool { return isFinite(v) && v >= lo && v <= hi }

	for _, style := range Styles() {
		t.Run(style.String(), func(t *testing.T) {
			b := newTestBackdrop(t)
			b.Reset(style)
			if b.Style() != style || b.Elapsed() != 0 {
				t.Fatalf("after Reset: style %v elapsed %v", b.Style(), b.Elapsed())
			}

			// The base gradient must stay dark so gameplay remains the focus.
			p := b.Palette()
			for _, c := range []color.RGBA{p.TopLeft, p.TopRight, p.BottomLeft, p.BottomRight} {
				if c.R > 90 || c.G > 90 || c.B > 95 || c.A != 255 {
					t.Errorf("background color %v is not dark and opaque", c)
				}
			}

			// Two minutes of ambient animation.
			for range 240 {
				b.Update(500 * time.Millisecond)
			}
			if b.Elapsed() < 119*time.Second {
				t.Errorf("Elapsed() = %v, want about 2m", b.Elapsed())
			}

			switch style {
			case Constellation:
				if n := len(b.nodes); n < minNodes || n > maxNodes {
					t.Errorf("node count %d outside [%d, %d]", n, minNodes, maxNodes)
				}
				for _, n := range b.nodes {
					if !inBounds(n.x) || !inBounds(n.y) {
						t.Errorf("node drifted out of bounds: (%v, %v)", n.x, n.y)
					}
				}
			case FlowWaves:
				if n := len(b.waves); n < minWaves || n > maxWaves {
					t.Errorf("wave count %d outside [%d, %d]", n, minWaves, maxWaves)
				}
			case GeoDrift:
				if n := len(b.shapes); n < minShapes || n > maxShapes {
					t.Errorf("shape count %d outside [%d, %d]", n, minShapes, maxShapes)
				}
				for _, s := range b.shapes {
					if !inBounds(s.x) || !inBounds(s.y) || !isFinite(s.angle) || s.angle < 0 || s.angle >= 360 {
						t.Errorf("shape out of bounds: (%v, %v) angle %v", s.x, s.y, s.angle)
					}
				}
			case OrbitalContours:
				if n := len(b.orbits); n < minOrbits || n > maxOrbits {
					t.Errorf("orbit count %d outside [%d, %d]", n, minOrbits, maxOrbits)
				}
			case TruchetLattice:
				for _, row := range b.lattice.cells {
					for _, cell := range row {
						if cell >= 4 {
							t.Errorf("lattice cell %d out of range", cell)
						}
					}
				}
			}
		})
	}
}

func TestRegenerateChangesStyle(t *testing.T) {
	b := newTestBackdrop(t)
	for range 50 {
		prev := b.Style()
		b.Update(time.Second)
		b.Regenerate()
		if b.Style() == prev {
			t.Fatalf("Regenerate kept style %v", prev)
		}
		if b.Elapsed() != 0 {
			t.Fatalf("Regenerate did not reset the clock: %v", b.Elapsed())
		}
	}
}

// recorder is a Canvas that counts calls and checks every coordinate is finite.
type recorder struct {
	t     *testing.T
	calls int
}

func (r *recorder) check(pts ...Vec2) {
	r.t.Helper()
	r.calls++
	for _, p := range pts {
		if !isFinite(p.X) || !isFinite(p.Y) {
			r.t.Fatalf("non-finite coordinate %v", p)
		}
	}
}

func (r *recorder) GradientRect(rc geom.Rect, _, _, _, _ color.RGBA) { r.check(geom.Pt(rc.X, rc.Y)) }
func (r *recorder) VerticalGradient(rc geom.Rect, _, _ color.RGBA)   { r.check(geom.Pt(rc.X, rc.Y)) }
func (r *recorder) Triangle(a, b, c Vec2, _ color.RGBA)              { r.check(a, b, c) }
func (r *recorder) TriangleStrip(pts []Vec2, _ color.RGBA)           { r.check(pts...) }
func (r *recorder) Line(a, b Vec2, _ float32, _ color.RGBA)          { r.check(a, b) }
func (r *recorder) Circle(c Vec2, _ float32, _ color.RGBA)           { r.check(c) }
func (r *recorder) CircleOutline(c Vec2, _ float32, _ color.RGBA)    { r.check(c) }
func (r *recorder) RadialGradient(c Vec2, _ float32, _, _ color.RGBA) {
	r.check(c)
}
func (r *recorder) Polygon(c Vec2, _ int, _, _ float32, _ color.RGBA) { r.check(c) }
func (r *recorder) PolygonOutline(c Vec2, _ int, _, _, _ float32, _ color.RGBA) {
	r.check(c)
}

func TestDrawEveryStyle(t *testing.T) {
	for _, style := range Styles() {
		t.Run(style.String(), func(t *testing.T) {
			b := newTestBackdrop(t)
			b.Reset(style)
			b.Update(3 * time.Second)

			rec := &recorder{t: t}
			b.Draw(rec, 1280, 720)
			// Gradient base + two edge bands + at least one pattern primitive.
			if rec.calls < 4 {
				t.Errorf("Draw made %d canvas calls, want at least 4", rec.calls)
			}

			empty := &recorder{t: t}
			b.Draw(empty, 0, 720)
			if empty.calls != 0 {
				t.Errorf("Draw on empty viewport made %d calls, want 0", empty.calls)
			}
		})
	}
}

func BenchmarkDraw(b *testing.B) {
	for _, style := range Styles() {
		b.Run(style.String(), func(b *testing.B) {
			bd := New(rand.New(rand.NewPCG(1, 1)))
			bd.Reset(style)
			c := nopCanvas{}
			b.ReportAllocs()
			for b.Loop() {
				bd.Update(time.Second / 120)
				bd.Draw(c, 1920, 1080)
			}
		})
	}
}

type nopCanvas struct{}

func (nopCanvas) GradientRect(geom.Rect, color.RGBA, color.RGBA, color.RGBA, color.RGBA) {}
func (nopCanvas) VerticalGradient(geom.Rect, color.RGBA, color.RGBA)                     {}
func (nopCanvas) Triangle(Vec2, Vec2, Vec2, color.RGBA)                                  {}
func (nopCanvas) TriangleStrip([]Vec2, color.RGBA)                                       {}
func (nopCanvas) Line(Vec2, Vec2, float32, color.RGBA)                                   {}
func (nopCanvas) Circle(Vec2, float32, color.RGBA)                                       {}
func (nopCanvas) CircleOutline(Vec2, float32, color.RGBA)                                {}
func (nopCanvas) RadialGradient(Vec2, float32, color.RGBA, color.RGBA)                   {}
func (nopCanvas) Polygon(Vec2, int, float32, float32, color.RGBA)                        {}
func (nopCanvas) PolygonOutline(Vec2, int, float32, float32, float32, color.RGBA)        {}
