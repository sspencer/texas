package main

import "core:fmt"
import "core:math"
import "core:math/rand"
import "core:os"
import rl "vendor:raylib"

BACKGROUND :: rl.Color{43, 60, 80, 255}
BORDER_COLOR :: rl.Color{36, 36, 48, 255}
PANEL_BG :: rl.Color{16, 18, 26, 255}
GRID_COLOR :: rl.Color{33, 36, 46, 255}
TEXT_MUTED :: rl.Color{170, 180, 195, 255}

GAME_WIDTH :: 10
GAME_HEIGHT :: 20
HIDDEN_ROWS :: 4
BOARD_HEIGHT :: GAME_HEIGHT + HIDDEN_ROWS

OUTER_MARGIN :: 24
BLOCK_SIZE :: 30
BORDER_SIZE :: 8
PANEL_GAP :: 20
PANEL_WIDTH :: 252
BOARD_PX_W :: GAME_WIDTH * BLOCK_SIZE
BOARD_PX_H :: GAME_HEIGHT * BLOCK_SIZE
BORDER_LEFT :: OUTER_MARGIN + BORDER_SIZE
BORDER_TOP :: OUTER_MARGIN + BORDER_SIZE
BASE_WIDTH :: OUTER_MARGIN + BORDER_SIZE + BOARD_PX_W + BORDER_SIZE + PANEL_GAP + PANEL_WIDTH + OUTER_MARGIN
BASE_HEIGHT :: OUTER_MARGIN + BORDER_SIZE + BOARD_PX_H + BORDER_SIZE + OUTER_MARGIN

NEXT_QUEUE_SIZE :: 3
MAX_START_SPEED :: 9
MAX_START_HEIGHT :: 5
MAX_GRAVITY_LEVEL :: 19
LOCK_DELAY :: 0.50
MAX_LOCK_RESETS :: 15
LINE_CLEAR_DURATION :: 0.22
DAS_DELAY :: 0.15
ARR_INTERVAL :: 0.04
SOFT_DROP_INTERVAL :: 0.045
GATE_DURATION :: 0.60
GAME_LAUNCH_TIME :: 4.0 // 3..2..1 countdown then launch
ACTION_BANNER_DURATION :: 1.5

// Classic Nintendo Tetris Type-B starting garbage heights (rows for Height 0..5)
HEIGHT_ROWS := [MAX_START_HEIGHT + 1]int{0, 3, 5, 8, 10, 12}

Game_State :: enum {
	Title,
	Playing,
	Line_Clear,
	Paused,
	Game_Over,
	Countdown,
}

Title_Focus :: enum {
	Speed,
	Height,
}

T_Spin_Kind :: enum {
	None,
	Mini,
	Full,
}

Vec2i :: struct {
	x, y: int,
}

Launch_Options :: struct {
	fullscreen: bool,
}

Game :: struct {
	state:              Game_State,
	title_focus:        Title_Focus,
	title_hover_speed:  int,
	title_hover_height: int,
	title_hover_start:  bool,
	start_speed:        int,
	start_height:       int,
	fullscreen:         bool,
	level:              int,
	total_lines:        int,
	game_score:         int,
	high_score:         int,
	combo:              int,
	back_to_back:       bool,
	board:              [BOARD_HEIGHT][GAME_WIDTH]i32,
	bag:                Piece_Bag,
	tetro:              Tetromino,
	next_queue:         [NEXT_QUEUE_SIZE]Tetromino,
	hold_tetro:         Tetromino,
	hold_used:          bool,
	ghost:              Tetromino,
	gravity_accum:      f64,
	lock_timer:         f64,
	lock_resets:        int,
	lowest_y:           int,
	on_ground:          bool,
	last_action_rotate: bool,
	last_kick_index:    int,
	clear_rows:         [4]int,
	clear_row_count:    int,
	clear_timer:        f64,
	pending_tspin:      T_Spin_Kind,
	state_timer:        f64,
	banner_buf:         [64]u8,
	banner_len:         int,
	banner_sub_buf:     [64]u8,
	banner_sub_len:     int,
	banner_timer:       f64,
	horiz_dir:          int,
	horiz_hold_time:    f64,
	horiz_repeat_accum: f64,
	soft_drop_accum:    f64,
	debug:              bool,
}

parse_cli_args :: proc(args: []string) -> (opts: Launch_Options, show_help: bool, err: string) {
	opts.fullscreen = true

	for arg in args {
		switch arg {
		case "-w", "--windowed", "--no-fullscreen":
			opts.fullscreen = false
		case "-f", "--fullscreen":
			opts.fullscreen = true
		case "-h", "--help":
			show_help = true
			return
		case:
			err = arg
			return
		}
	}

	return
}

print_usage :: proc(prog: string) {
	fmt.printf("Usage: %s [options]\n\n", prog)
	fmt.println("Options:")
	fmt.println("  -w, --windowed, --no-fullscreen  Start in windowed mode (default: fullscreen)")
	fmt.println("  -f, --fullscreen                 Start in fullscreen mode (default)")
	fmt.println("  -h, --help                       Show this help message")
}

compute_windowed_size :: proc(monitor_w, monitor_h: i32) -> (w, h: i32) {
	if monitor_w <= 0 || monitor_h <= 0 {
		return BASE_WIDTH, BASE_HEIGHT
	}
	scale := clamp((f32(monitor_h) * 0.82) / f32(BASE_HEIGHT), 1.0, 1.5)
	max_scale_w := (f32(monitor_w) * 0.85) / f32(BASE_WIDTH)
	scale = math.max(math.min(scale, max_scale_w), 1.0)
	w = i32(math.round(f32(BASE_WIDTH) * scale))
	h = i32(math.round(f32(BASE_HEIGHT) * scale))
	return
}

center_windowed_frame :: proc() {
	monitor := rl.GetCurrentMonitor()
	mw := rl.GetMonitorWidth(monitor)
	mh := rl.GetMonitorHeight(monitor)
	w, h := compute_windowed_size(mw, mh)
	rl.SetWindowSize(w, h)
	if mw > w && mh > h {
		rl.SetWindowPosition((mw - w) / 2, (mh - h) / 2)
	}
}

toggle_fullscreen :: proc(g: ^Game) {
	rl.ToggleBorderlessWindowed()
	g.fullscreen = rl.IsWindowState({.BORDERLESS_WINDOWED_MODE}) || rl.IsWindowFullscreen()
	if !g.fullscreen {
		center_windowed_frame()
	}
}

compute_viewport :: proc(screen_w, screen_h: f32) -> (scale, offset_x, offset_y: f32) {
	if screen_w <= 0 || screen_h <= 0 {
		return 1.0, 0.0, 0.0
	}
	scale = math.min(screen_w / f32(BASE_WIDTH), screen_h / f32(BASE_HEIGHT))
	offset_x = (screen_w - f32(BASE_WIDTH) * scale) * 0.5
	offset_y = (screen_h - f32(BASE_HEIGHT) * scale) * 0.5
	return scale, offset_x, offset_y
}

screen_to_ui :: proc(screen_pos: rl.Vector2, screen_w, screen_h: f32) -> rl.Vector2 {
	scale, offset_x, offset_y := compute_viewport(screen_w, screen_h)
	if scale <= 0 {
		return screen_pos
	}
	return rl.Vector2{(screen_pos.x - offset_x) / scale, (screen_pos.y - offset_y) / scale}
}

point_in_rect :: proc(p: rl.Vector2, r: rl.Rectangle) -> bool {
	return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height
}

Title_Layout :: struct {
	card_outer:   rl.Rectangle,
	card_inner:   rl.Rectangle,
	grid_x:       f32,
	grid_right:   f32,
	div_y:        f32,
	speed_rects:  [MAX_START_SPEED + 1]rl.Rectangle,
	height_rects: [MAX_START_HEIGHT + 1]rl.Rectangle,
	start_rect:   rl.Rectangle,
}

