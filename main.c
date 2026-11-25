#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv) {
	const char *prog = (argc > 0 && argv[0]) ? argv[0] : "texas";
	Launch_Options opts = {0};
	bool show_help = false;
	const char *err = NULL;

	parse_cli_args(argc > 1 ? argc - 1 : 0, (const char *const *)(argc > 1 ? argv + 1 : NULL), &opts, &show_help, &err);
	if (show_help) {
		print_usage(prog);
		return 0;
	}
	if (err != NULL) {
		fprintf(stderr, "Unknown option: %s\n\n", err);
		print_usage(prog);
		return 1;
	}

	srand((unsigned int)time(NULL));

	SetTraceLogLevel(LOG_WARNING);
	// FLAG_WINDOW_HIGHDPI keeps GetScreenWidth/Height in logical points on Retina displays.
	// Without it, raylib on macOS reports the framebuffer size (2x) after a resize,
	// so the viewport scale doubles and the windowed UI renders cropped/oversized.
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
	InitWindow(BASE_WIDTH, BASE_HEIGHT, "Texas");
	SetWindowMinSize(BASE_WIDTH / 2, BASE_HEIGHT / 2);
	SetTargetFPS(120);
	InitAudioDevice();

	Game game;
	init_game(&game);

	char cfg_path[512];
	if (config_default_path(cfg_path, sizeof(cfg_path))) {
		game_attach_config(&game, cfg_path);
	}

	if (opts.muted_set) {
		music_set_muted(&game.music, opts.muted);
	}
	music_open_audio(&game.music);

	bool start_fullscreen = opts.fullscreen_set ? opts.fullscreen : game.fullscreen;
	if (start_fullscreen) {
		toggle_fullscreen(&game);
	} else {
		game.fullscreen = false;
		center_windowed_frame();
	}

	while (!WindowShouldClose()) {
		double frame_dt = (double)GetFrameTime();
		if (frame_dt > 0.1) {
			frame_dt = 0.1;
		}
		handle_input(&game, frame_dt);
		update_game(&game, frame_dt);
		music_update_audio(&game.music);
		draw_game(&game);
	}

	game_save_config(&game);
	music_close_audio(&game.music);
	CloseAudioDevice();
	CloseWindow();
	return 0;
}
