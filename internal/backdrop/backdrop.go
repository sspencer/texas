// Package backdrop generates the subtle, slowly animating procedural art drawn
// behind the playfield.
//
// Each Backdrop picks one of several styles (constellation meshes, flowing
// waves, drifting geometry, orbital contours, or a Truchet lattice) and a
// muted palette, then animates gently over time. Rendering goes through the
// Canvas interface so this package stays independent of any graphics library
// and can be tested headlessly.
package backdrop

import (
	"image/color"
	"math/rand/v2"
	"time"
)

// Style selects the generative pattern a Backdrop draws.
type Style uint8

// The available backdrop styles.
const (
	Constellation Style = iota
	FlowWaves
	GeoDrift
	OrbitalContours
	TruchetLattice

	numStyles = iota
)

var styleNames = [...]string{"Constellation", "FlowWaves", "GeoDrift", "OrbitalContours", "TruchetLattice"}

// String returns the name of s.
func (s Style) String() string {
	if int(s) < len(styleNames) {
		return styleNames[s]
	}
	return "Style(?)"
}

// Styles returns every available style.
func Styles() []Style {
	out := make([]Style, numStyles)
	for i := range out {
		out[i] = Style(i)
	}
	return out
}

// Palette is a backdrop's color scheme: a four-corner background gradient
// and three accent colors for the pattern.
type Palette struct {
	TopLeft, TopRight, BottomLeft, BottomRight color.RGBA
	AccentA, AccentB, AccentC                  color.RGBA
}

// accent returns one of the three accent colors, cycling on idx.
func (p Palette) accent(idx uint8) color.RGBA {
	switch idx % 3 {
	case 0:
		return p.AccentA
	case 1:
		return p.AccentB
	default:
		return p.AccentC
	}
}

// palettes are curated low-contrast, dark, muted schemes so patterns stay calm
// and never compete with gameplay.
var palettes = [...]Palette{
	{ // Slate Twilight (homage to the original slate blue-gray backdrop)
		TopLeft: rgb(28, 40, 56), TopRight: rgb(34, 48, 66), BottomLeft: rgb(42, 56, 74), BottomRight: rgb(32, 46, 62),
		AccentA: rgb(96, 126, 158), AccentB: rgb(76, 106, 138), AccentC: rgb(118, 144, 172),
	},
	{ // Midnight Indigo
		TopLeft: rgb(26, 30, 50), TopRight: rgb(34, 38, 60), BottomLeft: rgb(40, 44, 68), BottomRight: rgb(30, 34, 54),
		AccentA: rgb(98, 108, 152), AccentB: rgb(80, 90, 136), AccentC: rgb(120, 128, 166),
	},
	{ // Deep Fjord (muted teal-slate)
		TopLeft: rgb(24, 42, 50), TopRight: rgb(30, 50, 58), BottomLeft: rgb(36, 56, 64), BottomRight: rgb(28, 46, 54),
		AccentA: rgb(84, 126, 136), AccentB: rgb(68, 108, 118), AccentC: rgb(108, 144, 152),
	},
	{ // Dusk Plum (dark charcoal-violet)
		TopLeft: rgb(34, 30, 46), TopRight: rgb(42, 36, 54), BottomLeft: rgb(48, 42, 60), BottomRight: rgb(38, 34, 50),
		AccentA: rgb(114, 100, 136), AccentB: rgb(94, 84, 118), AccentC: rgb(132, 118, 150),
	},
	{ // Quiet Pine (dark sage-slate)
		TopLeft: rgb(26, 40, 40), TopRight: rgb(32, 48, 46), BottomLeft: rgb(38, 54, 52), BottomRight: rgb(30, 44, 44),
		AccentA: rgb(86, 118, 114), AccentB: rgb(72, 102, 98), AccentC: rgb(108, 136, 130),
	},
	{ // Warm Charcoal (smoky graphite)
		TopLeft: rgb(34, 36, 44), TopRight: rgb(42, 44, 52), BottomLeft: rgb(48, 50, 58), BottomRight: rgb(38, 40, 48),
		AccentA: rgb(112, 114, 126), AccentB: rgb(96, 100, 114), AccentC: rgb(130, 128, 138),
	},
}

func rgb(r, g, b uint8) color.RGBA { return color.RGBA{R: r, G: g, B: b, A: 255} }

func withAlpha(c color.RGBA, a uint8) color.RGBA {
	c.A = a
	return c
}

// Pattern capacity limits and lattice dimensions.
const (
	minNodes, maxNodes   = 28, 38
	minWaves, maxWaves   = 5, 7
	minShapes, maxShapes = 18, 26
	minOrbits, maxOrbits = 2, 4
	latticeCols          = 16
	latticeRows          = 10

	// wrapMargin is how far (as a fraction of the viewport) drifting
	// elements travel past an edge before wrapping to the opposite side.
	wrapMargin = 0.12
)