compute_title_layout :: proc() -> Title_Layout {
	layout: Title_Layout
	card_w: f32 = f32(BASE_WIDTH - OUTER_MARGIN * 2) // 588px
	layout.card_outer = rl.Rectangle{OUTER_MARGIN, 174, card_w, 404}
	layout.card_inner = rl.Rectangle{
		layout.card_outer.x + BORDER_SIZE,
		layout.card_outer.y + BORDER_SIZE,
		layout.card_outer.width - 2 * BORDER_SIZE,
		layout.card_outer.height - 2 * BORDER_SIZE,
	}

	btn_w: f32 = 86
	btn_h: f32 = 46
	btn_gap: f32 = 14
	speed_grid_w := 5 * btn_w + 4 * btn_gap // 486px
	layout.grid_x = layout.card_inner.x + (layout.card_inner.width - speed_grid_w) * 0.5
	layout.grid_right = layout.grid_x + speed_grid_w
	layout.div_y = layout.card_inner.y + 174

	for i in 0 ..= MAX_START_SPEED {
		row := i / 5
		col := i % 5
		layout.speed_rects[i] = rl.Rectangle{
			layout.grid_x + f32(col) * (btn_w + btn_gap),
			layout.card_inner.y + 52 + f32(row) * (btn_h + btn_gap),
			btn_w,
			btn_h,
		}
	}

	h_btn_gap: f32 = 12
	h_btn_w := (speed_grid_w - 5 * h_btn_gap) / 6.0
	for i in 0 ..= MAX_START_HEIGHT {
		layout.height_rects[i] = rl.Rectangle{
			layout.grid_x + f32(i) * (h_btn_w + h_btn_gap),
			layout.div_y + 52,
			h_btn_w,
			btn_h,
		}
	}

	layout.start_rect = rl.Rectangle{
		layout.grid_x,
		layout.card_inner.y + layout.card_inner.height - 64,
		speed_grid_w,
		46,
	}
	return layout
}

// Main entry point for Texas
main :: proc() {
	prog := os.args[0] if len(os.args) > 0 else "texas"
	cli_args := os.args[1:] if len(os.args) > 1 else nil
	opts, show_help, err := parse_cli_args(cli_args)
	if show_help {
		print_usage(prog)
		return
	}
	if len(err) > 0 {
		fmt.eprintf("Unknown option: %s\n\n", err)
		print_usage(prog)
		os.exit(1)
	}

	rl.SetTraceLogLevel(.WARNING)
	// WINDOW_HIGHDPI keeps GetScreenWidth/Height in logical points on Retina displays.
	// Without it, raylib on macOS reports the framebuffer size (2x) after a resize,
	// so the viewport scale doubles and the windowed UI renders cropped/oversized.
	rl.SetConfigFlags({.VSYNC_HINT, .WINDOW_RESIZABLE, .WINDOW_HIGHDPI})
	rl.InitWindow(BASE_WIDTH, BASE_HEIGHT, "Texas")
	defer rl.CloseWindow()
	rl.SetWindowMinSize(BASE_WIDTH / 2, BASE_HEIGHT / 2)
	rl.SetTargetFPS(120)

	game: Game
	init_game(&game)

	if opts.fullscreen {
		toggle_fullscreen(&game)
	} else {
		center_windowed_frame()
	}

	for !rl.WindowShouldClose() {
		frame_dt := math.min(f64(rl.GetFrameTime()), 0.1)
		handle_input(&game, frame_dt)
		update_game(&game, frame_dt)
		draw(&game)
		free_all(context.temp_allocator)
	}
}

init_game :: proc(g: ^Game) {
	g.state = .Title
	g.title_focus = .Speed
	g.title_hover_speed = -1
	g.title_hover_height = -1
	g.title_hover_start = false
	g.start_speed = 0
	g.start_height = 0
	g.level = 0
	g.total_lines = 0
	g.game_score = 0
	g.combo = -1
	g.back_to_back = false
	clear_board(g)
}

return_to_title :: proc(g: ^Game) {
	update_high_score(g)
	g.state = .Title
	g.title_hover_speed = -1
	g.title_hover_height = -1
	g.title_hover_start = false
	g.banner_timer = 0
	g.horiz_dir = 0
	g.horiz_hold_time = 0
	g.horiz_repeat_accum = 0
	g.soft_drop_accum = 0
}

title_adjust_value :: proc(g: ^Game, delta: int) {
	switch g.title_focus {
	case .Speed:
		g.start_speed = clamp(g.start_speed + delta, 0, MAX_START_SPEED)
	case .Height:
		g.start_height = clamp(g.start_height + delta, 0, MAX_START_HEIGHT)
	}
}

title_set_value :: proc(g: ^Game, value: int) {
	switch g.title_focus {
	case .Speed:
		if value >= 0 && value <= MAX_START_SPEED {
			g.start_speed = value
		}
	case .Height:
		if value >= 0 && value <= MAX_START_HEIGHT {
			g.start_height = value
		}
	}
}

// Fills the bottom rows of the board with random garbage blocks for starting heights 1..5.
// Guarantees every generated row has both filled blocks and open gaps (never full or empty).
fill_starting_height :: proc(g: ^Game, height: int) {
	h := clamp(height, 0, MAX_START_HEIGHT)
	rows := HEIGHT_ROWS[h]
	if rows <= 0 {
		return
	}

	start_y := BOARD_HEIGHT - rows
	for y in start_y ..< BOARD_HEIGHT {
		filled := 0
		for x in 0 ..< GAME_WIDTH {
			if rand.int_max(100) < 58 {
				g.board[y][x] = i32(rand.int_max(7) + 1)
				filled += 1
			}
		}

		// Ensure at least 2 empty gaps so no row starts completely full
		for filled > GAME_WIDTH - 2 {
			x := rand.int_max(GAME_WIDTH)
			if g.board[y][x] != 0 {
				g.board[y][x] = 0
				filled -= 1
			}
		}

		// Ensure at least 3 filled blocks so no row starts empty
		for filled < 3 {
			x := rand.int_max(GAME_WIDTH)
			if g.board[y][x] == 0 {
				g.board[y][x] = i32(rand.int_max(7) + 1)
				filled += 1
			}
		}
	}
}

start_new_game :: proc(g: ^Game) {
	g.state = .Playing
	g.start_speed = clamp(g.start_speed, 0, MAX_START_SPEED)
	g.start_height = clamp(g.start_height, 0, MAX_START_HEIGHT)
	g.level = g.start_speed
	g.total_lines = 0
	g.game_score = 0
	g.combo = -1
	g.back_to_back = false
	clear_board(g)
	fill_starting_height(g, g.start_height)

	g.bag = Piece_Bag{}
	bag_refill(&g.bag)

	g.hold_tetro = Tetromino{}
	g.hold_used = false

	g.clear_row_count = 0
	g.clear_timer = 0
	g.pending_tspin = .None
	g.state_timer = 0
	g.banner_len = 0
	g.banner_sub_len = 0
	g.banner_timer = 0
	g.horiz_dir = 0
	g.horiz_hold_time = 0
	g.horiz_repeat_accum = 0
	g.soft_drop_accum = 0

	for i in 0 ..< NEXT_QUEUE_SIZE {
		set_tetromino(&g.next_queue[i], bag_next(&g.bag))
	}

	spawn_next_tetro(g)
}

reset_piece_timers :: proc(g: ^Game) {
	g.gravity_accum = 0
	g.lock_timer = 0
	g.lock_resets = 0
	g.lowest_y = g.tetro.y
	g.on_ground = false
	g.last_action_rotate = false
	g.last_kick_index = 0
}

