#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const Color BACKGROUND   = {43, 60, 80, 255};
const Color BORDER_COLOR = {36, 36, 48, 255};
const Color PANEL_BG     = {16, 18, 26, 255};
const Color GRID_COLOR   = {33, 36, 46, 255};
const Color TEXT_MUTED   = {170, 180, 195, 255};

// Classic Nintendo Tetris Type-B starting garbage heights (rows for Height 0..5)
const int HEIGHT_ROWS[MAX_START_HEIGHT + 1] = {0, 3, 5, 8, 10, 12};

static int clamp_int(int v, int lo, int hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static float clamp_f32(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static double clamp_f64(double v, double lo, double hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

void parse_cli_args(int argc, const char *const *argv, Launch_Options *opts, bool *show_help, const char **err) {
	opts->fullscreen = true;
	*show_help = false;
	*err = NULL;

	for (int i = 0; i < argc; i++) {
		const char *arg = argv[i];
		if (strcmp(arg, "-w") == 0 || strcmp(arg, "--windowed") == 0 || strcmp(arg, "--no-fullscreen") == 0) {
			opts->fullscreen = false;
		} else if (strcmp(arg, "-f") == 0 || strcmp(arg, "--fullscreen") == 0) {
			opts->fullscreen = true;
		} else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			*show_help = true;
			return;
		} else {
			*err = arg;
			return;
		}
	}
}

void print_usage(const char *prog) {
	printf("Usage: %s [options]\n\n", prog);
	printf("Options:\n");
	printf("  -w, --windowed, --no-fullscreen  Start in windowed mode (default: fullscreen)\n");
	printf("  -f, --fullscreen                 Start in fullscreen mode (default)\n");
	printf("  -h, --help                       Show this help message\n");
}

void compute_windowed_size(int monitor_w, int monitor_h, int *out_w, int *out_h) {
	if (monitor_w <= 0 || monitor_h <= 0) {
		*out_w = BASE_WIDTH;
		*out_h = BASE_HEIGHT;
		return;
	}
	float scale = clamp_f32(((float)monitor_h * 0.82f) / (float)BASE_HEIGHT, 1.0f, 1.5f);
	float max_scale_w = ((float)monitor_w * 0.85f) / (float)BASE_WIDTH;
	float s = (scale < max_scale_w) ? scale : max_scale_w;
	if (s < 1.0f) {
		s = 1.0f;
	}
	*out_w = (int)roundf((float)BASE_WIDTH * s);
	*out_h = (int)roundf((float)BASE_HEIGHT * s);
}

void center_windowed_frame(void) {
	int monitor = GetCurrentMonitor();
	int mw = GetMonitorWidth(monitor);
	int mh = GetMonitorHeight(monitor);
	int w = 0, h = 0;
	compute_windowed_size(mw, mh, &w, &h);
	SetWindowSize(w, h);
	if (mw > w && mh > h) {
		SetWindowPosition((mw - w) / 2, (mh - h) / 2);
	}
}

void toggle_fullscreen(Game *g) {
	ToggleBorderlessWindowed();
	g->fullscreen = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) || IsWindowFullscreen();
	if (!g->fullscreen) {
		center_windowed_frame();
	}
}

void compute_viewport(float screen_w, float screen_h, float *out_scale, float *out_offset_x, float *out_offset_y) {
	if (screen_w <= 0.0f || screen_h <= 0.0f) {
		*out_scale = 1.0f;
		*out_offset_x = 0.0f;
		*out_offset_y = 0.0f;
		return;
	}
	float sx = screen_w / (float)BASE_WIDTH;
	float sy = screen_h / (float)BASE_HEIGHT;
	if (sx <= sy) {
		*out_scale = sx;
		*out_offset_x = 0.0f;
		*out_offset_y = (screen_h - (float)BASE_HEIGHT * sx) * 0.5f;
	} else {
		*out_scale = sy;
		*out_offset_x = (screen_w - (float)BASE_WIDTH * sy) * 0.5f;
		*out_offset_y = 0.0f;
	}
}

Vector2 screen_to_ui(Vector2 screen_pos, float screen_w, float screen_h) {
	float scale = 1.0f, offset_x = 0.0f, offset_y = 0.0f;
	compute_viewport(screen_w, screen_h, &scale, &offset_x, &offset_y);
	if (scale <= 0.0f) {
		return screen_pos;
	}
	return (Vector2){(screen_pos.x - offset_x) / scale, (screen_pos.y - offset_y) / scale};
}

bool point_in_rect(Vector2 p, Rectangle r) {
	return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

Title_Layout compute_title_layout(void) {
	Title_Layout layout = {0};
	float card_w = (float)(BASE_WIDTH - OUTER_MARGIN * 2); // 588px
	layout.card_outer = (Rectangle){(float)OUTER_MARGIN, 174.0f, card_w, 404.0f};
	layout.card_inner = (Rectangle){
		layout.card_outer.x + (float)BORDER_SIZE,
		layout.card_outer.y + (float)BORDER_SIZE,
		layout.card_outer.width - 2.0f * (float)BORDER_SIZE,
		layout.card_outer.height - 2.0f * (float)BORDER_SIZE,
	};

	float btn_w = 86.0f;
	float btn_h = 46.0f;
	float btn_gap = 14.0f;
	float speed_grid_w = 5.0f * btn_w + 4.0f * btn_gap; // 486px
	layout.grid_x = layout.card_inner.x + (layout.card_inner.width - speed_grid_w) * 0.5f;
	layout.grid_right = layout.grid_x + speed_grid_w;
	layout.div_y = layout.card_inner.y + 174.0f;

	for (int i = 0; i <= MAX_START_SPEED; i++) {
		int row = i / 5;
		int col = i % 5;
		layout.speed_rects[i] = (Rectangle){
			layout.grid_x + (float)col * (btn_w + btn_gap),
			layout.card_inner.y + 52.0f + (float)row * (btn_h + btn_gap),
			btn_w,
			btn_h,
		};
	}

	float h_btn_gap = 12.0f;
	float h_btn_w = (speed_grid_w - 5.0f * h_btn_gap) / 6.0f;
	for (int i = 0; i <= MAX_START_HEIGHT; i++) {
		layout.height_rects[i] = (Rectangle){
			layout.grid_x + (float)i * (h_btn_w + h_btn_gap),
			layout.div_y + 52.0f,
			h_btn_w,
			btn_h,
		};
	}

	layout.start_rect = (Rectangle){
		layout.grid_x,
		layout.card_inner.y + layout.card_inner.height - 64.0f,
		speed_grid_w,
		46.0f,
	};
	return layout;
}

void init_game(Game *g) {
	memset(g, 0, sizeof(*g));
	g->state = STATE_TITLE;
	g->title_focus = TITLE_FOCUS_SPEED;
	g->title_hover_speed = -1;
	g->title_hover_height = -1;
	g->title_hover_start = false;
	g->start_speed = 0;
	g->start_height = 0;
	g->level = 0;
	g->total_lines = 0;
	g->game_score = 0;
	g->combo = -1;
	g->back_to_back = false;
	clear_board(g);
	init_backdrop(&g->backdrop, false);
}

void return_to_title(Game *g) {
	update_high_score(g);
	g->state = STATE_TITLE;
	g->title_hover_speed = -1;
	g->title_hover_height = -1;
	g->title_hover_start = false;
	g->banner_timer = 0.0;
	g->horiz_dir = 0;
	g->horiz_hold_time = 0.0;
	g->horiz_repeat_accum = 0.0;
	g->soft_drop_accum = 0.0;
}

void title_adjust_value(Game *g, int delta) {
	switch (g->title_focus) {
	case TITLE_FOCUS_SPEED:
		g->start_speed = clamp_int(g->start_speed + delta, 0, MAX_START_SPEED);
		break;
	case TITLE_FOCUS_HEIGHT:
		g->start_height = clamp_int(g->start_height + delta, 0, MAX_START_HEIGHT);
		break;
	}
}

void title_set_value(Game *g, int value) {
	switch (g->title_focus) {
	case TITLE_FOCUS_SPEED:
		if (value >= 0 && value <= MAX_START_SPEED) {
			g->start_speed = value;
		}
		break;
	case TITLE_FOCUS_HEIGHT:
		if (value >= 0 && value <= MAX_START_HEIGHT) {
			g->start_height = value;
		}
		break;
	}
}

// Fills the bottom rows of the board with random garbage blocks for starting heights 1..5.
// Guarantees every generated row has both filled blocks and open gaps (never full or empty).
void fill_starting_height(Game *g, int height) {
	int h = clamp_int(height, 0, MAX_START_HEIGHT);
	int rows = HEIGHT_ROWS[h];
	if (rows <= 0) {
		return;
	}

	int start_y = BOARD_HEIGHT - rows;
	for (int y = start_y; y < BOARD_HEIGHT; y++) {
		int filled = 0;
		for (int x = 0; x < GAME_WIDTH; x++) {
			if ((rand() % 100) < 58) {
				g->board[y][x] = (int32_t)((rand() % 7) + 1);
				filled++;
			}
		}

		// Ensure at least 2 empty gaps so no row starts completely full
		while (filled > GAME_WIDTH - 2) {
			int x = rand() % GAME_WIDTH;
			if (g->board[y][x] != 0) {
				g->board[y][x] = 0;
				filled--;
			}
		}

		// Ensure at least 3 filled blocks so no row starts empty
		while (filled < 3) {
			int x = rand() % GAME_WIDTH;
			if (g->board[y][x] == 0) {
				g->board[y][x] = (int32_t)((rand() % 7) + 1);
				filled++;
			}
		}
	}
}

void start_new_game(Game *g) {
	g->state = STATE_PLAYING;
	g->start_speed = clamp_int(g->start_speed, 0, MAX_START_SPEED);
	g->start_height = clamp_int(g->start_height, 0, MAX_START_HEIGHT);
	g->level = g->start_speed;
	g->total_lines = 0;
	g->game_score = 0;
	g->combo = -1;
	g->back_to_back = false;
	clear_board(g);
	fill_starting_height(g, g->start_height);
	init_backdrop(&g->backdrop, true);

	memset(&g->bag, 0, sizeof(g->bag));
	bag_refill(&g->bag);

	memset(&g->hold_tetro, 0, sizeof(g->hold_tetro));
	g->hold_used = false;

	g->clear_row_count = 0;
	g->clear_timer = 0.0;
	g->pending_tspin = TSPIN_NONE;
	g->state_timer = 0.0;
	g->banner_len = 0;
	g->banner_sub_len = 0;
	g->banner_timer = 0.0;
	g->horiz_dir = 0;
	g->horiz_hold_time = 0.0;
	g->horiz_repeat_accum = 0.0;
	g->soft_drop_accum = 0.0;

	for (int i = 0; i < NEXT_QUEUE_SIZE; i++) {
		set_tetromino(&g->next_queue[i], bag_next(&g->bag));
	}

	spawn_next_tetro(g);
}

void reset_piece_timers(Game *g) {
	g->gravity_accum = 0.0;
	g->lock_timer = 0.0;
	g->lock_resets = 0;
	g->lowest_y = g->tetro.y;
	g->on_ground = false;
	g->last_action_rotate = false;
	g->last_kick_index = 0;
}

void spawn_next_tetro(Game *g) {
	g->tetro = g->next_queue[0];
	set_tetromino_start(&g->tetro);

	for (int i = 0; i < NEXT_QUEUE_SIZE - 1; i++) {
		g->next_queue[i] = g->next_queue[i + 1];
	}
	set_tetromino(&g->next_queue[NEXT_QUEUE_SIZE - 1], bag_next(&g->bag));

	g->hold_used = false;
	reset_piece_timers(g);
	update_ghost(g);

	// Block-Out: newly spawned piece immediately collides with existing blocks
	if (collide(g, g->tetro)) {
		set_game_over(g);
	}
}

void set_game_over(Game *g) {
	update_high_score(g);
	g->state = STATE_GAME_OVER;
	g->state_timer = GATE_DURATION;
	g->horiz_dir = 0;
	g->horiz_hold_time = 0.0;
	g->horiz_repeat_accum = 0.0;
	g->soft_drop_accum = 0.0;
}

void begin_restart_countdown(Game *g) {
	clear_board(g);
	g->state = STATE_COUNTDOWN;
	g->state_timer = GAME_LAUNCH_TIME;
}

void update_high_score(Game *g) {
	if (g->game_score > g->high_score) {
		g->high_score = g->game_score;
	}
}

void set_banner(Game *g, const char *title, const char *sub) {
	if (!title) {
		title = "";
	}
	if (!sub) {
		sub = "";
	}

	int n = snprintf(g->banner_buf, sizeof(g->banner_buf), "%s", title);
	if (n < 0) {
		n = 0;
	} else if ((size_t)n >= sizeof(g->banner_buf)) {
		n = (int)sizeof(g->banner_buf) - 1;
	}
	g->banner_len = n;

	int m = snprintf(g->banner_sub_buf, sizeof(g->banner_sub_buf), "%s", sub);
	if (m < 0) {
		m = 0;
	} else if ((size_t)m >= sizeof(g->banner_sub_buf)) {
		m = (int)sizeof(g->banner_sub_buf) - 1;
	}
	g->banner_sub_len = m;

	g->banner_timer = ACTION_BANNER_DURATION;
}

// Recomputes the ghost piece position directly from the active tetromino.
void update_ghost(Game *g) {
	g->ghost = g->tetro;
	if (g->ghost.id == 0 || collide(g, g->ghost)) {
		return;
	}

	while (g->ghost.y < BOARD_HEIGHT) {
		g->ghost.y++;
		if (collide(g, g->ghost)) {
			g->ghost.y--;
			break;
		}
	}
}

// Checks whether the active tetromino is currently resting on the floor or another block.
bool is_grounded(const Game *g) {
	Tetromino test = g->tetro;
	test.y++;
	return collide(g, test);
}

// Called after a successful horizontal move or rotation to apply Guideline lock-delay move reset.
void on_piece_manipulated(Game *g) {
	if (g->on_ground && g->lock_resets < MAX_LOCK_RESETS) {
		g->lock_timer = 0.0;
		g->lock_resets++;
	}
}

// Attempts to move the active tetromino by dir. Updates ghost and lock reset state.
bool move_tetro(Game *g, Vec2i dir) {
	g->tetro.x += dir.x;
	g->tetro.y += dir.y;

	if (collide(g, g->tetro)) {
		g->tetro.x -= dir.x;
		g->tetro.y -= dir.y;
		return false;
	}

	g->last_action_rotate = false;
	if (dir.x != 0) {
		on_piece_manipulated(g);
	} else if (dir.y > 0) {
		g->lock_timer = 0.0;
		if (g->tetro.y > g->lowest_y) {
			g->lowest_y = g->tetro.y;
			g->lock_resets = 0;
		}
	}

	update_ghost(g);
	return true;
}

// Soft drops by 1 cell, awarding 1 point only if the piece actually moved down.
bool soft_drop_step(Game *g) {
	if (move_tetro(g, (Vec2i){0, 1})) {
		g->game_score += 1;
		g->gravity_accum = 0.0;
		update_high_score(g);
		return true;
	}
	return false;
}

// Instantly drops the active tetromino to its ghost position, awards 2 points per cell, and locks.
int hard_drop(Game *g) {
	if (g->state != STATE_PLAYING) {
		return 0;
	}

	update_ghost(g);
	int dropped = g->ghost.y - g->tetro.y;
	if (dropped > 0) {
		g->tetro.y = g->ghost.y;
		g->game_score += dropped * 2;
		g->last_action_rotate = false;
		update_high_score(g);
	}

	lock_current_piece(g);
	return dropped;
}

// Swaps the current tetromino with the hold slot (once per locked piece).
bool hold_piece(Game *g) {
	if (g->state != STATE_PLAYING || g->hold_used) {
		return false;
	}

	int current_id = g->tetro.id;
	if (g->hold_tetro.id == 0) {
		set_tetromino(&g->hold_tetro, current_id);
		spawn_next_tetro(g);
	} else {
		int swap_id = g->hold_tetro.id;
		set_tetromino(&g->hold_tetro, current_id);
		set_tetromino(&g->tetro, swap_id);
		reset_piece_timers(g);
		update_ghost(g);
		if (collide(g, g->tetro)) {
			set_game_over(g);
		}
	}

	g->hold_used = true;
	return true;
}

// Detects T-Spin (Full or Mini) using the Guideline 3-corner rule before merging the T-piece.
T_Spin_Kind detect_tspin(const Game *g) {
	if (g->tetro.id != 3 || !g->last_action_rotate) {
		return TSPIN_NONE;
	}

	int cx = g->tetro.x + 1;
	int cy = g->tetro.y + 1;

	// Corner coordinates: TL, TR, BR, BL
	Vec2i corners[4] = {
		{cx - 1, cy - 1},
		{cx + 1, cy - 1},
		{cx + 1, cy + 1},
		{cx - 1, cy + 1},
	};

	bool occupied[4] = {false, false, false, false};
	int count = 0;
	for (int i = 0; i < 4; i++) {
		Vec2i corner = corners[i];
		if (corner.x < 0 ||
		    corner.x >= GAME_WIDTH ||
		    corner.y < 0 ||
		    corner.y >= BOARD_HEIGHT ||
		    g->board[corner.y][corner.x] != 0) {
			occupied[i] = true;
			count++;
		}
	}

	if (count < 3) {
		return TSPIN_NONE;
	}

	int front_a = 0, front_b = 1;
	switch (g->tetro.direction) {
	case DIR_NORTH:
		front_a = 0;
		front_b = 1; // TL, TR
		break;
	case DIR_EAST:
		front_a = 1;
		front_b = 2; // TR, BR
		break;
	case DIR_SOUTH:
		front_a = 2;
		front_b = 3; // BR, BL
		break;
	case DIR_WEST:
		front_a = 3;
		front_b = 0; // BL, TL
		break;
	}

	if ((occupied[front_a] && occupied[front_b]) || g->last_kick_index == 4) {
		return TSPIN_FULL;
	}
	return TSPIN_MINI;
}

// Locks the active piece into the board, checks Lock-Out, detects full lines, and triggers clear or spawn.
void lock_current_piece(Game *g) {
	T_Spin_Kind tspin = detect_tspin(g);
	bool any_visible = false;

	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			int32_t cell = g->tetro.shape[r][c];
			if (cell == 0) {
				continue;
			}
			int bx = g->tetro.x + c;
			int by = g->tetro.y + r;
			if (by >= HIDDEN_ROWS) {
				any_visible = true;
			}
			if (by >= 0 && by < BOARD_HEIGHT && bx >= 0 && bx < GAME_WIDTH) {
				g->board[by][bx] = cell;
			}
		}
	}

	// Lock-Out: piece locked completely above the visible playfield
	if (!any_visible) {
		set_game_over(g);
		return;
	}

	int row_count = find_full_rows(g);
	if (row_count > 0) {
		g->clear_row_count = row_count;
		g->clear_timer = LINE_CLEAR_DURATION;
		g->pending_tspin = tspin;
		g->state = STATE_LINE_CLEAR;
		return;
	}

	// No lines cleared
	g->combo = -1;
	if (tspin != TSPIN_NONE) {
		int pts = compute_score(g->level, 0, tspin, false);
		g->game_score += pts;
		update_high_score(g);
		if (tspin == TSPIN_FULL) {
			set_banner(g, "T-SPIN!", "");
		} else {
			set_banner(g, "T-SPIN MINI", "");
		}
	}

	spawn_next_tetro(g);
}

