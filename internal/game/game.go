package game

import (
	"math"
	"math/rand/v2"
	"slices"
	"time"

	"github.com/sspencer/texas/internal/backdrop"
	"github.com/sspencer/texas/internal/tetromino"
)

// State is the top-level phase of the game.
type State uint8

// Game states.
const (
	StateTitle     State = iota // choosing starting speed and height
	StatePlaying                // a piece is falling
	StateLineClear              // full rows are flashing before removal
	StatePaused                 // play is suspended
	StateGameOver               // the curtain falls; restart or return to title
	StateCountdown              // the curtain rises and a 3..2..1 countdown runs
)

var stateNames = [...]string{"Title", "Playing", "LineClear", "Paused", "GameOver", "Countdown"}

// String returns the name of s.
func (s State) String() string {
	if int(s) < len(stateNames) {
		return stateNames[s]
	}
	return "State(?)"
}

// TSpin classifies a T-spin under the Guideline three-corner rule.
type TSpin uint8

// T-spin kinds.
const (
	TSpinNone TSpin = iota
	TSpinMini
	TSpinFull
)

// Stats are the scoring figures shown to the player.
type Stats struct {
	Level      int
	Lines      int
	Score      int
	HighScore  int  // best score this session
	Combo      int  // consecutive line clears minus one; -1 when no combo
	BackToBack bool // the last clear was "difficult" (Tetris or T-spin)
}

// Banner is a transient announcement such as "TETRIS!" or "COMBO x3".
type Banner struct {
	Title     string
	Subtitle  string
	Remaining time.Duration
}

// Visible reports whether the banner should be drawn.
func (b Banner) Visible() bool {
	return b.Remaining > 0 && b.Title != ""
}

// autoRepeat tracks DAS/ARR state for horizontal movement.
type autoRepeat struct {
	dir    int // -1 left, +1 right, 0 idle
	held   time.Duration
	repeat time.Duration
}

// Game is the complete state of one Texas session. Create one with New.
//
// A Game is not safe for concurrent use.
type Game struct {
	rng      *rand.Rand
	state    State
	menu     Menu
	stats    Stats
	board    Board
	bag      *tetromino.Bag
	backdrop *backdrop.Backdrop
	debug    bool

	active   tetromino.Piece
	ghost    tetromino.Piece
	hold     tetromino.Piece
	holdUsed bool
	next     [NextQueueSize]tetromino.Piece

	// Per-piece timing.
	gravityAccum     time.Duration
	lockTimer        time.Duration
	lockResets       int
	lowestY          int
	onGround         bool
	lastActionRotate bool
	lastKick         int

	// Line clear animation.
	clearRows    []int
	clearTimer   time.Duration
	pendingTSpin TSpin

	stateTimer time.Duration // game-over curtain and countdown
	banner     Banner
	das        autoRepeat
	softDrop   time.Duration
}

// New returns a game at the title screen. Randomness (piece order, garbage,
// backdrop) is drawn from rng; pass nil to use a randomly seeded source.
func New(rng *rand.Rand) *Game {
	if rng == nil {
		rng = rand.New(rand.NewPCG(rand.Uint64(), rand.Uint64()))
	}
	return &Game{
		rng:       rng,
		state:     StateTitle,
		stats:     Stats{Combo: -1},
		bag:       tetromino.NewBag(rng),
		backdrop:  backdrop.New(rng),
		clearRows: make([]int, 0, 4),
	}
}

// State returns the current phase of the game.
func (g *Game) State() State { return g.state }

// Menu returns the title screen state.
func (g *Game) Menu() Menu { return g.menu }

// Stats returns the current score, level, and related figures.
func (g *Game) Stats() Stats { return g.stats }

// Board returns a copy of the playfield.
func (g *Game) Board() Board { return g.board }

// Active returns the falling piece.
func (g *Game) Active() tetromino.Piece { return g.active }

// Ghost returns the landing preview of the falling piece.
func (g *Game) Ghost() tetromino.Piece { return g.ghost }

// Hold returns the held piece (Kind None if empty) and whether hold has
// already been used for the current piece.
func (g *Game) Hold() (tetromino.Piece, bool) { return g.hold, g.holdUsed }

// Next returns the upcoming pieces, soonest first.
func (g *Game) Next() [NextQueueSize]tetromino.Piece { return g.next }

// ClearingRows returns the rows flashing during a line clear.
func (g *Game) ClearingRows() []int { return slices.Clone(g.clearRows) }

// ClearRemaining returns the time left in the line-clear animation.
func (g *Game) ClearRemaining() time.Duration { return g.clearTimer }

// StateTimer returns the time left in the game-over curtain or countdown.
func (g *Game) StateTimer() time.Duration { return g.stateTimer }

// Banner returns the current action banner.
func (g *Game) Banner() Banner { return g.banner }

// Debug reports whether debug mode (frozen gravity) is on.
func (g *Game) Debug() bool { return g.debug }

// Backdrop returns the generative background.
func (g *Game) Backdrop() *backdrop.Backdrop { return g.backdrop }

