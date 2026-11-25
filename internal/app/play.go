package app

import (
	"fmt"
	"math"

	rl "github.com/gen2brain/raylib-go/raylib"

	"github.com/sspencer/texas/internal/game"
	"github.com/sspencer/texas/internal/layout"
	"github.com/sspencer/texas/internal/tetromino"
)

const (
	cell        = float32(layout.BlockSize)
	boardCenter = layout.BoardWidthPx / 2
)

// drawBoardContents draws everything inside the playfield. It runs in board
// space: the origin is the top-left visible cell.
func drawBoardContents(g *game.Game) {
	board := g.Board()
	for vy := range game.VisibleHeight {
		for x := range game.Width {
			r := rl.NewRectangle(float32(x)*cell, float32(vy)*cell, cell, cell)
			rl.DrawRectangleRec(r, colorPanel)
			if k := board[vy+game.HiddenRows][x]; k != tetromino.None {
				drawBlock(r.X, r.Y, cell, pieceColor(k))
			} else {
				rl.DrawRectangleLinesEx(r, 0.5, colorGrid)
			}
		}
	}

	switch g.State() {
	case game.StatePlaying, game.StatePaused:
		// Ghost first so the active piece covers any overlap.
		drawGhost(g.Ghost())
		drawPiece(g.Active())
	case game.StateLineClear:
		drawLineClearFlash(g)
	case game.StateGameOver:
		drawCurtain(float64(g.StateTimer()) / float64(game.GameOverDuration))
		if g.StateTimer() <= 0 {
			drawGameOverBox(g)
		}
	case game.StateCountdown:
		drawCountdown(g)
	case game.StateTitle:
	}

	drawBanner(g.Banner())
}

// visibleBlocks yields the on-screen cells of p in board space.
func visibleBlocks(p tetromino.Piece, yield func(x, y float32)) {
	for pt := range p.Blocks() {
		vy := pt.Y - game.HiddenRows
		if vy >= 0 && vy < game.VisibleHeight {
			yield(float32(pt.X)*cell, float32(vy)*cell)
		}
	}
}

func drawPiece(p tetromino.Piece) {
	col := pieceColor(p.Kind)
	visibleBlocks(p, func(x, y float32) { drawBlock(x, y, cell, col) })
}

func drawGhost(p tetromino.Piece) {
	if p.Kind == tetromino.None {
		return
	}
	base := pieceColor(p.Kind)
	fill, outline := withAlpha(base, 48), withAlpha(base, 195)
	visibleBlocks(p, func(x, y float32) {
		r := rl.NewRectangle(x+1, y+1, cell-2, cell-2)
		rl.DrawRectangleRounded(r, 0.35, 16, fill)
		rl.DrawRectangleRoundedLinesEx(r, 0.35, 16, 2, outline)
	})
}

func drawLineClearFlash(g *game.Game) {
	progress := 1 - float64(g.ClearRemaining())/float64(game.LineClearDuration)
	alpha := uint8(min(max((1-progress)*230, 40), 230))
	for _, y := range g.ClearingRows() {
		vy := y - game.HiddenRows
		if vy >= 0 && vy < game.VisibleHeight {
			rl.DrawRectangleRec(rl.NewRectangle(0, float32(vy)*cell, layout.BoardWidthPx, cell), rl.NewColor(255, 255, 255, alpha))
		}
	}
}

// drawCurtain fills the playfield with curtain blocks from the bottom up,
// leaving the fraction pct of cells uncovered.
func drawCurtain(pct float64) {
	const total = game.Width * game.VisibleHeight
	n := total - int(min(max(pct, 0), 1)*total)
	if n <= 0 {
		return
	}
	fullRows, remainder := n/game.Width, n%game.Width
	top := game.VisibleHeight - fullRows
	for y := game.VisibleHeight - 1; y >= top; y-- {
		for x := range game.Width {
			drawCurtainSegment(x, y)
		}
	}
	if top > 0 {
		for x := range remainder {
			drawCurtainSegment(x, top-1)
		}
	}
}

func drawCurtainSegment(x, y int) {
	r := rl.NewRectangle(float32(x)*cell, float32(y)*cell, cell-1, cell-1)
	rl.DrawRectangleRec(r, rl.DarkGray)
	shade := uint8(224)
	rl.DrawRectangleLinesEx(r, 1, rl.NewColor(shade, shade, shade, 255))
	shade -= 32
	const rings = 6
	for i := range rings {
		r.X, r.Y, r.Width, r.Height = r.X+2, r.Y+2, r.Width-4, r.Height-4
		if i == rings-1 {
			rl.DrawRectangleRec(r, rl.Black)
			break
		}
		shade -= 32
		rl.DrawRectangleLinesEx(r, 1, rl.NewColor(shade, shade, shade, 255))
	}
}

