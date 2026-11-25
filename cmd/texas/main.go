// Command texas is a Guideline-style falling-block puzzle game.
//
// Usage:
//
//	texas [flags]
//
// The flags are:
//
//	-f, --fullscreen
//		Start in borderless fullscreen (the default).
//	-w, --windowed, --no-fullscreen
//		Start in a window sized for the current monitor.
//	-h, --help
//		Print usage and exit.
//
// When several display flags are given, the last one wins.
package main

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"

	"github.com/sspencer/texas/internal/app"
)

func main() {
	os.Exit(run(os.Args[1:], os.Stderr))
}

// run parses args and runs the game, returning the process exit code.
func run(args []string, stderr io.Writer) int {
	opts, err := parseArgs(args, stderr)
	switch {
	case errors.Is(err, flag.ErrHelp):
		return 0
	case err != nil:
		return 2
	}

	if err := app.Run(opts); err != nil {
		fmt.Fprintln(stderr, "texas:", err)
		return 1
	}
	return 0
}

// parseArgs parses command-line flags. Errors and usage go to output. It
// returns flag.ErrHelp when help was requested.
func parseArgs(args []string, output io.Writer) (app.Options, error) {
	opts := app.Options{Fullscreen: true}

	fs := flag.NewFlagSet("texas", flag.ContinueOnError)
	fs.SetOutput(output)
	fs.Usage = func() {
		fmt.Fprint(output, `Usage: texas [flags]

Flags:
  -f, --fullscreen                 start in borderless fullscreen (default)
  -w, --windowed, --no-fullscreen  start in a window
  -h, --help                       show this help

In game: arrows move/rotate, Space hard drops, C holds, P pauses,
F toggles fullscreen, Esc quits.
`)
	}

	setter := func(fullscreen bool) func(string) error {
		return func(v string) error {
			on, err := strconv.ParseBool(v)
			if err != nil {
				return err
			}
			opts.Fullscreen = on == fullscreen
			return nil
		}
	}
	for _, name := range []string{"f", "fullscreen"} {
		fs.BoolFunc(name, "start in borderless fullscreen", setter(true))
	}
	for _, name := range []string{"w", "windowed", "no-fullscreen"} {
		fs.BoolFunc(name, "start in a window", setter(false))
	}

	if err := fs.Parse(args); err != nil {
		return opts, err
	}
	if fs.NArg() > 0 {
		err := fmt.Errorf("unexpected argument %q", fs.Arg(0))
		fmt.Fprintln(output, err)
		fs.Usage()
		return opts, err
	}
	return opts, nil
}
