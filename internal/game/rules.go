package game

import (
	"fmt"

	"github.com/sspencer/texas/internal/tetromino"
)

// spawned returns p positioned at its Guideline spawn location, centered at the
// top of the visible playfield.
func spawned(p tetromino.Piece) tetromino.Piece {
	p.X = (Width - p.Size()) / 2
	p.Y = HiddenRows
	if p.Kind == tetromino.I {
		// The I piece occupies row 1 of its 4x4 box; put that row at the
		// top visible row.
		p.Y = HiddenRows - 1
	}
	return p
}

func (g *Game) startNewGame() {
	g.state = StatePlaying
	g.menu.Speed = clamp(g.menu.Speed, 0, MaxStartSpeed)
	g.menu.Height = clamp(g.menu.Height, 0, MaxStartHeight)
	g.menu.Hover = Target{}
	g.stats = Stats{Level: g.menu.Speed, Combo: -1, HighScore: g.stats.HighScore}

	g.board.Reset()
	g.fillGarbage(g.menu.Height)
	g.backdrop.Regenerate()
	g.bag = tetromino.NewBag(g.rng)

	g.hold, g.holdUsed = tetromino.Piece{}, false
	g.clearRows = g.clearRows[:0]
	g.clearTimer = 0
	g.pendingTSpin = TSpinNone
	g.stateTimer = 0
	g.banner = Banner{}
	g.resetRepeat()

	for i := range g.next {
		g.next[i] = tetromino.New(g.bag.Next())
	}
	g.spawnNext()
}

// fillGarbage fills the bottom rows with random blocks for the starting
// height. Every generated row has at least three blocks and at least two gaps,
// so it is never empty and never clears on its own.
func (g *Game) fillGarbage(height int) {
	rows := GarbageRows(height)
	randomKind := func() tetromino.Kind { return tetromino.Kind(g.rng.IntN(tetromino.Count)) + tetromino.I }

	for y := Height - rows; y < Height; y++ {
		row := &g.board[y]
		filled := 0
		for x := range row {
			if g.rng.IntN(100) < 58 {
				row[x] = randomKind()
				filled++
			}
		}
		for filled > Width-2 {
			if x := g.rng.IntN(Width); row[x] != tetromino.None {
				row[x] = tetromino.None
				filled--
			}
		}
		for filled < 3 {
			if x := g.rng.IntN(Width); row[x] == tetromino.None {
				row[x] = randomKind()
				filled++
			}
		}
	}
}

func (g *Game) resetRepeat() {
	g.das = autoRepeat{}
	g.softDrop = 0
}

func (g *Game) resetPieceTimers() {
	g.gravityAccum = 0
	g.lockTimer = 0
	g.lockResets = 0
	g.lowestY = g.active.Y
	g.onGround = false
	g.lastActionRotate = false
	g.lastKick = 0
}

// spawnNext promotes the head of the next queue to the active piece.
func (g *Game) spawnNext() {
	g.active = spawned(g.next[0])
	copy(g.next[:], g.next[1:])
	g.next[len(g.next)-1] = tetromino.New(g.bag.Next())

	g.holdUsed = false
	g.resetPieceTimers()
	g.updateGhost()

	// Block out: the new piece overlaps the stack.
	if !g.board.Fits(g.active) {
		g.setGameOver()
	}
}

func (g *Game) setGameOver() {
	g.updateHighScore()
	g.state = StateGameOver
	g.stateTimer = GameOverDuration
	g.resetRepeat()
}

func (g *Game) beginRestartCountdown() {
	g.board.Reset()
	g.state = StateCountdown
	g.stateTimer = CountdownDuration
}

func (g *Game) returnToTitle() {
	g.updateHighScore()
	g.state = StateTitle
	g.menu.Hover = Target{}
	g.banner.Remaining = 0
	g.resetRepeat()
}

func (g *Game) updateHighScore() {
	g.stats.HighScore = max(g.stats.HighScore, g.stats.Score)
}

