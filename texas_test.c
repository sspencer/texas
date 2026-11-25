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

	printf("\nFinished %d tests (%d failed).\n", tests_run, tests_failed);
	return (tests_failed == 0) ? 0 : 1;
}
