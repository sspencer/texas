package game

import (
	"math/rand/v2"
	"testing"
	"time"

	"github.com/sspencer/texas/internal/tetromino"
)

func newTestGame(t testing.TB) *Game {
	t.Helper()
	return New(rand.New(rand.NewPCG(1, 2)))
}

// newPlayingGame returns a game that has just started at speed 0, height 0.
func newPlayingGame(t testing.TB) *Game {
	t.Helper()
	g := newTestGame(t)
	g.startNewGame()
	if g.state != StatePlaying {
		t.Fatalf("state after start = %v, want Playing", g.state)
	}
	return g
}

// setActive replaces the falling piece with a freshly spawned piece of kind k.
func (g *Game) setActive(k tetromino.Kind) {
	g.active = spawned(tetromino.New(k))
	g.resetPieceTimers()
	g.updateGhost()
}

func assertEmptyRows(t *testing.T, b *Board, from, to int) {
	t.Helper()
	for y := from; y < to; y++ {
		for x := range Width {
			if b[y][x] != tetromino.None {
				t.Fatalf("cell (%d,%d) = %v, want empty", x, y, b[y][x])
			}
		}
	}
}

func TestGhostFollowsStack(t *testing.T) {
	g := newPlayingGame(t)
	g.setActive(tetromino.T)

	// A T occupies rows 0 and 1 of its box, so on an empty board it rests
	// with its box top two rows above the floor.
	if g.ghost.X != g.active.X || g.ghost.Y != Height-2 || g.ghost.Shape != g.active.Shape {
		t.Fatalf("ghost = (%d,%d), want (%d,%d) with same shape", g.ghost.X, g.ghost.Y, g.active.X, Height-2)
	}

	g.board[Height-5][4] = tetromino.Z
	g.updateGhost()
	if want := Height - 7; g.ghost.Y != want {
		t.Errorf("ghost.Y over obstacle = %d, want %d", g.ghost.Y, want)
	}

	if !g.move(3, 0) {
		t.Fatal("move right failed")
	}
	if g.ghost.X != g.active.X || g.ghost.Y != Height-2 {
		t.Errorf("ghost after move = (%d,%d), want (%d,%d)", g.ghost.X, g.ghost.Y, g.active.X, Height-2)
	}
}

func TestGhostMatchesActiveAfterFailedRotation(t *testing.T) {
	g := newPlayingGame(t)
	g.setActive(tetromino.I)
	g.active.X, g.active.Y = 3, HiddenRows+6

	// Box the I piece in completely so every kick fails.
	for y := HiddenRows + 4; y <= HiddenRows+10; y++ {
		for x := range Width {
			if y == g.active.Y+1 && x >= g.active.X && x < g.active.X+4 {
				continue
			}
			g.board[y][x] = tetromino.O
		}
	}
	g.updateGhost()
	before := g.active

	if g.rotate(true) {
		t.Fatal("rotate succeeded, want failure")
	}
	if g.active != before {
		t.Errorf("active changed after failed rotation: %+v", g.active)
	}
	if g.ghost != g.active {
		t.Errorf("ghost = %+v, want %+v", g.ghost, g.active)
	}
}

func TestGhostOfEmptyPiece(t *testing.T) {
	g := newTestGame(t)
	g.active = tetromino.Piece{}
	g.updateGhost() // must terminate
	if g.ghost.Kind != tetromino.None {
		t.Errorf("ghost kind = %v, want None", g.ghost.Kind)
	}
}

func TestWallKickUpdatesGhost(t *testing.T) {
	g := newPlayingGame(t)
	g.setActive(tetromino.T)
	g.active = g.active.RotatedCW() // East: occupies box columns 1 and 2
	g.active.X, g.active.Y = -1, HiddenRows+5
	if !g.board.Fits(g.active) {
		t.Fatal("T flush with left wall should fit")
	}

	if !g.rotate(true) {
		t.Fatal("rotate East->South failed")
	}
	if g.lastKick != 1 || g.active.X != 0 || g.active.Dir != tetromino.South {
		t.Errorf("after kick: kick=%d x=%d dir=%v, want 1 0 South", g.lastKick, g.active.X, g.active.Dir)
	}
	if g.ghost.Shape != g.active.Shape || g.ghost.X != g.active.X {
		t.Error("ghost out of sync with active piece after kick")
	}
}

