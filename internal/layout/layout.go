// Package layout defines the fixed logical coordinate space Texas is drawn
// in and the mapping from window pixels into it.
//
// All UI is laid out on a BaseWidth x BaseHeight canvas and uniformly scaled
// (letterboxed) to fit the window, so the game looks identical at any window
// size or display density.
package layout

import (
	"math"
	"sync"

	"github.com/sspencer/texas/internal/game"
	"github.com/sspencer/texas/internal/geom"
)

// Logical layout metrics, in UI units.
const (
	OuterMargin = 24
	BlockSize   = 30
	BorderSize  = 8
	PanelGap    = 20
	PanelWidth  = 252

	BoardWidthPx  = game.Width * BlockSize
	BoardHeightPx = game.VisibleHeight * BlockSize

	// BoardLeft and BoardTop locate the top-left visible cell of the
	// playfield.
	BoardLeft = OuterMargin + BorderSize
	BoardTop  = OuterMargin + BorderSize

	// PanelX is the left edge of the side panel.
	PanelX = BoardLeft + BoardWidthPx + BorderSize + PanelGap

	BaseWidth  = PanelX + PanelWidth + OuterMargin
	BaseHeight = BoardTop + BoardHeightPx + BorderSize + OuterMargin
)

// Viewport maps the logical canvas onto a window of a given size, preserving
// aspect ratio and centring the result.
type Viewport struct {
	Scale   float32
	OffsetX float32
	OffsetY float32
}

// NewViewport returns the viewport that fits the canvas into a w x h window.
// Non-positive sizes yield the identity viewport.
func NewViewport(w, h float32) Viewport {
	if w <= 0 || h <= 0 {
		return Viewport{Scale: 1}
	}
	s := min(w/BaseWidth, h/BaseHeight)
	return Viewport{
		Scale:   s,
		OffsetX: (w - BaseWidth*s) / 2,
		OffsetY: (h - BaseHeight*s) / 2,
	}
}

// ToUI converts a window position to logical UI coordinates.
func (v Viewport) ToUI(p geom.Vec2) geom.Vec2 {
	if v.Scale <= 0 {
		return p
	}
	return geom.Pt((p.X-v.OffsetX)/v.Scale, (p.Y-v.OffsetY)/v.Scale)
}

// FromUI converts logical UI coordinates to a window position.
func (v Viewport) FromUI(p geom.Vec2) geom.Vec2 {
	return geom.Pt(v.OffsetX+p.X*v.Scale, v.OffsetY+p.Y*v.Scale)
}

// WindowedSize picks a comfortable initial window size for a monitor of
// mw x mh pixels: about 82% of the monitor height, scaled between 1x and 1.5x
// of the base canvas and never wider than 85% of the monitor (but never below
// 1x).
func WindowedSize(mw, mh int) (w, h int) {
	if mw <= 0 || mh <= 0 {
		return BaseWidth, BaseHeight
	}
	scale := min(max(float64(mh)*0.82/BaseHeight, 1), 1.5)
	scale = max(min(scale, float64(mw)*0.85/BaseWidth), 1)
	return int(math.Round(BaseWidth * scale)), int(math.Round(BaseHeight * scale))
}

// TitleLayout holds the positions of the title screen's card and controls.
type TitleLayout struct {
	CardOuter, CardInner geom.Rect
	GridX, GridRight     float32
	DividerY             float32
	Speed                [game.MaxStartSpeed + 1]geom.Rect
	Height               [game.MaxStartHeight + 1]geom.Rect
	Start                geom.Rect
}

// Title returns the title screen layout.
var Title = sync.OnceValue(func() *TitleLayout {
	const (
		btnW, btnH, btnGap = 86, 46, 14
		gridW              = 5*btnW + 4*btnGap
		heightGap          = 12
		heightW            = (gridW - 5*heightGap) / 6.0
	)

	var l TitleLayout
	l.CardOuter = geom.R(OuterMargin, 174, BaseWidth-2*OuterMargin, 404)
	l.CardInner = l.CardOuter.Inset(BorderSize)
	in := l.CardInner

	l.GridX = in.X + (in.Width-gridW)/2
	l.GridRight = l.GridX + gridW
	l.DividerY = in.Y + 174

	for i := range l.Speed {
		row, col := float32(i/5), float32(i%5)
		l.Speed[i] = geom.R(l.GridX+col*(btnW+btnGap), in.Y+52+row*(btnH+btnGap), btnW, btnH)
	}
	for i := range l.Height {
		l.Height[i] = geom.R(l.GridX+float32(i)*(heightW+heightGap), l.DividerY+52, heightW, btnH)
	}
	l.Start = geom.R(l.GridX, in.Y+in.Height-64, gridW, btnH)
	return &l
})

// HitTest returns the control at logical position p.
func (l *TitleLayout) HitTest(p geom.Vec2) game.Target {
	for i, r := range l.Speed {
		if r.Contains(p) {
			return game.Target{Kind: game.TargetSpeed, Value: i}
		}
	}
	for i, r := range l.Height {
		if r.Contains(p) {
			return game.Target{Kind: game.TargetHeight, Value: i}
		}
	}
	if l.Start.Contains(p) {
		return game.Target{Kind: game.TargetStart}
	}
	return game.Target{}
}
