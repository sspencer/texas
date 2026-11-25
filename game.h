#ifndef TEXAS_GAME_H
#define TEXAS_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

#include "backdrop.h"
#include "config.h"
#include "music.h"
#include "tetromino.h"

#define GAME_WIDTH   10
#define GAME_HEIGHT  20
#define HIDDEN_ROWS  4
#define BOARD_HEIGHT (GAME_HEIGHT + HIDDEN_ROWS)

#define OUTER_MARGIN 24
#define BLOCK_SIZE   30
#define BORDER_SIZE  8
#define PANEL_GAP    20
#define PANEL_WIDTH  252
#define BOARD_PX_W   (GAME_WIDTH * BLOCK_SIZE)
#define BOARD_PX_H   (GAME_HEIGHT * BLOCK_SIZE)
#define BORDER_LEFT  (OUTER_MARGIN + BORDER_SIZE)
#define BORDER_TOP   (OUTER_MARGIN + BORDER_SIZE)
#define BASE_WIDTH   (OUTER_MARGIN + BORDER_SIZE + BOARD_PX_W + BORDER_SIZE + PANEL_GAP + PANEL_WIDTH + OUTER_MARGIN)
#define BASE_HEIGHT  (OUTER_MARGIN + BORDER_SIZE + BOARD_PX_H + BORDER_SIZE + OUTER_MARGIN)

#define NEXT_QUEUE_SIZE        3
#define MAX_START_SPEED        9
#define MAX_START_HEIGHT       5
#define MAX_GRAVITY_LEVEL      19
#define LOCK_DELAY             0.50
#define MAX_LOCK_RESETS        15
#define LINE_CLEAR_DURATION    0.22
#define DAS_DELAY              0.15
#define ARR_INTERVAL           0.04
#define SOFT_DROP_INTERVAL     0.045
#define GATE_DURATION          0.60
#define GAME_LAUNCH_TIME       4.0
#define ACTION_BANNER_DURATION 1.5

extern const Color BACKGROUND;
extern const Color BORDER_COLOR;
extern const Color PANEL_BG;
extern const Color GRID_COLOR;
extern const Color TEXT_MUTED;
extern const int HEIGHT_ROWS[MAX_START_HEIGHT + 1];

typedef enum Game_State {
	STATE_TITLE = 0,
	STATE_PLAYING,
	STATE_LINE_CLEAR,
	STATE_PAUSED,
	STATE_GAME_OVER,
	STATE_COUNTDOWN,
} Game_State;

typedef enum Title_Focus {
	TITLE_FOCUS_SPEED = 0,
	TITLE_FOCUS_HEIGHT,
	TITLE_FOCUS_MUSIC,
} Title_Focus;

typedef enum T_Spin_Kind {
	TSPIN_NONE = 0,
	TSPIN_MINI,
	TSPIN_FULL,
} T_Spin_Kind;

typedef struct Vec2i {
	int x;
	int y;
} Vec2i;

typedef struct Launch_Options {
	bool fullscreen;
	bool fullscreen_set;
	bool muted;
	bool muted_set;
} Launch_Options;

typedef struct Title_Layout {
	Rectangle card_outer;
	Rectangle card_inner;
	float     grid_x;
	float     grid_right;
	float     div_y;
	float     div2_y;
	Rectangle speed_rects[MAX_START_SPEED + 1];
	Rectangle height_rects[MAX_START_HEIGHT + 1];
	Rectangle music_rects[MUSIC_SEL_COUNT];
	Rectangle start_rect;
} Title_Layout;

struct Game {
	Game_State      state;
	Title_Focus     title_focus;
	int             title_hover_speed;
	int             title_hover_height;
	int             title_hover_music;
	bool            title_hover_start;
	int             start_speed;
	int             start_height;
	Music_Selection start_music;
	bool            fullscreen;
	int             level;
	int             total_lines;
	int             game_score;
	int             high_score;                      // High score for the currently selected start_speed
	int             high_scores[MAX_START_SPEED + 1]; // Per-starting-speed (0..9) high scores
	int             combo;
	bool            back_to_back;
	int32_t         board[BOARD_HEIGHT][GAME_WIDTH];
	Piece_Bag       bag;
	Tetromino       tetro;
	Tetromino       next_queue[NEXT_QUEUE_SIZE];
	Tetromino       hold_tetro;
	bool            hold_used;
	Tetromino       ghost;
	double          gravity_accum;
	double          lock_timer;
	int             lock_resets;
	int             lowest_y;
	bool            on_ground;
	bool            last_action_rotate;
	int             last_kick_index;
	int             clear_rows[4];
	int             clear_row_count;
	double          clear_timer;
	T_Spin_Kind     pending_tspin;
	double          state_timer;
	char            banner_buf[64];
	int             banner_len;
	char            banner_sub_buf[64];
	int             banner_sub_len;
	double          banner_timer;
	int             horiz_dir;
	double          horiz_hold_time;
	double          horiz_repeat_accum;
	double          soft_drop_accum;
	Backdrop        backdrop;
	Music_Engine    music;
	bool            music_danger;
	char            config_path[512];
	bool            debug;
};

// CLI & Window helpers
void         parse_cli_args(int argc, const char *const *argv, Launch_Options *opts, bool *show_help, const char **err);
void         print_usage(const char *prog);
void         compute_windowed_size(int monitor_w, int monitor_h, int *out_w, int *out_h);
void         center_windowed_frame(void);
void         toggle_fullscreen(Game *g);
void         compute_viewport(float screen_w, float screen_h, float *out_scale, float *out_offset_x, float *out_offset_y);
Vector2      screen_to_ui(Vector2 screen_pos, float screen_w, float screen_h);
bool         point_in_rect(Vector2 p, Rectangle r);
Title_Layout compute_title_layout(void);

// Game lifecycle & mechanics
void        init_game(Game *g);
void        return_to_title(Game *g);
void        title_adjust_value(Game *g, int delta);
void        title_set_value(Game *g, int value);
void        fill_starting_height(Game *g, int height);
void        start_new_game(Game *g);
void        reset_piece_timers(Game *g);
void        spawn_next_tetro(Game *g);
void        set_game_over(Game *g);
void        begin_restart_countdown(Game *g);
void        update_high_score(Game *g);
int         get_high_score_for_speed(const Game *g, int speed);
int         get_overall_high_score(const Game *g);
void        set_banner(Game *g, const char *title, const char *sub);
void        update_ghost(Game *g);
bool        is_grounded(const Game *g);
void        on_piece_manipulated(Game *g);
bool        move_tetro(Game *g, Vec2i dir);
bool        soft_drop_step(Game *g);
int         hard_drop(Game *g);
bool        hold_piece(Game *g);
T_Spin_Kind detect_tspin(const Game *g);
void        lock_current_piece(Game *g);
int         find_full_rows(Game *g);
void        finish_line_clear(Game *g);
int         compute_score(int level, int lines, T_Spin_Kind tspin, bool b2b);
bool        update_title_pointer(Game *g, Vector2 ui_pos, bool clicked);
void        handle_input(Game *g, double frame_dt);
void        update_game(Game *g, double frame_dt);
bool        collide(const Game *g, Tetromino t);
void        clear_board(Game *g);
int         clear_lines(Game *g);
double      level_time(int level);
void        toggle_music_mute(Game *g);
void        cycle_music_selection(Game *g, int delta);
bool        is_board_in_danger(const Game *g);
bool        game_attach_config(Game *g, const char *path);
void        game_save_config(const Game *g);

// Rendering
void        draw_game(const Game *g);

#endif // TEXAS_GAME_H
