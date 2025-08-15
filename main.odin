package main

import "core:fmt"
import "core:math"
import "core:math/rand"
import rl "vendor:raylib"

BACKGROUND :: rl.Color{43, 60, 80, 255}
BORDER_COLOR :: rl.Color{36, 36, 48, 255}
GAME_WIDTH :: 10
GAME_HEIGHT :: 20
BORDER_LEFT :: 60
BORDER_RIGHT :: 300
BORDER_HEIGHT :: 60
BLOCK_SIZE :: 30
BORDER_SIZE :: 8
GATE_DURATION :: 0.60
GAME_LAUNCH_TIME :: 4.0 // (count down from 3 then start new game in remaining time)

Game :: struct {
	level:            int,
	lines_cleared:    int,
	game_score:       int,
	board:            [GAME_HEIGHT][GAME_WIDTH]i32,
	camera:           rl.Camera2D,
	tetro:            Tetromino,
	next_tetro:       Tetromino,
	ghost:            Tetromino,
	last_tick:        f64,
	delta_time:       f64,
	gate_time:        f64,
	debug:            bool,
	end_game:         bool,
	game_over:        bool,
	game_over_time:   f64,
	game_launch_time: f64,
	tetro_dropped:    bool,
	game_paused:      bool,
}

Vec2i :: struct {
	x, y: int,
}

// Main entry point for the Tetris game
// Initializes the game window, sets up the camera, and runs the main game loop
main :: proc() {
	game_w := i32(BORDER_LEFT + BORDER_RIGHT + GAME_WIDTH * BLOCK_SIZE)
	game_h := i32(BORDER_HEIGHT * 2 + GAME_HEIGHT * BLOCK_SIZE)

	rl.SetTraceLogLevel(.WARNING)
	rl.SetConfigFlags({.VSYNC_HINT})
	rl.InitWindow(game_w, game_h, "Texas")
	defer rl.CloseWindow()
	rl.SetTargetFPS(120)

	game := Game{}
	game.level = 1
	game.board = [GAME_HEIGHT][GAME_WIDTH]i32{}

	// Set up camera for game board specifically
	game.camera.offset = rl.Vector2{BORDER_LEFT, BORDER_HEIGHT} // Border offset (top-left of game area)
	game.camera.target = rl.Vector2{0, 0} // Focus on game’s (0,0)
	game.camera.rotation = 0.0
	game.camera.zoom = 1.0

	game.delta_time = level_time(game.level)
	game.last_tick = rl.GetTime()

	random_tetromino(&game.tetro)
	random_tetromino(&game.next_tetro)
	set_tetromino(&game.ghost, game.tetro.id)

	fmt.println("Texas")

	for !rl.WindowShouldClose() {
		dt := rl.GetTime()
		update(&game, dt)
		draw(&game, dt)
		rl.EndDrawing()
		free_all(context.temp_allocator)
	}
}

draw :: proc(g: ^Game, dt: f64) {
	rl.BeginDrawing()
	rl.ClearBackground(BACKGROUND)

	// Draw border (outside camera, in screen coordinates)
	rec := rl.Rectangle {
		BORDER_LEFT - BORDER_SIZE,
		BORDER_HEIGHT - BORDER_SIZE,
		GAME_WIDTH * BLOCK_SIZE + 1 + 2 * BORDER_SIZE,
		GAME_HEIGHT * BLOCK_SIZE + 1 + 2 * BORDER_SIZE,
	}
	rl.DrawRectangleRec(rec, BORDER_COLOR)

	// Begin camera mode (all drawing commands are offset by camera.offset)
	rl.BeginMode2D(g.camera)

	// rl.DrawRectangle(0, 0, GAME_WIDTH * BLOCK, GAME_HEIGHT * BLOCK, rl.BLACK)
	draw_board(g)

	if g.game_over {
		draw_game_over(g, dt)
	} else {
		draw_tetromino(g.tetro)
		draw_ghost(g.ghost)
		// fmt.printf("piece: %d, ghost: %d\n", g.tetro.y, g.ghost.y)
	}

	rl.EndMode2D()

	draw_score(g)
}