spawn_next_tetro :: proc(g: ^Game) {
	g.tetro = g.next_queue[0]
	set_tetromino_start(&g.tetro)

	for i in 0 ..< NEXT_QUEUE_SIZE - 1 {
		g.next_queue[i] = g.next_queue[i + 1]
	}
	set_tetromino(&g.next_queue[NEXT_QUEUE_SIZE - 1], bag_next(&g.bag))

	g.hold_used = false
	reset_piece_timers(g)
	update_ghost(g)

	// Block-Out: newly spawned piece immediately collides with existing blocks
	if collide(g, g.tetro) {
		set_game_over(g)
	}
}

set_game_over :: proc(g: ^Game) {
	update_high_score(g)
	g.state = .Game_Over
	g.state_timer = GATE_DURATION
	g.horiz_dir = 0
	g.horiz_hold_time = 0
	g.horiz_repeat_accum = 0
	g.soft_drop_accum = 0
}

begin_restart_countdown :: proc(g: ^Game) {
	clear_board(g)
	g.state = .Countdown
	g.state_timer = GAME_LAUNCH_TIME
}

update_high_score :: proc(g: ^Game) {
	if g.game_score > g.high_score {
		g.high_score = g.game_score
	}
}

set_banner :: proc(g: ^Game, title: string, sub: string = "") {
	n := copy(g.banner_buf[:len(g.banner_buf) - 1], title)
	g.banner_buf[n] = 0
	g.banner_len = n

	m := copy(g.banner_sub_buf[:len(g.banner_sub_buf) - 1], sub)
	g.banner_sub_buf[m] = 0
	g.banner_sub_len = m

	g.banner_timer = ACTION_BANNER_DURATION
}

// Recomputes the ghost piece position directly from the active tetromino.
// Always stays synchronized with g.tetro's shape, orientation, and x position.
update_ghost :: proc(g: ^Game) {
	g.ghost = g.tetro
	if g.ghost.id == 0 || collide(g, g.ghost) {
		return
	}

	for g.ghost.y < BOARD_HEIGHT {
		g.ghost.y += 1
		if collide(g, g.ghost) {
			g.ghost.y -= 1
			break
		}
	}
}

// Checks whether the active tetromino is currently resting on the floor or another block.
is_grounded :: proc(g: ^Game) -> bool {
	test := g.tetro
	test.y += 1
	return collide(g, test)
}

// Called after a successful horizontal move or rotation to apply Guideline lock-delay move reset.
on_piece_manipulated :: proc(g: ^Game) {
	if g.on_ground && g.lock_resets < MAX_LOCK_RESETS {
		g.lock_timer = 0
		g.lock_resets += 1
	}
}

// Attempts to move the active tetromino by dir. Updates ghost and lock reset state.
move_tetro :: proc(g: ^Game, dir: Vec2i) -> bool {
	g.tetro.x += dir.x
	g.tetro.y += dir.y

	if collide(g, g.tetro) {
		g.tetro.x -= dir.x
		g.tetro.y -= dir.y
		return false
	}

	g.last_action_rotate = false
	if dir.x != 0 {
		on_piece_manipulated(g)
	} else if dir.y > 0 {
		g.lock_timer = 0
		if g.tetro.y > g.lowest_y {
			g.lowest_y = g.tetro.y
			g.lock_resets = 0
		}
	}

	update_ghost(g)
	return true
}

// Soft drops by 1 cell, awarding 1 point only if the piece actually moved down.
soft_drop_step :: proc(g: ^Game) -> bool {
	if move_tetro(g, {0, 1}) {
		g.game_score += 1
		g.gravity_accum = 0
		update_high_score(g)
		return true
	}
	return false
}

// Instantly drops the active tetromino to its ghost position, awards 2 points per cell, and locks.
hard_drop :: proc(g: ^Game) -> int {
	if g.state != .Playing {
		return 0
	}

	update_ghost(g)
	dropped := g.ghost.y - g.tetro.y
	if dropped > 0 {
		g.tetro.y = g.ghost.y
		g.game_score += dropped * 2
		g.last_action_rotate = false
		update_high_score(g)
	}

	lock_current_piece(g)
	return dropped
}

// Swaps the current tetromino with the hold slot (once per locked piece).
hold_piece :: proc(g: ^Game) -> bool {
	if g.state != .Playing || g.hold_used {
		return false
	}

	current_id := g.tetro.id
	if g.hold_tetro.id == 0 {
		set_tetromino(&g.hold_tetro, current_id)
		spawn_next_tetro(g)
	} else {
		swap_id := g.hold_tetro.id
		set_tetromino(&g.hold_tetro, current_id)
		set_tetromino(&g.tetro, swap_id)
		reset_piece_timers(g)
		update_ghost(g)
		if collide(g, g.tetro) {
			set_game_over(g)
		}
	}

	g.hold_used = true
	return true
}

// Detects T-Spin (Full or Mini) using the Guideline 3-corner rule before merging the T-piece.
detect_tspin :: proc(g: ^Game) -> T_Spin_Kind {
	if g.tetro.id != 3 || !g.last_action_rotate {
		return .None
	}

	cx := g.tetro.x + 1
	cy := g.tetro.y + 1

	// Corner coordinates: TL, TR, BR, BL
	corners := [4]Vec2i{
		{cx - 1, cy - 1},
		{cx + 1, cy - 1},
		{cx + 1, cy + 1},
		{cx - 1, cy + 1},
	}

	occupied: [4]bool
	count := 0
	for corner, i in corners {
		if corner.x < 0 ||
		   corner.x >= GAME_WIDTH ||
		   corner.y < 0 ||
		   corner.y >= BOARD_HEIGHT ||
		   g.board[corner.y][corner.x] != 0 {
			occupied[i] = true
			count += 1
		}
	}

	if count < 3 {
		return .None
	}

	// Front corners depend on which direction the T-piece is pointing
	front_a, front_b: int
	switch g.tetro.direction {
	case .North:
		front_a, front_b = 0, 1 // TL, TR
	case .East:
		front_a, front_b = 1, 2 // TR, BR
	case .South:
		front_a, front_b = 2, 3 // BR, BL
	case .West:
		front_a, front_b = 3, 0 // BL, TL
	}

	if (occupied[front_a] && occupied[front_b]) || g.last_kick_index == 4 {
		return .Full
	}
	return .Mini
}

// Locks the active piece into the board, checks Lock-Out, detects full lines, and triggers clear or spawn.
lock_current_piece :: proc(g: ^Game) {
	tspin := detect_tspin(g)
	any_visible := false

	for r in 0 ..< 4 {
		for c in 0 ..< 4 {
			cell := g.tetro.shape[r, c]
			if cell == 0 {
				continue
			}
			bx := g.tetro.x + c
			by := g.tetro.y + r
			if by >= HIDDEN_ROWS {
				any_visible = true
			}
			if by >= 0 && by < BOARD_HEIGHT && bx >= 0 && bx < GAME_WIDTH {
				g.board[by][bx] = cell
			}
		}
	}

	// Lock-Out: piece locked completely above the visible playfield
	if !any_visible {
		set_game_over(g)
		return
	}

	row_count := find_full_rows(g)
	if row_count > 0 {
		g.clear_row_count = row_count
		g.clear_timer = LINE_CLEAR_DURATION
		g.pending_tspin = tspin
		g.state = .Line_Clear
		return
	}

	// No lines cleared
	g.combo = -1
	if tspin != .None {
		pts := compute_score(g.level, 0, tspin, false)
		g.game_score += pts
		update_high_score(g)
		if tspin == .Full {
			set_banner(g, "T-SPIN!")
		} else {
			set_banner(g, "T-SPIN MINI")
		}
	}

	spawn_next_tetro(g)
}

find_full_rows :: proc(g: ^Game) -> int {
	count := 0
	for y in 0 ..< BOARD_HEIGHT {
		filled := true
		for x in 0 ..< GAME_WIDTH {
			if g.board[y][x] == 0 {
				filled = false
				break
			}
		}
		if filled && count < 4 {
			g.clear_rows[count] = y
			count += 1
		}
	}
	return count
}