func drawGameOverBox(g *game.Game) {
	box := rl.NewRectangle(12, 150, layout.BoardWidthPx-24, 216)
	rl.DrawRectangleRounded(box, 0.08, 8, rl.NewColor(0, 0, 0, 220))
	rl.DrawRectangleRoundedLinesEx(box, 0.08, 8, 2, colorBorder)

	y := int32(box.Y)
	drawTextCentered("GAME OVER", boardCenter, y+22, 34, rl.NewColor(235, 70, 75, 255))
	drawTextCentered(fmt.Sprintf("Score: %d", g.Stats().Score), boardCenter, y+72, 22, rl.White)
	drawTextCentered("<SPACE> Restart", boardCenter, y+116, 20, rl.White)
	drawTextCentered("<T> Title Screen", boardCenter, y+152, 20, colorTextMuted)
}

func drawCountdown(g *game.Game) {
	remaining := g.StateTimer().Seconds()
	launch := game.CountdownDuration.Seconds()
	drawCurtain(1 - (remaining-1)/(launch-1))

	count := int(math.Floor(remaining))
	if count <= 0 {
		return
	}
	box := rl.NewRectangle(40, 190, layout.BoardWidthPx-80, 140)
	rl.DrawRectangleRounded(box, 0.1, 8, rl.NewColor(0, 0, 0, 200))
	label := "1."
	switch {
	case count >= 3:
		label = "3..."
	case count == 2:
		label = "2.."
	}
	drawTextCentered(label, boardCenter, int32(box.Y)+22, 96, rl.White)
}

func drawBanner(b game.Banner) {
	if !b.Visible() {
		return
	}
	alpha := uint8(min(max(float64(b.Remaining)/float64(game.BannerDuration)*255, 0), 255))

	const titleSize, y = 24, 18
	drawTextCentered(b.Title, boardCenter+2, y+2, titleSize, rl.NewColor(0, 0, 0, alpha))
	drawTextCentered(b.Title, boardCenter, y, titleSize, rl.NewColor(255, 240, 120, alpha))

	if b.Subtitle != "" {
		const subSize, sy = 16, y + titleSize + 4
		drawTextCentered(b.Subtitle, boardCenter+1, sy+1, subSize, rl.NewColor(0, 0, 0, alpha))
		drawTextCentered(b.Subtitle, boardCenter, sy, subSize, rl.NewColor(130, 230, 255, alpha))
	}
}

// drawPreview draws p centered in box with the given cell size.
func drawPreview(p tetromino.Piece, box rl.Rectangle, size float32, dimmed bool) {
	if p.Kind == tetromino.None {
		return
	}
	p.X, p.Y = 0, 0
	minX, minY, maxX, maxY := 4, 4, -1, -1
	for pt := range p.Blocks() {
		minX, maxX = min(minX, pt.X), max(maxX, pt.X)
		minY, maxY = min(minY, pt.Y), max(maxY, pt.Y)
	}
	if maxX < minX {
		return
	}

	ox := box.X + (box.Width-float32(maxX-minX+1)*size)/2
	oy := box.Y + (box.Height-float32(maxY-minY+1)*size)/2
	col := pieceColor(p.Kind)
	if dimmed {
		col = rl.NewColor(col.R/3+35, col.G/3+35, col.B/3+35, 190)
	}
	for pt := range p.Blocks() {
		drawBlock(ox+float32(pt.X-minX)*size, oy+float32(pt.Y-minY)*size, size, col)
	}
}

var controls = [...]struct{ keys, action string }{
	{"Left / Right", "Move"},
	{"Down", "Soft Drop"},
	{"Space", "Hard Drop"},
	{"Up / X / Z", "Rotate"},
	{"C / Shift", "Hold"},
	{"F / F11", "Fullscreen"},
	{"P", "Pause"},
}