draw_score :: proc(g: ^Game) {
	x := f32(rl.GetScreenWidth() - BORDER_RIGHT)
	rec := rl.Rectangle {
		x + 2.0 * BORDER_SIZE,
		BORDER_HEIGHT - BORDER_SIZE,
		BORDER_RIGHT - BORDER_LEFT - BORDER_SIZE,
		360,
	}

	rl.DrawRectangleRec(rec, BORDER_COLOR)
	rec.x += BORDER_SIZE
	rec.y += BORDER_SIZE
	rec.width -= 2 * BORDER_SIZE
	rec.height -= 2 * BORDER_SIZE
	rl.DrawRectangleRec(rec, rl.BLACK)

	font_size: i32 = 24
	tx := i32(rec.x + BORDER_SIZE)
	ty := i32(rec.y + BORDER_SIZE)
	rl.DrawText(
		fmt.ctprintf("Level: %d", g.level),
		tx,
		ty,
		font_size,
		rl.WHITE,
	)
	r := 6
	m := 10 - g.lines_cleared
	rec.x = f32(tx)
	rec.y = f32(ty + 30)
	rec.width = 12
	rec.height = 12

	for i in 1 ..< 11 {
		if i > g.lines_cleared {
			rl.DrawRectangleRec(rec, rl.DARKGRAY)
		} else {
			rl.DrawRectangleRec(rec, rl.GREEN)
		}
		rec.x += rec.width + 2
	}

	ty += font_size + 40
	rl.DrawText(
		fmt.ctprintf("Score: %d", g.game_score),
		tx,
		ty,
		font_size,
		rl.WHITE,
	)
	ty += 80

	rl.DrawText("Next:", tx, ty, font_size, rl.WHITE)
	g.next_tetro.x = 14
	g.next_tetro.y = 8
	if g.next_tetro.size == 2 {
		g.next_tetro.y = 9
	}

	draw_tetromino(g.next_tetro) // Or pass Game ??

	// Draw Status over game board
	msg: cstring = ""
	font_size = 32
	y :=
		BORDER_HEIGHT +
		BORDER_SIZE +
		GAME_HEIGHT * BLOCK_SIZE / 2 -
		font_size / 2

	if g.debug {
		msg = "Debug (D)"
	} else if g.game_paused && !g.game_over {
		msg = "Pause (P)"
	}

	if len(msg) > 0 {
		offset := i32(BORDER_LEFT + BORDER_SIZE)
		w := rl.MeasureText(msg, font_size)
		rl.DrawText(
			msg,
			offset + GAME_WIDTH * BLOCK_SIZE / 2 - w / 2,
			y,
			font_size,
			rl.WHITE,
		)
	}
}

// Updates the game state each frame
// Handles user input for tetromino movement and rotation
// Manages tetromino dropping and collision detection
update :: proc(g: ^Game, dt: f64) {
	// Update
	//update_tetromino(&current_piece)
	if g.tetro_dropped == false {
		if g.game_paused == false {
			if rl.IsKeyPressed(.RIGHT) || rl.IsKeyPressedRepeat(.RIGHT) {
				move_tetro(g, {1, 0})
			}

			if rl.IsKeyPressed(.LEFT) || rl.IsKeyPressedRepeat(.LEFT) {
				move_tetro(g, {-1, 0})
			}

			if rl.IsKeyPressed(.DOWN) || rl.IsKeyPressedRepeat(.DOWN) {
				move_tetro(g, {0, 1})
				g.game_score += 1
			}

			if rl.IsKeyPressed(.UP) || rl.IsKeyPressed(.X) {
				rotate_with_kick(g)
				if collide(g, g.tetro) {
					rotate_ccw_with_kick(g)
				} else {
					rotate(&g.ghost)
				}
			}

			if rl.IsKeyPressed(.Z) {
				rotate_ccw_with_kick(g)
				if collide(g, g.tetro) {
					rotate_with_kick(g)
				} else {
					rotate_ccw(&g.ghost)
				}
			}

			if rl.IsKeyPressed(.N) {
				random_tetromino(&g.next_tetro)
			}

			if rl.IsKeyPressed(.D) {
				g.debug = !g.debug
			}

			if !g.debug && rl.IsKeyPressed(.SPACE) {
				g.delta_time = f64(rl.GetFrameTime())
				g.tetro_dropped = true
				fmt.printf("dropping piece at height: %d\n", g.tetro.y)
				collided := false
				prev_y := g.tetro.y
				lines_dropped := g.tetro.y //-1, 0 or 1 depending on piece
				for !collided {
					g.tetro.y += 1
					lines_dropped += 1
					if collide(g, g.tetro) {
						break
					}
				}
				g.tetro.y = prev_y
				g.game_score += lines_dropped
			}

			collided := false
			g.ghost.y = g.tetro.y
			g.ghost.x = g.tetro.x
			for !collided {
				g.ghost.y += 1
				if collide(g, g.ghost) {
					break
				}
			}

			g.ghost.y -= 1
			// fmt.printf("g.ghost from %d to %d\n", g.ghost.y, prev_y)
			// g.ghost.y = prev_y

			if rl.IsKeyPressed(.G) {
				fmt.println("GAME OVER TEST")
				set_game_over(g, dt)
			}
		}

		if rl.IsKeyPressed(.P) {
			g.game_paused = !g.game_paused
		}

	}

	if g.game_over &&
	   g.game_over_time - dt < 0 &&
	   g.game_launch_time - dt < 0 &&
	   rl.IsKeyPressed(.SPACE) {
		// new game in 3.., 2.., 1..
		g.game_launch_time = dt + GAME_LAUNCH_TIME
	}

	if g.debug == false &&
	   g.game_paused == false &&
	   dt - g.last_tick >= g.delta_time {
		g.last_tick = dt
		if collide(g, g.tetro) || move_tetro(g, {0, 1}) == false {
			lines := merge_tetro(g)
			g.lines_cleared += lines
			g.game_score += compute_score(g.level, lines)
			if g.lines_cleared >= 10 {
				g.level += 1
				g.lines_cleared = 0
			}

			next_tetro(g)

			if g.end_game == true {
				set_game_over(g, dt)
				g.end_game = false
			}
			if collide(g, g.tetro) {
				g.end_game = true
				//set_game_over(g)
				fmt.println("GAME OVER COLLISION")
			}
			g.delta_time = level_time(g.level)
			g.tetro_dropped = false
		}
	}
}

