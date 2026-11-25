package layout

import (
	"testing"

	"github.com/sspencer/texas/internal/game"
	"github.com/sspencer/texas/internal/geom"
)

func TestBaseSize(t *testing.T) {
	if BaseWidth != 636 || BaseHeight != 664 {
		t.Errorf("base size = %dx%d, want 636x664", BaseWidth, BaseHeight)
	}
}

func TestViewport(t *testing.T) {
	if v := NewViewport(BaseWidth, BaseHeight); v != (Viewport{Scale: 1}) {
		t.Errorf("native viewport = %+v, want identity", v)
	}
	if v := NewViewport(0, 100); v != (Viewport{Scale: 1}) {
		t.Errorf("degenerate viewport = %+v, want identity", v)
	}

	v := NewViewport(1920, 1080)
	wantScale := float32(1080) / BaseHeight
	if v.Scale != wantScale || abs(v.OffsetY) > 1e-3 || abs(v.OffsetX-(1920-BaseWidth*wantScale)/2) > 1e-3 {
		t.Errorf("1080p viewport = %+v", v)
	}

	p := geom.Pt(100, 200)
	if got := v.ToUI(v.FromUI(p)); abs(got.X-p.X) > 1e-3 || abs(got.Y-p.Y) > 1e-3 {
		t.Errorf("round trip %v -> %v", p, got)
	}
}

func abs(f float32) float32 { return max(f, -f) }

func TestWindowedSize(t *testing.T) {
	tests := []struct {
		mw, mh, w, h int
	}{
		{0, 0, BaseWidth, BaseHeight},
		{1280, 720, BaseWidth, BaseHeight}, // small monitor: never below 1x
		{2560, 1440, 954, 996},             // capped at 1.5x
		{1920, 1080, 848, 886},             // 82% of height
		{700, 2000, BaseWidth, BaseHeight}, // narrow: width cap, floor of 1x
	}
	for _, tt := range tests {
		if w, h := WindowedSize(tt.mw, tt.mh); w != tt.w || h != tt.h {
			t.Errorf("WindowedSize(%d, %d) = %dx%d, want %dx%d", tt.mw, tt.mh, w, h, tt.w, tt.h)
		}
	}
}

func TestTitleLayoutGeometry(t *testing.T) {
	l := Title()
	if l != Title() {
		t.Error("Title should be computed once")
	}
	if l.CardOuter != geom.R(24, 174, 588, 404) {
		t.Errorf("card = %+v", l.CardOuter)
	}
	if w := l.GridRight - l.GridX; w != 486 {
		t.Errorf("grid width = %v, want 486", w)
	}
	if !l.CardInner.Contains(l.Start.Center()) {
		t.Error("start button outside card")
	}
}

func TestTitleHitTest(t *testing.T) {
	l := Title()
	tests := []struct {
		name string
		p    geom.Vec2
		want game.Target
	}{
		{"speed 7", l.Speed[7].Center(), game.Target{Kind: game.TargetSpeed, Value: 7}},
		{"height 4", l.Height[4].Center(), game.Target{Kind: game.TargetHeight, Value: 4}},
		{"start", l.Start.Center(), game.Target{Kind: game.TargetStart}},
		{"outside", geom.Pt(5, 5), game.Target{}},
	}
	for _, tt := range tests {
		if got := l.HitTest(tt.p); got != tt.want {
			t.Errorf("%s: HitTest = %+v, want %+v", tt.name, got, tt.want)
		}
	}
}

// TestTitleMouseFullscreen clicks through a scaled 1080p viewport, as the
// real input path does.
func TestTitleMouseFullscreen(t *testing.T) {
	g := game.New(nil)
	v := NewViewport(1920, 1080)
	click := func(target geom.Rect) {
		screen := v.FromUI(target.Center())
		g.HandleInput(game.Input{Pointer: Title().HitTest(v.ToUI(screen)), Click: true}, 0)
	}

	click(Title().Speed[7])
	click(Title().Height[4])
	if m := g.Menu(); m.Speed != 7 || m.Height != 4 || m.Focus != game.FieldHeight {
		t.Fatalf("menu = %+v", m)
	}
	click(Title().Start)
	if g.State() != game.StatePlaying || g.Stats().Level != 7 {
		t.Errorf("state=%v level=%d, want Playing 7", g.State(), g.Stats().Level)
	}
}
