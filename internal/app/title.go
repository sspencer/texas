package app

import (
	"fmt"
	"strconv"

	rl "github.com/gen2brain/raylib-go/raylib"

	"github.com/sspencer/texas/internal/game"
	"github.com/sspencer/texas/internal/layout"
	"github.com/sspencer/texas/internal/tetromino"
)

// digitLabels avoids formatting tile labels every frame.
var digitLabels = func() (l [10]string) {
	for i := range l {
		l[i] = strconv.Itoa(i)
	}
	return l
}()

func drawTitle(g *game.Game) {
	const cx = layout.BaseWidth / 2
	l := layout.Title()
	menu := g.Menu()

	// Logo with drop shadow.
	drawTextCentered("TEXAS", cx+4, 28, 80, rl.NewColor(16, 20, 30, 220))
	drawTextCentered("TEXAS", cx, 24, 80, rl.White)

	// A swatch of the seven piece colors.
	const swatch, gap = 24, 8
	x := float32(layout.BaseWidth-(tetromino.Count*swatch+(tetromino.Count-1)*gap)) / 2
	for k := tetromino.I; k <= tetromino.L; k++ {
		drawBlock(x, 110, swatch, pieceColor(k))
		x += swatch + gap
	}

	drawTextCentered("SELECT STARTING SPEED & HEIGHT", cx, 146, 16, colorTextMuted)

	card := drawPanelCard(rect(l.CardOuter))
	gridX, gridRight := int32(l.GridX), int32(l.GridRight)

	// Speed row.
	speedActive := menu.Focus == game.FieldSpeed
	speedAccent := colorTextMuted
	if speedActive {
		speedAccent = colorCyan
	}
	drawFieldHeading("SPEED", gridX, int32(card.Y+18), speedActive, speedAccent)
	info := fmt.Sprintf("Level %d  (%.2fs/row)", menu.Speed, game.GravityInterval(menu.Speed).Seconds())
	drawTextRight(info, gridRight, int32(card.Y+22), 16, speedAccent)
	for i, r := range l.Speed {
		hovered := menu.Hover == game.Target{Kind: game.TargetSpeed, Value: i}
		drawOptionTile(rect(r), digitLabels[i], i == menu.Speed, speedActive, hovered)
	}

	rl.DrawLineEx(rl.NewVector2(l.GridX, l.DividerY), rl.NewVector2(l.GridRight, l.DividerY), 1, colorGrid)

	// Height row.
	heightActive := menu.Focus == game.FieldHeight
	heightAccent := colorTextMuted
	if heightActive {
		heightAccent = colorGold
	}
	divY := int32(l.DividerY)
	drawFieldHeading("HEIGHT", gridX, divY+16, heightActive, heightAccent)
	info = "Clear Board"
	if rows := game.GarbageRows(menu.Height); rows > 0 {
		info = fmt.Sprintf("%d Garbage Rows", rows)
	}
	drawTextRight(info, gridRight, divY+20, 16, heightAccent)
	for i, r := range l.Height {
		hovered := menu.Hover == game.Target{Kind: game.TargetHeight, Value: i}
		drawOptionTile(rect(r), digitLabels[i], i == menu.Height, heightActive, hovered)
	}

	// Start button.
	start := rect(l.Start)
	bg, border := withAlpha(colorGreen, 50), withAlpha(colorGreen, 220)
	if menu.Hover.Kind == game.TargetStart {
		bg, border = withAlpha(colorGreen, 110), rl.White
	}
	rl.DrawRectangleRounded(start, 0.25, 8, bg)
	rl.DrawRectangleRoundedLinesEx(start, 0.25, 8, 2, border)
	drawTextCentered("START GAME  (ENTER / SPACE)", int32(start.X+start.Width/2), int32(start.Y+14), 18, rl.White)

	if hs := g.Stats().HighScore; hs > 0 {
		drawTextCentered(fmt.Sprintf("HIGH SCORE: %d", hs), cx, 588, 18, colorGold)
	}

	drawTextCentered("Click or Arrows / 0-9: Choose Speed & Height", cx, 616, 14, colorTextMuted)
	drawTextCentered("Click Start or Enter / Space    F: Fullscreen    Esc: Quit", cx, 638, 14, colorTextMuted)
}

func drawFieldHeading(label string, x, y int32, active bool, accent rl.Color) {
	col := colorTextMuted
	if active {
		drawText(">", x-22, y, 22, accent)
		col = rl.White
	}
	drawText(label, x, y, 22, col)
}

func drawOptionTile(r rl.Rectangle, label string, selected, rowActive, hovered bool) {
	bg, border, text := rl.NewColor(28, 32, 46, 255), colorGrid, colorTextMuted
	switch {
	case selected && (rowActive || hovered):
		bg, border, text = rl.NewColor(0, 175, 210, 255), rl.White, rl.NewColor(12, 16, 24, 255)
	case selected:
		bg, border, text = rl.NewColor(52, 66, 92, 255), rl.NewColor(140, 165, 200, 255), rl.White
	case hovered:
		bg, border, text = rl.NewColor(46, 54, 76, 255), rl.NewColor(150, 195, 225, 255), rl.White
	}

	thick := float32(1)
	if selected || hovered {
		thick = 2
	}
	rl.DrawRectangleRounded(r, 0.25, 8, bg)
	rl.DrawRectangleRoundedLinesEx(r, 0.25, 8, thick, border)

	const size = 22
	drawTextCentered(label, int32(r.X+r.Width/2), int32(r.Y+(r.Height-size)/2), size, text)
}