next_tetro :: proc(g: ^Game) {
	g.tetro = g.next_tetro
	set_tetromino_start(&g.tetro)
	set_tetromino(&g.ghost, g.tetro.id)
	random_tetromino(&g.next_tetro)
}

set_game_over :: proc(g: ^Game, dt: f64) {
	clear_board(g)
	g.game_paused = true
	g.game_over = true
	g.game_over_time = dt + GATE_DURATION
}

start_new_game :: proc(g: ^Game, dt: f64) {
	g.game_score = 0
	g.game_paused = false
	g.game_over = false
	g.lines_cleared = 0
	g.level = 1
	g.last_tick = dt
	g.delta_time = level_time(1)
	g.gate_time = 0
	g.game_over_time = 0
	g.game_launch_time = 0
	g.tetro_dropped = false
	clear_board(g)

	next_tetro(g)
}

compute_score :: proc(level, lines: int) -> int {
	switch (lines) {
	case 1:
		return level * 100
	case 2:
		return level * 300
	case 3:
		return level * 500
	case 4:
		return level * 800
	case:
		return 0
	}
}

// Renders a tetromino on the game board
// Takes a tetromino and draws each of its non-empty blocks with the appropriate color
draw_tetromino :: proc(t: Tetromino) {
	size := f32(BLOCK_SIZE)

	// Draw the main 4x4 grid
	for r in 0 ..< 4 {
		for c in 0 ..< 4 {
			if t.shape[c][r] != 0 {
				draw_block(t.x + c, t.y + r, size, Tetro_Color[t.shape[c][r]])
			}
		}
	}
}

draw_ghost :: proc(t: Tetromino) {
	size := f32(BLOCK_SIZE)
	// Draw the main 4x4 grid
	for r in 0 ..< 4 {
		for c in 0 ..< 4 {
			if t.shape[c][r] != 0 {
				//draw_block(t.x + c, t.y + r, size, Tetro_Color[t.shape[c][r]])
				draw_outline(t.x + c, t.y + r, size, rl.SKYBLUE)
			}
		}
	}
}

// Draws a single block at the specified grid position with the given color
// Used by both the tetromino and board rendering functions
draw_block :: proc(x, y: int, size: f32, color: rl.Color) {
	rec := rl.Rectangle{size * f32(x), size * f32(y), size - 1, size - 1}

	rl.DrawRectangleRounded(rec, 0.4, 32, color)
}

draw_outline :: proc(x, y: int, size: f32, color: rl.Color) {
	rec := rl.Rectangle{size * f32(x), size * f32(y), size - 1, size - 1}

	rl.DrawRectangleRoundedLines(rec, 0.4, 32, color)
	//     void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments, Color color);         // Draw rectangle lines with rounded edges

}

