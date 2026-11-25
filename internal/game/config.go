package game

import "time"

// Playfield dimensions.
const (
	// Width is the number of columns in the playfield.
	Width = 10
	// VisibleHeight is the number of rows shown to the player.
	VisibleHeight = 20
	// HiddenRows is the number of buffer rows above the visible playfield
	// where pieces spawn and may rotate.
	HiddenRows = 4
	// Height is the total number of rows, hidden buffer included.
	Height = VisibleHeight + HiddenRows
)

// Game rules.
const (
	// NextQueueSize is the number of upcoming pieces previewed.
	NextQueueSize = 3
	// MaxStartSpeed is the highest selectable starting level.
	MaxStartSpeed = 9
	// MaxStartHeight is the highest selectable starting garbage height.
	MaxStartHeight = 5

	maxGravityLevel = 19
	maxLockResets   = 15
)

// Timings.
const (
	// LockDelay is how long a grounded piece may rest before it locks.
	LockDelay = 500 * time.Millisecond
	// LineClearDuration is the length of the line-clear flash animation.
	LineClearDuration = 220 * time.Millisecond
	// GameOverDuration is the length of the game-over curtain animation.
	GameOverDuration = 600 * time.Millisecond
	// CountdownDuration is the length of the restart sequence: the curtain
	// rises, then a 3..2..1 countdown runs before play resumes.
	CountdownDuration = 4 * time.Second
	// BannerDuration is how long an action banner such as "TETRIS!" shows.
	BannerDuration = 1500 * time.Millisecond

	dasDelay         = 150 * time.Millisecond
	arrInterval      = 40 * time.Millisecond
	softDropInterval = 45 * time.Millisecond
)

// garbageRows maps each starting height to the number of garbage rows, as in
// the classic Nintendo Type-B mode.
var garbageRows = [MaxStartHeight + 1]int{0, 3, 5, 8, 10, 12}

// GarbageRows returns the number of garbage rows for a starting height,
// clamping height to [0, MaxStartHeight].
func GarbageRows(height int) int {
	return garbageRows[clamp(height, 0, MaxStartHeight)]
}

func clamp(v, lo, hi int) int { //nolint:unparam // general-purpose helper
	return min(max(v, lo), hi)
}