func TestSoftAndHardDropScoring(t *testing.T) {
	g := newPlayingGame(t)
	g.setActive(tetromino.O)
	distance := (Height - 2) - g.active.Y

	if !g.softDropStep() {
		t.Fatal("soft drop failed")
	}
	if g.stats.Score != 1 {
		t.Errorf("score after soft drop = %d, want 1", g.stats.Score)
	}

	if got := g.hardDrop(); got != distance-1 {
		t.Errorf("hard drop distance = %d, want %d", got, distance-1)
	}
	if want := 1 + (distance-1)*2; g.stats.Score != want {
		t.Errorf("score after hard drop = %d, want %d", g.stats.Score, want)
	}

	g.setActive(tetromino.O)
	g.active.X, g.active.Y = 0, Height-2
	before := g.stats.Score
	if g.softDropStep() {
		t.Error("soft drop on floor succeeded")
	}
	if g.stats.Score != before {
		t.Errorf("blocked soft drop scored %d points", g.stats.Score-before)
	}
}

func TestHold(t *testing.T) {
	g := newPlayingGame(t)
	first, next := g.active.Kind, g.next[0].Kind

	if !g.holdPiece() {
		t.Fatal("first hold rejected")
	}
	if g.hold.Kind != first || g.active.Kind != next || !g.holdUsed || g.ghost.Kind != next {
		t.Fatalf("after hold: hold=%v active=%v used=%v ghost=%v", g.hold.Kind, g.active.Kind, g.holdUsed, g.ghost.Kind)
	}
	if g.holdPiece() {
		t.Error("second hold before lock accepted")
	}

	g.hardDrop()
	if g.state == StateLineClear {
		g.Tick(LineClearDuration + 10*time.Millisecond)
	}
	if g.holdUsed {
		t.Error("hold not re-enabled after lock")
	}

	current := g.active.Kind
	if !g.holdPiece() {
		t.Fatal("hold after lock rejected")
	}
	if g.active.Kind != first || g.hold.Kind != current {
		t.Errorf("swap: active=%v hold=%v, want %v %v", g.active.Kind, g.hold.Kind, first, current)
	}
}

func TestTetrisLevelCarryoverAndBackToBack(t *testing.T) {
	g := newPlayingGame(t)
	g.stats.Lines = 8

	for y := Height - 4; y < Height; y++ {
		for x := 1; x < Width; x++ {
			g.board[y][x] = tetromino.J
		}
	}
	g.active = tetromino.New(tetromino.I).RotatedCW() // vertical in box column 2
	g.active.X, g.active.Y = -2, HiddenRows
	g.updateGhost()

	g.hardDrop()
	if g.state != StateLineClear || len(g.clearRows) != 4 {
		t.Fatalf("state=%v clearing=%d, want LineClear 4", g.state, len(g.clearRows))
	}

	g.Tick(LineClearDuration + 10*time.Millisecond)
	if g.state != StatePlaying {
		t.Fatalf("state = %v, want Playing", g.state)
	}
	if g.stats.Lines != 12 || g.stats.Level != 1 || !g.stats.BackToBack {
		t.Errorf("stats = %+v, want 12 lines, level 1, back-to-back", g.stats)
	}
	if g.banner.Title != "TETRIS!" {
		t.Errorf("banner = %q, want TETRIS!", g.banner.Title)
	}
	assertEmptyRows(t, &g.board, 0, Height)
}

func TestTSpinDetection(t *testing.T) {
	g := newPlayingGame(t)
	bottom := Height - 1
	for x := range Width {
		if x != 4 {
			g.board[bottom][x] = tetromino.Z
		}
		if x < 3 || x > 5 {
			g.board[bottom-1][x] = tetromino.Z
		}
	}
	g.board[bottom-2][3] = tetromino.Z
	g.board[bottom-2][5] = tetromino.Z

	g.active = tetromino.New(tetromino.T).RotatedCW().RotatedCW()
	g.active.X, g.active.Y = 3, bottom-2
	g.lastActionRotate = true

	if !g.board.Fits(g.active) {
		t.Fatal("T should fit in slot")
	}
	if got := g.detectTSpin(); got != TSpinFull {
		t.Errorf("detectTSpin = %v, want TSpinFull", got)
	}

	g.lastActionRotate = false
	if got := g.detectTSpin(); got != TSpinNone {
		t.Errorf("detectTSpin without rotation = %v, want TSpinNone", got)
	}
}