finish_line_clear :: proc(g: ^Game) {
	lines := clear_lines(g)
	tspin := g.pending_tspin
	g.clear_row_count = 0
	g.pending_tspin = .None

	difficult := lines == 4 || (tspin != .None && lines > 0)
	b2b_bonus := difficult && g.back_to_back

	points := compute_score(g.level, lines, tspin, b2b_bonus)

	g.combo += 1
	if g.combo > 0 {
		points += 50 * g.combo * (g.level + 1)
	}

	if difficult {
		g.back_to_back = true
	} else if lines > 0 {
		g.back_to_back = false
	}

	g.game_score += points
	update_high_score(g)

	g.total_lines += lines
	g.level = g.start_speed + g.total_lines / 10

	// Build action banner text
	title := ""
	switch tspin {
	case .Full:
		switch lines {
		case 1:
			title = "T-SPIN SINGLE!"
		case 2:
			title = "T-SPIN DOUBLE!"
		case 3:
			title = "T-SPIN TRIPLE!"
		}
	case .Mini:
		switch lines {
		case 1:
			title = "T-SPIN MINI!"
		case 2:
			title = "T-SPIN MINI x2!"
		}
	case .None:
		switch lines {
		case 1:
			title = "SINGLE"
		case 2:
			title = "DOUBLE"
		case 3:
			title = "TRIPLE!"
		case 4:
			title = "TETRIS!"
		}
	}

	sub_buf: [64]u8
	sub := ""
	if b2b_bonus && g.combo > 0 {
		sub = fmt.bprintf(sub_buf[:], "B2B  COMBO x%d", g.combo)
	} else if b2b_bonus {
		sub = "BACK-TO-BACK!"
	} else if g.combo > 0 {
		sub = fmt.bprintf(sub_buf[:], "COMBO x%d", g.combo)
	}

	if len(title) > 0 {
		set_banner(g, title, sub)
	}

	g.state = .Playing
	spawn_next_tetro(g)
}

compute_score :: proc(level, lines: int, tspin: T_Spin_Kind = .None, b2b: bool = false) -> int {
	base := 0
	switch tspin {
	case .Full:
		switch lines {
		case 0:
			base = 400
		case 1:
			base = 800
		case 2:
			base = 1200
		case 3:
			base = 1600
		}
	case .Mini:
		switch lines {
		case 0:
			base = 100
		case 1:
			base = 200
		case 2:
			base = 400
		}
	case .None:
		switch lines {
		case 1:
			base = 100
		case 2:
			base = 300
		case 3:
			base = 500
		case 4:
			base = 800
		}
	}

	score := base * (level + 1)
	if b2b {
		score = (score * 3) / 2
	}
	return score
}

update_title_pointer :: proc(g: ^Game, ui_pos: rl.Vector2, clicked: bool) -> (hovering: bool) {
	layout := compute_title_layout()
	g.title_hover_speed = -1
	g.title_hover_height = -1
	g.title_hover_start = false

	for i in 0 ..= MAX_START_SPEED {
		if point_in_rect(ui_pos, layout.speed_rects[i]) {
			g.title_hover_speed = i
			hovering = true
			if clicked {
				g.start_speed = i
				g.title_focus = .Speed
			}
			return
		}
	}

	for i in 0 ..= MAX_START_HEIGHT {
		if point_in_rect(ui_pos, layout.height_rects[i]) {
			g.title_hover_height = i
			hovering = true
			if clicked {
				g.start_height = i
				g.title_focus = .Height
			}
			return
		}
	}

	if point_in_rect(ui_pos, layout.start_rect) {
		g.title_hover_start = true
		hovering = true
		if clicked {
			start_new_game(g)
		}
		return
	}

	return false
}

handle_title_input :: proc(g: ^Game) {
	mouse_ui := screen_to_ui(
		rl.GetMousePosition(),
		f32(rl.GetScreenWidth()),
		f32(rl.GetScreenHeight()),
	)
	hovering := update_title_pointer(g, mouse_ui, rl.IsMouseButtonPressed(.LEFT))

	if g.state != .Title {
		rl.SetMouseCursor(.DEFAULT)
		return
	}

	if rl.IsKeyPressed(.UP) || rl.IsKeyPressed(.DOWN) || rl.IsKeyPressed(.TAB) || rl.IsKeyPressed(.W) || rl.IsKeyPressed(.S) {
		g.title_focus = .Height if g.title_focus == .Speed else .Speed
	}

	if rl.IsKeyPressed(.LEFT) || rl.IsKeyPressed(.A) {
		title_adjust_value(g, -1)
	}
	if rl.IsKeyPressed(.RIGHT) || rl.IsKeyPressed(.D) {
		title_adjust_value(g, 1)
	}

	// Allow direct 0-9 number selection
	num_keys := [10]rl.KeyboardKey{
		.ZERO, .ONE, .TWO, .THREE, .FOUR, .FIVE, .SIX, .SEVEN, .EIGHT, .NINE,
	}
	for key, digit in num_keys {
		if rl.IsKeyPressed(key) {
			title_set_value(g, digit)
		}
	}

	if rl.IsKeyPressed(.ENTER) || rl.IsKeyPressed(.KP_ENTER) || rl.IsKeyPressed(.SPACE) {
		rl.SetMouseCursor(.DEFAULT)
		start_new_game(g)
		return
	}

	rl.SetMouseCursor(.POINTING_HAND if hovering else .DEFAULT)
}

// Handles Raylib keyboard input and DAS/ARR auto-repeat.
handle_input :: proc(g: ^Game, frame_dt: f64) {
	if rl.IsKeyPressed(.F) || rl.IsKeyPressed(.F11) {
		toggle_fullscreen(g)
	}

	if g.state == .Title {
		handle_title_input(g)
		return
	}

	if rl.IsKeyPressed(.P) {
		if g.state == .Playing {
			g.state = .Paused
			return
		} else if g.state == .Paused {
			g.state = .Playing
			return
		}
	}

	if g.state == .Paused {
		if rl.IsKeyPressed(.T) {
			return_to_title(g)
		}
		return
	}

	if g.state == .Game_Over {
		if g.state_timer <= 0 {
			if rl.IsKeyPressed(.SPACE) || rl.IsKeyPressed(.ENTER) {
				begin_restart_countdown(g)
			} else if rl.IsKeyPressed(.T) {
				return_to_title(g)
			}
		}
		return
	}

	if g.state != .Playing {
		return
	}

	// Debug mode toggle (freezes gravity and enables debug shortcuts)
	if rl.IsKeyPressed(.D) {
		g.debug = !g.debug
	}

	if g.debug {
		if rl.IsKeyPressed(.N) {
			set_tetromino(&g.next_queue[0], bag_next(&g.bag))
		}
		if rl.IsKeyPressed(.G) {
			set_game_over(g)
			return
		}
	}

	// Horizontal movement with DAS + ARR
	left_down := rl.IsKeyDown(.LEFT)
	right_down := rl.IsKeyDown(.RIGHT)

	if rl.IsKeyPressed(.LEFT) {
		move_tetro(g, {-1, 0})
		g.horiz_dir = -1
		g.horiz_hold_time = 0
		g.horiz_repeat_accum = 0
	} else if rl.IsKeyPressed(.RIGHT) {
		move_tetro(g, {1, 0})
		g.horiz_dir = 1
		g.horiz_hold_time = 0
		g.horiz_repeat_accum = 0
	} else {
		if g.horiz_dir == -1 && !left_down {
			g.horiz_dir = 1 if right_down else 0
			g.horiz_hold_time = 0
			g.horiz_repeat_accum = 0
		} else if g.horiz_dir == 1 && !right_down {
			g.horiz_dir = -1 if left_down else 0
			g.horiz_hold_time = 0
			g.horiz_repeat_accum = 0
		} else if g.horiz_dir == 0 {
			if left_down {
				g.horiz_dir = -1
				g.horiz_hold_time = 0
				g.horiz_repeat_accum = 0
			} else if right_down {
				g.horiz_dir = 1
				g.horiz_hold_time = 0
				g.horiz_repeat_accum = 0
			}
		}

		if g.horiz_dir != 0 {
			g.horiz_hold_time += frame_dt
			if g.horiz_hold_time >= DAS_DELAY {
				g.horiz_repeat_accum += frame_dt
				for g.horiz_repeat_accum >= ARR_INTERVAL {
					g.horiz_repeat_accum -= ARR_INTERVAL
					if !move_tetro(g, {g.horiz_dir, 0}) {
						break
					}
				}
			}
		}
	}

	// Soft drop
	if rl.IsKeyPressed(.DOWN) {
		soft_drop_step(g)
		g.soft_drop_accum = 0
	} else if rl.IsKeyDown(.DOWN) {
		g.soft_drop_accum += frame_dt
		for g.soft_drop_accum >= SOFT_DROP_INTERVAL {
			g.soft_drop_accum -= SOFT_DROP_INTERVAL
			if !soft_drop_step(g) {
				break
			}
		}
	} else {
		g.soft_drop_accum = 0
	}

	// Rotation (CW: Up / X, CCW: Z / Left Control)
	if rl.IsKeyPressed(.UP) || rl.IsKeyPressed(.X) {
		rotate_with_kick(g, true)
	}
	if rl.IsKeyPressed(.Z) || rl.IsKeyPressed(.LEFT_CONTROL) {
		rotate_with_kick(g, false)
	}

	// Hold piece (C / Shift)
	if rl.IsKeyPressed(.C) || rl.IsKeyPressed(.LEFT_SHIFT) || rl.IsKeyPressed(.RIGHT_SHIFT) {
		hold_piece(g)
	}

	// Hard drop (Space)
	if !g.debug && rl.IsKeyPressed(.SPACE) {
		hard_drop(g)
	}
}

