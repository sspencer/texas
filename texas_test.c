#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_failed = 0;
static int current_test_failed = 0;

#define EXPECT(cond)                                                              \
	do {                                                                          \
		if (!(cond)) {                                                            \
			fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
			current_test_failed = 1;                                              \
		}                                                                         \
	} while (0)

#define EXPECT_EQ_INT(actual, expected)                                                       \
	do {                                                                                      \
		long long _a = (long long)(actual);                                                   \
		long long _e = (long long)(expected);                                                 \
		if (_a != _e) {                                                                       \
			fprintf(stderr, "  FAIL %s:%d: %s == %lld (expected %lld)\n",                     \
			        __FILE__, __LINE__, #actual, _a, _e);                                     \
			current_test_failed = 1;                                                          \
		}                                                                                     \
	} while (0)

#define EXPECT_EQ_FLOAT(actual, expected)                                                     \
	do {                                                                                      \
		float _a = (float)(actual);                                                           \
		float _e = (float)(expected);                                                         \
		if (fabsf(_a - _e) > 1e-5f) {                                                         \
			fprintf(stderr, "  FAIL %s:%d: %s == %.6f (expected %.6f)\n",                     \
			        __FILE__, __LINE__, #actual, _a, _e);                                     \
			current_test_failed = 1;                                                          \
		}                                                                                     \
	} while (0)

#define EXPECT_EQ_DOUBLE(actual, expected)                                                    \
	do {                                                                                      \
		double _a = (double)(actual);                                                         \
		double _e = (double)(expected);                                                       \
		if (fabs(_a - _e) > 1e-9) {                                                           \
			fprintf(stderr, "  FAIL %s:%d: %s == %.9f (expected %.9f)\n",                     \
			        __FILE__, __LINE__, #actual, _a, _e);                                     \
			current_test_failed = 1;                                                          \
		}                                                                                     \
	} while (0)

#define EXPECT_SHAPES_EQ(a, b)                                                                \
	do {                                                                                      \
		if (memcmp((a), (b), sizeof(a)) != 0) {                                               \
			fprintf(stderr, "  FAIL %s:%d: shape mismatch (%s != %s)\n",                      \
			        __FILE__, __LINE__, #a, #b);                                              \
			current_test_failed = 1;                                                          \
		}                                                                                     \
	} while (0)

static Game new_playing_game(void) {
	Game g;
	init_game(&g);
	start_new_game(&g);
	return g;
}

static Vector2 rect_center(Rectangle r) {
	return (Vector2){r.x + r.width * 0.5f, r.y + r.height * 0.5f};
}

static void test_ghost_empty_board_and_stack(void) {
	Game g = new_playing_game();

	// Place a T-piece at spawn on an empty board
	set_tetromino(&g.tetro, 3);
	update_ghost(&g);

	EXPECT_EQ_INT(g.ghost.x, g.tetro.x);
	EXPECT_EQ_INT(g.ghost.y, BOARD_HEIGHT - 2);
	EXPECT_SHAPES_EQ(g.ghost.shape, g.tetro.shape);

	// Place an obstacle directly underneath column 4 at row BOARD_HEIGHT - 5
	g.board[BOARD_HEIGHT - 5][4] = 1;
	update_ghost(&g);
	EXPECT_EQ_INT(g.ghost.y, BOARD_HEIGHT - 7);

	// Moving horizontally off the obstacle should drop the ghost back to the floor
	bool moved = move_tetro(&g, (Vec2i){3, 0});
	EXPECT(moved);
	EXPECT_EQ_INT(g.ghost.x, g.tetro.x);
	EXPECT_EQ_INT(g.ghost.y, BOARD_HEIGHT - 2);
}

static void test_ghost_never_desyncs_on_failed_rotation(void) {
	Game g = new_playing_game();

	set_tetromino(&g.tetro, 1);
	g.tetro.x = 3;
	g.tetro.y = HIDDEN_ROWS + 6;
	for (int y = HIDDEN_ROWS + 4; y <= HIDDEN_ROWS + 10; y++) {
		for (int x = 0; x < GAME_WIDTH; x++) {
			if (y == g.tetro.y + 1 && x >= g.tetro.x && x < g.tetro.x + 4) {
				continue;
			}
			g.board[y][x] = 2;
		}
	}
	update_ghost(&g);

	int32_t before_shape[4][4];
	memcpy(before_shape, g.tetro.shape, sizeof(before_shape));
	Direction before_dir = g.tetro.direction;

	bool rotated = rotate_with_kick(&g, true, NULL);
	EXPECT(!rotated);
	EXPECT_SHAPES_EQ(g.tetro.shape, before_shape);
	EXPECT_EQ_INT(g.tetro.direction, before_dir);
	EXPECT_SHAPES_EQ(g.ghost.shape, g.tetro.shape);
	EXPECT_EQ_INT(g.ghost.direction, g.tetro.direction);
	EXPECT_EQ_INT(g.ghost.x, g.tetro.x);
	EXPECT_EQ_INT(g.ghost.y, g.tetro.y);
}

static void test_ghost_empty_id_no_infinite_loop(void) {
	Game g = {0};
	update_ghost(&g);
	EXPECT_EQ_INT(g.ghost.id, 0);
}

static void test_tetromino_shapes_and_j_l_distinct(void) {
	// J (6) has its top-left hook at [0][0]
	EXPECT_EQ_INT(TETROMINOS[6].shape[0][0], 6);
	EXPECT_EQ_INT(TETROMINOS[6].shape[0][2], 0);
	EXPECT_EQ_INT(TETROMINOS[6].shape[1][0], 6);
	EXPECT_EQ_INT(TETROMINOS[6].shape[1][1], 6);
	EXPECT_EQ_INT(TETROMINOS[6].shape[1][2], 6);

	// L (7) has its top-right hook at [0][2]
	EXPECT_EQ_INT(TETROMINOS[7].shape[0][0], 0);
	EXPECT_EQ_INT(TETROMINOS[7].shape[0][2], 7);
	EXPECT_EQ_INT(TETROMINOS[7].shape[1][0], 7);
	EXPECT_EQ_INT(TETROMINOS[7].shape[1][1], 7);
	EXPECT_EQ_INT(TETROMINOS[7].shape[1][2], 7);

	// Every piece 1..7 must have exactly 4 non-zero cells and spawn North
	for (int id = 1; id <= 7; id++) {
		Tetromino piece = TETROMINOS[id];
		EXPECT_EQ_INT(piece.direction, DIR_NORTH);
		int count = 0;
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				if (piece.shape[r][c] != 0) {
					count++;
				}
			}
		}
		EXPECT_EQ_INT(count, 4);
	}
}

static void test_srs_rotation_roundtrip_and_wall_kick(void) {
	Game g = new_playing_game();

	for (int id = 1; id <= 7; id++) {
		set_tetromino(&g.tetro, id);
		g.tetro.y = HIDDEN_ROWS + 5;
		Tetromino orig = g.tetro;
		for (int k = 0; k < 4; k++) {
			rotate_cw(&g.tetro);
		}
		EXPECT_SHAPES_EQ(g.tetro.shape, orig.shape);
		EXPECT_EQ_INT(g.tetro.direction, orig.direction);

		rotate_cw(&g.tetro);
		rotate_ccw(&g.tetro);
		EXPECT_SHAPES_EQ(g.tetro.shape, orig.shape);
		EXPECT_EQ_INT(g.tetro.direction, orig.direction);
	}

	// Wall kick test: T-piece vertical against left wall, rotate CW -> kicks right by +1
	set_tetromino(&g.tetro, 3);
	g.tetro.y = HIDDEN_ROWS + 5;
	rotate_cw(&g.tetro); // East: occupies cols 1 & 2 of 3x3
	g.tetro.x = -1;      // Flush against left wall (col 1 at board x=0)
	EXPECT(!collide(&g, g.tetro));

	int kick_idx = -1;
	bool rotated = rotate_with_kick(&g, true, &kick_idx); // East -> South (needs kick +1 right)
	EXPECT(rotated);
	EXPECT_EQ_INT(kick_idx, 1);
	EXPECT_EQ_INT(g.tetro.x, 0);
	EXPECT_EQ_INT(g.tetro.direction, DIR_SOUTH);
	EXPECT_SHAPES_EQ(g.ghost.shape, g.tetro.shape);
	EXPECT_EQ_INT(g.ghost.x, g.tetro.x);
}

static void test_seven_bag_randomizer(void) {
	Piece_Bag bag = {0};
	bag_refill(&bag);

	for (int round = 0; round < 10; round++) {
		int seen[8] = {0};
		for (int i = 0; i < 7; i++) {
			int id = bag_next(&bag);
			EXPECT(id >= 1 && id <= 7);
			seen[id]++;
		}
		for (int id = 1; id <= 7; id++) {
			EXPECT_EQ_INT(seen[id], 1);
		}
	}
}

static void test_soft_drop_and_hard_drop_scoring(void) {
	Game g = new_playing_game();

	set_tetromino(&g.tetro, 2); // O-piece at y = HIDDEN_ROWS (4)
	update_ghost(&g);
	int expected_drop = (BOARD_HEIGHT - 2) - g.tetro.y;

	EXPECT(soft_drop_step(&g));
	EXPECT_EQ_INT(g.game_score, 1);

	int dropped = hard_drop(&g);
	EXPECT_EQ_INT(dropped, expected_drop - 1);
	EXPECT_EQ_INT(g.game_score, 1 + (expected_drop - 1) * 2);

	// Soft drop when blocked on floor must NOT award score
	set_tetromino(&g.tetro, 2);
	g.tetro.y = BOARD_HEIGHT - 2;
	g.tetro.x = 0;
	int score_before = g.game_score;
	EXPECT(!soft_drop_step(&g));
	EXPECT_EQ_INT(g.game_score, score_before);
}

static void test_hold_piece_mechanics(void) {
	Game g = new_playing_game();

	int first_id = g.tetro.id;
	int next_id = g.next_queue[0].id;

	EXPECT(hold_piece(&g));
	EXPECT_EQ_INT(g.hold_tetro.id, first_id);
	EXPECT_EQ_INT(g.tetro.id, next_id);
	EXPECT(g.hold_used);
	EXPECT_EQ_INT(g.ghost.id, next_id);

	// Second hold before locking must be rejected
	EXPECT(!hold_piece(&g));

	// Lock piece via hard drop -> hold resets
	hard_drop(&g);
	if (g.state == STATE_LINE_CLEAR) {
		update_game(&g, LINE_CLEAR_DURATION + 0.01);
	}
	EXPECT(!g.hold_used);

	// Holding again swaps with the previously held piece
	int cur_id = g.tetro.id;
	EXPECT(hold_piece(&g));
	EXPECT_EQ_INT(g.tetro.id, first_id);
	EXPECT_EQ_INT(g.hold_tetro.id, cur_id);
}

static void test_line_clear_level_carryover_and_b2b(void) {
	Game g = new_playing_game();
	g.total_lines = 8;
	g.level = 0;

	// Fill bottom 4 rows except column 0, then drop a vertical I-piece into column 0 for a Tetris
	for (int y = BOARD_HEIGHT - 4; y < BOARD_HEIGHT; y++) {
		for (int x = 1; x < GAME_WIDTH; x++) {
			g.board[y][x] = 6;
		}
	}

	set_tetromino(&g.tetro, 1); // I-piece
	rotate_cw(&g.tetro);        // vertical at col 2 of 4x4
	g.tetro.x = -2;             // col 2 lands at board x=0
	g.tetro.y = HIDDEN_ROWS;
	update_ghost(&g);

	hard_drop(&g);
	EXPECT_EQ_INT(g.state, STATE_LINE_CLEAR);
	EXPECT_EQ_INT(g.clear_row_count, 4);

	update_game(&g, LINE_CLEAR_DURATION + 0.01);
	EXPECT_EQ_INT(g.state, STATE_PLAYING);
	EXPECT_EQ_INT(g.total_lines, 12);
	EXPECT_EQ_INT(g.level, 1);
	EXPECT(g.back_to_back);

	for (int y = 0; y < BOARD_HEIGHT; y++) {
		for (int x = 0; x < GAME_WIDTH; x++) {
			EXPECT_EQ_INT(g.board[y][x], 0);
		}
	}
}

static void test_tspin_detection(void) {
	Game g = new_playing_game();

	int y_bot = BOARD_HEIGHT - 1;
	for (int x = 0; x < GAME_WIDTH; x++) {
		if (x != 4) {
			g.board[y_bot][x] = 5;
		}
	}
	for (int x = 0; x < GAME_WIDTH; x++) {
		if (x < 3 || x > 5) {
			g.board[y_bot - 1][x] = 5;
		}
	}
	g.board[y_bot - 2][3] = 5;
	g.board[y_bot - 2][5] = 5;

	set_tetromino(&g.tetro, 3);
	rotate_cw(&g.tetro);
	rotate_cw(&g.tetro);
	g.tetro.x = 3;
	g.tetro.y = y_bot - 2;
	g.last_action_rotate = true;

	EXPECT(!collide(&g, g.tetro));
	EXPECT_EQ_INT(detect_tspin(&g), TSPIN_FULL);
}

static void test_lock_delay_and_game_over_restart(void) {
	Game g = new_playing_game();

	set_tetromino(&g.tetro, 2);
	g.tetro.y = BOARD_HEIGHT - 2;
	update_ghost(&g);

	update_game(&g, LOCK_DELAY * 0.5);
	EXPECT_EQ_INT(g.state, STATE_PLAYING);
	EXPECT(g.on_ground);
	EXPECT_EQ_INT(g.board[BOARD_HEIGHT - 1][4], 0);

	EXPECT(move_tetro(&g, (Vec2i){1, 0}));
	EXPECT_EQ_DOUBLE(g.lock_timer, 0.0);
	EXPECT_EQ_INT(g.lock_resets, 1);

	update_game(&g, LOCK_DELAY + 0.05);
	EXPECT_EQ_INT(g.board[BOARD_HEIGHT - 1][5], 2);

	g.game_score = 4200;
	set_game_over(&g);
	EXPECT_EQ_INT(g.state, STATE_GAME_OVER);
	EXPECT_EQ_INT(g.high_score, 4200);

	update_game(&g, GATE_DURATION + 0.1);
	EXPECT_EQ_DOUBLE(g.state_timer, 0.0);

	begin_restart_countdown(&g);
	EXPECT_EQ_INT(g.state, STATE_COUNTDOWN);
	update_game(&g, GAME_LAUNCH_TIME);
	EXPECT_EQ_INT(g.state, STATE_PLAYING);
	EXPECT_EQ_INT(g.game_score, 0);
	EXPECT_EQ_INT(g.high_score, 4200);
	EXPECT_EQ_INT(g.level, 0);
}

static void test_level_time_safe_and_monotonic(void) {
	double prev = level_time(0);
	EXPECT(prev > 0.99 && prev <= 1.0);

	for (int lv = 1; lv <= 150; lv++) {
		double cur = level_time(lv);
		EXPECT(!isnan(cur));
		EXPECT(!isinf(cur));
		EXPECT(cur >= 0.016);
		EXPECT(cur <= prev);
		prev = cur;
	}
}

static void test_title_screen_defaults_and_starting_height_speed(void) {
	Game g;
	init_game(&g);

	EXPECT_EQ_INT(g.state, STATE_TITLE);
	EXPECT_EQ_INT(g.start_speed, 0);
	EXPECT_EQ_INT(g.start_height, 0);
	EXPECT_EQ_INT(g.title_focus, TITLE_FOCUS_SPEED);

	start_new_game(&g);
	EXPECT_EQ_INT(g.state, STATE_PLAYING);
	EXPECT_EQ_INT(g.level, 0);
	EXPECT_EQ_DOUBLE(level_time(g.level), 1.0);
	for (int y = 0; y < BOARD_HEIGHT; y++) {
		for (int x = 0; x < GAME_WIDTH; x++) {
			EXPECT_EQ_INT(g.board[y][x], 0);
		}
	}

	g.game_score = 1234;
	return_to_title(&g);
	EXPECT_EQ_INT(g.state, STATE_TITLE);
	EXPECT_EQ_INT(g.high_score, 1234);

	title_adjust_value(&g, -5);
	EXPECT_EQ_INT(g.start_speed, 0);
	title_adjust_value(&g, 20);
	EXPECT_EQ_INT(g.start_speed, 9);

	g.title_focus = TITLE_FOCUS_HEIGHT;
	title_set_value(&g, 8);
	EXPECT_EQ_INT(g.start_height, 0);
	title_adjust_value(&g, 10);
	EXPECT_EQ_INT(g.start_height, 5);

	start_new_game(&g);
	EXPECT_EQ_INT(g.state, STATE_PLAYING);
	EXPECT_EQ_INT(g.level, 9);
	EXPECT(level_time(g.level) < 0.1);

	int garbage_rows = HEIGHT_ROWS[5];
	EXPECT_EQ_INT(garbage_rows, 12);
	for (int y = 0; y < BOARD_HEIGHT - garbage_rows; y++) {
		for (int x = 0; x < GAME_WIDTH; x++) {
			EXPECT_EQ_INT(g.board[y][x], 0);
		}
	}
	for (int y = BOARD_HEIGHT - garbage_rows; y < BOARD_HEIGHT; y++) {
		int filled = 0;
		for (int x = 0; x < GAME_WIDTH; x++) {
			if (g.board[y][x] != 0) {
				filled++;
			}
		}
		EXPECT(filled >= 3 && filled <= GAME_WIDTH - 2);
	}

	EXPECT(!collide(&g, g.tetro));
	EXPECT(!collide(&g, g.ghost));
	EXPECT(g.ghost.y >= g.tetro.y);
}

static void test_cli_args_and_viewport_scaling(void) {
	Launch_Options opts;
	bool help = false;
	const char *err = NULL;

	// Default: fullscreen enabled
	parse_cli_args(0, NULL, &opts, &help, &err);
	EXPECT(opts.fullscreen);
	EXPECT(!help);
	EXPECT(err == NULL);

	// Windowed flags disable fullscreen
	const char *windowed_flags[3] = {"-w", "--windowed", "--no-fullscreen"};
	for (int i = 0; i < 3; i++) {
		parse_cli_args(1, &windowed_flags[i], &opts, &help, &err);
		EXPECT(!opts.fullscreen);
		EXPECT(!help);
		EXPECT(err == NULL);
	}

	// Explicit fullscreen flag overrides earlier windowed flag
	const char *override_args[2] = {"--windowed", "--fullscreen"};
	parse_cli_args(2, override_args, &opts, &help, &err);
	EXPECT(opts.fullscreen);
	EXPECT(!help);
	EXPECT(err == NULL);

	// Help flag
	const char *help_arg = "--help";
	parse_cli_args(1, &help_arg, &opts, &help, &err);
	EXPECT(help);
	EXPECT(err == NULL);

	// Unknown flag returns error
	const char *bogus_arg = "--bogus";
	parse_cli_args(1, &bogus_arg, &opts, &help, &err);
	EXPECT(!help);
	EXPECT(err != NULL && strcmp(err, "--bogus") == 0);

	// Mute flags enable muted launch option
	const char *mute_flags[3] = {"-m", "--mute", "--no-music"};
	for (int i = 0; i < 3; i++) {
		parse_cli_args(1, &mute_flags[i], &opts, &help, &err);
		EXPECT(opts.muted);
		EXPECT(!help);
		EXPECT(err == NULL);
	}

	// macOS Finder/LaunchServices -psn_* argument is ignored cleanly
	const char *psn_arg = "-psn_0_123456";
	parse_cli_args(1, &psn_arg, &opts, &help, &err);
	EXPECT(opts.fullscreen);
	EXPECT(!opts.muted);
	EXPECT(!help);
	EXPECT(err == NULL);

	// Viewport scaling: exact native window -> scale 1, offset 0
	float s1 = 0, ox1 = 0, oy1 = 0;
	compute_viewport((float)BASE_WIDTH, (float)BASE_HEIGHT, &s1, &ox1, &oy1);
	EXPECT_EQ_FLOAT(s1, 1.0f);
	EXPECT_EQ_FLOAT(ox1, 0.0f);
	EXPECT_EQ_FLOAT(oy1, 0.0f);

	// 1920x1080 fullscreen -> height-limited scale, centered horizontally
	float s2 = 0, ox2 = 0, oy2 = 0;
	compute_viewport(1920.0f, 1080.0f, &s2, &ox2, &oy2);
	float expected_scale = 1080.0f / (float)BASE_HEIGHT;
	EXPECT_EQ_FLOAT(s2, expected_scale);
	EXPECT_EQ_FLOAT(oy2, 0.0f);
	EXPECT_EQ_FLOAT(ox2, (1920.0f - (float)BASE_WIDTH * expected_scale) * 0.5f);
}

static void test_title_mouse_selection_and_start(void) {
	Game g;
	init_game(&g);
	Title_Layout layout = compute_title_layout();

	bool hover = update_title_pointer(&g, rect_center(layout.speed_rects[7]), false);
	EXPECT(hover);
	EXPECT_EQ_INT(g.title_hover_speed, 7);
	EXPECT_EQ_INT(g.start_speed, 0);

	float s = 0, ox = 0, oy = 0;
	compute_viewport(1920.0f, 1080.0f, &s, &ox, &oy);
	Vector2 sp7 = rect_center(layout.speed_rects[7]);
	Vector2 screen_pt = {ox + sp7.x * s, oy + sp7.y * s};
	Vector2 ui_pt = screen_to_ui(screen_pt, 1920.0f, 1080.0f);
	hover = update_title_pointer(&g, ui_pt, true);
	EXPECT(hover);
	EXPECT_EQ_INT(g.start_speed, 7);
	EXPECT_EQ_INT(g.title_focus, TITLE_FOCUS_SPEED);

	hover = update_title_pointer(&g, rect_center(layout.height_rects[4]), true);
	EXPECT(hover);
	EXPECT_EQ_INT(g.start_height, 4);
	EXPECT_EQ_INT(g.title_focus, TITLE_FOCUS_HEIGHT);

	hover = update_title_pointer(&g, (Vector2){5.0f, 5.0f}, true);
	EXPECT(!hover);
	EXPECT_EQ_INT(g.state, STATE_TITLE);

	hover = update_title_pointer(&g, rect_center(layout.start_rect), true);
	EXPECT(hover);
	EXPECT_EQ_INT(g.state, STATE_PLAYING);
	EXPECT_EQ_INT(g.level, 7);
	EXPECT_EQ_INT(g.start_height, 4);
}

static void test_generative_backdrop_styles_and_bounds(void) {
	Backdrop b = {0};

	for (int st = 0; st < BACKDROP_STYLE_COUNT; st++) {
		Backdrop_Style style = (Backdrop_Style)st;
		init_backdrop_style(&b, style);
		EXPECT(b.initialized);
		EXPECT_EQ_INT(b.style, style);
		EXPECT_EQ_DOUBLE(b.time, 0.0);

		Color corners[4] = {
			b.palette.bg_top_left,
			b.palette.bg_top_right,
			b.palette.bg_bottom_left,
			b.palette.bg_bottom_right,
		};
		for (int i = 0; i < 4; i++) {
			EXPECT(corners[i].r <= 90 && corners[i].g <= 90 && corners[i].b <= 95);
			EXPECT_EQ_INT(corners[i].a, 255);
		}

		for (int step = 0; step < 240; step++) {
			update_backdrop(&b, 0.5);
		}
		EXPECT(b.time > 119.0);

		float lo = -BACKDROP_WRAP_MARGIN - 0.001f;
		float hi = 1.0f + BACKDROP_WRAP_MARGIN + 0.001f;
		switch (style) {
		case BACKDROP_CONSTELLATION:
			EXPECT(b.node_count >= 28 && b.node_count <= BACKDROP_MAX_NODES);
			for (int i = 0; i < b.node_count; i++) {
				Backdrop_Node n = b.nodes[i];
				EXPECT(!isnan(n.x) && !isnan(n.y));
				EXPECT(n.x >= lo && n.x <= hi);
				EXPECT(n.y >= lo && n.y <= hi);
			}
			break;
		case BACKDROP_FLOW_WAVES:
			EXPECT(b.wave_count >= 5 && b.wave_count <= BACKDROP_MAX_WAVES);
			break;
		case BACKDROP_GEO_DRIFT:
			EXPECT(b.shape_count >= 18 && b.shape_count <= BACKDROP_MAX_SHAPES);
			for (int i = 0; i < b.shape_count; i++) {
				Backdrop_Shape s = b.shapes[i];
				EXPECT(!isnan(s.x) && !isnan(s.y) && !isnan(s.angle));
				EXPECT(s.x >= lo && s.x <= hi);
				EXPECT(s.y >= lo && s.y <= hi);
				EXPECT(s.angle >= 0.0f && s.angle < 360.0f);
			}
			break;
		case BACKDROP_ORBITAL_CONTOURS:
			EXPECT(b.orbit_count >= 2 && b.orbit_count <= BACKDROP_MAX_ORBITS);
			break;
		case BACKDROP_TRUCHET_LATTICE:
			for (int r = 0; r < BACKDROP_LATTICE_ROWS; r++) {
				for (int c = 0; c < BACKDROP_LATTICE_COLS; c++) {
					EXPECT(b.lattice_cells[r][c] < 4);
				}
			}
			break;
		case BACKDROP_STYLE_COUNT:
			break;
		}
	}
}

static void test_generative_backdrop_game_lifecycle(void) {
	Game g;
	init_game(&g);
	EXPECT(g.backdrop.initialized);

	update_game(&g, 0.25);
	EXPECT(g.backdrop.time > 0.2);

	Backdrop_Style prev_style = g.backdrop.style;
	start_new_game(&g);
	EXPECT(g.backdrop.initialized);
	EXPECT(g.backdrop.style != prev_style);
	EXPECT_EQ_DOUBLE(g.backdrop.time, 0.0);

	update_game(&g, 0.5);
	double t_playing = g.backdrop.time;
	EXPECT(t_playing >= 0.5);

	g.state = STATE_PAUSED;
	update_game(&g, 1.0);
	EXPECT_EQ_DOUBLE(g.backdrop.time, t_playing);
}

static void test_music_song_step_alignment_and_synthesis(void) {
	Music_Engine m;
	music_init(&m);

	int16_t buf[MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS];

	for (int song = SONG_TITLE; song < SONG_COUNT; song++) {
		Music_Song s = (Music_Song)song;
		EXPECT(music_validate_song_lengths(s));
		EXPECT(music_song_total_steps(s) > 0);

		music_play_song(&m, s);
		EXPECT_EQ_INT(m.song, s);
		EXPECT(!m.finished);

		double sum_sq = 0.0;
		double sum_dc = 0.0;
		int peak = 0;
		int total_samples = 0;

		// Render ~2.5 seconds of stereo audio
		int sample_chunks = (int)((2.5 * MUSIC_SAMPLE_RATE) / MUSIC_BUFFER_FRAMES) + 1;
		for (int chunk = 0; chunk < sample_chunks; chunk++) {
			music_render(&m, buf, MUSIC_BUFFER_FRAMES);
			for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
				int v = buf[i];
				int av = (v < 0) ? -v : v;
				if (av > peak) peak = av;
				sum_sq += (double)v * (double)v;
				sum_dc += (double)v;
				total_samples++;
			}
		}

		double rms = sqrt(sum_sq / (double)total_samples);
		double dc = fabs(sum_dc / (double)total_samples);
		EXPECT(rms > 800.0);     // Audible, healthy signal level
		EXPECT(peak > 3000);     // Dynamic peaks present
		EXPECT(peak <= 32700);   // Soft-limited, never hard clips
		EXPECT(dc < 400.0);      // Properly DC-blocked
	}

	// Verify looping wraps seamlessly on SONG_TITLE (~15.2s -> render up to 20s)
	music_play_song(&m, SONG_TITLE);
	bool saw_wrap = false;
	int prev_step = m.current_step;
	int wrap_chunks = (20 * MUSIC_SAMPLE_RATE) / MUSIC_BUFFER_FRAMES + 1;
	for (int chunk = 0; chunk < wrap_chunks; chunk++) {
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
		if (m.current_step < prev_step) {
			saw_wrap = true;
			break;
		}
		prev_step = m.current_step;
	}
	EXPECT(saw_wrap);
	EXPECT(!m.finished);

	// Verify one-shot SONG_GAME_OVER finishes (~3.6s -> render 5.5s) and settles to silence
	music_play_song(&m, SONG_GAME_OVER);
	int go_chunks = (int)((5.5 * MUSIC_SAMPLE_RATE) / MUSIC_BUFFER_FRAMES) + 1;
	for (int chunk = 0; chunk < go_chunks; chunk++) {
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	}
	EXPECT(m.finished);
	music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	int tail_peak = 0;
	for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
		int av = (buf[i] < 0) ? -buf[i] : buf[i];
		if (av > tail_peak) tail_peak = av;
	}
	EXPECT(tail_peak < 16);
}

