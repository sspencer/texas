package main

import "core:math"
import "core:testing"
import rl "vendor:raylib"

@(private)
new_playing_game :: proc() -> Game {
	g: Game
	init_game(&g)
	start_new_game(&g)
	return g
}

@(test)
test_ghost_empty_board_and_stack :: proc(t: ^testing.T) {
	g := new_playing_game()

	// Place a T-piece at spawn on an empty board
	set_tetromino(&g.tetro, 3)
	update_ghost(&g)

	// T occupies rows 0 and 1 of its 3x3 matrix, so bottom row is ghost.y + 1 == BOARD_HEIGHT - 1
	testing.expect_value(t, g.ghost.x, g.tetro.x)
	testing.expect_value(t, g.ghost.y, BOARD_HEIGHT - 2)
	testing.expect_value(t, g.ghost.shape, g.tetro.shape)

	// Place an obstacle directly underneath column 4 at row BOARD_HEIGHT - 5
	g.board[BOARD_HEIGHT - 5][4] = 1
	update_ghost(&g)
	// T's bottom bar (row 1) should rest directly on top of BOARD_HEIGHT - 5 -> ghost.y + 1 == BOARD_HEIGHT - 6
	testing.expect_value(t, g.ghost.y, BOARD_HEIGHT - 7)

	// Moving horizontally off the obstacle should drop the ghost back to the floor
	moved := move_tetro(&g, {3, 0})
	testing.expect(t, moved)
	testing.expect_value(t, g.ghost.x, g.tetro.x)
	testing.expect_value(t, g.ghost.y, BOARD_HEIGHT - 2)
}

@(test)
test_ghost_never_desyncs_on_failed_rotation :: proc(t: ^testing.T) {
	g := new_playing_game()

	// Spawn an I-piece and surround it tightly so rotation with kicks must fail
	set_tetromino(&g.tetro, 1)
	g.tetro.x = 3
	g.tetro.y = HIDDEN_ROWS + 6
	// Fill rows above and below the I-piece completely except the 4 cells it occupies
	for y in (HIDDEN_ROWS + 4) ..= (HIDDEN_ROWS + 10) {
		for x in 0 ..< GAME_WIDTH {
			if y == g.tetro.y + 1 && x >= g.tetro.x && x < g.tetro.x + 4 {
				continue
			}
			g.board[y][x] = 2
		}
	}
	update_ghost(&g)

	before_shape := g.tetro.shape
	before_dir := g.tetro.direction

	rotated, _ := rotate_with_kick(&g, true)
	testing.expect(t, !rotated)
	testing.expect_value(t, g.tetro.shape, before_shape)
	testing.expect_value(t, g.tetro.direction, before_dir)
	// Critical regression check: ghost must STILL match active piece shape & direction!
	testing.expect_value(t, g.ghost.shape, g.tetro.shape)
	testing.expect_value(t, g.ghost.direction, g.tetro.direction)
	testing.expect_value(t, g.ghost.x, g.tetro.x)
	testing.expect_value(t, g.ghost.y, g.tetro.y)
}

@(test)
test_ghost_empty_id_no_infinite_loop :: proc(t: ^testing.T) {
	g: Game
	g.tetro = Tetromino{}
	update_ghost(&g)
	testing.expect_value(t, g.ghost.id, 0)
}

@(test)
test_tetromino_shapes_and_j_l_distinct :: proc(t: ^testing.T) {
	// J (6) has its top-left hook at [0, 0]
	testing.expect_value(t, tetrominos[6].shape[0, 0], i32(6))
	testing.expect_value(t, tetrominos[6].shape[0, 2], i32(0))
	testing.expect_value(t, tetrominos[6].shape[1, 0], i32(6))
	testing.expect_value(t, tetrominos[6].shape[1, 1], i32(6))
	testing.expect_value(t, tetrominos[6].shape[1, 2], i32(6))

	// L (7) has its top-right hook at [0, 2]
	testing.expect_value(t, tetrominos[7].shape[0, 0], i32(0))
	testing.expect_value(t, tetrominos[7].shape[0, 2], i32(7))
	testing.expect_value(t, tetrominos[7].shape[1, 0], i32(7))
	testing.expect_value(t, tetrominos[7].shape[1, 1], i32(7))
	testing.expect_value(t, tetrominos[7].shape[1, 2], i32(7))

	// Every piece 1..7 must have exactly 4 non-zero cells and spawn North
	for id in 1 ..= 7 {
		piece := tetrominos[id]
		testing.expect_value(t, piece.direction, Direction.North)
		count := 0
		for r in 0 ..< 4 {
			for c in 0 ..< 4 {
				if piece.shape[r, c] != 0 {
					count += 1
				}
			}
		}
		testing.expect_value(t, count, 4)
	}
}