func TestLockDelayGameOverAndRestart(t *testing.T) {
	g := newPlayingGame(t)
	g.setActive(tetromino.O)
	g.active.Y = Height - 2
	g.updateGhost()

	g.Tick(LockDelay / 2)
	if g.state != StatePlaying || !g.onGround || g.board[Height-1][4] != tetromino.None {
		t.Fatalf("locked too early: state=%v onGround=%v", g.state, g.onGround)
	}

	if !g.move(1, 0) {
		t.Fatal("move right failed")
	}
	if g.lockTimer != 0 || g.lockResets != 1 {
		t.Errorf("after move: lockTimer=%v resets=%d, want 0 1", g.lockTimer, g.lockResets)
	}

	g.Tick(LockDelay + 50*time.Millisecond)
	if g.board[Height-1][5] != tetromino.O {
		t.Fatal("piece did not lock after lock delay")
	}

	g.stats.Score = 4200
	g.setGameOver()
	if g.state != StateGameOver || g.stats.HighScore != 4200 {
		t.Fatalf("state=%v high=%d, want GameOver 4200", g.state, g.stats.HighScore)
	}

	g.Tick(GameOverDuration + 100*time.Millisecond)
	if g.stateTimer != 0 {
		t.Errorf("curtain timer = %v, want 0", g.stateTimer)
	}

	g.HandleInput(Input{Pressed: Restart}, 0)
	if g.state != StateCountdown {
		t.Fatalf("state = %v, want Countdown", g.state)
	}
	g.Tick(CountdownDuration)
	if g.state != StatePlaying {
		t.Fatalf("state = %v, want Playing", g.state)
	}
	if g.stats.Score != 0 || g.stats.HighScore != 4200 || g.stats.Level != 0 {
		t.Errorf("stats after restart = %+v", g.stats)
	}
}

func TestLockResetsAreLimited(t *testing.T) {
	g := newPlayingGame(t)
	g.setActive(tetromino.O)
	g.active.Y = Height - 2
	g.updateGhost()
	g.Tick(time.Millisecond) // become grounded

	for i := range maxLockResets + 5 {
		dx := 1 - 2*(i%2)
		g.move(dx, 0)
	}
	if g.lockResets != maxLockResets {
		t.Errorf("lockResets = %d, want %d", g.lockResets, maxLockResets)
	}
}

func TestGravityInterval(t *testing.T) {
	if got := GravityInterval(0); got != time.Second {
		t.Errorf("GravityInterval(0) = %v, want 1s", got)
	}
	prev := GravityInterval(0)
	for lv := 1; lv <= 150; lv++ {
		cur := GravityInterval(lv)
		if cur < 16*time.Millisecond || cur > prev {
			t.Fatalf("GravityInterval(%d) = %v (prev %v): want monotonic and >= 16ms", lv, cur, prev)
		}
		prev = cur
	}
	if got := GravityInterval(-3); got != time.Second {
		t.Errorf("GravityInterval(-3) = %v, want 1s", got)
	}
}

func TestScore(t *testing.T) {
	tests := []struct {
		level, lines int
		tspin        TSpin
		b2b          bool
		want         int
	}{
		{0, 0, TSpinNone, false, 0},
		{0, 1, TSpinNone, false, 100},
		{0, 4, TSpinNone, false, 800},
		{0, 4, TSpinNone, true, 1200},
		{2, 2, TSpinNone, false, 900},
		{0, 0, TSpinMini, false, 100},
		{0, 2, TSpinMini, false, 400},
		{1, 2, TSpinFull, false, 2400},
		{1, 2, TSpinFull, true, 3600},
		{0, 9, TSpinNone, false, 0},
	}
	for _, tt := range tests {
		if got := score(tt.level, tt.lines, tt.tspin, tt.b2b); got != tt.want {
			t.Errorf("score(%d, %d, %v, %v) = %d, want %d", tt.level, tt.lines, tt.tspin, tt.b2b, got, tt.want)
		}
	}
}