int find_full_rows(Game *g) {
	int count = 0;
	for (int y = 0; y < BOARD_HEIGHT; y++) {
		bool filled = true;
		for (int x = 0; x < GAME_WIDTH; x++) {
			if (g->board[y][x] == 0) {
				filled = false;
				break;
			}
		}
		if (filled && count < 4) {
			g->clear_rows[count] = y;
			count++;
		}
	}
	return count;
}

void finish_line_clear(Game *g) {
	int lines = clear_lines(g);
	T_Spin_Kind tspin = g->pending_tspin;
	g->clear_row_count = 0;
	g->pending_tspin = TSPIN_NONE;

	bool difficult = (lines == 4) || (tspin != TSPIN_NONE && lines > 0);
	bool b2b_bonus = difficult && g->back_to_back;

	int points = compute_score(g->level, lines, tspin, b2b_bonus);

	g->combo++;
	if (g->combo > 0) {
		points += 50 * g->combo * (g->level + 1);
	}

	if (difficult) {
		g->back_to_back = true;
	} else if (lines > 0) {
		g->back_to_back = false;
	}

	g->game_score += points;
	update_high_score(g);

	g->total_lines += lines;
	g->level = g->start_speed + g->total_lines / 10;

	const char *title = "";
	switch (tspin) {
	case TSPIN_FULL:
		switch (lines) {
		case 1: title = "T-SPIN SINGLE!"; break;
		case 2: title = "T-SPIN DOUBLE!"; break;
		case 3: title = "T-SPIN TRIPLE!"; break;
		default: break;
		}
		break;
	case TSPIN_MINI:
		switch (lines) {
		case 1: title = "T-SPIN MINI!"; break;
		case 2: title = "T-SPIN MINI x2!"; break;
		default: break;
		}
		break;
	case TSPIN_NONE:
		switch (lines) {
		case 1: title = "SINGLE"; break;
		case 2: title = "DOUBLE"; break;
		case 3: title = "TRIPLE!"; break;
		case 4: title = "TETRIS!"; break;
		default: break;
		}
		break;
	}

	char sub_buf[64] = {0};
	const char *sub = "";
	if (b2b_bonus && g->combo > 0) {
		snprintf(sub_buf, sizeof(sub_buf), "B2B  COMBO x%d", g->combo);
		sub = sub_buf;
	} else if (b2b_bonus) {
		sub = "BACK-TO-BACK!";
	} else if (g->combo > 0) {
		snprintf(sub_buf, sizeof(sub_buf), "COMBO x%d", g->combo);
		sub = sub_buf;
	}

	if (title[0] != '\0') {
		set_banner(g, title, sub);
	}

	g->state = STATE_PLAYING;
	spawn_next_tetro(g);
}

