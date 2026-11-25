#ifndef TEXAS_CONFIG_H
#define TEXAS_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "music.h"

#define CONFIG_SPEED_COUNT 10

typedef struct Texas_Config {
	int             start_speed;
	int             start_height;
	Music_Selection start_music;
	bool            muted;
	bool            fullscreen;
	int             high_scores[CONFIG_SPEED_COUNT];
} Texas_Config;

// Populates cfg with clean default values.
void config_set_defaults(Texas_Config *cfg);

// Resolves the default user config file path (macOS: ~/Library/Application Support/Texas/config.ini,
// POSIX: $XDG_CONFIG_HOME/texas/config.ini or ~/.config/texas/config.ini).
bool config_default_path(char *out_path, size_t out_size);

// Loads settings from path into cfg (starting from defaults and clamping out-of-range values).
// Returns true if the file existed and was parsed, false if missing/unreadable (cfg stays defaults).
bool config_load(const char *path, Texas_Config *cfg);

// Saves settings to path, creating parent directories as needed and writing atomically.
bool config_save(const char *path, const Texas_Config *cfg);

#endif // TEXAS_CONFIG_H