static void test_music_controls_pause_mute_and_danger_tempo(void) {
	Music_Engine m;
	music_init(&m);
	int16_t buf[MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS];

	// Start Sugar Plum and advance briefly
	music_play_song(&m, SONG_SUGAR_PLUM);
	music_render(&m, buf, MUSIC_BUFFER_FRAMES);

	// Pause should fade to silence and freeze sequencer step
	music_set_paused(&m, true);
	music_render(&m, buf, MUSIC_BUFFER_FRAMES); // 15ms fade completes within chunk
	int paused_step = m.current_step;
	// Let DC-blocker tail settle over ~100ms, then verify exact silence & frozen step
	for (int c = 0; c < 5; c++) {
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	}
	EXPECT_EQ_INT(m.current_step, paused_step);
	int paused_peak = 0;
	for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
		int av = (buf[i] < 0) ? -buf[i] : buf[i];
		if (av > paused_peak) paused_peak = av;
	}
	EXPECT(paused_peak < 4);

	// Unpausing resumes advancing
	music_set_paused(&m, false);
	for (int c = 0; c < 12; c++) {
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	}
	EXPECT(m.current_step > paused_step);

	// Muting fades output cleanly to exact silence
	music_set_muted(&m, true);
	music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	int muted_peak = 0;
	for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
		int av = (buf[i] < 0) ? -buf[i] : buf[i];
		if (av > muted_peak) muted_peak = av;
	}
	EXPECT_EQ_INT(muted_peak, 0);
	music_set_muted(&m, false);

	// Danger mode accelerates tempo scale on gameplay tracks
	music_play_song(&m, SONG_TETRA_MARCH);
	EXPECT_EQ_FLOAT(m.target_tempo_scale, 1.0f);
	music_set_danger(&m, true);
	EXPECT(m.target_tempo_scale > 1.2f);
	for (int c = 0; c < 40; c++) {
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	}
	EXPECT(m.tempo_scale > 1.2f);
}