@(test)
test_srs_rotation_roundtrip_and_wall_kick :: proc(t: ^testing.T) {
	g := new_playing_game()

	// 4 CW rotations return to original shape and direction; CW + CCW cancels out
	for id in 1 ..= 7 {
		set_tetromino(&g.tetro, id)
		g.tetro.y = HIDDEN_ROWS + 5
		orig := g.tetro
		for _ in 0 ..< 4 {
			rotate_cw(&g.tetro)
		}
		testing.expect_value(t, g.tetro.shape, orig.shape)
		testing.expect_value(t, g.tetro.direction, orig.direction)

		rotate_cw(&g.tetro)
		rotate_ccw(&g.tetro)
		testing.expect_value(t, g.tetro.shape, orig.shape)
		testing.expect_value(t, g.tetro.direction, orig.direction)
	}

	// Wall kick test: T-piece vertical against left wall, rotate CW -> kicks right by +1
	set_tetromino(&g.tetro, 3)
	g.tetro.y = HIDDEN_ROWS + 5
	rotate_cw(&g.tetro) // East: occupies cols 1 & 2 of 3x3
	g.tetro.x = -1 // Flush against left wall (col 1 at board x=0)
	testing.expect(t, !collide(&g, g.tetro))

	rotated, kick_idx := rotate_with_kick(&g, true) // East -> South (needs kick +1 right)
	testing.expect(t, rotated)
	testing.expect_value(t, kick_idx, 1)
	testing.expect_value(t, g.tetro.x, 0)
	testing.expect_value(t, g.tetro.direction, Direction.South)
	testing.expect_value(t, g.ghost.shape, g.tetro.shape)
	testing.expect_value(t, g.ghost.x, g.tetro.x)
}

@(test)
test_seven_bag_randomizer :: proc(t: ^testing.T) {
	bag: Piece_Bag
	bag_refill(&bag)

	for _ in 0 ..< 10 {
		seen: [8]int
		for _ in 0 ..< 7 {
			id := bag_next(&bag)
			testing.expect(t, id >= 1 && id <= 7)
			seen[id] += 1
		}
		for id in 1 ..= 7 {
			testing.expect_value(t, seen[id], 1)
		}
	}
}

@(test)
test_soft_drop_and_hard_drop_scoring :: proc(t: ^testing.T) {
	g := new_playing_game()

	set_tetromino(&g.tetro, 2) // O-piece at y = HIDDEN_ROWS (4), bottom row is y+1
	update_ghost(&g)
	expected_drop := (BOARD_HEIGHT - 2) - g.tetro.y

	// Soft drop 1 cell -> +1 score
	testing.expect(t, soft_drop_step(&g))
	testing.expect_value(t, g.game_score, 1)

	// Hard drop remaining distance -> +2 * remaining
	dropped := hard_drop(&g)
	testing.expect_value(t, dropped, expected_drop - 1)
	testing.expect_value(t, g.game_score, 1 + (expected_drop - 1) * 2)

	// Soft drop when blocked on floor must NOT award score
	set_tetromino(&g.tetro, 2)
	g.tetro.y = BOARD_HEIGHT - 2
	// Move left away from the locked O-piece at x=4..5
	g.tetro.x = 0
	score_before := g.game_score
	testing.expect(t, !soft_drop_step(&g))
	testing.expect_value(t, g.game_score, score_before)
}

