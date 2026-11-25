// Package app runs Texas in a desktop window using raylib.
//
// It is the only package that touches the platform: it translates keyboard
// and mouse state into game.Input, steps the game once per frame, and draws
// the result. Everything is drawn in the logical coordinate space defined by
// package layout and scaled to the window.
package app