static void test_music_game_lifecycle_and_title_selection(void) {
	Game g;
	init_game(&g);

	EXPECT_EQ_INT(g.start_music, MUSIC_SEL_1);
	EXPECT_EQ_INT(g.music.song, SONG_TITLE);
	EXPECT(!g.music.muted);

	// Clicking Music 3 on title screen previews Korobeiniki
	Title_Layout layout = compute_title_layout();
	bool hit = update_title_pointer(&g, rect_center(layout.music_rects[MUSIC_SEL_3]), true);
	EXPECT(hit);
	EXPECT_EQ_INT(g.start_music, MUSIC_SEL_3);
	EXPECT_EQ_INT(g.title_focus, TITLE_FOCUS_MUSIC);
	EXPECT_EQ_INT(g.music.song, SONG_KOROBEINIKI);

	// Clicking OFF stops preview
	hit = update_title_pointer(&g, rect_center(layout.music_rects[MUSIC_SEL_OFF]), true);
	EXPECT(hit);
	EXPECT_EQ_INT(g.start_music, MUSIC_SEL_OFF);
	EXPECT_EQ_INT(g.music.song, SONG_NONE);

	// Keyboard digit 2 selects Music 2 (Tetra March)
	title_set_value(&g, 2);
	EXPECT_EQ_INT(g.start_music, MUSIC_SEL_2);
	EXPECT_EQ_INT(g.music.song, SONG_TETRA_MARCH);

	// Starting game plays Tetra March
	start_new_game(&g);
	EXPECT_EQ_INT(g.music.song, SONG_TETRA_MARCH);
	EXPECT(!g.music_danger);

	// Stack reaching top 6 visible rows triggers danger tempo
	g.board[HIDDEN_ROWS + 4][0] = 1;
	update_game(&g, 0.016);
	EXPECT(g.music_danger);
	EXPECT(g.music.danger);

	// Clearing stack exits danger tempo
	clear_board(&g);
	update_game(&g, 0.016);
	EXPECT(!g.music_danger);
	EXPECT(!g.music.danger);

	// Cycling track during gameplay switches to Korobeiniki
	cycle_music_selection(&g, 1);
	EXPECT_EQ_INT(g.start_music, MUSIC_SEL_3);
	EXPECT_EQ_INT(g.music.song, SONG_KOROBEINIKI);

	// Mute toggle works cleanly
	toggle_music_mute(&g);
	EXPECT(g.music.muted);
	toggle_music_mute(&g);
	EXPECT(!g.music.muted);

	// Pausing game pauses music
	g.state = STATE_PAUSED;
	update_game(&g, 0.016);
	EXPECT(g.music.paused);
	g.state = STATE_PLAYING;
	update_game(&g, 0.016);
	EXPECT(!g.music.paused);

	// Game over triggers one-shot Game Over jingle
	set_game_over(&g);
	EXPECT_EQ_INT(g.music.song, SONG_GAME_OVER);

	// Restart countdown silences music until new game starts
	begin_restart_countdown(&g);
	EXPECT_EQ_INT(g.music.song, SONG_NONE);

	// Returning to title plays Title theme
	return_to_title(&g);
	EXPECT_EQ_INT(g.music.song, SONG_TITLE);
}