func (g *Game) addScore(points int) {
	g.stats.Score += points
	g.updateHighScore()
}

func (g *Game) setBanner(title, subtitle string) {
	g.banner = Banner{Title: title, Subtitle: subtitle, Remaining: BannerDuration}
}

// updateGhost recomputes the landing preview from the active piece.
func (g *Game) updateGhost() {
	g.ghost = g.active
	if g.ghost.Kind == tetromino.None || !g.board.Fits(g.ghost) {
		return
	}
	for g.board.Fits(g.ghost.Moved(0, 1)) {
		g.ghost.Y++
	}
}

// grounded reports whether the active piece rests on the floor or the stack.
func (g *Game) grounded() bool {
	return !g.board.Fits(g.active.Moved(0, 1))
}

// onPieceManipulated applies the Guideline lock-delay move reset after a
// successful shift or rotation.
func (g *Game) onPieceManipulated() {
	if g.onGround && g.lockResets < maxLockResets {
		g.lockTimer = 0
		g.lockResets++
	}
}

// move shifts the active piece by (dx, dy) if it fits, reporting success.
func (g *Game) move(dx, dy int) bool {
	moved := g.active.Moved(dx, dy)
	if !g.board.Fits(moved) {
		return false
	}
	g.active = moved
	g.lastActionRotate = false

	switch {
	case dx != 0:
		g.onPieceManipulated()
	case dy > 0:
		g.lockTimer = 0
		if g.active.Y > g.lowestY {
			g.lowestY = g.active.Y
			g.lockResets = 0
		}
	}

	g.updateGhost()
	return true
}

// rotate turns the active piece with SRS wall kicks, reporting success.
func (g *Game) rotate(clockwise bool) bool {
	p, kick, ok := g.active.Rotate(clockwise, g.board.Fits)
	if !ok {
		return false
	}
	g.active = p
	g.lastActionRotate = true
	g.lastKick = kick
	g.onPieceManipulated()
	g.updateGhost()
	return true
}

// softDropStep moves the piece down one row, scoring a point if it moved.
func (g *Game) softDropStep() bool {
	if !g.move(0, 1) {
		return false
	}
	g.gravityAccum = 0
	g.addScore(1)
	return true
}

// hardDrop drops the piece to its ghost position, scores two points per row,
// locks it, and returns the distance dropped.
func (g *Game) hardDrop() int {
	if g.state != StatePlaying {
		return 0
	}
	g.updateGhost()
	dropped := g.ghost.Y - g.active.Y
	if dropped > 0 {
		g.active.Y = g.ghost.Y
		g.addScore(dropped * 2)
		g.lastActionRotate = false
	}
	g.lockActive()
	return dropped
}

// holdPiece swaps the active piece into the hold slot, once per lock.
func (g *Game) holdPiece() bool {
	if g.state != StatePlaying || g.holdUsed {
		return false
	}

	current := g.active.Kind
	if g.hold.Kind == tetromino.None {
		g.hold = tetromino.New(current)
		g.spawnNext()
	} else {
		swap := g.hold.Kind
		g.hold = tetromino.New(current)
		g.active = spawned(tetromino.New(swap))
		g.resetPieceTimers()
		g.updateGhost()
		if !g.board.Fits(g.active) {
			g.setGameOver()
		}
	}

	g.holdUsed = true
	return true
}

// detectTSpin classifies the active piece's placement using the Guideline
// three-corner rule. It must run before the piece is merged into the board.
func (g *Game) detectTSpin() TSpin {
	if g.active.Kind != tetromino.T || !g.lastActionRotate {
		return TSpinNone
	}

	cx, cy := g.active.X+1, g.active.Y+1
	// Corners in clockwise order from top-left: TL, TR, BR, BL.
	corners := [4]tetromino.Point{
		{X: cx - 1, Y: cy - 1},
		{X: cx + 1, Y: cy - 1},
		{X: cx + 1, Y: cy + 1},
		{X: cx - 1, Y: cy + 1},
	}
	var occupied [4]bool
	count := 0
	for i, c := range corners {
		if g.board.Occupied(c.X, c.Y) {
			occupied[i] = true
			count++
		}
	}
	if count < 3 {
		return TSpinNone
	}

	// The two "front" corners are the ones the T points toward; with the
	// corners listed clockwise from TL they are Dir and Dir+1.
	a := int(g.active.Dir)
	b := (a + 1) % 4
	if (occupied[a] && occupied[b]) || g.lastKick == 4 {
		return TSpinFull
	}
	return TSpinMini
}

