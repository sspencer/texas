# Texas

A clean-room Tetris implementation written in Odin and Raylib.

## Running

Install [Odin](https://odin-lang.org/) and run (launches fullscreen by default):

```sh
make run
# or directly:
odin run .
```

To launch in windowed (non-fullscreen) mode:

```sh
make run-windowed
# or directly:
odin run . -- --windowed
./bin/texas -w          # also accepts --windowed or --no-fullscreen
```

To build, check, or run the unit tests:

```sh
make          # debug build -> bin/texas
make release  # optimized build -> bin/texas
make test     # run unit tests
make check    # type-check and vet
make clean    # remove bin/
```

## Features

- **Title Screen (Nintendo-Style Setup)**: Choose starting **Speed (0–9)** and **Height (0–5)** with mouse/trackpad or keyboard before starting; defaults to Speed `0` (slowest) and Height `0` (clear board).
- **Ghost Piece (Block Shadow)**: Color-matched landing preview rendered beneath the active piece, always synchronized across moves, rotations, wall kicks, and holds.
- **Super Rotation System (SRS)**: Accurate Guideline spawn shapes (`I`, `O`, `T`, `S`, `Z`, `J`, `L`), clockwise/counter-clockwise rotations, and full `JLSTZ` and `I` wall-kick tables.
- **7-Bag Randomizer & Next Queue**: Fair Guideline 7-bag piece generation with a 3-piece Next preview.
- **Hold Piece**: Swap the active piece into the Hold slot once per lock.
- **Lock Delay & Responsive Input**: Guideline lock delay with move-reset cap and smooth DAS/ARR auto-repeat.
- **Scoring & Line Clear Polish**: Soft/hard drop scoring, Single/Double/Triple/Tetris, T-Spin (Full & Mini), Back-to-Back, Combo bonuses, session high score, line-clear flash animation, and animated Game Over / restart countdown curtain.

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