int compute_score(int level, int lines, T_Spin_Kind tspin, bool b2b) {
	int base = 0;
	switch (tspin) {
	case TSPIN_FULL:
		switch (lines) {
		case 0: base = 400; break;
		case 1: base = 800; break;
		case 2: base = 1200; break;
		case 3: base = 1600; break;
		default: break;
		}
		break;
	case TSPIN_MINI:
		switch (lines) {
		case 0: base = 100; break;
		case 1: base = 200; break;
		case 2: base = 400; break;
		default: break;
		}
		break;
	case TSPIN_NONE:
		switch (lines) {
		case 1: base = 100; break;
		case 2: base = 300; break;
		case 3: base = 500; break;
		case 4: base = 800; break;
		default: break;
		}
		break;
	}

	int score = base * (level + 1);
	if (b2b) {
		score = (score * 3) / 2;
	}
	return score;
}

bool update_title_pointer(Game *g, Vector2 ui_pos, bool clicked) {
	Title_Layout layout = compute_title_layout();
	g->title_hover_speed = -1;
	g->title_hover_height = -1;
	g->title_hover_start = false;

	for (int i = 0; i <= MAX_START_SPEED; i++) {
		if (point_in_rect(ui_pos, layout.speed_rects[i])) {
			g->title_hover_speed = i;
			if (clicked) {
				g->start_speed = i;
				g->title_focus = TITLE_FOCUS_SPEED;
			}
			return true;
		}
	}

	for (int i = 0; i <= MAX_START_HEIGHT; i++) {
		if (point_in_rect(ui_pos, layout.height_rects[i])) {
			g->title_hover_height = i;
			if (clicked) {
				g->start_height = i;
				g->title_focus = TITLE_FOCUS_HEIGHT;
			}
			return true;
		}
	}

	if (point_in_rect(ui_pos, layout.start_rect)) {
		g->title_hover_start = true;
		if (clicked) {
			start_new_game(g);
		}
		return true;
	}

	return false;
}