// GravityInterval returns the time a piece takes to fall one row at level,
// following the Guideline curve (1s at level 0, about 64ms at level 9).
func GravityInterval(level int) time.Duration {
	lv := float64(clamp(level, 0, maxGravityLevel))
	secs := max(math.Pow(0.8-lv*0.007, lv), 0.016)
	return time.Duration(secs * float64(time.Second))
}

// Step processes one frame: it applies in, then advances timers by dt.
func (g *Game) Step(in Input, dt time.Duration) {
	g.HandleInput(in, dt)
	g.Tick(dt)
}

// HandleInput applies the player's input for a frame lasting dt. The duration
// drives DAS/ARR auto-repeat and held soft drop.
func (g *Game) HandleInput(in Input, dt time.Duration) {
	p := in.Pressed

	switch g.state {
	case StateTitle:
		g.handleTitleInput(in)
		return
	case StatePlaying:
		if p.Has(Pause) {
			g.state = StatePaused
			return
		}
	case StatePaused:
		if p.Has(Pause) {
			g.state = StatePlaying
		} else if p.Has(ToTitle) {
			g.returnToTitle()
		}
		return
	case StateGameOver:
		if g.stateTimer <= 0 {
			if p.Has(Restart) {
				g.beginRestartCountdown()
			} else if p.Has(ToTitle) {
				g.returnToTitle()
			}
		}
		return
	case StateLineClear, StateCountdown:
		return
	}

	if p.Has(DebugToggle) {
		g.debug = !g.debug
	}
	if g.debug {
		if p.Has(DebugSkipNext) {
			g.next[0] = tetromino.New(g.bag.Next())
		}
		if p.Has(DebugGameOver) {
			g.setGameOver()
			return
		}
	}

	g.handleHorizontal(in, dt)
	g.handleSoftDrop(in, dt)

	if p.Has(RotateCW) {
		g.rotate(true)
	}
	if p.Has(RotateCCW) {
		g.rotate(false)
	}
	if p.Has(Hold) {
		g.holdPiece()
	}
	if !g.debug && p.Has(HardDrop) {
		g.hardDrop()
	}
}

func (g *Game) handleHorizontal(in Input, dt time.Duration) {
	leftDown, rightDown := in.Held.Has(MoveLeft), in.Held.Has(MoveRight)

	switch {
	case in.Pressed.Has(MoveLeft):
		g.move(-1, 0)
		g.das = autoRepeat{dir: -1}
		return
	case in.Pressed.Has(MoveRight):
		g.move(1, 0)
		g.das = autoRepeat{dir: 1}
		return
	case g.das.dir == -1 && !leftDown:
		g.das = autoRepeat{}
		if rightDown {
			g.das.dir = 1
		}
	case g.das.dir == 1 && !rightDown:
		g.das = autoRepeat{}
		if leftDown {
			g.das.dir = -1
		}
	case g.das.dir == 0 && leftDown:
		g.das = autoRepeat{dir: -1}
	case g.das.dir == 0 && rightDown:
		g.das = autoRepeat{dir: 1}
	}

	if g.das.dir == 0 {
		return
	}
	g.das.held += dt
	if g.das.held < dasDelay {
		return
	}
	g.das.repeat += dt
	for g.das.repeat >= arrInterval {
		g.das.repeat -= arrInterval
		if !g.move(g.das.dir, 0) {
			break
		}
	}
}

func (g *Game) handleSoftDrop(in Input, dt time.Duration) {
	switch {
	case in.Pressed.Has(SoftDrop):
		g.softDropStep()
		g.softDrop = 0
	case in.Held.Has(SoftDrop):
		g.softDrop += dt
		for g.softDrop >= softDropInterval {
			g.softDrop -= softDropInterval
			if !g.softDropStep() {
				break
			}
		}
	default:
		g.softDrop = 0
	}
}

// Tick advances gravity, lock delay, animations and the restart countdown by
// dt.
func (g *Game) Tick(dt time.Duration) {
	if g.state != StatePaused {
		g.backdrop.Update(dt)
	}
	g.banner.Remaining = max(0, g.banner.Remaining-dt)

	switch g.state {
	case StateTitle, StatePaused:

	case StateGameOver:
		g.stateTimer = max(0, g.stateTimer-dt)

	case StateCountdown:
		g.stateTimer -= dt
		if g.stateTimer <= time.Second {
			g.startNewGame()
		}

	case StateLineClear:
		g.clearTimer -= dt
		if g.clearTimer <= 0 {
			g.finishLineClear()
		}

	case StatePlaying:
		if g.debug {
			return
		}

		interval := GravityInterval(g.stats.Level)
		g.gravityAccum += dt
		for g.gravityAccum >= interval {
			g.gravityAccum -= interval
			if !g.move(0, 1) {
				g.gravityAccum = 0
				break
			}
		}

		if g.grounded() {
			g.onGround = true
			g.lockTimer += dt
			if g.lockTimer >= LockDelay {
				g.lockActive()
			}
		} else {
			g.onGround = false
			g.lockTimer = 0
		}
	}
}