@(test)
test_hold_piece_mechanics :: proc(t: ^testing.T) {
	g := new_playing_game()

	first_id := g.tetro.id
	next_id := g.next_queue[0].id

	// First hold stores current piece and pulls next from queue
	testing.expect(t, hold_piece(&g))
	testing.expect_value(t, g.hold_tetro.id, first_id)
	testing.expect_value(t, g.tetro.id, next_id)
	testing.expect(t, g.hold_used)
	testing.expect_value(t, g.ghost.id, next_id)

	// Second hold before locking must be rejected
	testing.expect(t, !hold_piece(&g))

	// Lock piece via hard drop -> hold resets
	hard_drop(&g)
	if g.state == .Line_Clear {
		update_game(&g, LINE_CLEAR_DURATION + 0.01)
	}
	testing.expect(t, !g.hold_used)

	// Holding again swaps with the previously held piece
	cur_id := g.tetro.id
	testing.expect(t, hold_piece(&g))
	testing.expect_value(t, g.tetro.id, first_id)
	testing.expect_value(t, g.hold_tetro.id, cur_id)
}

@(test)
test_line_clear_level_carryover_and_b2b :: proc(t: ^testing.T) {
	g := new_playing_game()
	g.total_lines = 8
	g.level = 0

	// Fill bottom 4 rows except column 0, then drop a vertical I-piece into column 0 for a Tetris
	for y in (BOARD_HEIGHT - 4) ..< BOARD_HEIGHT {
		for x in 1 ..< GAME_WIDTH {
			g.board[y][x] = 6
		}
	}

	set_tetromino(&g.tetro, 1) // I-piece
	rotate_cw(&g.tetro) // vertical at col 2 of 4x4
	g.tetro.x = -2 // col 2 lands at board x=0
	g.tetro.y = HIDDEN_ROWS
	update_ghost(&g)

	hard_drop(&g)
	testing.expect_value(t, g.state, Game_State.Line_Clear)
	testing.expect_value(t, g.clear_row_count, 4)

	// Finish line clear animation
	update_game(&g, LINE_CLEAR_DURATION + 0.01)
	testing.expect_value(t, g.state, Game_State.Playing)
	// 8 + 4 = 12 lines -> level 1 with 2 lines carried over!
	testing.expect_value(t, g.total_lines, 12)
	testing.expect_value(t, g.level, 1)
	testing.expect(t, g.back_to_back)

	// Board should now be completely empty
	for y in 0 ..< BOARD_HEIGHT {
		for x in 0 ..< GAME_WIDTH {
			testing.expect_value(t, g.board[y][x], i32(0))
		}
	}
}

@(test)
test_tspin_detection :: proc(t: ^testing.T) {
	g := new_playing_game()

	y_bot := BOARD_HEIGHT - 1
	for x in 0 ..< GAME_WIDTH {
		if x != 4 {
			g.board[y_bot][x] = 5
		}
	}
	for x in 0 ..< GAME_WIDTH {
		if x < 3 || x > 5 {
			g.board[y_bot - 1][x] = 5
		}
	}
	g.board[y_bot - 2][3] = 5 // top-left corner occupied
	g.board[y_bot - 2][5] = 5 // top-right corner occupied

	// Place T-piece pointing South at (3, y_bot - 2) as if just rotated into place
	set_tetromino(&g.tetro, 3)
	rotate_cw(&g.tetro)
	rotate_cw(&g.tetro)
	g.tetro.x = 3
	g.tetro.y = y_bot - 2
	g.last_action_rotate = true

	testing.expect(t, !collide(&g, g.tetro))
	testing.expect_value(t, detect_tspin(&g), T_Spin_Kind.Full)
}