static void handle_title_input(Game *g) {
	Vector2 mouse_ui = screen_to_ui(
		GetMousePosition(),
		(float)GetScreenWidth(),
		(float)GetScreenHeight()
	);
	bool hovering = update_title_pointer(g, mouse_ui, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));

	if (g->state != STATE_TITLE) {
		SetMouseCursor(MOUSE_CURSOR_DEFAULT);
		return;
	}

	if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB) ||
	    IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S)) {
		g->title_focus = (g->title_focus == TITLE_FOCUS_SPEED) ? TITLE_FOCUS_HEIGHT : TITLE_FOCUS_SPEED;
	}

	if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
		title_adjust_value(g, -1);
	}
	if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
		title_adjust_value(g, 1);
	}

	static const int num_keys[10] = {
		KEY_ZERO, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR,
		KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE,
	};
	for (int digit = 0; digit < 10; digit++) {
		if (IsKeyPressed(num_keys[digit])) {
			title_set_value(g, digit);
		}
	}

	if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE)) {
		SetMouseCursor(MOUSE_CURSOR_DEFAULT);
		start_new_game(g);
		return;
	}

	SetMouseCursor(hovering ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
}

// Handles Raylib keyboard input and DAS/ARR auto-repeat.
void handle_input(Game *g, double frame_dt) {
	if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_F11)) {
		toggle_fullscreen(g);
	}

	if (g->state == STATE_TITLE) {
		handle_title_input(g);
		return;
	}

	if (IsKeyPressed(KEY_P)) {
		if (g->state == STATE_PLAYING) {
			g->state = STATE_PAUSED;
			return;
		} else if (g->state == STATE_PAUSED) {
			g->state = STATE_PLAYING;
			return;
		}
	}

	if (g->state == STATE_PAUSED) {
		if (IsKeyPressed(KEY_T)) {
			return_to_title(g);
		}
		return;
	}

	if (g->state == STATE_GAME_OVER) {
		if (g->state_timer <= 0.0) {
			if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
				begin_restart_countdown(g);
			} else if (IsKeyPressed(KEY_T)) {
				return_to_title(g);
			}
		}
		return;
	}

	if (g->state != STATE_PLAYING) {
		return;
	}

	// Debug mode toggle (freezes gravity and enables debug shortcuts)
	if (IsKeyPressed(KEY_D)) {
		g->debug = !g->debug;
	}

	if (g->debug) {
		if (IsKeyPressed(KEY_N)) {
			set_tetromino(&g->next_queue[0], bag_next(&g->bag));
		}
		if (IsKeyPressed(KEY_G)) {
			set_game_over(g);
			return;
		}
	}

	// Horizontal movement with DAS + ARR
	bool left_down = IsKeyDown(KEY_LEFT);
	bool right_down = IsKeyDown(KEY_RIGHT);

	if (IsKeyPressed(KEY_LEFT)) {
		move_tetro(g, (Vec2i){-1, 0});
		g->horiz_dir = -1;
		g->horiz_hold_time = 0.0;
		g->horiz_repeat_accum = 0.0;
	} else if (IsKeyPressed(KEY_RIGHT)) {
		move_tetro(g, (Vec2i){1, 0});
		g->horiz_dir = 1;
		g->horiz_hold_time = 0.0;
		g->horiz_repeat_accum = 0.0;
	} else {
		if (g->horiz_dir == -1 && !left_down) {
			g->horiz_dir = right_down ? 1 : 0;
			g->horiz_hold_time = 0.0;
			g->horiz_repeat_accum = 0.0;
		} else if (g->horiz_dir == 1 && !right_down) {
			g->horiz_dir = left_down ? -1 : 0;
			g->horiz_hold_time = 0.0;
			g->horiz_repeat_accum = 0.0;
		} else if (g->horiz_dir == 0) {
			if (left_down) {
				g->horiz_dir = -1;
				g->horiz_hold_time = 0.0;
				g->horiz_repeat_accum = 0.0;
			} else if (right_down) {
				g->horiz_dir = 1;
				g->horiz_hold_time = 0.0;
				g->horiz_repeat_accum = 0.0;
			}
		}

		if (g->horiz_dir != 0) {
			g->horiz_hold_time += frame_dt;
			if (g->horiz_hold_time >= DAS_DELAY) {
				g->horiz_repeat_accum += frame_dt;
				while (g->horiz_repeat_accum >= ARR_INTERVAL) {
					g->horiz_repeat_accum -= ARR_INTERVAL;
					if (!move_tetro(g, (Vec2i){g->horiz_dir, 0})) {
						break;
					}
				}
			}
		}
	}

	// Soft drop
	if (IsKeyPressed(KEY_DOWN)) {
		soft_drop_step(g);
		g->soft_drop_accum = 0.0;
	} else if (IsKeyDown(KEY_DOWN)) {
		g->soft_drop_accum += frame_dt;
		while (g->soft_drop_accum >= SOFT_DROP_INTERVAL) {
			g->soft_drop_accum -= SOFT_DROP_INTERVAL;
			if (!soft_drop_step(g)) {
				break;
			}
		}
	} else {
		g->soft_drop_accum = 0.0;
	}

	// Rotation (CW: Up / X, CCW: Z / Left Control)
	if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X)) {
		rotate_with_kick(g, true, NULL);
	}
	if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_LEFT_CONTROL)) {
		rotate_with_kick(g, false, NULL);
	}

	// Hold piece (C / Shift)
	if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) {
		hold_piece(g);
	}

	// Hard drop (Space)
	if (!g->debug && IsKeyPressed(KEY_SPACE)) {
		hard_drop(g);
	}
}