// Renders the game board with all placed tetromino blocks
// Empty cells are drawn with a grid, while filled cells show the tetromino color
draw_board :: proc(g: ^Game) {
	size := f32(BLOCK_SIZE)
	grid_color := rl.Color{33, 33, 33, 255}
	for y in 0 ..< GAME_HEIGHT {
		for x in 0 ..< GAME_WIDTH {
			rec := rl.Rectangle{f32(x) * size, f32(y) * size, size, size}
			if g.board[y][x] == 0 {
				rl.DrawRectangleRec(rec, Tetro_Color[0])
				rl.DrawRectangleLinesEx(rec, 0.5, grid_color)
			} else {
				rec.width = rec.width - 1
				rec.height = rec.height - 1
				rl.DrawRectangleRounded(
					rec,
					0.4,
					32,
					Tetro_Color[g.board[y][x]],
				)
			}
		}
	}
}

draw_game_over :: proc(g: ^Game, dt: f64) {
	got := g.game_over_time - dt // game over timer
	cdt := g.game_launch_time - dt // countdown timer

	game_over_text := got < 0.0
	countdown_timer := cdt < GAME_LAUNCH_TIME && cdt > 0

	if cdt < 0 {
		draw_curtain(got / GATE_DURATION)
	}

	if game_over_text {
		border: i32 = 10
		y: i32 = 50
		rec := rl.Rectangle {
			f32(border),
			f32(y),
			GAME_WIDTH * f32(BLOCK_SIZE) - f32(border) * 2.0,
			140,
		}
		rl.DrawRectangleRec(rec, rl.Color{0, 0, 0, 214})

		if countdown_timer {
			count := int(math.floor(cdt))
			if count > 0 {
				//draw_falling_curtain((cdt - 1.0) / (GAME_LAUNCH_TIME - 1.0))
				draw_curtain(1.0 - (cdt - 1.0) / (GAME_LAUNCH_TIME - 1.0))
				draw_game_launch_text(g, y, border, count)
			} else {
				start_new_game(g, dt)
			}
		} else {
			draw_game_over_text(g, y, border)
		}
	}
}

draw_curtain :: proc(pct: f64) {
	n := 0
	sx := 0
	sy := 0
	if pct > 0.0 {
		n = (GAME_WIDTH * GAME_HEIGHT) - int(pct * GAME_WIDTH * GAME_HEIGHT)
		sx = n % GAME_WIDTH
		sy = GAME_HEIGHT - n / GAME_WIDTH
	}

	for y := GAME_HEIGHT - 1; y >= sy; y -= 1 {
		mx := GAME_WIDTH
		if pct > 0.0 && y == sy {
			mx = sx
		}

		for x := 0; x < mx; x += 1 {
			draw_curtain_segment(x, y)
		}
	}
}

//draw_falling_curtain :: proc(pct: f64) {
//    sy := GAME_HEIGHT - int(GAME_HEIGHT * pct)
//    for y := sy; y < GAME_HEIGHT; y += 1 {
//        for x := 0; x < GAME_WIDTH; x += 1 {
//            draw_curtain_segment(x, y)
//        }
//    }
//}

draw_curtain_segment :: proc(x, y: int) {
	size := f32(BLOCK_SIZE)
	rec := rl.Rectangle{f32(x) * size, f32(y) * size, size - 1, size - 1}
	rl.DrawRectangleRec(rec, rl.DARKGRAY)
	c := rl.Color{224, 224, 224, 255}
	rl.DrawRectangleLinesEx(rec, 1, c)
	dc: u8 = 32
	c.r -= dc
	c.g -= dc
	c.b -= dc
	iters := 6
	for i := 0; i < iters; i += 1 {
		rec.x += 2
		rec.y += 2
		rec.width -= 4
		rec.height -= 4
		if i == iters - 1 {
			rl.DrawRectangleRec(rec, rl.BLACK)
		} else {
			c.r -= dc
			c.g -= dc
			c.b -= dc
			rl.DrawRectangleLinesEx(rec, 1, c)
		}
	}
}

draw_game_launch_text :: proc(g: ^Game, y, border: i32, n: int) {
	font: i32 = 100
	text: cstring
	if n == 3 {
		text = "3..."
	} else if n == 2 {
		text = "2.."
	} else {
		text = "1."
	}

	w := rl.MeasureText(text, font)
	y := y + font / 4
	rl.DrawText(text, GAME_WIDTH * BLOCK_SIZE / 2 - w / 2, y, font, rl.WHITE)
}