@(test)
test_lock_delay_and_game_over_restart :: proc(t: ^testing.T) {
	g := new_playing_game()

	// Place O-piece right on the floor
	set_tetromino(&g.tetro, 2)
	g.tetro.y = BOARD_HEIGHT - 2
	update_ghost(&g)

	// Advance half of lock delay -> still playing, not yet locked
	update_game(&g, LOCK_DELAY * 0.5)
	testing.expect_value(t, g.state, Game_State.Playing)
	testing.expect(t, g.on_ground)
	testing.expect_value(t, g.board[BOARD_HEIGHT - 1][4], i32(0))

	// Move right -> resets lock timer!
	testing.expect(t, move_tetro(&g, {1, 0}))
	testing.expect_value(t, g.lock_timer, 0.0)
	testing.expect_value(t, g.lock_resets, 1)

	// Advance past lock delay -> locks onto board
	update_game(&g, LOCK_DELAY + 0.05)
	testing.expect_value(t, g.board[BOARD_HEIGHT - 1][5], i32(2))

	// Trigger game over and verify restart countdown resets state cleanly
	g.game_score = 4200
	set_game_over(&g)
	testing.expect_value(t, g.state, Game_State.Game_Over)
	testing.expect_value(t, g.high_score, 4200)

	// Wait for curtain to finish
	update_game(&g, GATE_DURATION + 0.1)
	testing.expect_value(t, g.state_timer, 0.0)

	// Start countdown and let it complete
	begin_restart_countdown(&g)
	testing.expect_value(t, g.state, Game_State.Countdown)
	update_game(&g, GAME_LAUNCH_TIME)
	testing.expect_value(t, g.state, Game_State.Playing)
	testing.expect_value(t, g.game_score, 0)
	testing.expect_value(t, g.high_score, 4200) // High score preserved across restarts
	testing.expect_value(t, g.level, 0)
}

@(test)
test_level_time_safe_and_monotonic :: proc(t: ^testing.T) {
	prev := level_time(0)
	testing.expect(t, prev > 0.99 && prev <= 1.0)

	for lv in 1 ..= 150 {
		cur := level_time(lv)
		testing.expect(t, !math.is_nan(cur))
		testing.expect(t, !math.is_inf(cur))
		testing.expect(t, cur >= 0.016)
		testing.expect(t, cur <= prev)
		prev = cur
	}
}

@(test)
test_title_screen_defaults_and_starting_height_speed :: proc(t: ^testing.T) {
	g: Game
	init_game(&g)

	// Default launch lands on Title screen at slowest speed (0) and clear board (height 0)
	testing.expect_value(t, g.state, Game_State.Title)
	testing.expect_value(t, g.start_speed, 0)
	testing.expect_value(t, g.start_height, 0)
	testing.expect_value(t, g.title_focus, Title_Focus.Speed)

	// Default start produces a completely clear board at level 0 (1.0s/row)
	start_new_game(&g)
	testing.expect_value(t, g.state, Game_State.Playing)
	testing.expect_value(t, g.level, 0)
	testing.expect_value(t, level_time(g.level), 1.0)
	for y in 0 ..< BOARD_HEIGHT {
		for x in 0 ..< GAME_WIDTH {
			testing.expect_value(t, g.board[y][x], i32(0))
		}
	}

	// Return to title, select max Speed 9 and max Height 5 (with clamping bounds)
	g.game_score = 1234
	return_to_title(&g)
	testing.expect_value(t, g.state, Game_State.Title)
	testing.expect_value(t, g.high_score, 1234)

	title_adjust_value(&g, -5) // clamps to 0
	testing.expect_value(t, g.start_speed, 0)
	title_adjust_value(&g, 20) // clamps to MAX_START_SPEED (9)
	testing.expect_value(t, g.start_speed, 9)

	g.title_focus = .Height
	title_set_value(&g, 8) // out of range for Height -> ignored
	testing.expect_value(t, g.start_height, 0)
	title_adjust_value(&g, 10) // clamps to MAX_START_HEIGHT (5)
	testing.expect_value(t, g.start_height, 5)

	// Start with Speed 9, Height 5 (12 garbage rows)
	start_new_game(&g)
	testing.expect_value(t, g.state, Game_State.Playing)
	testing.expect_value(t, g.level, 9)
	testing.expect(t, level_time(g.level) < 0.1)

	garbage_rows := HEIGHT_ROWS[5]
	testing.expect_value(t, garbage_rows, 12)
	// Rows above garbage must be completely clear
	for y in 0 ..< (BOARD_HEIGHT - garbage_rows) {
		for x in 0 ..< GAME_WIDTH {
			testing.expect_value(t, g.board[y][x], i32(0))
		}
	}
	// Every garbage row must have between 3 and GAME_WIDTH-2 filled cells (never empty, never full)
	for y in (BOARD_HEIGHT - garbage_rows) ..< BOARD_HEIGHT {
		filled := 0
		for x in 0 ..< GAME_WIDTH {
			if g.board[y][x] != 0 {
				filled += 1
			}
		}
		testing.expect(t, filled >= 3 && filled <= GAME_WIDTH - 2)
	}

	// Ghost must rest above/on top of the garbage without colliding
	testing.expect(t, !collide(&g, g.tetro))
	testing.expect(t, !collide(&g, g.ghost))
	testing.expect(t, g.ghost.y >= g.tetro.y)
}