// Advances game timers, gravity, lock delay, line clear animation, and restart countdown.
void update_game(Game *g, double frame_dt) {
	if (g->state != STATE_PAUSED) {
		update_backdrop(&g->backdrop, frame_dt);
	}

	if (g->banner_timer > 0.0) {
		g->banner_timer -= frame_dt;
		if (g->banner_timer < 0.0) {
			g->banner_timer = 0.0;
		}
	}

	switch (g->state) {
	case STATE_TITLE:
	case STATE_PAUSED:
		return;

	case STATE_GAME_OVER:
		if (g->state_timer > 0.0) {
			g->state_timer -= frame_dt;
			if (g->state_timer < 0.0) {
				g->state_timer = 0.0;
			}
		}
		return;

	case STATE_COUNTDOWN:
		g->state_timer -= frame_dt;
		if (g->state_timer <= 1.0) {
			start_new_game(g);
		}
		return;

	case STATE_LINE_CLEAR:
		g->clear_timer -= frame_dt;
		if (g->clear_timer <= 0.0) {
			finish_line_clear(g);
		}
		return;

	case STATE_PLAYING:
		if (g->debug) {
			return;
		}

		// Gravity
		{
			double tick_interval = level_time(g->level);
			g->gravity_accum += frame_dt;
			while (g->gravity_accum >= tick_interval) {
				g->gravity_accum -= tick_interval;
				if (!move_tetro(g, (Vec2i){0, 1})) {
					g->gravity_accum = 0.0;
					break;
				}
			}
		}

		// Lock delay when resting on the floor or stack
		if (is_grounded(g)) {
			g->on_ground = true;
			g->lock_timer += frame_dt;
			if (g->lock_timer >= LOCK_DELAY) {
				lock_current_piece(g);
			}
		} else {
			g->on_ground = false;
			g->lock_timer = 0.0;
		}
		break;
	}
}

// Checks if a tetromino collides with playfield boundaries or placed blocks.
bool collide(const Game *g, Tetromino t) {
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			if (t.shape[r][c] == 0) {
				continue;
			}

			int bx = t.x + c;
			int by = t.y + r;

			if (bx < 0 || bx >= GAME_WIDTH || by < 0 || by >= BOARD_HEIGHT) {
				return true;
			}

			if (g->board[by][bx] != 0) {
				return true;
			}
		}
	}
	return false;
}

void clear_board(Game *g) {
	memset(g->board, 0, sizeof(g->board));
}

// Removes all completely filled rows and shifts rows above them down.
int clear_lines(Game *g) {
	int cleared = 0;

	for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
		bool row_filled = true;
		for (int x = 0; x < GAME_WIDTH; x++) {
			if (g->board[y][x] == 0) {
				row_filled = false;
				break;
			}
		}

		if (row_filled) {
			for (int move_y = y; move_y > 0; move_y--) {
				for (int x = 0; x < GAME_WIDTH; x++) {
					g->board[move_y][x] = g->board[move_y - 1][x];
				}
			}
			for (int x = 0; x < GAME_WIDTH; x++) {
				g->board[0][x] = 0;
			}

			y++;
			cleared++;
		}
	}

	return cleared;
}

// Guideline gravity curve: seconds per row drop (level 0 = 1.0s, level 9 = ~0.064s).
double level_time(int level) {
	double lv = (double)clamp_int(level, 0, MAX_GRAVITY_LEVEL);
	double t = pow(0.8 - (lv * 0.007), lv);
	return (t > 0.016) ? t : 0.016;
}

// =============================================================================
// Rendering
// =============================================================================

static void draw_block_px(float px, float py, float size, Color color) {
	Rectangle rec = {px + 0.5f, py + 0.5f, size - 1.5f, size - 1.5f};
	DrawRectangleRounded(rec, 0.35f, 16, color);

	// Subtle top-left highlight for depth
	Color highlight = {
		(uint8_t)clamp_int((int)color.r + 55, 0, 255),
		(uint8_t)clamp_int((int)color.g + 55, 0, 255),
		(uint8_t)clamp_int((int)color.b + 55, 0, 255),
		120,
	};
	float inset = size * 0.14f;
	if (inset < 2.0f) {
		inset = 2.0f;
	}
	Rectangle inner = {px + inset, py + inset, size - inset * 2.0f - 1.0f, size * 0.25f};
	DrawRectangleRounded(inner, 0.5f, 8, highlight);
}

static Rectangle draw_panel_card(Rectangle rec) {
	DrawRectangleRec(rec, BORDER_COLOR);
	Rectangle inner = {
		rec.x + (float)BORDER_SIZE,
		rec.y + (float)BORDER_SIZE,
		rec.width - 2.0f * (float)BORDER_SIZE,
		rec.height - 2.0f * (float)BORDER_SIZE,
	};
	DrawRectangleRec(inner, PANEL_BG);
	return inner;
}

static void draw_title_option_tile(
	Rectangle rec,
	const char *label,
	bool selected,
	bool row_active,
	bool hovered
) {
	Color bg = {28, 32, 46, 255};
	Color border = GRID_COLOR;
	Color text_color = TEXT_MUTED;

	if (selected) {
		if (row_active || hovered) {
			bg = (Color){0, 175, 210, 255};
			border = WHITE;
			text_color = (Color){12, 16, 24, 255};
		} else {
			bg = (Color){52, 66, 92, 255};
			border = (Color){140, 165, 200, 255};
			text_color = WHITE;
		}
	} else if (hovered) {
		bg = (Color){46, 54, 76, 255};
		border = (Color){150, 195, 225, 255};
		text_color = WHITE;
	}

	DrawRectangleRounded(rec, 0.25f, 8, bg);
	DrawRectangleRoundedLinesEx(rec, 0.25f, 8, (selected || hovered) ? 2.0f : 1.0f, border);

	int font_size = 22;
	int lw = MeasureText(label, font_size);
	DrawText(
		label,
		(int)(rec.x + (rec.width - (float)lw) * 0.5f),
		(int)(rec.y + (rec.height - (float)font_size) * 0.5f),
		font_size,
		text_color
	);
}

