# Texas

A clean-room Tetris implementation written in C (C11) and Raylib.

## Running

Install a C11 compiler (`cc`/`clang`/`gcc`), `pkg-config`, and [Raylib](https://www.raylib.com/) (e.g. `brew install raylib pkg-config` on macOS), then build and run (launches fullscreen by default):

```sh
make run
```

To launch in windowed (non-fullscreen) mode:

```sh
make run-windowed
# or after building:
./bin/texas -w          # also accepts --windowed or --no-fullscreen
```

To build the standalone macOS `.app` bundle (with app icon) or install it into `/Applications`:

```sh
make app                # builds bin/Texas.app (double-click to launch)
make install            # builds and installs /Applications/Texas.app
make uninstall          # removes /Applications/Texas.app
```

To build, check, or run the unit tests:

```sh
make          # debug build -> bin/texas
make release  # optimized build -> bin/texas
make test     # build and run unit tests
make check    # strict C11 warning check (-Wall -Wextra -Wpedantic -Werror)
make clean    # remove bin/
```

## Features

- **Title Screen (Nintendo-Style Setup)**: Choose starting **Speed (0–9)**, **Height (0–5)**, and **Music (`1`, `2`, `3`, `OFF`)** with mouse/trackpad or keyboard before starting; defaults to Speed `0` (slowest), Height `0` (clear board), and Music `1`.
- **Saved Settings & Per-Speed High Scores**: Automatically persists Speed, Height, Music selection, Mute state, Fullscreen preference, and separate High Scores for each starting speed (`0`–`9`) to `~/Library/Application Support/Texas/config.ini` on macOS (or `$XDG_CONFIG_HOME/texas/config.ini` on POSIX), restoring them on the next launch.
- **NES-Style 8-Bit Music & Sound Effects**: Built-in pure-C NES 2A03 chiptune synthesizer (band-limited Pulse 1 & 2, 4-bit quantized Triangle bass, and 15-bit LFSR Noise percussion) with a Title theme, three selectable gameplay tracks (**1: Sugar Plum**, **2: Tetra March**, **3: Korobeiniki**), a Game Over cadence, live Title-screen previews, danger-tempo speedup, and full Tetris sound effects (move, rotate, lock, hard drop, hold, 1–3 line clears, a distinct 4-line **Tetris** fanfare, T-Spin shimmer, level-up fanfare, pause chime, and countdown ticks). Sound effects continue playing even when background music is set to `OFF`.
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
| `M` | Mute / unmute background music |
| `[` / `]` | Cycle background music track during play |
| `P` | Pause / Resume |
| `T` | Return to Title Screen (from Pause or Game Over) |
| `D` | Toggle debug mode (freezes gravity; enables `N` next-piece skip and `G` game-over test) |
