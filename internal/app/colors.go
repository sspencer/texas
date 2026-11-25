package app

import (
	rl "github.com/gen2brain/raylib-go/raylib"

	"github.com/sspencer/texas/internal/tetromino"
)

// UI palette.
var (
	colorBorder    = rl.NewColor(36, 36, 48, 255)
	colorPanel     = rl.NewColor(16, 18, 26, 255)
	colorGrid      = rl.NewColor(33, 36, 46, 255)
	colorTextMuted = rl.NewColor(170, 180, 195, 255)
	colorGold      = rl.NewColor(245, 210, 80, 255)
	colorGreen     = rl.NewColor(50, 215, 75, 255)
	colorCyan      = rl.NewColor(0, 210, 240, 255)
)

// pieceColors gives the Guideline color of each tetromino kind.
var pieceColors = [...]rl.Color{
	tetromino.None: rl.Black,
	tetromino.I:    rl.NewColor(0, 210, 240, 255),
	tetromino.O:    rl.NewColor(245, 220, 0, 255),
	tetromino.T:    rl.NewColor(175, 65, 235, 255),
	tetromino.S:    rl.NewColor(50, 215, 75, 255),
	tetromino.Z:    rl.NewColor(235, 50, 60, 255),
	tetromino.J:    rl.NewColor(45, 115, 245, 255),
	tetromino.L:    rl.NewColor(255, 150, 25, 255),
}

func pieceColor(k tetromino.Kind) rl.Color {
	if int(k) < len(pieceColors) {
		return pieceColors[k]
	}
	return rl.Magenta
}

func withAlpha(c rl.Color, a uint8) rl.Color {
	c.A = a
	return c
}