static void draw_title_screen(const Game *g) {
	int center_x = BASE_WIDTH / 2;
	Title_Layout layout = compute_title_layout();

	// Stylized title logo with shadow
	const char *title = "TEXAS";
	int title_font = 80;
	int tw = MeasureText(title, title_font);
	DrawText(title, center_x - tw / 2 + 4, 24 + 4, title_font, (Color){16, 20, 30, 220});
	DrawText(title, center_x - tw / 2, 24, title_font, WHITE);

	// Decorative bar of the 7 tetromino colors
	float swatch_size = 24.0f;
	float swatch_gap = 8.0f;
	float swatch_total = 7.0f * swatch_size + 6.0f * swatch_gap;
	float swatch_x = ((float)BASE_WIDTH - swatch_total) * 0.5f;
	for (int id = 1; id <= 7; id++) {
		draw_block_px(swatch_x, 110.0f, swatch_size, TETRO_COLOR[id]);
		swatch_x += swatch_size + swatch_gap;
	}

	const char *sub = "SELECT STARTING SPEED & HEIGHT";
	int sub_w = MeasureText(sub, 16);
	DrawText(sub, center_x - sub_w / 2, 146, 16, TEXT_MUTED);

	// Main setup card (centered symmetrically across BASE_WIDTH)
	Rectangle card = draw_panel_card(layout.card_outer);
	float grid_x = layout.grid_x;
	float grid_right = layout.grid_right;

	// --- SPEED (0..9) ---
	bool speed_active = (g->title_focus == TITLE_FOCUS_SPEED);
	Color speed_accent = speed_active ? (Color){0, 210, 240, 255} : TEXT_MUTED;

	if (speed_active) {
		DrawText(">", (int)(grid_x - 22.0f), (int)(card.y + 18.0f), 22, speed_accent);
	}
	DrawText("SPEED", (int)grid_x, (int)(card.y + 18.0f), 22, speed_active ? WHITE : TEXT_MUTED);

	char speed_info[64];
	snprintf(speed_info, sizeof(speed_info), "Level %d  (%.2fs/row)", g->start_speed, level_time(g->start_speed));
	int si_w = MeasureText(speed_info, 16);
	DrawText(speed_info, (int)grid_right - si_w, (int)(card.y + 22.0f), 16, speed_accent);

	for (int i = 0; i <= MAX_START_SPEED; i++) {
		char num[8];
		snprintf(num, sizeof(num), "%d", i);
		draw_title_option_tile(
			layout.speed_rects[i],
			num,
			i == g->start_speed,
			speed_active,
			i == g->title_hover_speed
		);
	}

	// Divider
	float div_y = layout.div_y;
	DrawLineEx((Vector2){grid_x, div_y}, (Vector2){grid_right, div_y}, 1.0f, GRID_COLOR);

	// --- HEIGHT (0..5) ---
	bool height_active = (g->title_focus == TITLE_FOCUS_HEIGHT);
	Color height_accent = height_active ? (Color){245, 210, 80, 255} : TEXT_MUTED;

	if (height_active) {
		DrawText(">", (int)(grid_x - 22.0f), (int)(div_y + 16.0f), 22, height_accent);
	}
	DrawText("HEIGHT", (int)grid_x, (int)(div_y + 16.0f), 22, height_active ? WHITE : TEXT_MUTED);

	int height_rows = HEIGHT_ROWS[g->start_height];
	char height_info[64];
	if (height_rows == 0) {
		snprintf(height_info, sizeof(height_info), "Clear Board");
	} else {
		snprintf(height_info, sizeof(height_info), "%d Garbage Rows", height_rows);
	}
	int hi_w = MeasureText(height_info, 16);
	DrawText(height_info, (int)grid_right - hi_w, (int)(div_y + 20.0f), 16, height_accent);

	for (int i = 0; i <= MAX_START_HEIGHT; i++) {
		char num[8];
		snprintf(num, sizeof(num), "%d", i);
		draw_title_option_tile(
			layout.height_rects[i],
			num,
			i == g->start_height,
			height_active,
			i == g->title_hover_height
		);
	}

	// Start button
	Rectangle start_box = layout.start_rect;
	Color start_bg = g->title_hover_start ? (Color){50, 215, 75, 110} : (Color){50, 215, 75, 50};
	Color start_border = g->title_hover_start ? WHITE : (Color){50, 215, 75, 220};
	DrawRectangleRounded(start_box, 0.25f, 8, start_bg);
	DrawRectangleRoundedLinesEx(start_box, 0.25f, 8, 2.0f, start_border);
	const char *start_label = "START GAME  (ENTER / SPACE)";
	int sl_w = MeasureText(start_label, 18);
	DrawText(
		start_label,
		(int)(start_box.x + (start_box.width - (float)sl_w) * 0.5f),
		(int)(start_box.y + 14.0f),
		18,
		WHITE
	);

	// Session High Score (when non-zero)
	if (g->high_score > 0) {
		char hs[64];
		snprintf(hs, sizeof(hs), "HIGH SCORE: %d", g->high_score);
		int hs_w = MeasureText(hs, 18);
		DrawText(hs, center_x - hs_w / 2, 588, 18, (Color){245, 210, 80, 255});
	}

	// Footer hints
	const char *hint1 = "Click or Arrows / 0-9: Choose Speed & Height";
	int h1_w = MeasureText(hint1, 14);
	DrawText(hint1, center_x - h1_w / 2, 616, 14, TEXT_MUTED);

	const char *hint2 = "Click Start or Enter / Space    F: Fullscreen    Esc: Quit";
	int h2_w = MeasureText(hint2, 14);
	DrawText(hint2, center_x - h2_w / 2, 638, 14, TEXT_MUTED);
}

static void draw_board(const Game *g) {
	float size = (float)BLOCK_SIZE;
	for (int vy = 0; vy < GAME_HEIGHT; vy++) {
		int by = vy + HIDDEN_ROWS;
		for (int x = 0; x < GAME_WIDTH; x++) {
			Rectangle rec = {(float)x * size, (float)vy * size, size, size};
			int32_t cell = g->board[by][x];
			if (cell == 0) {
				DrawRectangleRec(rec, PANEL_BG);
				DrawRectangleLinesEx(rec, 0.5f, GRID_COLOR);
			} else {
				DrawRectangleRec(rec, PANEL_BG);
				draw_block_px((float)x * size, (float)vy * size, size, TETRO_COLOR[cell]);
			}
		}
	}
}

static void draw_line_clear_flash(const Game *g) {
	double progress = 1.0 - (g->clear_timer / LINE_CLEAR_DURATION);
	uint8_t alpha = (uint8_t)clamp_f64((1.0 - progress) * 230.0, 40.0, 230.0);
	float size = (float)BLOCK_SIZE;

	for (int i = 0; i < g->clear_row_count; i++) {
		int vy = g->clear_rows[i] - HIDDEN_ROWS;
		if (vy < 0 || vy >= GAME_HEIGHT) {
			continue;
		}
		Rectangle rec = {0.0f, (float)vy * size, (float)GAME_WIDTH * size, size};
		DrawRectangleRec(rec, (Color){255, 255, 255, alpha});
	}
}

static void draw_tetromino(Tetromino t) {
	float size = (float)BLOCK_SIZE;
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			int32_t cell = t.shape[r][c];
			if (cell == 0) {
				continue;
			}
			int vy = t.y + r - HIDDEN_ROWS;
			if (vy < 0 || vy >= GAME_HEIGHT) {
				continue;
			}
			draw_block_px((float)(t.x + c) * size, (float)vy * size, size, TETRO_COLOR[cell]);
		}
	}
}

static void draw_ghost(Tetromino t) {
	if (t.id == 0) {
		return;
	}
	float size = (float)BLOCK_SIZE;
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			int32_t cell = t.shape[r][c];
			if (cell == 0) {
				continue;
			}
			int vy = t.y + r - HIDDEN_ROWS;
			if (vy < 0 || vy >= GAME_HEIGHT) {
				continue;
			}
			Color base = TETRO_COLOR[cell];
			Color fill = {base.r, base.g, base.b, 48};
			Color outline = {base.r, base.g, base.b, 195};
			Rectangle rec = {(float)(t.x + c) * size + 1.0f, (float)vy * size + 1.0f, size - 2.0f, size - 2.0f};
			DrawRectangleRounded(rec, 0.35f, 16, fill);
			DrawRectangleRoundedLinesEx(rec, 0.35f, 16, 2.0f, outline);
		}
	}
}

// Draws a tetromino centered within a target rectangle (used for Hold and Next previews).
static void draw_piece_preview(Tetromino t, Rectangle box, float block_size, bool dimmed) {
	if (t.id == 0) {
		return;
	}

	int min_c = 4, max_c = -1;
	int min_r = 4, max_r = -1;
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			if (t.shape[r][c] != 0) {
				if (c < min_c) min_c = c;
				if (c > max_c) max_c = c;
				if (r < min_r) min_r = r;
				if (r > max_r) max_r = r;
			}
		}
	}
	if (max_c < min_c) {
		return;
	}

	float pw = (float)(max_c - min_c + 1) * block_size;
	float ph = (float)(max_r - min_r + 1) * block_size;
	float ox = box.x + (box.width - pw) * 0.5f;
	float oy = box.y + (box.height - ph) * 0.5f;

	for (int r = min_r; r <= max_r; r++) {
		for (int c = min_c; c <= max_c; c++) {
			int32_t cell = t.shape[r][c];
			if (cell == 0) {
				continue;
			}
			Color color = TETRO_COLOR[cell];
			if (dimmed) {
				color = (Color){
					(uint8_t)(color.r / 3 + 35),
					(uint8_t)(color.g / 3 + 35),
					(uint8_t)(color.b / 3 + 35),
					190,
				};
			}
			draw_block_px(
				ox + (float)(c - min_c) * block_size,
				oy + (float)(r - min_r) * block_size,
				block_size,
				color
			);
		}
	}
}

