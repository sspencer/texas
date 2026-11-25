package backdrop

import (
	"image/color"

	"github.com/sspencer/texas/internal/geom"
)

// Vec2 is the point type used by Canvas.
type Vec2 = geom.Vec2

// Canvas is the set of drawing primitives a Backdrop needs. Coordinates are in
// screen space with +x right and +y down; rotation angles are in degrees.
type Canvas interface {
	// GradientRect fills r with a bilinear gradient between its corners.
	GradientRect(r geom.Rect, topLeft, bottomLeft, bottomRight, topRight color.RGBA)
	// VerticalGradient fills r blending from top to bottom.
	VerticalGradient(r geom.Rect, top, bottom color.RGBA)
	// Triangle fills a triangle whose vertices are in counter-clockwise order.
	Triangle(a, b, c Vec2, col color.RGBA)
	// TriangleStrip fills a strip of triangles over consecutive vertices.
	TriangleStrip(points []Vec2, col color.RGBA)
	// Line draws a segment of the given thickness.
	Line(from, to Vec2, thick float32, col color.RGBA)
	// Circle fills a circle.
	Circle(center Vec2, radius float32, col color.RGBA)
	// CircleOutline strokes a circle.
	CircleOutline(center Vec2, radius float32, col color.RGBA)
	// RadialGradient fills a circle blending from inner at the center to
	// outer at the rim.
	RadialGradient(center Vec2, radius float32, inner, outer color.RGBA)
	// Polygon fills a regular polygon.
	Polygon(center Vec2, sides int, radius, rotation float32, col color.RGBA)
	// PolygonOutline strokes a regular polygon.
	PolygonOutline(center Vec2, sides int, radius, rotation, thick float32, col color.RGBA)
}

// Draw renders the backdrop across a width x height viewport.
func (b *Backdrop) Draw(c Canvas, width, height float32) {
	if width <= 0 || height <= 0 {
		return
	}

	// 1. Subtle four-corner atmospheric gradient base.
	p := b.palette
	c.GradientRect(geom.R(0, 0, width, height), p.TopLeft, p.BottomLeft, p.BottomRight, p.TopRight)

	// 2. The active generative pattern.
	switch b.style {
	case Constellation:
		b.drawConstellation(c, width, height)
	case FlowWaves:
		b.drawFlowWaves(c, width, height)
	case GeoDrift:
		b.drawGeoDrift(c, width, height)
	case OrbitalContours:
		b.drawOrbitalContours(c, width, height)
	case TruchetLattice:
		b.drawTruchetLattice(c, width, height)
	}

	// 3. Soft edge framing so the outer margins recede behind the board.
	dark := color.RGBA{R: 8, G: 12, B: 20, A: 48}
	clear := color.RGBA{R: 8, G: 12, B: 20}
	band := max(height*0.14, 24)
	c.VerticalGradient(geom.R(0, 0, ceil(width), ceil(band)), dark, clear)
	c.VerticalGradient(geom.R(0, floor(height-band), ceil(width), ceil(band)+1), clear, dark)
}

// buffer returns a scratch slice of length n, reusing earlier allocations.
func (b *Backdrop) buffer(n int) []Vec2 {
	if cap(b.scratch) < n {
		b.scratch = make([]Vec2, n)
	}
	return b.scratch[:n]
}

// triangleCCW fills a triangle, reordering vertices for counter-clockwise
// winding in screen space.
func triangleCCW(c Canvas, a, b, d Vec2, col color.RGBA) {
	cross := (b.X-a.X)*(d.Y-a.Y) - (b.Y-a.Y)*(d.X-a.X)
	if cross < 0 {
		c.Triangle(a, b, d, col)
	} else {
		c.Triangle(a, d, b, col)
	}
}

// Arc stroke parameters for the Truchet lattice.
const (
	arcSegments = 14
	arcThick    = 1.4
)

// arc strokes a circular arc as a polyline, with angles in radians
// (0 = +x, π/2 = +y).
func arc(c Canvas, center Vec2, radius, start, end float32, col color.RGBA) {
	const segments, thick = arcSegments, arcThick
	if radius <= 0 {
		return
	}
	prev := geom.Pt(center.X+cos(start)*radius, center.Y+sin(start)*radius)
	step := (end - start) / float32(segments)
	for i := 1; i <= segments; i++ {
		a := start + float32(i)*step
		cur := geom.Pt(center.X+cos(a)*radius, center.Y+sin(a)*radius)
		c.Line(prev, cur, thick, col)
		prev = cur
	}
}

func distSq(a, b Vec2) float32 {
	dx, dy := b.X-a.X, b.Y-a.Y
	return dx*dx + dy*dy
}

