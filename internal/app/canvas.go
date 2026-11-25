package app

import (
	"image/color"

	rl "github.com/gen2brain/raylib-go/raylib"

	"github.com/sspencer/texas/internal/backdrop"
	"github.com/sspencer/texas/internal/geom"
)

// canvas implements backdrop.Canvas with raylib draw calls.
type canvas struct {
	buf []rl.Vector2 // reused across TriangleStrip calls to avoid allocation
}

var _ backdrop.Canvas = (*canvas)(nil)

func vec(p geom.Vec2) rl.Vector2    { return rl.Vector2(p) }
func rect(r geom.Rect) rl.Rectangle { return rl.Rectangle(r) }

func (*canvas) GradientRect(r geom.Rect, tl, bl, br, tr color.RGBA) {
	rl.DrawRectangleGradientEx(rect(r), tl, bl, br, tr)
}

func (*canvas) VerticalGradient(r geom.Rect, top, bottom color.RGBA) {
	rl.DrawRectangleGradientV(int32(r.X), int32(r.Y), int32(r.Width), int32(r.Height), top, bottom)
}

func (*canvas) Triangle(a, b, c geom.Vec2, col color.RGBA) {
	rl.DrawTriangle(vec(a), vec(b), vec(c), col)
}

func (cv *canvas) TriangleStrip(points []geom.Vec2, col color.RGBA) {
	cv.buf = cv.buf[:0]
	for _, p := range points {
		cv.buf = append(cv.buf, vec(p))
	}
	rl.DrawTriangleStrip(cv.buf, col)
}

func (*canvas) Line(from, to geom.Vec2, thick float32, col color.RGBA) {
	rl.DrawLineEx(vec(from), vec(to), thick, col)
}

func (*canvas) Circle(center geom.Vec2, radius float32, col color.RGBA) {
	rl.DrawCircleV(vec(center), radius, col)
}

func (*canvas) CircleOutline(center geom.Vec2, radius float32, col color.RGBA) {
	rl.DrawCircleLinesV(vec(center), radius, col)
}

func (*canvas) RadialGradient(center geom.Vec2, radius float32, inner, outer color.RGBA) {
	rl.DrawCircleGradient(vec(center), radius, inner, outer)
}

func (*canvas) Polygon(center geom.Vec2, sides int, radius, rotation float32, col color.RGBA) {
	rl.DrawPoly(vec(center), int32(sides), radius, rotation, col)
}

func (*canvas) PolygonOutline(center geom.Vec2, sides int, radius, rotation, thick float32, col color.RGBA) {
	rl.DrawPolyLinesEx(vec(center), int32(sides), radius, rotation, thick, col)
}