static void draw_side_panel(const Game *g) {
	float panel_x = (float)(OUTER_MARGIN + BORDER_SIZE + BOARD_PX_W + BORDER_SIZE + PANEL_GAP);
	float panel_w = (float)PANEL_WIDTH;

	// Card 1: Stats (Level, progress bar, Score, Best, Lines)
	Rectangle stats_inner = draw_panel_card((Rectangle){panel_x, (float)OUTER_MARGIN, panel_w, 174.0f});
	int tx = (int)(stats_inner.x + 12.0f);
	int ty = (int)(stats_inner.y + 10.0f);

	char buf[64];
	snprintf(buf, sizeof(buf), "Level: %d", g->level);
	DrawText(buf, tx, ty, 22, WHITE);

	// 10-line level progress bar
	Rectangle bar = {(float)tx, (float)(ty + 28), 16.0f, 10.0f};
	int progress = g->total_lines % 10;
	for (int i = 0; i < 10; i++) {
		Color bar_color = (i < progress) ? (Color){50, 215, 75, 255} : DARKGRAY;
		DrawRectangleRounded(bar, 0.3f, 4, bar_color);
		bar.x += bar.width + 4.0f;
	}

	ty += 48;
	snprintf(buf, sizeof(buf), "Score: %d", g->game_score);
	DrawText(buf, tx, ty, 22, WHITE);
	ty += 30;
	snprintf(buf, sizeof(buf), "Best:  %d", g->high_score);
	DrawText(buf, tx, ty, 18, (Color){245, 210, 80, 255});
	ty += 26;
	snprintf(buf, sizeof(buf), "Lines: %d", g->total_lines);
	DrawText(buf, tx, ty, 18, TEXT_MUTED);

	// Card 2: Hold & Next Queue
	float pieces_y = (float)(OUTER_MARGIN + 174 + 12);
	Rectangle pieces_inner = draw_panel_card((Rectangle){panel_x, pieces_y, panel_w, 266.0f});

	float half_w = pieces_inner.width * 0.5f;
	float div_x = pieces_inner.x + half_w;
	DrawLineEx(
		(Vector2){div_x, pieces_inner.y + 8.0f},
		(Vector2){div_x, pieces_inner.y + pieces_inner.height - 8.0f},
		1.0f,
		GRID_COLOR
	);

	// Hold column (left)
	const char *hold_label = "HOLD";
	int hl_w = MeasureText(hold_label, 18);
	DrawText(
		hold_label,
		(int)(pieces_inner.x + (half_w - (float)hl_w) * 0.5f),
		(int)(pieces_inner.y + 10.0f),
		18,
		TEXT_MUTED
	);

	Rectangle hold_box = {pieces_inner.x + 8.0f, pieces_inner.y + 38.0f, half_w - 16.0f, 80.0f};
	DrawRectangleLinesEx(hold_box, 1.0f, GRID_COLOR);
	if (g->hold_tetro.id != 0) {
		draw_piece_preview(g->hold_tetro, hold_box, 20.0f, g->hold_used);
	}

	// Combo / B2B badge underneath Hold box
	int badge_y = (int)(hold_box.y + hold_box.height + 16.0f);
	if (g->back_to_back) {
		const char *b2b_text = "B2B";
		int bw = MeasureText(b2b_text, 16);
		DrawText(
			b2b_text,
			(int)(pieces_inner.x + (half_w - (float)bw) * 0.5f),
			badge_y,
			16,
			(Color){255, 190, 50, 255}
		);
		badge_y += 22;
	}
	if (g->combo > 0) {
		snprintf(buf, sizeof(buf), "COMBO x%d", g->combo);
		int cw = MeasureText(buf, 15);
		DrawText(
			buf,
			(int)(pieces_inner.x + (half_w - (float)cw) * 0.5f),
			badge_y,
			15,
			(Color){80, 220, 255, 255}
		);
	}

	// Next column (right) — shows next 3 pieces
	const char *next_label = "NEXT";
	int nl_w = MeasureText(next_label, 18);
	DrawText(
		next_label,
		(int)(div_x + (half_w - (float)nl_w) * 0.5f),
		(int)(pieces_inner.y + 10.0f),
		18,
		TEXT_MUTED
	);

	float slot_y = pieces_inner.y + 38.0f;
	for (int i = 0; i < NEXT_QUEUE_SIZE; i++) {
		float slot_h = (i == 0) ? 72.0f : 62.0f;
		float block_sz = (i == 0) ? 20.0f : 17.0f;
		Rectangle slot_box = {div_x + 8.0f, slot_y, half_w - 16.0f, slot_h};
		if (i == 0) {
			DrawRectangleLinesEx(slot_box, 1.0f, GRID_COLOR);
		}
		draw_piece_preview(g->next_queue[i], slot_box, block_sz, false);
		slot_y += slot_h + 6.0f;
	}

	// Card 3: Controls legend
	float ctrl_y = pieces_y + 266.0f + 12.0f;
	Rectangle ctrl_inner = draw_panel_card((Rectangle){panel_x, ctrl_y, panel_w, 152.0f});
	int cx = (int)(ctrl_inner.x + 12.0f);
	int cy = (int)(ctrl_inner.y + 6.0f);
	typedef struct Control_Entry {
		const char *keys;
		const char *action;
	} Control_Entry;
	static const Control_Entry controls[7] = {
		{"Left / Right", "Move"},
		{"Down", "Soft Drop"},
		{"Space", "Hard Drop"},
		{"Up / X / Z", "Rotate"},
		{"C / Shift", "Hold"},
		{"F / F11", "Fullscreen"},
		{"P", "Pause"},
	};
	for (int i = 0; i < 7; i++) {
		DrawText(controls[i].keys, cx, cy, 14, WHITE);
		DrawText(controls[i].action, cx + 110, cy, 14, TEXT_MUTED);
		cy += 18;
	}
}

static void draw_banner(const Game *g) {
	if (g->banner_timer <= 0.0 || g->banner_len == 0) {
		return;
	}

	uint8_t alpha = (uint8_t)clamp_f64((g->banner_timer / ACTION_BANNER_DURATION) * 255.0, 0.0, 255.0);
	int font_size = 24;
	int tw = MeasureText(g->banner_buf, font_size);
	int x = GAME_WIDTH * BLOCK_SIZE / 2 - tw / 2;
	int y = 18;

	DrawText(g->banner_buf, x + 2, y + 2, font_size, (Color){0, 0, 0, alpha});
	DrawText(g->banner_buf, x, y, font_size, (Color){255, 240, 120, alpha});

	if (g->banner_sub_len > 0) {
		int sub_font = 16;
		int sw = MeasureText(g->banner_sub_buf, sub_font);
		int sx = GAME_WIDTH * BLOCK_SIZE / 2 - sw / 2;
		int sy = y + font_size + 4;
		DrawText(g->banner_sub_buf, sx + 1, sy + 1, sub_font, (Color){0, 0, 0, alpha});
		DrawText(g->banner_sub_buf, sx, sy, sub_font, (Color){130, 230, 255, alpha});
	}
}