// Advances game timers, gravity, lock delay, line clear animation, and restart countdown.
update_game :: proc(g: ^Game, frame_dt: f64) {
	if g.banner_timer > 0 {
		g.banner_timer = math.max(0.0, g.banner_timer - frame_dt)
	}

	switch g.state {
	case .Title, .Paused:
		return

	case .Game_Over:
		if g.state_timer > 0 {
			g.state_timer = math.max(0.0, g.state_timer - frame_dt)
		}
		return

	case .Countdown:
		g.state_timer -= frame_dt
		if g.state_timer <= 1.0 {
			start_new_game(g)
		}
		return

	case .Line_Clear:
		g.clear_timer -= frame_dt
		if g.clear_timer <= 0 {
			finish_line_clear(g)
		}
		return

	case .Playing:
		if g.debug {
			return
		}

		// Gravity
		tick_interval := level_time(g.level)
		g.gravity_accum += frame_dt
		for g.gravity_accum >= tick_interval {
			g.gravity_accum -= tick_interval
			if !move_tetro(g, {0, 1}) {
				g.gravity_accum = 0
				break
			}
		}

		// Lock delay when resting on the floor or stack
		if is_grounded(g) {
			g.on_ground = true
			g.lock_timer += frame_dt
			if g.lock_timer >= LOCK_DELAY {
				lock_current_piece(g)
			}
		} else {
			g.on_ground = false
			g.lock_timer = 0
		}
	}
}

// Checks if a tetromino collides with playfield boundaries or placed blocks.
collide :: proc(g: ^Game, t: Tetromino) -> bool {
	for r in 0 ..< 4 {
		for c in 0 ..< 4 {
			if t.shape[r, c] == 0 {
				continue
			}

			bx := t.x + c
			by := t.y + r

			if bx < 0 || bx >= GAME_WIDTH || by < 0 || by >= BOARD_HEIGHT {
				return true
			}

			if g.board[by][bx] != 0 {
				return true
			}
		}
	}

	return false
}

clear_board :: proc(g: ^Game) {
	for y in 0 ..< BOARD_HEIGHT {
		for x in 0 ..< GAME_WIDTH {
			g.board[y][x] = 0
		}
	}
}

// Removes all completely filled rows and shifts rows above them down.
clear_lines :: proc(g: ^Game) -> int {
	cleared := 0

	for y := BOARD_HEIGHT - 1; y >= 0; y -= 1 {
		row_filled := true
		for x in 0 ..< GAME_WIDTH {
			if g.board[y][x] == 0 {
				row_filled = false
				break
			}
		}

		if row_filled {
			for move_y := y; move_y > 0; move_y -= 1 {
				for x in 0 ..< GAME_WIDTH {
					g.board[move_y][x] = g.board[move_y - 1][x]
				}
			}
			for x in 0 ..< GAME_WIDTH {
				g.board[0][x] = 0
			}

			y += 1
			cleared += 1
		}
	}

	return cleared
}

// Guideline gravity curve: seconds per row drop (level 0 = 1.0s, level 9 = ~0.064s).
level_time :: proc(level: int) -> f64 {
	lv := f64(clamp(level, 0, MAX_GRAVITY_LEVEL))
	return math.max(math.pow(0.8 - (lv * 0.007), lv), 0.016)
}

// =============================================================================
// Rendering
// =============================================================================

draw :: proc(g: ^Game) {
	rl.BeginDrawing()
	defer rl.EndDrawing()

	rl.ClearBackground(BACKGROUND)

	// Camera2D transforms run in framebuffer pixels: raylib's BeginMode2D drops the
	// HiDPI screen-scale matrix, so size the viewport from the render (pixel) size.
	// Mouse input stays in logical points (see screen_to_ui callers).
	scale, offset_x, offset_y := compute_viewport(
		f32(rl.GetRenderWidth()),
		f32(rl.GetRenderHeight()),
	)

	ui_camera := rl.Camera2D{
		offset   = rl.Vector2{offset_x, offset_y},
		target   = rl.Vector2{0, 0},
		rotation = 0.0,
		zoom     = scale,
	}

	if g.state == .Title {
		rl.BeginMode2D(ui_camera)
		draw_title_screen(g)
		rl.EndMode2D()
		return
	}

	board_camera := rl.Camera2D{
		offset   = rl.Vector2{
			offset_x + f32(BORDER_LEFT) * scale,
			offset_y + f32(BORDER_TOP) * scale,
		},
		target   = rl.Vector2{0, 0},
		rotation = 0.0,
		zoom     = scale,
	}

	// Draw playfield outer border in scaled UI coordinates
	rl.BeginMode2D(ui_camera)
	board_border := rl.Rectangle{
		OUTER_MARGIN,
		OUTER_MARGIN,
		BOARD_PX_W + 2 * BORDER_SIZE,
		BOARD_PX_H + 2 * BORDER_SIZE,
	}
	rl.DrawRectangleRec(board_border, BORDER_COLOR)
	rl.EndMode2D()

	// Draw playfield inside scaled board camera space (origin = top-left of visible board)
	rl.BeginMode2D(board_camera)

	draw_board(g)

	switch g.state {
	case .Title:
		// Handled above
	case .Playing, .Paused:
		// Draw ghost underneath active piece so active piece cleanly covers overlapping cells
		draw_ghost(g.ghost)
		draw_tetromino(g.tetro)
	case .Line_Clear:
		draw_line_clear_flash(g)
	case .Game_Over, .Countdown:
		draw_game_over_overlay(g)
	}

	draw_banner(g)

	rl.EndMode2D()

	rl.BeginMode2D(ui_camera)
	draw_side_panel(g)
	draw_status_overlay(g)
	rl.EndMode2D()
}