draw_game_over_text :: proc(g: ^Game, y, border: i32) {
	font: i32 = 28

	text1: cstring = "Press <SPACE>"
	text2: cstring = "to restart"

	w := rl.MeasureText(text1, font)
	y := y + font + 8
	rl.DrawText(text1, GAME_WIDTH * BLOCK_SIZE / 2 - w / 2, y, font, rl.WHITE)

	w = rl.MeasureText(text2, font)
	y += font + font / 2
	rl.DrawText(text2, GAME_WIDTH * BLOCK_SIZE / 2 - w / 2, y, font, rl.WHITE)
}


// Attempts to move a tetromino in the specified direction
// Returns true if the move was successful, false if blocked by collision
move_tetro :: proc(g: ^Game, dir: Vec2i) -> bool {
	g.tetro.x += dir.x
	g.tetro.y += dir.y

	if collide(g, g.tetro) {
		g.tetro.x -= dir.x
		g.tetro.y -= dir.y
		return false // can't move
	}

	return true // can move
}

// Checks if a tetromino collides with the game boundaries or existing blocks on the board
// Returns true if there is a collision, false otherwise
collide :: proc(g: ^Game, t: Tetromino) -> bool {
	for c in 0 ..< 4 {
		for r in 0 ..< 4 {
			if t.shape[c][r] == 0 {
				continue
			}

			// Check if the tetromino is outside the game boundaries
			if t.x + c < 0 ||
			   t.x + c >= GAME_WIDTH ||
			   t.y + r < 0 ||
			   t.y + r >= GAME_HEIGHT {
				return true
			}

			// Check if the tetromino collides with existing blocks on the board
			if g.board[t.y + r][t.x + c] != 0 {
				return true
			}
		}
	}

	return false
}

// Merges a tetromino into the game board when it can no longer move down
// Transfers each non-empty block from the tetromino to the board
// Returns the number of lines cleared after merging
merge_tetro :: proc(g: ^Game) -> int {
	for c in 0 ..< 4 {
		for r in 0 ..< 4 {
			if g.tetro.shape[c][r] != 0 {
				g.board[g.tetro.y + r][g.tetro.x + c] = g.tetro.shape[c][r]
			}
		}
	}

	// remove filled lines
	return clear_lines(g)
}

// Fills the bottom portion of the board with random tetromino blocks
// Used for testing or creating a starting difficulty level
// The higher the level, the more rows will be filled
fill_board :: proc(g: ^Game) {
	for y in 0 ..< GAME_HEIGHT {
		for x in 0 ..< GAME_WIDTH {
			if y > GAME_HEIGHT - g.level {
				g.board[y][x] = rand.int31() % 8
			}
		}
	}
}

clear_board :: proc(g: ^Game) {
	for y in 0 ..< GAME_HEIGHT {
		for x in 0 ..< GAME_WIDTH {
			g.board[y][x] = 0
		}
	}
}

// Check each row of the board for any rows that don't contain zeros
// Remove those rows and shift all rows above them down by one
// Up to 4 rows may be filled at once
clear_lines :: proc(g: ^Game) -> int {
	cleared := 0

	for y := GAME_HEIGHT - 1; y >= 0; y -= 1 {
		// Check if this row is filled (doesn't contain any zeros)
		row_filled := true
		for x in 0 ..< GAME_WIDTH {
			if g.board[y][x] == 0 {
				row_filled = false
				break
			}
		}

		if row_filled {
			// This row is filled, remove it by shifting all rows above it down
			for move_y := y; move_y > 0; move_y -= 1 {
				for x in 0 ..< GAME_WIDTH {
					g.board[move_y][x] = g.board[move_y - 1][x]
				}
			}

			// Clear the top row
			for x in 0 ..< GAME_WIDTH {
				g.board[0][x] = 0
			}

			// Since we shifted rows down, we need to check this row again
			y += 1

			// Increment the number of lines cleared
			cleared += 1
		}
	}

	return cleared
}

// Calculates the time interval between tetromino drops based on the current level
// As the level increases, the time interval decreases, making the game faster
level_time :: proc(level: int) -> f64 {
	lv := f64(level - 1)
	return math.pow(0.8 - (lv * 0.007), lv)
}