func (b *Backdrop) drawConstellation(c Canvas, width, height float32) {
	const maxFacets = 20

	linkDist := clamp(min(width, height)*0.25, 110, 260)
	linkDistSq := linkDist * linkDist
	triDist := linkDist * 0.72
	triDistSq := triDist * triDist

	pos := b.buffer(len(b.nodes))
	for i, n := range b.nodes {
		pos[i] = geom.Pt(n.x*width, n.y*height)
	}

	// Subtle translucent mesh facets, capped to keep the composition airy.
	facets := 0
outer:
	for i := range pos {
		for j := i + 1; j < len(pos); j++ {
			dij := distSq(pos[i], pos[j])
			if dij > triDistSq {
				continue
			}
			for k := j + 1; k < len(pos); k++ {
				if facets >= maxFacets {
					break outer
				}
				dik, djk := distSq(pos[i], pos[k]), distSq(pos[j], pos[k])
				if dik > triDistSq || djk > triDistSq {
					continue
				}
				fade := clamp(1-sqrt(max(dij, dik, djk))/triDist, 0, 1)
				if alpha := uint8(fade * fade * 14); alpha > 0 {
					col := withAlpha(b.palette.accent(b.nodes[i].colorIdx), alpha)
					triangleCCW(c, pos[i], pos[j], pos[k], col)
					facets++
				}
			}
		}
	}

	// Connecting edges fading smoothly with distance.
	for i := range pos {
		for j := i + 1; j < len(pos); j++ {
			d2 := distSq(pos[i], pos[j])
			if d2 >= linkDistSq {
				continue
			}
			fade := 1 - sqrt(d2)/linkDist
			if alpha := uint8(fade * fade * 40); alpha > 1 {
				c.Line(pos[i], pos[j], 1.1, withAlpha(b.palette.accent(b.nodes[i].colorIdx), alpha))
			}
		}
	}

	// Nodes with whisper-soft halos.
	t := float32(b.elapsed)
	for i, n := range b.nodes {
		pulse := 0.8 + 0.2*sin(t*0.65+n.phase)
		accent := b.palette.accent(n.colorIdx)
		c.Circle(pos[i], n.radius*2.3, withAlpha(accent, uint8(12*pulse)))
		c.Circle(pos[i], n.radius, withAlpha(accent, uint8(36*pulse)))
	}
}

func (b *Backdrop) drawFlowWaves(c Canvas, width, height float32) {
	const samples = 44
	t := float32(b.elapsed)

	buf := b.buffer(samples * 3)
	strip, crest := buf[:samples*2], buf[samples*2:]

	for _, w := range b.waves {
		accent := b.palette.accent(w.colorIdx)
		halfThick := w.thickness * height * 0.5

		for s := range samples {
			u := -0.02 + 1.04*(float32(s)/float32(samples-1))
			x := u * width
			yNorm := w.baseY +
				(u-0.5)*w.tilt +
				w.amp1*sin(u*w.freq1*tau+t*w.speed1+w.phase1) +
				w.amp2*cos(u*w.freq2*tau+t*w.speed2+w.phase2)
			cy := yNorm * height
			ht := halfThick * (0.78 + 0.22*sin(u*2.3*tau-t*0.18+w.phase1))

			// Top-then-bottom ordering yields CCW triangles with +y down.
			strip[s*2] = geom.Pt(x, cy-ht)
			strip[s*2+1] = geom.Pt(x, cy+ht)
			crest[s] = geom.Pt(x, cy-ht)
		}

		c.TriangleStrip(strip, withAlpha(accent, 14))

		// Delicate crest line along the top edge of each ribbon.
		line := withAlpha(accent, 30)
		for s := range samples - 1 {
			c.Line(crest[s], crest[s+1], 1.2, line)
		}
	}
}

func (b *Backdrop) drawGeoDrift(c Canvas, width, height float32) {
	minDim := min(width, height)

	for _, s := range b.shapes {
		center := geom.Pt(s.x*width, s.y*height)
		radius := s.size * minDim
		accent := b.palette.accent(s.colorIdx)

		if s.sides == 0 {
			if s.filled {
				c.Circle(center, radius, withAlpha(accent, 14))
			}
			c.CircleOutline(center, radius, withAlpha(accent, 30))
			if s.nested {
				c.CircleOutline(center, radius*0.58, withAlpha(accent, 20))
			}
			continue
		}

		if s.filled {
			c.Polygon(center, s.sides, radius, s.angle, withAlpha(accent, 14))
		}
		c.PolygonOutline(center, s.sides, radius, s.angle, 1.3, withAlpha(accent, 32))
		if s.nested {
			c.PolygonOutline(center, s.sides, radius*0.58, s.angle+180/float32(s.sides), 1.0, withAlpha(accent, 20))
		}
	}
}