draw_title_screen :: proc(g: ^Game) {
	center_x := i32(BASE_WIDTH / 2)
	layout := compute_title_layout()

	// Stylized title logo with shadow
	title: cstring = "TEXAS"
	title_font: i32 = 80
	tw := rl.MeasureText(title, title_font)
	rl.DrawText(title, center_x - tw / 2 + 4, 24 + 4, title_font, rl.Color{16, 20, 30, 220})
	rl.DrawText(title, center_x - tw / 2, 24, title_font, rl.WHITE)

	// Decorative bar of the 7 tetromino colors
	swatch_size: f32 = 24
	swatch_gap: f32 = 8
	swatch_total := 7 * swatch_size + 6 * swatch_gap
	swatch_x := (f32(BASE_WIDTH) - swatch_total) * 0.5
	for id in 1 ..= 7 {
		draw_block_px(swatch_x, 110, swatch_size, Tetro_Color[id])
		swatch_x += swatch_size + swatch_gap
	}

	sub: cstring = "SELECT STARTING SPEED & HEIGHT"
	sub_w := rl.MeasureText(sub, 16)
	rl.DrawText(sub, center_x - sub_w / 2, 146, 16, TEXT_MUTED)

	// Main setup card (centered symmetrically across BASE_WIDTH)
	card := draw_panel_card(layout.card_outer)
	grid_x := layout.grid_x
	grid_right := layout.grid_right

	// --- SPEED (0..9) ---
	speed_active := g.title_focus == .Speed
	speed_accent := rl.Color{0, 210, 240, 255} if speed_active else TEXT_MUTED

	if speed_active {
		rl.DrawText(">", i32(grid_x - 22), i32(card.y + 18), 22, speed_accent)
	}
	rl.DrawText("SPEED", i32(grid_x), i32(card.y + 18), 22, rl.WHITE if speed_active else TEXT_MUTED)

	speed_info := fmt.ctprintf("Level %d  (%.2fs/row)", g.start_speed, level_time(g.start_speed))
	si_w := rl.MeasureText(speed_info, 16)
	rl.DrawText(speed_info, i32(grid_right) - si_w, i32(card.y + 22), 16, speed_accent)

	for i in 0 ..= MAX_START_SPEED {
		selected := i == g.start_speed
		hovered := i == g.title_hover_speed
		draw_title_option_tile(
			layout.speed_rects[i],
			fmt.ctprintf("%d", i),
			selected,
			speed_active,
			hovered,
		)
	}

	// Divider
	div_y := layout.div_y
	rl.DrawLineEx(
		rl.Vector2{grid_x, div_y},
		rl.Vector2{grid_right, div_y},
		1.0,
		GRID_COLOR,
	)

	// --- HEIGHT (0..5) ---
	height_active := g.title_focus == .Height
	height_accent := rl.Color{245, 210, 80, 255} if height_active else TEXT_MUTED

	if height_active {
		rl.DrawText(">", i32(grid_x - 22), i32(div_y + 16), 22, height_accent)
	}
	rl.DrawText("HEIGHT", i32(grid_x), i32(div_y + 16), 22, rl.WHITE if height_active else TEXT_MUTED)

	height_rows := HEIGHT_ROWS[g.start_height]
	height_info: cstring = "Clear Board" if height_rows == 0 else fmt.ctprintf("%d Garbage Rows", height_rows)
	hi_w := rl.MeasureText(height_info, 16)
	rl.DrawText(height_info, i32(grid_right) - hi_w, i32(div_y + 20), 16, height_accent)

	for i in 0 ..= MAX_START_HEIGHT {
		selected := i == g.start_height
		hovered := i == g.title_hover_height
		draw_title_option_tile(
			layout.height_rects[i],
			fmt.ctprintf("%d", i),
			selected,
			height_active,
			hovered,
		)
	}

	// Start button
	start_box := layout.start_rect
	start_bg := rl.Color{50, 215, 75, 110} if g.title_hover_start else rl.Color{50, 215, 75, 50}
	start_border := rl.WHITE if g.title_hover_start else rl.Color{50, 215, 75, 220}
	rl.DrawRectangleRounded(start_box, 0.25, 8, start_bg)
	rl.DrawRectangleRoundedLinesEx(start_box, 0.25, 8, 2.0, start_border)
	start_label: cstring = "START GAME  (ENTER / SPACE)"
	sl_w := rl.MeasureText(start_label, 18)
	rl.DrawText(
		start_label,
		i32(start_box.x + (start_box.width - f32(sl_w)) * 0.5),
		i32(start_box.y + 14),
		18,
		rl.WHITE,
	)

	// Session High Score (when non-zero)
	if g.high_score > 0 {
		hs := fmt.ctprintf("HIGH SCORE: %d", g.high_score)
		hs_w := rl.MeasureText(hs, 18)
		rl.DrawText(hs, center_x - hs_w / 2, 588, 18, rl.Color{245, 210, 80, 255})
	}

	// Footer hints
	hint1: cstring = "Click or Arrows / 0-9: Choose Speed & Height"
	h1_w := rl.MeasureText(hint1, 14)
	rl.DrawText(hint1, center_x - h1_w / 2, 616, 14, TEXT_MUTED)

	hint2: cstring = "Click Start or Enter / Space    F: Fullscreen    Esc: Quit"
	h2_w := rl.MeasureText(hint2, 14)
	rl.DrawText(hint2, center_x - h2_w / 2, 638, 14, TEXT_MUTED)
}

draw_title_option_tile :: proc(
	rec: rl.Rectangle,
	label: cstring,
	selected, row_active, hovered: bool,
) {
	bg := rl.Color{28, 32, 46, 255}
	border := GRID_COLOR
	text_color := TEXT_MUTED

	if selected {
		if row_active || hovered {
			bg = rl.Color{0, 175, 210, 255}
			border = rl.WHITE
			text_color = rl.Color{12, 16, 24, 255}
		} else {
			bg = rl.Color{52, 66, 92, 255}
			border = rl.Color{140, 165, 200, 255}
			text_color = rl.WHITE
		}
	} else if hovered {
		bg = rl.Color{46, 54, 76, 255}
		border = rl.Color{150, 195, 225, 255}
		text_color = rl.WHITE
	}

	rl.DrawRectangleRounded(rec, 0.25, 8, bg)
	rl.DrawRectangleRoundedLinesEx(rec, 0.25, 8, 2.0 if (selected || hovered) else 1.0, border)

	font_size: i32 = 22
	lw := rl.MeasureText(label, font_size)
	rl.DrawText(
		label,
		i32(rec.x + (rec.width - f32(lw)) * 0.5),
		i32(rec.y + (rec.height - f32(font_size)) * 0.5),
		font_size,
		text_color,
	)
}

draw_board :: proc(g: ^Game) {
	size := f32(BLOCK_SIZE)
	for vy in 0 ..< GAME_HEIGHT {
		by := vy + HIDDEN_ROWS
		for x in 0 ..< GAME_WIDTH {
			rec := rl.Rectangle{f32(x) * size, f32(vy) * size, size, size}
			cell := g.board[by][x]
			if cell == 0 {
				rl.DrawRectangleRec(rec, PANEL_BG)
				rl.DrawRectangleLinesEx(rec, 0.5, GRID_COLOR)
			} else {
				rl.DrawRectangleRec(rec, PANEL_BG)
				draw_block_px(f32(x) * size, f32(vy) * size, size, Tetro_Color[cell])
			}
		}
	}
}

draw_line_clear_flash :: proc(g: ^Game) {
	progress := 1.0 - (g.clear_timer / LINE_CLEAR_DURATION)
	alpha := u8(clamp((1.0 - progress) * 230.0, 40.0, 230.0))
	size := f32(BLOCK_SIZE)

	for i in 0 ..< g.clear_row_count {
		vy := g.clear_rows[i] - HIDDEN_ROWS
		if vy < 0 || vy >= GAME_HEIGHT {
			continue
		}
		rec := rl.Rectangle{0, f32(vy) * size, GAME_WIDTH * size, size}
		rl.DrawRectangleRec(rec, rl.Color{255, 255, 255, alpha})
	}
}

