// Package geom provides the small 2D value types shared by the layout,
// rendering, and generative backdrop packages.
//
// The field names deliberately mirror raylib's Vector2 and Rectangle so the
// platform layer can convert between them with a plain type conversion.
package geom

// Vec2 is a point or vector in 2D space.
type Vec2 struct {
	X, Y float32
}

// Pt is shorthand for Vec2{X: x, Y: y}.
func Pt(x, y float32) Vec2 {
	return Vec2{X: x, Y: y}
}

// Rect is an axis-aligned rectangle with its origin at the top-left corner.
type Rect struct {
	X, Y, Width, Height float32
}

// R is shorthand for Rect{X: x, Y: y, Width: w, Height: h}.
func R(x, y, w, h float32) Rect {
	return Rect{X: x, Y: y, Width: w, Height: h}
}

// Contains reports whether p lies inside r, edges included.
func (r Rect) Contains(p Vec2) bool {
	return p.X >= r.X && p.X <= r.X+r.Width && p.Y >= r.Y && p.Y <= r.Y+r.Height
}

// Center returns the midpoint of r.
func (r Rect) Center() Vec2 {
	return Pt(r.X+r.Width*0.5, r.Y+r.Height*0.5)
}

// Inset returns r shrunk by d on every side.
func (r Rect) Inset(d float32) Rect {
	return R(r.X+d, r.Y+d, r.Width-2*d, r.Height-2*d)
}