// drawSidePanel draws the stats, hold/next, and controls cards in UI space.
func drawSidePanel(g *game.Game) {
	const (
		x          = float32(layout.PanelX)
		w          = float32(layout.PanelWidth)
		statsH     = 174
		piecesH    = 266
		controlsH  = 152
		cardGap    = 12
		piecesTop  = layout.OuterMargin + statsH + cardGap
		controlTop = piecesTop + piecesH + cardGap
	)
	stats := g.Stats()

	// Stats card.
	inner := drawPanelCard(rl.NewRectangle(x, layout.OuterMargin, w, statsH))
	tx, ty := int32(inner.X+12), int32(inner.Y+10)
	drawText(fmt.Sprintf("Level: %d", stats.Level), tx, ty, 22, rl.White)
	bar := rl.NewRectangle(float32(tx), float32(ty+28), 16, 10)
	for i := range 10 {
		col := rl.DarkGray
		if i < stats.Lines%10 {
			col = colorGreen
		}
		rl.DrawRectangleRounded(bar, 0.3, 4, col)
		bar.X += bar.Width + 4
	}
	ty += 48
	drawText(fmt.Sprintf("Score: %d", stats.Score), tx, ty, 22, rl.White)
	ty += 30
	drawText(fmt.Sprintf("Best:  %d", stats.HighScore), tx, ty, 18, colorGold)
	ty += 26
	drawText(fmt.Sprintf("Lines: %d", stats.Lines), tx, ty, 18, colorTextMuted)

	// Hold and next card.
	inner = drawPanelCard(rl.NewRectangle(x, piecesTop, w, piecesH))
	half := inner.Width / 2
	divX := inner.X + half
	rl.DrawLineEx(rl.NewVector2(divX, inner.Y+8), rl.NewVector2(divX, inner.Y+inner.Height-8), 1, colorGrid)

	leftCenter, rightCenter := int32(inner.X+half/2), int32(divX+half/2)
	drawTextCentered("HOLD", leftCenter, int32(inner.Y+10), 18, colorTextMuted)
	holdBox := rl.NewRectangle(inner.X+8, inner.Y+38, half-16, 80)
	rl.DrawRectangleLinesEx(holdBox, 1, colorGrid)
	held, used := g.Hold()
	drawPreview(held, holdBox, 20, used)

	badgeY := int32(holdBox.Y + holdBox.Height + 16)
	if stats.BackToBack {
		drawTextCentered("B2B", leftCenter, badgeY, 16, rl.NewColor(255, 190, 50, 255))
		badgeY += 22
	}
	if stats.Combo > 0 {
		drawTextCentered(fmt.Sprintf("COMBO x%d", stats.Combo), leftCenter, badgeY, 15, rl.NewColor(80, 220, 255, 255))
	}

	drawTextCentered("NEXT", rightCenter, int32(inner.Y+10), 18, colorTextMuted)
	slotY := inner.Y + 38
	for i, p := range g.Next() {
		slotH, size := float32(62), float32(17)
		if i == 0 {
			slotH, size = 72, 20
		}
		slot := rl.NewRectangle(divX+8, slotY, half-16, slotH)
		if i == 0 {
			rl.DrawRectangleLinesEx(slot, 1, colorGrid)
		}
		drawPreview(p, slot, size, false)
		slotY += slotH + 6
	}

	// Controls card.
	inner = drawPanelCard(rl.NewRectangle(x, controlTop, w, controlsH))
	cx, cy := int32(inner.X+12), int32(inner.Y+6)
	for _, c := range controls {
		drawText(c.keys, cx, cy, 14, rl.White)
		drawText(c.action, cx+110, cy, 14, colorTextMuted)
		cy += 18
	}
}

// drawStatusOverlay shows the pause screen or debug marker over the board,
// in UI space.
func drawStatusOverlay(g *game.Game) {
	var msg string
	switch {
	case g.State() == game.StatePaused:
		msg = "PAUSED"
		rl.DrawRectangleRec(rl.NewRectangle(layout.BoardLeft, layout.BoardTop, layout.BoardWidthPx, layout.BoardHeightPx), rl.NewColor(0, 0, 0, 180))
	case g.Debug() && g.State() == game.StatePlaying:
		msg = "DEBUG (D)"
	default:
		return
	}

	const size = 34
	cx := int32(layout.BoardLeft + layout.BoardWidthPx/2)
	cy := int32(layout.BoardTop + layout.BoardHeightPx/2 - size/2)
	drawTextCentered(msg, cx, cy, size, rl.White)
	if g.State() == game.StatePaused {
		drawTextCentered("P: Resume   T: Title", cx, cy+44, 18, colorTextMuted)
	}
}