type node struct {
	x, y, vx, vy  float32
	radius, phase float32
	colorIdx      uint8
}

type wave struct {
	baseY, amp1, amp2, freq1, freq2 float32
	speed1, speed2, phase1, phase2  float32
	thickness, tilt                 float32
	colorIdx                        uint8
}

type shape struct {
	x, y, vx, vy          float32
	size, angle, rotSpeed float32
	sides                 int // 0 = circle/ring, 3..8 = regular polygon
	filled, nested        bool
	colorIdx              uint8
}

type orbit struct {
	cx, cy, orbitRX, orbitRY       float32
	orbitSpeed, orbitPhase         float32
	baseRadius, ringStep           float32
	ringCount                      int
	harmK1, harmK2, harmA1, harmA2 float32
	morphSpeed, morphPhase         float32
	colorIdx                       uint8
}

type latticeMotif uint8

const (
	quarterArcs latticeMotif = iota
	diagonalWeave
	cornerTicks

	numMotifs = iota
)

type lattice struct {
	motif        latticeMotif
	cells        [latticeRows][latticeCols]uint8
	phase, angle float32
}

// Backdrop is the state of one generative backdrop. Create one with New.
//
// A Backdrop is not safe for concurrent use.
type Backdrop struct {
	rng     *rand.Rand
	style   Style
	palette Palette
	elapsed float64 // seconds since the pattern was generated

	nodes   []node
	waves   []wave
	shapes  []shape
	orbits  []orbit
	lattice lattice

	// scratch is reused between frames to avoid per-frame allocation.
	scratch []Vec2
}

// New returns a Backdrop with a random style and palette, drawing randomness
// from rng.
func New(rng *rand.Rand) *Backdrop {
	b := &Backdrop{rng: rng}
	b.Reset(Style(rng.IntN(numStyles)))
	return b
}

// Style returns the active style.
func (b *Backdrop) Style() Style { return b.style }

// Palette returns the active palette.
func (b *Backdrop) Palette() Palette { return b.palette }

// Elapsed returns the animation time since the pattern was generated.
func (b *Backdrop) Elapsed() time.Duration {
	return time.Duration(b.elapsed * float64(time.Second))
}

// Regenerate randomizes the backdrop with a style different from the current
// one, so consecutive games always look distinct.
func (b *Backdrop) Regenerate() {
	offset := 1 + b.rng.IntN(numStyles-1)
	b.Reset((b.style + Style(offset)) % numStyles)
}