func TestDelayedAutoShift(t *testing.T) {
	g := newPlayingGame(t)
	g.debug = true // freeze gravity
	g.setActive(tetromino.O)
	x0 := g.active.X
	frame := 10 * time.Millisecond

	g.Step(Input{Pressed: MoveLeft, Held: MoveLeft}, frame)
	if g.active.X != x0-1 {
		t.Fatalf("initial press: x = %d, want %d", g.active.X, x0-1)
	}

	// Holding below the DAS threshold must not move the piece.
	for range int(dasDelay/frame) - 2 {
		g.Step(Input{Held: MoveLeft}, frame)
	}
	if g.active.X != x0-1 {
		t.Fatalf("moved before DAS: x = %d", g.active.X)
	}

	// Well past DAS, auto-repeat slides the piece to the wall.
	for range 100 {
		g.Step(Input{Held: MoveLeft}, frame)
	}
	if g.active.X != 0 {
		t.Errorf("after auto-repeat: x = %d, want 0", g.active.X)
	}
}

func TestTitleMenu(t *testing.T) {
	g := newTestGame(t)
	if g.state != StateTitle || g.menu != (Menu{}) {
		t.Fatalf("initial state=%v menu=%+v, want Title with zero menu", g.state, g.menu)
	}

	g.startNewGame()
	if g.stats.Level != 0 || GravityInterval(g.stats.Level) != time.Second {
		t.Errorf("default level = %d", g.stats.Level)
	}
	assertEmptyRows(t, &g.board, 0, Height)

	g.stats.Score = 1234
	g.HandleInput(Input{Pressed: Pause}, 0)
	g.HandleInput(Input{Pressed: ToTitle}, 0)
	if g.state != StateTitle || g.stats.HighScore != 1234 {
		t.Fatalf("state=%v high=%d, want Title 1234", g.state, g.stats.HighScore)
	}

	g.menu.adjust(-5)
	if g.menu.Speed != 0 {
		t.Errorf("speed = %d, want clamp to 0", g.menu.Speed)
	}
	g.menu.adjust(20)
	if g.menu.Speed != MaxStartSpeed {
		t.Errorf("speed = %d, want clamp to %d", g.menu.Speed, MaxStartSpeed)
	}

	g.HandleInput(Input{Pressed: MenuSwitchField}, 0)
	if g.menu.Focus != FieldHeight {
		t.Fatalf("focus = %v, want FieldHeight", g.menu.Focus)
	}
	g.HandleInput(Input{Pressed: MenuDigit(8)}, 0)
	if g.menu.Height != 0 {
		t.Errorf("height = %d, out-of-range digit should be ignored", g.menu.Height)
	}
	g.menu.adjust(10)
	if g.menu.Height != MaxStartHeight {
		t.Errorf("height = %d, want %d", g.menu.Height, MaxStartHeight)
	}

	g.HandleInput(Input{Pressed: MenuConfirm}, 0)
	if g.state != StatePlaying || g.stats.Level != MaxStartSpeed {
		t.Fatalf("state=%v level=%d, want Playing %d", g.state, g.stats.Level, MaxStartSpeed)
	}
	if GravityInterval(g.stats.Level) >= 100*time.Millisecond {
		t.Errorf("level 9 gravity too slow: %v", GravityInterval(g.stats.Level))
	}

	rows := GarbageRows(MaxStartHeight)
	if rows != 12 {
		t.Fatalf("GarbageRows(%d) = %d, want 12", MaxStartHeight, rows)
	}
	assertEmptyRows(t, &g.board, 0, Height-rows)
	for y := Height - rows; y < Height; y++ {
		filled := 0
		for _, c := range g.board[y] {
			if c != tetromino.None {
				filled++
			}
		}
		if filled < 3 || filled > Width-2 {
			t.Errorf("garbage row %d has %d blocks, want 3..%d", y, filled, Width-2)
		}
	}
	if !g.board.Fits(g.active) || !g.board.Fits(g.ghost) || g.ghost.Y < g.active.Y {
		t.Error("active or ghost piece collides with garbage")
	}
}