func (b *Backdrop) drawOrbitalContours(c Canvas, width, height float32) {
	const segments = 72
	t := float32(b.elapsed)
	minDim := min(width, height)
	pts := b.buffer(segments + 1)

	for _, o := range b.orbits {
		cx := (o.cx + cos(t*o.orbitSpeed+o.orbitPhase)*o.orbitRX) * width
		cy := (o.cy + sin(t*o.orbitSpeed*0.83+o.orbitPhase)*o.orbitRY) * height
		center := geom.Pt(cx, cy)
		accent := b.palette.accent(o.colorIdx)

		// Soft ambient radial glow at the focal point.
		glow := (o.baseRadius + float32(o.ringCount)*o.ringStep) * minDim * 0.85
		c.RadialGradient(center, glow, withAlpha(accent, 16), withAlpha(accent, 0))

		for ring := range o.ringCount {
			rBase := (o.baseRadius + float32(ring)*o.ringStep) * minDim
			phase := o.morphPhase + float32(ring)*0.65
			col := withAlpha(accent, uint8(clamp(32-ring*4, 14, 34)))

			for s := range pts {
				a := float32(s) / segments * tau
				wobble := 1 +
					o.harmA1*sin(o.harmK1*a+t*o.morphSpeed+phase) +
					o.harmA2*cos(o.harmK2*a-t*o.morphSpeed*0.72+phase)
				r := rBase * wobble
				pts[s] = geom.Pt(cx+cos(a)*r, cy+sin(a)*r)
			}
			for s := range segments {
				c.Line(pts[s], pts[s+1], 1.2, col)
			}
		}
	}
}

func (b *Backdrop) drawTruchetLattice(c Canvas, width, height float32) {
	t := float32(b.elapsed)
	cell := clamp(min(width, height)/8.5, 64, 128)
	half := cell * 0.5

	cols := int(ceil(width/cell)) + 1
	rows := int(ceil(height/cell)) + 1
	ox := (width - float32(cols)*cell) * 0.5
	oy := (height - float32(rows)*cell) * 0.5
	dirX, dirY := cos(b.lattice.angle), sin(b.lattice.angle)

	for r := range rows {
		for col := range cols {
			x0 := ox + float32(col)*cell
			y0 := oy + float32(r)*cell
			x1, y1 := x0+cell, y0+cell

			orient := b.lattice.cells[r%latticeRows][col%latticeCols]
			wave := 0.5 + 0.5*sin((float32(col)*dirX+float32(r)*dirY)*0.45-t*0.35+b.lattice.phase)
			alpha := uint8(12 + wave*22)
			accent := b.palette.accent(orient)
			ink := withAlpha(accent, alpha)

			switch b.lattice.motif {
			case quarterArcs:
				if orient%2 == 0 {
					// Top-left and bottom-right quarter circles.
					arc(c, geom.Pt(x0, y0), half, 0, pi*0.5, ink)
					arc(c, geom.Pt(x1, y1), half, pi, pi*1.5, ink)
				} else {
					// Top-right and bottom-left quarter circles.
					arc(c, geom.Pt(x1, y0), half, pi*0.5, pi, ink)
					arc(c, geom.Pt(x0, y1), half, pi*1.5, tau, ink)
				}

			case diagonalWeave:
				faint := withAlpha(accent, alpha/2)
				if orient%2 == 0 {
					c.Line(geom.Pt(x0, y0), geom.Pt(x1, y1), 1.3, ink)
					c.Line(geom.Pt(x0+half, y0), geom.Pt(x1, y0+half), 1.0, faint)
				} else {
					c.Line(geom.Pt(x1, y0), geom.Pt(x0, y1), 1.3, ink)
					c.Line(geom.Pt(x0, y0+half), geom.Pt(x0+half, y1), 1.0, faint)
				}

			case cornerTicks:
				// Minimalist architectural corner brackets and a center dot.
				arm := cell * 0.30
				var corner, h, v Vec2
				switch orient % 4 {
				case 0:
					corner, h, v = geom.Pt(x0, y0), geom.Pt(x0+arm, y0), geom.Pt(x0, y0+arm)
				case 1:
					corner, h, v = geom.Pt(x1, y0), geom.Pt(x1-arm, y0), geom.Pt(x1, y0+arm)
				case 2:
					corner, h, v = geom.Pt(x1, y1), geom.Pt(x1-arm, y1), geom.Pt(x1, y1-arm)
				default:
					corner, h, v = geom.Pt(x0, y1), geom.Pt(x0+arm, y1), geom.Pt(x0, y1-arm)
				}
				c.Line(h, corner, 1.3, ink)
				c.Line(corner, v, 1.3, ink)
				c.Circle(geom.Pt(x0+half, y0+half), 1.8, ink)
			}
		}
	}
}
