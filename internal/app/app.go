package app

import (
	"errors"
	"time"

	rl "github.com/gen2brain/raylib-go/raylib"

	"github.com/sspencer/texas/internal/game"
	"github.com/sspencer/texas/internal/layout"
)

// maxFrameTime caps the simulated time per frame so a stall (window drag,
// debugger pause) doesn't fast-forward the game.
const maxFrameTime = 100 * time.Millisecond

// Options configure how the game window opens.
type Options struct {
	// Fullscreen starts the game in borderless fullscreen mode.
	Fullscreen bool
}

// Run opens the game window and runs the main loop until the player quits.
// It must be called from the main goroutine.
func Run(opts Options) error {
	rl.SetTraceLogLevel(rl.LogWarning)
	// FlagWindowHighdpi keeps GetScreenWidth/Height in logical points on
	// Retina displays. Without it, raylib on macOS reports the framebuffer
	// size after a resize and the windowed UI renders cropped.
	rl.SetConfigFlags(rl.FlagVsyncHint | rl.FlagWindowResizable | rl.FlagWindowHighdpi)
	rl.InitWindow(layout.BaseWidth, layout.BaseHeight, "Texas")
	if !rl.IsWindowReady() {
		return errors.New("app: could not open window")
	}
	defer rl.CloseWindow()

	rl.SetWindowMinSize(layout.BaseWidth/2, layout.BaseHeight/2)
	rl.SetTargetFPS(120)

	a := &app{game: game.New(nil), cursor: rl.MouseCursorDefault}
	if opts.Fullscreen {
		toggleFullscreen()
	} else {
		centerWindow()
	}

	for !rl.WindowShouldClose() {
		a.frame()
	}
	return nil
}

type app struct {
	game   *game.Game
	canvas canvas
	cursor int32
}

func (a *app) frame() {
	dt := min(time.Duration(float64(rl.GetFrameTime())*float64(time.Second)), maxFrameTime)

	if rl.IsKeyPressed(rl.KeyF) || rl.IsKeyPressed(rl.KeyF11) {
		toggleFullscreen()
	}
	a.game.Step(readInput(), dt)
	a.updateCursor()
	a.draw()
}

// updateCursor shows a pointing hand over clickable title controls.
func (a *app) updateCursor() {
	want := rl.MouseCursorDefault
	if a.game.State() == game.StateTitle && a.game.Menu().Hover.Kind != game.TargetNone {
		want = rl.MouseCursorPointingHand
	}
	if want != a.cursor {
		rl.SetMouseCursor(want)
		a.cursor = want
	}
}

func (a *app) draw() {
	rl.BeginDrawing()
	defer rl.EndDrawing()

	g := a.game
	bd := g.Backdrop()
	rl.ClearBackground(bd.Palette().TopLeft)
	bd.Draw(&a.canvas, float32(rl.GetScreenWidth()), float32(rl.GetScreenHeight()))

	// Camera2D works in framebuffer pixels (BeginMode2D drops the HiDPI
	// scale matrix), so size the viewport from the render size.
	vp := layout.NewViewport(float32(rl.GetRenderWidth()), float32(rl.GetRenderHeight()))
	ui := rl.Camera2D{Offset: rl.NewVector2(vp.OffsetX, vp.OffsetY), Zoom: vp.Scale}

	if g.State() == game.StateTitle {
		rl.BeginMode2D(ui)
		drawTitle(g)
		rl.EndMode2D()
		return
	}

	rl.BeginMode2D(ui)
	rl.DrawRectangleRec(rl.NewRectangle(layout.OuterMargin, layout.OuterMargin,
		layout.BoardWidthPx+2*layout.BorderSize, layout.BoardHeightPx+2*layout.BorderSize), colorBorder)
	rl.EndMode2D()

	board := ui
	board.Offset.X += layout.BoardLeft * vp.Scale
	board.Offset.Y += layout.BoardTop * vp.Scale
	rl.BeginMode2D(board)
	drawBoardContents(g)
	rl.EndMode2D()

	rl.BeginMode2D(ui)
	drawSidePanel(g)
	drawStatusOverlay(g)
	rl.EndMode2D()
}

// toggleFullscreen switches between borderless fullscreen and a centered
// window.
func toggleFullscreen() {
	rl.ToggleBorderlessWindowed()
	if !rl.IsWindowState(rl.FlagBorderlessWindowedMode) && !rl.IsWindowFullscreen() {
		centerWindow()
	}
}

// centerWindow sizes the window comfortably for the current monitor and
// centers it.
func centerWindow() {
	m := rl.GetCurrentMonitor()
	mw, mh := rl.GetMonitorWidth(m), rl.GetMonitorHeight(m)
	w, h := layout.WindowedSize(mw, mh)
	rl.SetWindowSize(w, h)
	if mw > w && mh > h {
		rl.SetWindowPosition((mw-w)/2, (mh-h)/2)
	}
}
