package game

// Action is a set of player intents, one bit per action. Platform code maps
// physical keys onto actions; a single key may carry several actions whose
// meaning depends on the game state (Space is both HardDrop in play and
// MenuConfirm on the title screen, for example).
type Action uint32

// Gameplay and menu actions.
const (
	MoveLeft Action = 1 << iota
	MoveRight
	SoftDrop
	HardDrop
	RotateCW
	RotateCCW
	Hold
	Pause
	ToTitle
	Restart
	DebugToggle
	DebugSkipNext
	DebugGameOver
	MenuSwitchField
	MenuDecrease
	MenuIncrease
	MenuConfirm

	// MenuDigit0 is the first of ten consecutive bits selecting a value
	// directly on the title screen; see MenuDigit.
	MenuDigit0
)

// MenuDigit returns the action that selects value n (0-9) on the title
// screen, or 0 if n is out of range.
func MenuDigit(n int) Action {
	if n < 0 || n > 9 {
		return 0
	}
	return MenuDigit0 << n
}

// Has reports whether a contains any action in b.
func (a Action) Has(b Action) bool {
	return a&b != 0
}

// Input is a snapshot of the player's input for one frame.
type Input struct {
	// Pressed holds actions whose key went down this frame.
	Pressed Action
	// Held holds actions whose key is currently down.
	Held Action
	// Pointer is the title-screen control under the mouse, if any.
	Pointer Target
	// Click reports whether the primary mouse button was pressed this frame.
	Click bool
}