static void test_sfx_synthesis_distinct_line_clear_and_tetris(void) {
	Music_Engine m;
	music_init(&m);
	// Leave song as SONG_NONE (Music OFF): SFX must still synthesize cleanly
	int16_t buf[MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS];

	double line_clear_energy = 0.0;
	double tetris_energy = 0.0;
	double tetris_late_energy = 0.0;
	double line_clear_late_energy = 0.0;

	for (int kind = SFX_MOVE; kind < SFX_COUNT; kind++) {
		Music_Sfx sfx = (Music_Sfx)kind;
		music_clear_sfx(&m);
		music_play_sfx(&m, sfx);
		EXPECT_EQ_INT(m.last_sfx, sfx);

		int peak = 0;
		double total_sq = 0.0;
		double late_sq = 0.0;

		// Render ~0.95 seconds (41 chunks * 1024 frames)
		for (int chunk = 0; chunk < 41; chunk++) {
			music_render(&m, buf, MUSIC_BUFFER_FRAMES);
			for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
				int v = buf[i];
				int av = (v < 0) ? -v : v;
				if (av > peak) peak = av;
				total_sq += (double)v * (double)v;
				// Window between ~0.42s and ~0.68s (chunks 18..29)
				if (chunk >= 18 && chunk <= 29) {
					late_sq += (double)v * (double)v;
				}
			}
		}

		EXPECT(peak > 1200);   // Clearly audible
		EXPECT(peak <= 32700); // Never hard clips

		// After ~0.95s, voice has completed and settled to silence
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
		int tail_peak = 0;
		for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
			int av = (buf[i] < 0) ? -buf[i] : buf[i];
			if (av > tail_peak) tail_peak = av;
		}
		EXPECT(tail_peak < 16);

		if (sfx == SFX_LINE_CLEAR) {
			line_clear_energy = total_sq;
			line_clear_late_energy = late_sq;
		} else if (sfx == SFX_TETRIS) {
			tetris_energy = total_sq;
			tetris_late_energy = late_sq;
		}
	}

	// 4-row Tetris fanfare must be noticeably longer/richer than 1-3 row clear
	EXPECT(tetris_energy > line_clear_energy * 1.5);
	EXPECT(tetris_late_energy > 1e9);
	EXPECT(line_clear_late_energy < tetris_late_energy * 0.05);

	// Muting must silence SFX cleanly
	music_set_muted(&m, true);
	music_render(&m, buf, MUSIC_BUFFER_FRAMES); // Let 15ms mute ramp finish
	music_play_sfx(&m, SFX_TETRIS);
	music_render(&m, buf, MUSIC_BUFFER_FRAMES);
	for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
		EXPECT_EQ_INT(buf[i], 0);
	}
}

