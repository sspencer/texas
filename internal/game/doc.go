// Package game implements the rules and state machine of Texas, a
// Guideline-style falling-block puzzle game.
//
// The package is platform independent: callers translate keyboard and mouse
// state into an Input once per frame and pass it to Game.Step together with
// the elapsed time. Rendering code reads the resulting state through Game's
// accessor methods.
//
// Implemented Guideline features include SRS rotation with wall kicks, the
// 7-bag randomizer, hold, a three-piece next queue, ghost piece, lock delay
// with move reset, DAS/ARR auto-repeat, T-spin detection (full and mini),
// back-to-back and combo scoring, and Nintendo-style starting speed and
// garbage height selection.
package game