// Reset regenerates the backdrop with the given style and a freshly
// randomized palette and pattern, restarting the animation clock.
func (b *Backdrop) Reset(style Style) {
	b.style = style
	b.elapsed = 0
	b.nodes, b.waves, b.shapes, b.orbits = b.nodes[:0], b.waves[:0], b.shapes[:0], b.orbits[:0]

	base := palettes[b.rng.IntN(len(palettes))]
	b.palette = Palette{
		TopLeft:     b.jitter(base.TopLeft, 4),
		TopRight:    b.jitter(base.TopRight, 4),
		BottomLeft:  b.jitter(base.BottomLeft, 4),
		BottomRight: b.jitter(base.BottomRight, 4),
		AccentA:     b.jitter(base.AccentA, 8),
		AccentB:     b.jitter(base.AccentB, 8),
		AccentC:     b.jitter(base.AccentC, 8),
	}

	switch style {
	case Constellation:
		for range b.between(minNodes, maxNodes) {
			b.nodes = append(b.nodes, node{
				x:        b.uniform(-0.04, 1.04),
				y:        b.uniform(-0.04, 1.04),
				vx:       b.signed(0.003, 0.011),
				vy:       b.signed(0.003, 0.011),
				radius:   b.uniform(1.8, 3.6),
				phase:    b.uniform(0, tau),
				colorIdx: uint8(b.rng.IntN(3)),
			})
		}

	case FlowWaves:
		n := b.between(minWaves, maxWaves)
		for i := range n {
			t := (float32(i) + 0.5) / float32(n)
			b.waves = append(b.waves, wave{
				baseY:     clamp(t+b.uniform(-0.06, 0.06), 0.08, 0.92),
				amp1:      b.uniform(0.03, 0.08),
				amp2:      b.uniform(0.012, 0.035),
				freq1:     b.uniform(1.1, 2.4),
				freq2:     b.uniform(2.6, 4.8),
				speed1:    b.signed(0.08, 0.22),
				speed2:    b.signed(0.10, 0.26),
				phase1:    b.uniform(0, tau),
				phase2:    b.uniform(0, tau),
				thickness: b.uniform(0.06, 0.14),
				tilt:      b.uniform(-0.10, 0.10),
				colorIdx:  uint8(i % 3),
			})
		}

	case GeoDrift:
		sides := [...]int{0, 3, 4, 5, 6, 8}
		for range b.between(minShapes, maxShapes) {
			b.shapes = append(b.shapes, shape{
				x:        b.uniform(-0.05, 1.05),
				y:        b.uniform(-0.05, 1.05),
				vx:       b.signed(0.0025, 0.009),
				vy:       b.signed(0.0025, 0.009),
				size:     b.uniform(0.035, 0.105),
				angle:    b.uniform(0, 360),
				rotSpeed: b.signed(1.8, 6.5),
				sides:    sides[b.rng.IntN(len(sides))],
				filled:   b.rng.IntN(100) < 42,
				nested:   b.rng.IntN(100) < 40,
				colorIdx: uint8(b.rng.IntN(3)),
			})
		}

	case OrbitalContours:
		n := b.between(minOrbits, maxOrbits)
		for i := range n {
			// Spread focal centers across different regions of the viewport.
			sector := (float32(i) + 0.5) / float32(n)
			b.orbits = append(b.orbits, orbit{
				cx:         clamp(sector+b.uniform(-0.14, 0.14), 0.12, 0.88),
				cy:         b.uniform(0.20, 0.80),
				orbitRX:    b.uniform(0.03, 0.09),
				orbitRY:    b.uniform(0.03, 0.09),
				orbitSpeed: b.signed(0.05, 0.13),
				orbitPhase: b.uniform(0, tau),
				baseRadius: b.uniform(0.11, 0.20),
				ringStep:   b.uniform(0.048, 0.075),
				ringCount:  3 + b.rng.IntN(3),
				harmK1:     float32(2 + b.rng.IntN(4)),
				harmK2:     float32(3 + b.rng.IntN(5)),
				harmA1:     b.uniform(0.04, 0.10),
				harmA2:     b.uniform(0.025, 0.07),
				morphSpeed: b.signed(0.12, 0.25),
				morphPhase: b.uniform(0, tau),
				colorIdx:   uint8(i % 3),
			})
		}

	case TruchetLattice:
		b.lattice.motif = latticeMotif(b.rng.IntN(numMotifs))
		b.lattice.phase = b.uniform(0, tau)
		b.lattice.angle = b.uniform(0, tau)
		for r := range b.lattice.cells {
			for c := range b.lattice.cells[r] {
				b.lattice.cells[r][c] = uint8(b.rng.IntN(4))
			}
		}
	}
}

// Update advances the backdrop's ambient motion by dt.
func (b *Backdrop) Update(dt time.Duration) {
	if dt <= 0 {
		return
	}
	b.elapsed += dt.Seconds()
	step := float32(dt.Seconds())

	switch b.style {
	case Constellation:
		for i := range b.nodes {
			n := &b.nodes[i]
			n.x = wrap(n.x + n.vx*step)
			n.y = wrap(n.y + n.vy*step)
		}
	case GeoDrift:
		for i := range b.shapes {
			s := &b.shapes[i]
			s.x = wrap(s.x + s.vx*step)
			s.y = wrap(s.y + s.vy*step)
			s.angle = fmod(s.angle+s.rotSpeed*step, 360)
			if s.angle < 0 {
				s.angle += 360
			}
		}
	case FlowWaves, OrbitalContours, TruchetLattice:
		// Purely time-driven parametric motion derived from elapsed.
	}
}

// uniform returns a random float32 in [lo, hi).
func (b *Backdrop) uniform(lo, hi float32) float32 {
	return lo + b.rng.Float32()*(hi-lo)
}

// signed returns a random magnitude in [minAbs, maxAbs) with a random sign.
func (b *Backdrop) signed(minAbs, maxAbs float32) float32 {
	m := b.uniform(minAbs, maxAbs)
	if b.rng.IntN(2) == 0 {
		return m
	}
	return -m
}

// between returns a random int in [lo, hi].
func (b *Backdrop) between(lo, hi int) int {
	return lo + b.rng.IntN(hi-lo+1)
}

// jitter nudges each RGB channel of c by up to ±delta, keeping it muted.
func (b *Backdrop) jitter(c color.RGBA, delta int) color.RGBA {
	shift := func(v uint8) uint8 {
		n := int(v) + b.rng.IntN(delta*2+1) - delta
		return uint8(clamp(n, 14, 180))
	}
	return color.RGBA{R: shift(c.R), G: shift(c.G), B: shift(c.B), A: c.A}
}

// wrap folds v into [-wrapMargin, 1+wrapMargin].
func wrap(v float32) float32 {
	const lo, hi = -wrapMargin, 1 + wrapMargin
	const span = hi - lo
	for v < lo {
		v += span
	}
	for v > hi {
		v -= span
	}
	return v
}