// lockActive merges the active piece into the board and either starts a line
// clear or spawns the next piece.
func (g *Game) lockActive() {
	tspin := g.detectTSpin()

	if visible := g.board.Place(g.active); !visible {
		// Lock out: the piece locked entirely above the visible playfield.
		g.setGameOver()
		return
	}

	g.clearRows = g.board.FullRows(g.clearRows[:0])
	if len(g.clearRows) > 0 {
		g.clearTimer = LineClearDuration
		g.pendingTSpin = tspin
		g.state = StateLineClear
		return
	}

	g.stats.Combo = -1
	if tspin != TSpinNone {
		g.addScore(score(g.stats.Level, 0, tspin, false))
		if tspin == TSpinFull {
			g.setBanner("T-SPIN!", "")
		} else {
			g.setBanner("T-SPIN MINI", "")
		}
	}
	g.spawnNext()
}

var clearTitles = [...][]string{
	TSpinNone: {"", "SINGLE", "DOUBLE", "TRIPLE!", "TETRIS!"},
	TSpinMini: {"", "T-SPIN MINI!", "T-SPIN MINI x2!"},
	TSpinFull: {"", "T-SPIN SINGLE!", "T-SPIN DOUBLE!", "T-SPIN TRIPLE!"},
}

// finishLineClear removes the flashing rows, scores them, and resumes play.
func (g *Game) finishLineClear() {
	lines := g.board.ClearFullRows()
	tspin := g.pendingTSpin
	g.clearRows = g.clearRows[:0]
	g.pendingTSpin = TSpinNone

	difficult := lines == 4 || (tspin != TSpinNone && lines > 0)
	b2b := difficult && g.stats.BackToBack

	points := score(g.stats.Level, lines, tspin, b2b)
	g.stats.Combo++
	if g.stats.Combo > 0 {
		points += 50 * g.stats.Combo * (g.stats.Level + 1)
	}

	if difficult {
		g.stats.BackToBack = true
	} else if lines > 0 {
		g.stats.BackToBack = false
	}

	g.addScore(points)
	g.stats.Lines += lines
	g.stats.Level = g.menu.Speed + g.stats.Lines/10

	var title string
	if titles := clearTitles[tspin]; lines < len(titles) {
		title = titles[lines]
	}
	var subtitle string
	switch {
	case b2b && g.stats.Combo > 0:
		subtitle = fmt.Sprintf("B2B  COMBO x%d", g.stats.Combo)
	case b2b:
		subtitle = "BACK-TO-BACK!"
	case g.stats.Combo > 0:
		subtitle = fmt.Sprintf("COMBO x%d", g.stats.Combo)
	}
	if title != "" {
		g.setBanner(title, subtitle)
	}

	g.state = StatePlaying
	g.spawnNext()
}

var baseScores = [...][]int{
	TSpinNone: {0, 100, 300, 500, 800},
	TSpinMini: {100, 200, 400},
	TSpinFull: {400, 800, 1200, 1600},
}

// score returns the Guideline points for clearing lines at level, including
// the 1.5x back-to-back bonus.
func score(level, lines int, tspin TSpin, b2b bool) int {
	base := 0
	if row := baseScores[tspin]; lines >= 0 && lines < len(row) {
		base = row[lines]
	}
	s := base * (level + 1)
	if b2b {
		s = s * 3 / 2
	}
	return s
}