static void draw_status_overlay(const Game *g) {
	const char *msg = "";
	if (g->state == STATE_PAUSED) {
		msg = "PAUSED";
	} else if (g->debug && g->state == STATE_PLAYING) {
		msg = "DEBUG (D)";
	}

	if (msg[0] == '\0') {
		return;
	}

	Rectangle board_rec = {
		(float)BORDER_LEFT,
		(float)BORDER_TOP,
		(float)BOARD_PX_W,
		(float)BOARD_PX_H,
	};

	if (g->state == STATE_PAUSED) {
		DrawRectangleRec(board_rec, (Color){0, 0, 0, 180});
	}

	int font_size = 34;
	int w = MeasureText(msg, font_size);
	int cx = BORDER_LEFT + BOARD_PX_W / 2;
	int cy = BORDER_TOP + BOARD_PX_H / 2 - font_size / 2;
	DrawText(msg, cx - w / 2, cy, font_size, WHITE);

	if (g->state == STATE_PAUSED) {
		const char *sub = "P: Resume   T: Title";
		int sw = MeasureText(sub, 18);
		DrawText(sub, cx - sw / 2, cy + 44, 18, TEXT_MUTED);
	}
}

static void draw_curtain_segment(int x, int y) {
	float size = (float)BLOCK_SIZE;
	Rectangle rec = {(float)x * size, (float)y * size, size - 1.0f, size - 1.0f};
	DrawRectangleRec(rec, DARKGRAY);
	Color c = {224, 224, 224, 255};
	DrawRectangleLinesEx(rec, 1.0f, c);
	const uint8_t dc = 32;
	c.r -= dc;
	c.g -= dc;
	c.b -= dc;
	const int iters = 6;
	for (int i = 0; i < iters; i++) {
		rec.x += 2.0f;
		rec.y += 2.0f;
		rec.width -= 4.0f;
		rec.height -= 4.0f;
		if (i == iters - 1) {
			DrawRectangleRec(rec, BLACK);
		} else {
			c.r -= dc;
			c.g -= dc;
			c.b -= dc;
			DrawRectangleLinesEx(rec, 1.0f, c);
		}
	}
}

static void draw_curtain(double pct) {
	double clamped = clamp_f64(pct, 0.0, 1.0);
	int total = GAME_WIDTH * GAME_HEIGHT;
	int n = total - (int)(clamped * (double)total);
	if (n <= 0) {
		return;
	}

	int full_rows = n / GAME_WIDTH;
	int rem_cols = n % GAME_WIDTH;
	int top_full_y = GAME_HEIGHT - full_rows;

	for (int y = GAME_HEIGHT - 1; y >= top_full_y; y--) {
		for (int x = 0; x < GAME_WIDTH; x++) {
			draw_curtain_segment(x, y);
		}
	}

	int partial_y = top_full_y - 1;
	if (rem_cols > 0 && partial_y >= 0) {
		for (int x = 0; x < rem_cols; x++) {
			draw_curtain_segment(x, partial_y);
		}
	}
}

static void draw_game_launch_text(int y, int n) {
	int font = 96;
	const char *text = "1.";
	if (n >= 3) {
		text = "3...";
	} else if (n == 2) {
		text = "2..";
	}

	int w = MeasureText(text, font);
	DrawText(text, GAME_WIDTH * BLOCK_SIZE / 2 - w / 2, y + 22, font, WHITE);
}

static void draw_game_over_text(const Game *g, int y) {
	int center_x = GAME_WIDTH * BLOCK_SIZE / 2;

	const char *title = "GAME OVER";
	int tw = MeasureText(title, 34);
	DrawText(title, center_x - tw / 2, y + 22, 34, (Color){235, 70, 75, 255});

	char score_text[64];
	snprintf(score_text, sizeof(score_text), "Score: %d", g->game_score);
	int sw = MeasureText(score_text, 22);
	DrawText(score_text, center_x - sw / 2, y + 72, 22, WHITE);

	const char *prompt1 = "<SPACE> Restart";
	int p1w = MeasureText(prompt1, 20);
	DrawText(prompt1, center_x - p1w / 2, y + 116, 20, WHITE);

	const char *prompt2 = "<T> Title Screen";
	int p2w = MeasureText(prompt2, 20);
	DrawText(prompt2, center_x - p2w / 2, y + 152, 20, TEXT_MUTED);
}

static void draw_game_over_overlay(const Game *g) {
	if (g->state == STATE_GAME_OVER) {
		draw_curtain(g->state_timer / GATE_DURATION);

		if (g->state_timer <= 0.0) {
			Rectangle box = {12.0f, 150.0f, (float)(GAME_WIDTH * BLOCK_SIZE) - 24.0f, 216.0f};
			DrawRectangleRounded(box, 0.08f, 8, (Color){0, 0, 0, 220});
			DrawRectangleRoundedLinesEx(box, 0.08f, 8, 2.0f, BORDER_COLOR);
			draw_game_over_text(g, (int)box.y);
		}
	} else if (g->state == STATE_COUNTDOWN) {
		double pct = 1.0 - (g->state_timer - 1.0) / (GAME_LAUNCH_TIME - 1.0);
		draw_curtain(clamp_f64(pct, 0.0, 1.0));

		int count = (int)floor(g->state_timer);
		if (count > 0) {
			Rectangle box = {40.0f, 190.0f, (float)(GAME_WIDTH * BLOCK_SIZE) - 80.0f, 140.0f};
			DrawRectangleRounded(box, 0.1f, 8, (Color){0, 0, 0, 200});
			draw_game_launch_text((int)box.y, count);
		}
	}
}

void draw_game(const Game *g) {
	BeginDrawing();

	ClearBackground(g->backdrop.initialized ? g->backdrop.palette.bg_top_left : BACKGROUND);
	draw_backdrop(&g->backdrop, (float)GetScreenWidth(), (float)GetScreenHeight());

	// Camera2D transforms run in framebuffer pixels: raylib's BeginMode2D drops the
	// HiDPI screen-scale matrix, so size the viewport from the render (pixel) size.
	// Mouse input stays in logical points (see screen_to_ui callers).
	float scale = 1.0f, offset_x = 0.0f, offset_y = 0.0f;
	compute_viewport((float)GetRenderWidth(), (float)GetRenderHeight(), &scale, &offset_x, &offset_y);

	Camera2D ui_camera = {
		.offset   = {offset_x, offset_y},
		.target   = {0.0f, 0.0f},
		.rotation = 0.0f,
		.zoom     = scale,
	};

	if (g->state == STATE_TITLE) {
		BeginMode2D(ui_camera);
		draw_title_screen(g);
		EndMode2D();
		EndDrawing();
		return;
	}

	Camera2D board_camera = {
		.offset   = {
			offset_x + (float)BORDER_LEFT * scale,
			offset_y + (float)BORDER_TOP * scale,
		},
		.target   = {0.0f, 0.0f},
		.rotation = 0.0f,
		.zoom     = scale,
	};

	// Draw playfield outer border in scaled UI coordinates
	BeginMode2D(ui_camera);
	Rectangle board_border = {
		(float)OUTER_MARGIN,
		(float)OUTER_MARGIN,
		(float)(BOARD_PX_W + 2 * BORDER_SIZE),
		(float)(BOARD_PX_H + 2 * BORDER_SIZE),
	};
	DrawRectangleRec(board_border, BORDER_COLOR);
	EndMode2D();

	// Draw playfield inside scaled board camera space (origin = top-left of visible board)
	BeginMode2D(board_camera);

	draw_board(g);

	switch (g->state) {
	case STATE_TITLE:
		break;
	case STATE_PLAYING:
	case STATE_PAUSED:
		// Draw ghost underneath active piece so active piece cleanly covers overlapping cells
		draw_ghost(g->ghost);
		draw_tetromino(g->tetro);
		break;
	case STATE_LINE_CLEAR:
		draw_line_clear_flash(g);
		break;
	case STATE_GAME_OVER:
	case STATE_COUNTDOWN:
		draw_game_over_overlay(g);
		break;
	}

	draw_banner(g);

	EndMode2D();

	BeginMode2D(ui_camera);
	draw_side_panel(g);
	draw_status_overlay(g);
	EndMode2D();

	EndDrawing();
}