draw_tetromino :: proc(t: Tetromino) {
	size := f32(BLOCK_SIZE)
	for r in 0 ..< 4 {
		for c in 0 ..< 4 {
			cell := t.shape[r, c]
			if cell == 0 {
				continue
			}
			vy := t.y + r - HIDDEN_ROWS
			if vy < 0 || vy >= GAME_HEIGHT {
				continue
			}
			draw_block_px(f32(t.x + c) * size, f32(vy) * size, size, Tetro_Color[cell])
		}
	}
}

draw_ghost :: proc(t: Tetromino) {
	if t.id == 0 {
		return
	}
	size := f32(BLOCK_SIZE)
	for r in 0 ..< 4 {
		for c in 0 ..< 4 {
			cell := t.shape[r, c]
			if cell == 0 {
				continue
			}
			vy := t.y + r - HIDDEN_ROWS
			if vy < 0 || vy >= GAME_HEIGHT {
				continue
			}
			base := Tetro_Color[cell]
			fill := rl.Color{base.r, base.g, base.b, 48}
			outline := rl.Color{base.r, base.g, base.b, 195}
			rec := rl.Rectangle{f32(t.x + c) * size + 1, f32(vy) * size + 1, size - 2, size - 2}
			rl.DrawRectangleRounded(rec, 0.35, 16, fill)
			rl.DrawRectangleRoundedLinesEx(rec, 0.35, 16, 2.0, outline)
		}
	}
}

draw_block_px :: proc(px, py, size: f32, color: rl.Color) {
	rec := rl.Rectangle{px + 0.5, py + 0.5, size - 1.5, size - 1.5}
	rl.DrawRectangleRounded(rec, 0.35, 16, color)

	// Subtle top-left highlight for depth
	highlight := rl.Color{
		u8(math.min(int(color.r) + 55, 255)),
		u8(math.min(int(color.g) + 55, 255)),
		u8(math.min(int(color.b) + 55, 255)),
		120,
	}
	inset := math.max(size * 0.14, 2.0)
	inner := rl.Rectangle{px + inset, py + inset, size - inset * 2.0 - 1.0, size * 0.25}
	rl.DrawRectangleRounded(inner, 0.5, 8, highlight)
}

// Draws a tetromino centered within a target rectangle (used for Hold and Next previews).
draw_piece_preview :: proc(t: Tetromino, box: rl.Rectangle, block_size: f32, dimmed: bool = false) {
	if t.id == 0 {
		return
	}

	min_c, max_c := 4, -1
	min_r, max_r := 4, -1
	for r in 0 ..< 4 {
		for c in 0 ..< 4 {
			if t.shape[r, c] != 0 {
				if c < min_c {min_c = c}
				if c > max_c {max_c = c}
				if r < min_r {min_r = r}
				if r > max_r {max_r = r}
			}
		}
	}
	if max_c < min_c {
		return
	}

	pw := f32(max_c - min_c + 1) * block_size
	ph := f32(max_r - min_r + 1) * block_size
	ox := box.x + (box.width - pw) * 0.5
	oy := box.y + (box.height - ph) * 0.5

	for r in min_r ..= max_r {
		for c in min_c ..= max_c {
			cell := t.shape[r, c]
			if cell == 0 {
				continue
			}
			color := Tetro_Color[cell]
			if dimmed {
				color = rl.Color{color.r / 3 + 35, color.g / 3 + 35, color.b / 3 + 35, 190}
			}
			draw_block_px(
				ox + f32(c - min_c) * block_size,
				oy + f32(r - min_r) * block_size,
				block_size,
				color,
			)
		}
	}
}

draw_panel_card :: proc(rec: rl.Rectangle) -> rl.Rectangle {
	rl.DrawRectangleRec(rec, BORDER_COLOR)
	inner := rl.Rectangle{
		rec.x + BORDER_SIZE,
		rec.y + BORDER_SIZE,
		rec.width - 2 * BORDER_SIZE,
		rec.height - 2 * BORDER_SIZE,
	}
	rl.DrawRectangleRec(inner, PANEL_BG)
	return inner
}

draw_side_panel :: proc(g: ^Game) {
	panel_x := f32(OUTER_MARGIN + BORDER_SIZE + BOARD_PX_W + BORDER_SIZE + PANEL_GAP)
	panel_w := f32(PANEL_WIDTH)

	// Card 1: Stats (Level, progress bar, Score, Best, Lines)
	stats_inner := draw_panel_card(rl.Rectangle{panel_x, OUTER_MARGIN, panel_w, 174})
	tx := i32(stats_inner.x + 12)
	ty := i32(stats_inner.y + 10)

	rl.DrawText(fmt.ctprintf("Level: %d", g.level), tx, ty, 22, rl.WHITE)

	// 10-line level progress bar
	bar := rl.Rectangle{f32(tx), f32(ty + 28), 16, 10}
	progress := g.total_lines % 10
	for i in 0 ..< 10 {
		bar_color := rl.Color{50, 215, 75, 255} if i < progress else rl.DARKGRAY
		rl.DrawRectangleRounded(bar, 0.3, 4, bar_color)
		bar.x += bar.width + 4
	}

	ty += 48
	rl.DrawText(fmt.ctprintf("Score: %d", g.game_score), tx, ty, 22, rl.WHITE)
	ty += 30
	rl.DrawText(fmt.ctprintf("Best:  %d", g.high_score), tx, ty, 18, rl.Color{245, 210, 80, 255})
	ty += 26
	rl.DrawText(fmt.ctprintf("Lines: %d", g.total_lines), tx, ty, 18, TEXT_MUTED)

	// Card 2: Hold & Next Queue
	pieces_y := f32(OUTER_MARGIN + 174 + 12)
	pieces_inner := draw_panel_card(rl.Rectangle{panel_x, pieces_y, panel_w, 266})

	half_w := pieces_inner.width * 0.5
	// Vertical divider between Hold and Next
	div_x := pieces_inner.x + half_w
	rl.DrawLineEx(
		rl.Vector2{div_x, pieces_inner.y + 8},
		rl.Vector2{div_x, pieces_inner.y + pieces_inner.height - 8},
		1.0,
		GRID_COLOR,
	)

	// Hold column (left)
	hold_label: cstring = "HOLD"
	hl_w := rl.MeasureText(hold_label, 18)
	rl.DrawText(
		hold_label,
		i32(pieces_inner.x + (half_w - f32(hl_w)) * 0.5),
		i32(pieces_inner.y + 10),
		18,
		TEXT_MUTED,
	)

	hold_box := rl.Rectangle{pieces_inner.x + 8, pieces_inner.y + 38, half_w - 16, 80}
	rl.DrawRectangleLinesEx(hold_box, 1.0, GRID_COLOR)
	if g.hold_tetro.id != 0 {
		draw_piece_preview(g.hold_tetro, hold_box, 20, g.hold_used)
	}

	// Combo / B2B badge underneath Hold box
	badge_y := i32(hold_box.y + hold_box.height + 16)
	if g.back_to_back {
		b2b_text: cstring = "B2B"
		bw := rl.MeasureText(b2b_text, 16)
		rl.DrawText(
			b2b_text,
			i32(pieces_inner.x + (half_w - f32(bw)) * 0.5),
			badge_y,
			16,
			rl.Color{255, 190, 50, 255},
		)
		badge_y += 22
	}
	if g.combo > 0 {
		combo_text := fmt.ctprintf("COMBO x%d", g.combo)
		cw := rl.MeasureText(combo_text, 15)
		rl.DrawText(
			combo_text,
			i32(pieces_inner.x + (half_w - f32(cw)) * 0.5),
			badge_y,
			15,
			rl.Color{80, 220, 255, 255},
		)
	}

	// Next column (right) — shows next 3 pieces
	next_label: cstring = "NEXT"
	nl_w := rl.MeasureText(next_label, 18)
	rl.DrawText(
		next_label,
		i32(div_x + (half_w - f32(nl_w)) * 0.5),
		i32(pieces_inner.y + 10),
		18,
		TEXT_MUTED,
	)

	slot_y := pieces_inner.y + 38
	for i in 0 ..< NEXT_QUEUE_SIZE {
		slot_h: f32 = 72 if i == 0 else 62
		block_sz: f32 = 20 if i == 0 else 17
		slot_box := rl.Rectangle{div_x + 8, slot_y, half_w - 16, slot_h}
		if i == 0 {
			rl.DrawRectangleLinesEx(slot_box, 1.0, GRID_COLOR)
		}
		draw_piece_preview(g.next_queue[i], slot_box, block_sz)
		slot_y += slot_h + 6
	}

	// Card 3: Controls legend
	ctrl_y := pieces_y + 266 + 12
	ctrl_inner := draw_panel_card(rl.Rectangle{panel_x, ctrl_y, panel_w, 152})
	cx := i32(ctrl_inner.x + 12)
	cy := i32(ctrl_inner.y + 6)
	Control_Entry :: struct {
		keys:   cstring,
		action: cstring,
	}
	controls := [7]Control_Entry{
		{"Left / Right", "Move"},
		{"Down", "Soft Drop"},
		{"Space", "Hard Drop"},
		{"Up / X / Z", "Rotate"},
		{"C / Shift", "Hold"},
		{"F / F11", "Fullscreen"},
		{"P", "Pause"},
	}
	for entry in controls {
		rl.DrawText(entry.keys, cx, cy, 14, rl.WHITE)
		rl.DrawText(entry.action, cx + 110, cy, 14, TEXT_MUTED)
		cy += 18
	}
}

