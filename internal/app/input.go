package app

import (
	rl "github.com/gen2brain/raylib-go/raylib"

	"github.com/sspencer/texas/internal/game"
	"github.com/sspencer/texas/internal/geom"
	"github.com/sspencer/texas/internal/layout"
)

// binding maps a key to the actions it triggers. A key may carry actions for
// several game states; the game decides which apply.
type binding struct {
	key     int32
	actions game.Action
}

var keymap = []binding{
	{rl.KeyLeft, game.MoveLeft | game.MenuDecrease},
	{rl.KeyRight, game.MoveRight | game.MenuIncrease},
	{rl.KeyUp, game.RotateCW | game.MenuSwitchField},
	{rl.KeyDown, game.SoftDrop | game.MenuSwitchField},
	{rl.KeyA, game.MenuDecrease},
	{rl.KeyD, game.MenuIncrease | game.DebugToggle},
	{rl.KeyW, game.MenuSwitchField},
	{rl.KeyS, game.MenuSwitchField},
	{rl.KeyTab, game.MenuSwitchField},
	{rl.KeyX, game.RotateCW},
	{rl.KeyZ, game.RotateCCW},
	{rl.KeyLeftControl, game.RotateCCW},
	{rl.KeyC, game.Hold},
	{rl.KeyLeftShift, game.Hold},
	{rl.KeyRightShift, game.Hold},
	{rl.KeySpace, game.HardDrop | game.Restart | game.MenuConfirm},
	{rl.KeyEnter, game.Restart | game.MenuConfirm},
	{rl.KeyKpEnter, game.MenuConfirm},
	{rl.KeyP, game.Pause},
	{rl.KeyT, game.ToTitle},
	{rl.KeyN, game.DebugSkipNext},
	{rl.KeyG, game.DebugGameOver},
	{rl.KeyZero, game.MenuDigit(0)},
	{rl.KeyOne, game.MenuDigit(1)},
	{rl.KeyTwo, game.MenuDigit(2)},
	{rl.KeyThree, game.MenuDigit(3)},
	{rl.KeyFour, game.MenuDigit(4)},
	{rl.KeyFive, game.MenuDigit(5)},
	{rl.KeySix, game.MenuDigit(6)},
	{rl.KeySeven, game.MenuDigit(7)},
	{rl.KeyEight, game.MenuDigit(8)},
	{rl.KeyNine, game.MenuDigit(9)},
}

// readInput samples the keyboard and mouse for this frame.
func readInput() game.Input {
	var in game.Input
	for _, b := range keymap {
		if rl.IsKeyPressed(b.key) {
			in.Pressed |= b.actions
		}
		if rl.IsKeyDown(b.key) {
			in.Held |= b.actions
		}
	}

	// Mouse coordinates are in logical points, so map them through the
	// viewport for the logical screen size (not the HiDPI render size).
	vp := layout.NewViewport(float32(rl.GetScreenWidth()), float32(rl.GetScreenHeight()))
	mouse := geom.Vec2(rl.GetMousePosition())
	in.Pointer = layout.Title().HitTest(vp.ToUI(mouse))
	in.Click = rl.IsMouseButtonPressed(rl.MouseButtonLeft)
	return in
}