@(test)
test_cli_args_and_viewport_scaling :: proc(t: ^testing.T) {
	// Default: fullscreen enabled
	opts, help, err := parse_cli_args(nil)
	testing.expect(t, opts.fullscreen)
	testing.expect(t, !help)
	testing.expect_value(t, err, "")

	// Windowed flags disable fullscreen
	for flag in ([3]string{"-w", "--windowed", "--no-fullscreen"}) {
		args := []string{flag}
		o, h, e := parse_cli_args(args)
		testing.expect(t, !o.fullscreen)
		testing.expect(t, !h)
		testing.expect_value(t, e, "")
	}

	// Explicit fullscreen flag overrides earlier windowed flag
	o2, h2, e2 := parse_cli_args([]string{"--windowed", "--fullscreen"})
	testing.expect(t, o2.fullscreen)
	testing.expect(t, !h2)
	testing.expect_value(t, e2, "")

	// Help flag
	_, h3, e3 := parse_cli_args([]string{"--help"})
	testing.expect(t, h3)
	testing.expect_value(t, e3, "")

	// Unknown flag returns error
	_, h4, e4 := parse_cli_args([]string{"--bogus"})
	testing.expect(t, !h4)
	testing.expect_value(t, e4, "--bogus")

	// Viewport scaling: exact native window -> scale 1, offset 0
	s1, ox1, oy1 := compute_viewport(f32(BASE_WIDTH), f32(BASE_HEIGHT))
	testing.expect_value(t, s1, f32(1.0))
	testing.expect_value(t, ox1, f32(0.0))
	testing.expect_value(t, oy1, f32(0.0))

	// 1920x1080 fullscreen -> height-limited scale, centered horizontally with zero vertical margin
	s2, ox2, oy2 := compute_viewport(1920, 1080)
	expected_scale := f32(1080) / f32(BASE_HEIGHT)
	testing.expect_value(t, s2, expected_scale)
	testing.expect_value(t, oy2, f32(0.0))
	testing.expect_value(t, ox2, (1920 - f32(BASE_WIDTH) * expected_scale) * 0.5)
}

@(test)
test_title_mouse_selection_and_start :: proc(t: ^testing.T) {
	g: Game
	init_game(&g)
	layout := compute_title_layout()

	rect_center :: proc(r: rl.Rectangle) -> rl.Vector2 {
		return rl.Vector2{r.x + r.width * 0.5, r.y + r.height * 0.5}
	}

	// Hover speed 7 without clicking -> updates hover state only
	hover := update_title_pointer(&g, rect_center(layout.speed_rects[7]), false)
	testing.expect(t, hover)
	testing.expect_value(t, g.title_hover_speed, 7)
	testing.expect_value(t, g.start_speed, 0)

	// Click speed 7 via simulated 1920x1080 fullscreen screen coordinates
	s, ox, oy := compute_viewport(1920, 1080)
	sp7 := rect_center(layout.speed_rects[7])
	screen_pt := rl.Vector2{ox + sp7.x * s, oy + sp7.y * s}
	ui_pt := screen_to_ui(screen_pt, 1920, 1080)
	hover = update_title_pointer(&g, ui_pt, true)
	testing.expect(t, hover)
	testing.expect_value(t, g.start_speed, 7)
	testing.expect_value(t, g.title_focus, Title_Focus.Speed)

	// Click height 4
	hover = update_title_pointer(&g, rect_center(layout.height_rects[4]), true)
	testing.expect(t, hover)
	testing.expect_value(t, g.start_height, 4)
	testing.expect_value(t, g.title_focus, Title_Focus.Height)

	// Click outside interactive targets -> not hovering, state unchanged
	hover = update_title_pointer(&g, rl.Vector2{5, 5}, true)
	testing.expect(t, !hover)
	testing.expect_value(t, g.state, Game_State.Title)

	// Click Start Game button -> launches game at Speed 7, Height 4 (10 garbage rows)
	hover = update_title_pointer(&g, rect_center(layout.start_rect), true)
	testing.expect(t, hover)
	testing.expect_value(t, g.state, Game_State.Playing)
	testing.expect_value(t, g.level, 7)
	testing.expect_value(t, g.start_height, 4)
}