func TestTitlePointer(t *testing.T) {
	g := newTestGame(t)

	g.HandleInput(Input{Pointer: Target{TargetSpeed, 7}}, 0)
	if g.menu.Hover != (Target{TargetSpeed, 7}) || g.menu.Speed != 0 {
		t.Errorf("hover only: menu = %+v", g.menu)
	}

	g.HandleInput(Input{Pointer: Target{TargetSpeed, 7}, Click: true}, 0)
	if g.menu.Speed != 7 || g.menu.Focus != FieldSpeed {
		t.Errorf("click speed: menu = %+v", g.menu)
	}

	g.HandleInput(Input{Pointer: Target{TargetHeight, 4}, Click: true}, 0)
	if g.menu.Height != 4 || g.menu.Focus != FieldHeight {
		t.Errorf("click height: menu = %+v", g.menu)
	}

	g.HandleInput(Input{Click: true}, 0)
	if g.state != StateTitle || g.menu.Hover.Kind != TargetNone {
		t.Errorf("click on nothing: state=%v hover=%+v", g.state, g.menu.Hover)
	}

	g.HandleInput(Input{Pointer: Target{Kind: TargetStart}, Click: true}, 0)
	if g.state != StatePlaying || g.stats.Level != 7 || g.menu.Height != 4 {
		t.Errorf("start: state=%v level=%d height=%d", g.state, g.stats.Level, g.menu.Height)
	}
}

func TestBackdropLifecycle(t *testing.T) {
	g := newTestGame(t)

	g.Tick(250 * time.Millisecond)
	if g.Backdrop().Elapsed() < 200*time.Millisecond {
		t.Errorf("title backdrop elapsed = %v", g.Backdrop().Elapsed())
	}

	prev := g.Backdrop().Style()
	g.startNewGame()
	if g.Backdrop().Style() == prev || g.Backdrop().Elapsed() != 0 {
		t.Errorf("new game backdrop: style %v (prev %v), elapsed %v", g.Backdrop().Style(), prev, g.Backdrop().Elapsed())
	}

	g.Tick(500 * time.Millisecond)
	playing := g.Backdrop().Elapsed()
	if playing < 500*time.Millisecond {
		t.Errorf("playing backdrop elapsed = %v", playing)
	}

	g.HandleInput(Input{Pressed: Pause}, 0)
	g.Tick(time.Second)
	if g.Backdrop().Elapsed() != playing {
		t.Errorf("paused backdrop advanced from %v to %v", playing, g.Backdrop().Elapsed())
	}
}

func TestStateString(t *testing.T) {
	if got := StateLineClear.String(); got != "LineClear" {
		t.Errorf("String() = %q", got)
	}
	if got := State(200).String(); got != "State(?)" {
		t.Errorf("String() = %q", got)
	}
}

// FuzzStep drives the game with arbitrary input sequences and checks
// invariants that must hold after every frame.
func FuzzStep(f *testing.F) {
	f.Add(uint64(1), []byte{0, 1, 2, 3, 4, 5, 6, 7, 8})
	f.Add(uint64(42), []byte{255, 128, 64, 32, 16, 8, 4, 2, 1})
	f.Fuzz(func(t *testing.T, seed uint64, frames []byte) {
		g := New(rand.New(rand.NewPCG(seed, seed)))
		g.HandleInput(Input{Pressed: MenuConfirm}, 0)
		for _, b := range frames {
			in := Input{Pressed: Action(1) << (b % 17), Held: Action(b)}
			g.Step(in, time.Duration(b)*time.Millisecond)

			if g.state == StatePlaying && !g.board.Fits(g.active) {
				t.Fatalf("active piece %+v overlaps the board", g.active)
			}
			if g.stats.HighScore < g.stats.Score {
				t.Fatalf("high score %d below score %d", g.stats.HighScore, g.stats.Score)
			}
		}
	})
}

func BenchmarkStep(b *testing.B) {
	g := newPlayingGame(b)
	in := Input{Held: SoftDrop}
	for b.Loop() {
		g.Step(in, 16*time.Millisecond)
		if g.state == StateGameOver {
			g.startNewGame()
		}
	}
}
