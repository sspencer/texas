# Texas

[![CI](https://github.com/sspencer/texas/actions/workflows/ci.yml/badge.svg)](https://github.com/sspencer/texas/actions/workflows/ci.yml)

A clean-room, Guideline-style falling-block puzzle game written in Go with
[raylib](https://www.raylib.com/) (via [raylib-go](https://github.com/gen2brain/raylib-go)).

## Requirements

- Go 1.27 or newer
- A C toolchain for raylib's cgo bindings
  - **macOS:** Xcode Command Line Tools (`xcode-select --install`)
  - **Linux (Debian/Ubuntu):** `sudo apt-get install libgl1-mesa-dev libxi-dev libxcursor-dev libxrandr-dev libxinerama-dev libwayland-dev libxkbcommon-dev`
  - **Windows:** a MinGW-w64 GCC on `PATH`

## Install

```sh
go install github.com/sspencer/texas/cmd/texas@latest
```

## Running

The game starts fullscreen by default:

```sh
make run              # or: go run ./cmd/texas
make run-windowed     # or: go run ./cmd/texas --windowed
```

| Flag | Meaning |
| --- | --- |
| `-f`, `--fullscreen` | Start in borderless fullscreen (default) |
| `-w`, `--windowed`, `--no-fullscreen` | Start in a window sized for your monitor |
| `-h`, `--help` | Show usage |

## Development

```sh
make            # debug build -> bin/texas
make release    # stripped, trimpath build -> bin/texas
make test       # unit tests
make race       # unit tests with the race detector
make cover      # HTML coverage report
make bench      # benchmarks
make fuzz       # fuzz the game loop and SRS rotation
make lint       # golangci-lint (config in .golangci.yml)
make check      # vet + lint + race, as CI does
make help       # list all targets
```

### Layout

```
cmd/texas/          entry point: flag parsing, exit codes
internal/game/      rules and state machine (board, scoring, T-spins, menu)
internal/tetromino/ pieces, SRS rotation and wall kicks, 7-bag randomizer
internal/backdrop/  generative background, drawn through a Canvas interface
internal/layout/    logical canvas, viewport scaling, title-screen hit testing
internal/geom/      small float32 vector and rectangle types
internal/app/       raylib window, input mapping and rendering
```

Only `internal/app` imports raylib. The game logic is platform independent
and fully unit tested. It receives one `game.Input` and one `time.Duration`
per frame, so tests can drive it deterministically with a seeded
`math/rand/v2` source.

## Features

- **Title Screen (Nintendo-Style Setup)**: Choose starting **Speed (0–9)** and **Height (0–5)** with mouse/trackpad or keyboard before starting; defaults to Speed `0` (slowest) and Height `0` (clear board).
- **Ghost Piece (Block Shadow)**: Color-matched landing preview rendered beneath the active piece, always synchronized across moves, rotations, wall kicks, and holds.
- **Super Rotation System (SRS)**: Accurate Guideline spawn shapes (`I`, `O`, `T`, `S`, `Z`, `J`, `L`), clockwise/counter-clockwise rotations, and full `JLSTZ` and `I` wall-kick tables.
- **7-Bag Randomizer & Next Queue**: Fair Guideline 7-bag piece generation with a 3-piece Next preview.
- **Hold Piece**: Swap the active piece into the Hold slot once per lock.
- **Lock Delay & Responsive Input**: Guideline lock delay with move-reset cap and smooth DAS/ARR auto-repeat.
- **Scoring & Line Clear Polish**: Soft/hard drop scoring, Single/Double/Triple/Tetris, T-Spin (Full & Mini), Back-to-Back, Combo bonuses, session high score, line-clear flash animation, and animated Game Over / restart countdown curtain.
- **Generative Art Backdrop**: Subtle, low-contrast procedural graphics (constellation meshes, flowing waves, drifting geometry, orbital contours, and Truchet lattices) in curated muted palettes that animate gently and pick a fresh pattern each game without distracting from gameplay.

## Controls

| Key | Action |
| --- | --- |
| `Left` / `Right` | Move left / right |
| `Down` | Soft drop (+1/cell) |
| `Space` | Hard drop (+2/cell) / Restart on Game Over |
| `Up` / `X` | Rotate clockwise |
| `Z` / `Left Ctrl` | Rotate counter-clockwise |
| `C` / `Shift` | Hold piece |
| `F` / `F11` | Toggle fullscreen / windowed |
| `P` | Pause / Resume |
| `T` | Return to Title Screen (from Pause or Game Over) |
| `D` | Toggle debug mode (freezes gravity; enables `N` next-piece skip and `G` game-over test) |