@(test)
test_generative_backdrop_styles_and_bounds :: proc(t: ^testing.T) {
	b: Backdrop

	// Every style should initialize valid, muted palettes and stay bounded & finite over long updates
	for style in Backdrop_Style {
		init_backdrop_style(&b, style)
		testing.expect(t, b.initialized)
		testing.expect_value(t, b.style, style)
		testing.expect_value(t, b.time, 0.0)

		// Verify base gradient stays dark and muted so gameplay remains primary focus
		corners := [4]rl.Color{
			b.palette.bg_top_left,
			b.palette.bg_top_right,
			b.palette.bg_bottom_left,
			b.palette.bg_bottom_right,
		}
		for c in corners {
			testing.expect(t, c.r <= 90 && c.g <= 90 && c.b <= 95)
			testing.expect_value(t, c.a, u8(255))
		}

		// Simulate 2 minutes of gentle ambient animation
		for _ in 0 ..< 240 {
			update_backdrop(&b, 0.5)
		}
		testing.expect(t, b.time > 119.0)

		lo := f32(-BACKDROP_WRAP_MARGIN) - 0.001
		hi := f32(1.0 + BACKDROP_WRAP_MARGIN) + 0.001
		switch style {
		case .Constellation:
			testing.expect(t, b.node_count >= 28 && b.node_count <= BACKDROP_MAX_NODES)
			for i in 0 ..< b.node_count {
				n := b.nodes[i]
				testing.expect(t, !math.is_nan(n.x) && !math.is_nan(n.y))
				testing.expect(t, n.x >= lo && n.x <= hi)
				testing.expect(t, n.y >= lo && n.y <= hi)
			}
		case .Flow_Waves:
			testing.expect(t, b.wave_count >= 5 && b.wave_count <= BACKDROP_MAX_WAVES)
		case .Geo_Drift:
			testing.expect(t, b.shape_count >= 18 && b.shape_count <= BACKDROP_MAX_SHAPES)
			for i in 0 ..< b.shape_count {
				s := b.shapes[i]
				testing.expect(t, !math.is_nan(s.x) && !math.is_nan(s.y) && !math.is_nan(s.angle))
				testing.expect(t, s.x >= lo && s.x <= hi)
				testing.expect(t, s.y >= lo && s.y <= hi)
				testing.expect(t, s.angle >= 0 && s.angle < 360.0)
			}
		case .Orbital_Contours:
			testing.expect(t, b.orbit_count >= 2 && b.orbit_count <= BACKDROP_MAX_ORBITS)
		case .Truchet_Lattice:
			for r in 0 ..< BACKDROP_LATTICE_ROWS {
				for c in 0 ..< BACKDROP_LATTICE_COLS {
					testing.expect(t, b.lattice_cells[r][c] < 4)
				}
			}
		}
	}
}

@(test)
test_generative_backdrop_game_lifecycle :: proc(t: ^testing.T) {
	g: Game
	init_game(&g)
	testing.expect(t, g.backdrop.initialized)

	// Title screen gently animates the backdrop
	update_game(&g, 0.25)
	testing.expect(t, g.backdrop.time > 0.2)

	// Starting a new game generates a different style from the previous backdrop and resets time
	prev_style := g.backdrop.style
	start_new_game(&g)
	testing.expect(t, g.backdrop.initialized)
	testing.expect(t, g.backdrop.style != prev_style)
	testing.expect_value(t, g.backdrop.time, 0.0)

	// Playing advances backdrop; pausing freezes it
	update_game(&g, 0.5)
	t_playing := g.backdrop.time
	testing.expect(t, t_playing >= 0.5)

	g.state = .Paused
	update_game(&g, 1.0)
	testing.expect_value(t, g.backdrop.time, t_playing)
}