static void test_sfx_gameplay_triggers(void) {
	Game g = new_playing_game();
	EXPECT_EQ_INT(g.music.last_sfx, SFX_MENU_START);

	// Move left triggers SFX_MOVE
	set_tetromino(&g.tetro, 3); // T-piece
	g.tetro.y = HIDDEN_ROWS + 4;
	uint32_t before = g.music.sfx_trigger_count;
	EXPECT(move_tetro(&g, (Vec2i){-1, 0}));
	EXPECT_EQ_INT(g.music.last_sfx, SFX_MOVE);
	EXPECT_EQ_INT(g.music.sfx_trigger_count, before + 1);

	// Rotation triggers SFX_ROTATE
	EXPECT(rotate_with_kick(&g, true, NULL));
	EXPECT_EQ_INT(g.music.last_sfx, SFX_ROTATE);

	// Hold triggers SFX_HOLD
	EXPECT(hold_piece(&g));
	EXPECT_EQ_INT(g.music.last_sfx, SFX_HOLD);

	// Gravity lock (no clear) triggers SFX_LOCK
	set_tetromino(&g.tetro, 2); // O-piece
	g.tetro.x = 0;
	g.tetro.y = BOARD_HEIGHT - 2;
	lock_current_piece(&g);
	EXPECT_EQ_INT(g.music.last_sfx, SFX_LOCK);

	// Hard drop (no clear) triggers SFX_HARD_DROP
	clear_board(&g);
	set_tetromino(&g.tetro, 2);
	g.tetro.x = 4;
	g.tetro.y = HIDDEN_ROWS;
	hard_drop(&g);
	EXPECT_EQ_INT(g.music.last_sfx, SFX_HARD_DROP);

	// 1-row clear triggers SFX_LINE_CLEAR
	clear_board(&g);
	for (int x = 0; x < GAME_WIDTH; x++) {
		if (x != 0 && x != 1) {
			g.board[BOARD_HEIGHT - 1][x] = 5;
		}
	}
	set_tetromino(&g.tetro, 2); // O-piece completes bottom 1 row (row above stays partial)
	g.tetro.x = 0;
	g.tetro.y = HIDDEN_ROWS;
	hard_drop(&g);
	EXPECT_EQ_INT(g.state, STATE_LINE_CLEAR);
	EXPECT_EQ_INT(g.clear_row_count, 1);
	EXPECT_EQ_INT(g.music.last_sfx, SFX_LINE_CLEAR);
	update_game(&g, LINE_CLEAR_DURATION + 0.01);

	// 4-row clear (Tetris!) triggers distinct SFX_TETRIS, plus SFX_LEVEL_UP on level increment
	clear_board(&g);
	g.total_lines = 8;
	g.level = 0;
	for (int y = BOARD_HEIGHT - 4; y < BOARD_HEIGHT; y++) {
		for (int x = 1; x < GAME_WIDTH; x++) {
			g.board[y][x] = 6;
		}
	}
	set_tetromino(&g.tetro, 1); // I-piece
	rotate_cw(&g.tetro);
	g.tetro.x = -2;
	g.tetro.y = HIDDEN_ROWS;
	hard_drop(&g);
	EXPECT_EQ_INT(g.state, STATE_LINE_CLEAR);
	EXPECT_EQ_INT(g.clear_row_count, 4);
	EXPECT_EQ_INT(g.music.last_sfx, SFX_TETRIS);

	update_game(&g, LINE_CLEAR_DURATION + 0.01);
	EXPECT_EQ_INT(g.level, 1);
	EXPECT_EQ_INT(g.music.last_sfx, SFX_LEVEL_UP);
}

