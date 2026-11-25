package app

import (
	rl "github.com/gen2brain/raylib-go/raylib"

	"github.com/sspencer/texas/internal/layout"
)

// Small drawing helpers shared by the title and play screens.

func drawText(s string, x, y, size int32, col rl.Color) {
	rl.DrawText(s, x, y, size, col)
}

// drawTextCentered draws s horizontally centered on cx.
func drawTextCentered(s string, cx, y, size int32, col rl.Color) {
	rl.DrawText(s, cx-rl.MeasureText(s, size)/2, y, size, col)
}

// drawTextRight draws s with its right edge at right.
func drawTextRight(s string, right, y, size int32, col rl.Color) {
	rl.DrawText(s, right-rl.MeasureText(s, size), y, size, col)
}

// drawBlock draws one rounded tetromino cell with a soft top highlight.
func drawBlock(x, y, size float32, col rl.Color) {
	rl.DrawRectangleRounded(rl.NewRectangle(x+0.5, y+0.5, size-1.5, size-1.5), 0.35, 16, col)

	lighten := func(v uint8) uint8 { return uint8(min(int(v)+55, 255)) }
	highlight := rl.NewColor(lighten(col.R), lighten(col.G), lighten(col.B), 120)
	inset := max(size*0.14, 2)
	rl.DrawRectangleRounded(rl.NewRectangle(x+inset, y+inset, size-2*inset-1, size*0.25), 0.5, 8, highlight)
}

// drawPanelCard draws a bordered card and returns its inner rectangle.
func drawPanelCard(r rl.Rectangle) rl.Rectangle {
	rl.DrawRectangleRec(r, colorBorder)
	inner := rl.NewRectangle(r.X+layout.BorderSize, r.Y+layout.BorderSize,
		r.Width-2*layout.BorderSize, r.Height-2*layout.BorderSize)
	rl.DrawRectangleRec(inner, colorPanel)
	return inner
}