draw_banner :: proc(g: ^Game) {
	if g.banner_timer <= 0 || g.banner_len == 0 {
		return
	}

	alpha := u8(clamp((g.banner_timer / ACTION_BANNER_DURATION) * 255.0, 0.0, 255.0))
	title := cstring(&g.banner_buf[0])
	font_size: i32 = 24
	tw := rl.MeasureText(title, font_size)
	x := GAME_WIDTH * BLOCK_SIZE / 2 - tw / 2
	y: i32 = 18

	rl.DrawText(title, x + 2, y + 2, font_size, rl.Color{0, 0, 0, alpha})
	rl.DrawText(title, x, y, font_size, rl.Color{255, 240, 120, alpha})

	if g.banner_sub_len > 0 {
		sub := cstring(&g.banner_sub_buf[0])
		sub_font: i32 = 16
		sw := rl.MeasureText(sub, sub_font)
		sx := GAME_WIDTH * BLOCK_SIZE / 2 - sw / 2
		sy := y + font_size + 4
		rl.DrawText(sub, sx + 1, sy + 1, sub_font, rl.Color{0, 0, 0, alpha})
		rl.DrawText(sub, sx, sy, sub_font, rl.Color{130, 230, 255, alpha})
	}
}

draw_status_overlay :: proc(g: ^Game) {
	msg: cstring = ""
	if g.state == .Paused {
		msg = "PAUSED"
	} else if g.debug && g.state == .Playing {
		msg = "DEBUG (D)"
	}

	if len(msg) == 0 {
		return
	}

	board_rec := rl.Rectangle{
		BORDER_LEFT,
		BORDER_TOP,
		BOARD_PX_W,
		BOARD_PX_H,
	}

	if g.state == .Paused {
		rl.DrawRectangleRec(board_rec, rl.Color{0, 0, 0, 180})
	}

	font_size: i32 = 34
	w := rl.MeasureText(msg, font_size)
	cx := i32(BORDER_LEFT + BOARD_PX_W / 2)
	cy := i32(BORDER_TOP + BOARD_PX_H / 2 - font_size / 2)
	rl.DrawText(msg, cx - w / 2, cy, font_size, rl.WHITE)

	if g.state == .Paused {
		sub: cstring = "P: Resume   T: Title"
		sw := rl.MeasureText(sub, 18)
		rl.DrawText(sub, cx - sw / 2, cy + 44, 18, TEXT_MUTED)
	}
}

draw_game_over_overlay :: proc(g: ^Game) {
	if g.state == .Game_Over {
		draw_curtain(g.state_timer / GATE_DURATION)

		if g.state_timer <= 0 {
			box := rl.Rectangle{12, 150, GAME_WIDTH * f32(BLOCK_SIZE) - 24, 216}
			rl.DrawRectangleRounded(box, 0.08, 8, rl.Color{0, 0, 0, 220})
			rl.DrawRectangleRoundedLinesEx(box, 0.08, 8, 2.0, BORDER_COLOR)
			draw_game_over_text(g, i32(box.y))
		}
	} else if g.state == .Countdown {
		pct := 1.0 - (g.state_timer - 1.0) / (GAME_LAUNCH_TIME - 1.0)
		draw_curtain(clamp(pct, 0.0, 1.0))

		count := int(math.floor(g.state_timer))
		if count > 0 {
			box := rl.Rectangle{40, 190, GAME_WIDTH * f32(BLOCK_SIZE) - 80, 140}
			rl.DrawRectangleRounded(box, 0.1, 8, rl.Color{0, 0, 0, 200})
			draw_game_launch_text(i32(box.y), count)
		}
	}
}

draw_curtain :: proc(pct: f64) {
	clamped := clamp(pct, 0.0, 1.0)
	total := GAME_WIDTH * GAME_HEIGHT
	n := total - int(clamped * f64(total))
	if n <= 0 {
		return
	}

	full_rows := n / GAME_WIDTH
	rem_cols := n % GAME_WIDTH
	top_full_y := GAME_HEIGHT - full_rows

	for y := GAME_HEIGHT - 1; y >= top_full_y; y -= 1 {
		for x in 0 ..< GAME_WIDTH {
			draw_curtain_segment(x, y)
		}
	}

	partial_y := top_full_y - 1
	if rem_cols > 0 && partial_y >= 0 {
		for x in 0 ..< rem_cols {
			draw_curtain_segment(x, partial_y)
		}
	}
}

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

draw_game_launch_text :: proc(y: i32, n: int) {
	font: i32 = 96
	text: cstring
	if n >= 3 {
		text = "3..."
	} else if n == 2 {
		text = "2.."
	} else {
		text = "1."
	}

	w := rl.MeasureText(text, font)
	rl.DrawText(text, GAME_WIDTH * BLOCK_SIZE / 2 - w / 2, y + 22, font, rl.WHITE)
}

draw_game_over_text :: proc(g: ^Game, y: i32) {
	center_x := i32(GAME_WIDTH * BLOCK_SIZE / 2)

	title: cstring = "GAME OVER"
	tw := rl.MeasureText(title, 34)
	rl.DrawText(title, center_x - tw / 2, y + 22, 34, rl.Color{235, 70, 75, 255})

	score_text := fmt.ctprintf("Score: %d", g.game_score)
	sw := rl.MeasureText(score_text, 22)
	rl.DrawText(score_text, center_x - sw / 2, y + 72, 22, rl.WHITE)

	prompt1: cstring = "<SPACE> Restart"
	p1w := rl.MeasureText(prompt1, 20)
	rl.DrawText(prompt1, center_x - p1w / 2, y + 116, 20, rl.WHITE)

	prompt2: cstring = "<T> Title Screen"
	p2w := rl.MeasureText(prompt2, 20)
	rl.DrawText(prompt2, center_x - p2w / 2, y + 152, 20, TEXT_MUTED)
}