static void test_config_save_load_and_game_persistence(void) {
	const char *path = "/tmp/texas_cfg_test/nested/config.ini";
	remove(path);

	// Missing file returns false and populates clean defaults
	Texas_Config cfg;
	EXPECT(!config_load(path, &cfg));
	EXPECT_EQ_INT(cfg.start_speed, 0);
	EXPECT_EQ_INT(cfg.start_height, 0);
	EXPECT_EQ_INT(cfg.start_music, MUSIC_SEL_1);
	EXPECT(!cfg.muted);
	EXPECT(cfg.fullscreen);
	for (int i = 0; i < CONFIG_SPEED_COUNT; i++) {
		EXPECT_EQ_INT(cfg.high_scores[i], 0);
	}

	// Save & round-trip all settings including distinct per-speed high scores
	cfg.start_speed = 6;
	cfg.start_height = 4;
	cfg.start_music = MUSIC_SEL_3;
	cfg.muted = true;
	cfg.fullscreen = false;
	cfg.high_scores[0] = 4500;
	cfg.high_scores[6] = 12345;
	cfg.high_scores[9] = 29000;
	EXPECT(config_save(path, &cfg));

	Texas_Config loaded;
	EXPECT(config_load(path, &loaded));
	EXPECT_EQ_INT(loaded.start_speed, 6);
	EXPECT_EQ_INT(loaded.start_height, 4);
	EXPECT_EQ_INT(loaded.start_music, MUSIC_SEL_3);
	EXPECT(loaded.muted);
	EXPECT(!loaded.fullscreen);
	EXPECT_EQ_INT(loaded.high_scores[0], 4500);
	EXPECT_EQ_INT(loaded.high_scores[3], 0);
	EXPECT_EQ_INT(loaded.high_scores[6], 12345);
	EXPECT_EQ_INT(loaded.high_scores[9], 29000);

	// Out-of-range keys are clamped, unknown keys ignored, legacy high_score migrates to start_speed
	FILE *f = fopen(path, "w");
	EXPECT(f != NULL);
	if (f) {
		fprintf(f, "# comment\nunknown = 999\nspeed = 42\nheight = -3\nmusic = off\nmuted = no\nfullscreen = false\nhigh_score = 7777\nhigh_score_0 = -10\n");
		fclose(f);
	}
	EXPECT(config_load(path, &loaded));
	EXPECT_EQ_INT(loaded.start_speed, MAX_START_SPEED);
	EXPECT_EQ_INT(loaded.start_height, 0);
	EXPECT_EQ_INT(loaded.start_music, MUSIC_SEL_OFF);
	EXPECT(!loaded.muted);
	EXPECT(!loaded.fullscreen);
	EXPECT_EQ_INT(loaded.high_scores[0], 0);
	EXPECT_EQ_INT(loaded.high_scores[MAX_START_SPEED], 7777);

	// Game tracks separate high scores per starting speed and restores all of them on restart
	Game g1;
	init_game(&g1);
	EXPECT(game_attach_config(&g1, path));
	EXPECT_EQ_INT(g1.start_speed, MAX_START_SPEED);
	EXPECT_EQ_INT(g1.high_score, 7777);
	EXPECT_EQ_INT(g1.start_music, MUSIC_SEL_OFF);
	EXPECT_EQ_INT(g1.music.song, SONG_NONE); // Respects saved Music OFF on title

	// Score on speed 5, then score separately on speed 2
	g1.title_focus = TITLE_FOCUS_SPEED;
	title_set_value(&g1, 5);
	EXPECT_EQ_INT(g1.high_score, 0); // Speed 5 has no best yet
	g1.game_score = 8800;
	update_high_score(&g1);
	EXPECT_EQ_INT(get_high_score_for_speed(&g1, 5), 8800);
	EXPECT_EQ_INT(get_high_score_for_speed(&g1, 9), 7777);
	EXPECT_EQ_INT(get_overall_high_score(&g1), 8800);

	title_set_value(&g1, 2);
	EXPECT_EQ_INT(g1.high_score, 0); // Switching speed updates active high_score
	g1.game_score = 3100;
	update_high_score(&g1);
	EXPECT_EQ_INT(get_high_score_for_speed(&g1, 2), 3100);

	// Switch back to speed 5, change height/music/mute
	title_set_value(&g1, 5);
	EXPECT_EQ_INT(g1.high_score, 8800);
	g1.title_focus = TITLE_FOCUS_HEIGHT;
	title_set_value(&g1, 2);
	g1.title_focus = TITLE_FOCUS_MUSIC;
	title_set_value(&g1, 2); // Tetra March
	toggle_music_mute(&g1);

	// Simulate app restart with a brand-new Game instance
	Game g2;
	init_game(&g2);
	EXPECT(game_attach_config(&g2, path));
	EXPECT_EQ_INT(g2.start_speed, 5);
	EXPECT_EQ_INT(g2.start_height, 2);
	EXPECT_EQ_INT(g2.start_music, MUSIC_SEL_2);
	EXPECT(g2.music.muted);
	EXPECT(!g2.fullscreen);
	EXPECT_EQ_INT(g2.high_score, 8800);
	EXPECT_EQ_INT(get_high_score_for_speed(&g2, 2), 3100);
	EXPECT_EQ_INT(get_high_score_for_speed(&g2, 5), 8800);
	EXPECT_EQ_INT(get_high_score_for_speed(&g2, 9), 7777);
	EXPECT_EQ_INT(get_overall_high_score(&g2), 8800);
	EXPECT_EQ_INT(g2.music.song, SONG_TITLE);

	remove(path);
}

