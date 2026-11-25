package main

import (
	"bytes"
	"errors"
	"flag"
	"strings"
	"testing"
)

func TestParseArgs(t *testing.T) {
	tests := []struct {
		args       []string
		fullscreen bool
	}{
		{nil, true},
		{[]string{"-w"}, false},
		{[]string{"--windowed"}, false},
		{[]string{"--no-fullscreen"}, false},
		{[]string{"-f"}, true},
		{[]string{"--windowed", "--fullscreen"}, true},
		{[]string{"-f", "-w"}, false},
		{[]string{"--fullscreen=false"}, false},
		{[]string{"--windowed=false"}, true},
	}
	for _, tt := range tests {
		var out bytes.Buffer
		opts, err := parseArgs(tt.args, &out)
		if err != nil {
			t.Errorf("parseArgs(%q) error: %v", tt.args, err)
			continue
		}
		if opts.Fullscreen != tt.fullscreen {
			t.Errorf("parseArgs(%q).Fullscreen = %v, want %v", tt.args, opts.Fullscreen, tt.fullscreen)
		}
		if out.Len() != 0 {
			t.Errorf("parseArgs(%q) wrote output: %q", tt.args, out.String())
		}
	}
}

func TestParseArgsHelp(t *testing.T) {
	for _, arg := range []string{"-h", "--help"} {
		var out bytes.Buffer
		_, err := parseArgs([]string{arg}, &out)
		if !errors.Is(err, flag.ErrHelp) {
			t.Errorf("parseArgs(%q) error = %v, want flag.ErrHelp", arg, err)
		}
		if !strings.Contains(out.String(), "Usage: texas") {
			t.Errorf("parseArgs(%q) did not print usage", arg)
		}
	}
}

func TestParseArgsErrors(t *testing.T) {
	for _, args := range [][]string{{"--bogus"}, {"extra"}, {"--fullscreen=maybe"}} {
		var out bytes.Buffer
		_, err := parseArgs(args, &out)
		if err == nil || errors.Is(err, flag.ErrHelp) {
			t.Errorf("parseArgs(%q) error = %v, want a usage error", args, err)
		}
		if !strings.Contains(out.String(), "Usage: texas") {
			t.Errorf("parseArgs(%q) did not print usage", args)
		}
	}
}

func TestRunExitCodes(t *testing.T) {
	var out bytes.Buffer
	if code := run([]string{"-h"}, &out); code != 0 {
		t.Errorf("run(-h) = %d, want 0", code)
	}
	if code := run([]string{"--bogus"}, &out); code != 2 {
		t.Errorf("run(--bogus) = %d, want 2", code)
	}
}