static void test_sfx_balance_above_background_music(void) {
	int16_t buf[MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS];
	Music_Engine m;

	// Measure RMS of background music (Tetra March)
	music_init(&m);
	music_play_song(&m, SONG_TETRA_MARCH);
	double bgm_sq = 0.0;
	int bgm_samples = 0;
	for (int c = 0; c < 60; c++) {
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
		for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
			bgm_sq += (double)buf[i] * (double)buf[i];
			bgm_samples++;
		}
	}
	double bgm_rms = sqrt(bgm_sq / (double)bgm_samples);

	// Measure RMS of SFX_TETRIS over its active body (~0.5s)
	music_init(&m);
	music_play_sfx(&m, SFX_TETRIS);
	double sfx_sq = 0.0;
	int sfx_samples = 0;
	for (int c = 0; c < 22; c++) {
		music_render(&m, buf, MUSIC_BUFFER_FRAMES);
		for (int i = 0; i < MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS; i++) {
			sfx_sq += (double)buf[i] * (double)buf[i];
			sfx_samples++;
		}
	}
	double sfx_rms = sqrt(sfx_sq / (double)sfx_samples);

	EXPECT(bgm_rms > 1500.0);          // Music remains clearly audible
	EXPECT(sfx_rms > bgm_rms * 1.15);  // SFX clearly cut through above background music
}

#define RUN_TEST(fn)                                                    \
	do {                                                                \
		current_test_failed = 0;                                        \
		tests_run++;                                                    \
		fn();                                                           \
		if (current_test_failed) {                                      \
			fprintf(stderr, "[FAIL] %s\n", #fn);                        \
			tests_failed++;                                             \
		} else {                                                        \
			printf("[PASS] %s\n", #fn);                                 \
		}                                                               \
	} while (0)

int main(void) {
	srand(12345u);

	RUN_TEST(test_ghost_empty_board_and_stack);
	RUN_TEST(test_ghost_never_desyncs_on_failed_rotation);
	RUN_TEST(test_ghost_empty_id_no_infinite_loop);
	RUN_TEST(test_tetromino_shapes_and_j_l_distinct);
	RUN_TEST(test_srs_rotation_roundtrip_and_wall_kick);
	RUN_TEST(test_seven_bag_randomizer);
	RUN_TEST(test_soft_drop_and_hard_drop_scoring);
	RUN_TEST(test_hold_piece_mechanics);
	RUN_TEST(test_line_clear_level_carryover_and_b2b);
	RUN_TEST(test_tspin_detection);
	RUN_TEST(test_lock_delay_and_game_over_restart);
	RUN_TEST(test_level_time_safe_and_monotonic);
	RUN_TEST(test_title_screen_defaults_and_starting_height_speed);
	RUN_TEST(test_cli_args_and_viewport_scaling);
	RUN_TEST(test_title_mouse_selection_and_start);
	RUN_TEST(test_generative_backdrop_styles_and_bounds);
	RUN_TEST(test_generative_backdrop_game_lifecycle);
	RUN_TEST(test_music_song_step_alignment_and_synthesis);
	RUN_TEST(test_music_controls_pause_mute_and_danger_tempo);
	RUN_TEST(test_music_game_lifecycle_and_title_selection);
	RUN_TEST(test_sfx_synthesis_distinct_line_clear_and_tetris);
	RUN_TEST(test_sfx_gameplay_triggers);
	RUN_TEST(test_config_save_load_and_game_persistence);
	RUN_TEST(test_sfx_balance_above_background_music);

	printf("\nFinished %d tests (%d failed).\n", tests_run, tests_failed);
	return (tests_failed == 0) ? 0 : 1;
}
