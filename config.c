#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CFG_MAX_SPEED  (CONFIG_SPEED_COUNT - 1)
#define CFG_MAX_HEIGHT 5

static int clamp_int(int v, int lo, int hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static char *trim_inplace(char *s) {
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	size_t len = strlen(s);
	while (len > 0 && isspace((unsigned char)s[len - 1])) {
		s[--len] = '\0';
	}
	return s;
}

static bool eq_nocase(const char *a, const char *b) {
	while (*a && *b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
			return false;
		}
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static bool parse_bool(const char *val, bool *out) {
	if (eq_nocase(val, "true") || eq_nocase(val, "1") || eq_nocase(val, "yes") || eq_nocase(val, "on")) {
		*out = true;
		return true;
	}
	if (eq_nocase(val, "false") || eq_nocase(val, "0") || eq_nocase(val, "no") || eq_nocase(val, "off")) {
		*out = false;
		return true;
	}
	return false;
}

static bool parse_int(const char *val, int *out) {
	if (!val || *val == '\0') {
		return false;
	}
	char *end = NULL;
	errno = 0;
	long parsed = strtol(val, &end, 10);
	if (errno != 0 || end == val || *end != '\0') {
		return false;
	}
	if (parsed < -2147483647L || parsed > 2147483647L) {
		return false;
	}
	*out = (int)parsed;
	return true;
}

static bool parse_music_selection(const char *val, Music_Selection *out) {
	if (eq_nocase(val, "1") || eq_nocase(val, "sugar_plum") || eq_nocase(val, "sugar plum")) {
		*out = MUSIC_SEL_1;
		return true;
	}
	if (eq_nocase(val, "2") || eq_nocase(val, "tetra_march") || eq_nocase(val, "tetra march")) {
		*out = MUSIC_SEL_2;
		return true;
	}
	if (eq_nocase(val, "3") || eq_nocase(val, "korobeiniki")) {
		*out = MUSIC_SEL_3;
		return true;
	}
	if (eq_nocase(val, "off") || eq_nocase(val, "0") || eq_nocase(val, "none")) {
		*out = MUSIC_SEL_OFF;
		return true;
	}
	return false;
}

static const char *music_selection_config_value(Music_Selection sel) {
	switch (sel) {
	case MUSIC_SEL_1:   return "1";
	case MUSIC_SEL_2:   return "2";
	case MUSIC_SEL_3:   return "3";
	case MUSIC_SEL_OFF: return "off";
	case MUSIC_SEL_COUNT: break;
	}
	return "1";
}

static bool ensure_parent_dirs(const char *filepath) {
	char buf[1024];
	size_t len = strlen(filepath);
	if (len == 0 || len >= sizeof(buf)) {
		return false;
	}
	memcpy(buf, filepath, len + 1);

	for (size_t i = 1; i < len; i++) {
		if (buf[i] == '/') {
			buf[i] = '\0';
			if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
				return false;
			}
			buf[i] = '/';
		}
	}
	return true;
}

void config_set_defaults(Texas_Config *cfg) {
	if (!cfg) {
		return;
	}
	cfg->start_speed = 0;
	cfg->start_height = 0;
	cfg->start_music = MUSIC_SEL_1;
	cfg->muted = false;
	cfg->fullscreen = true;
	memset(cfg->high_scores, 0, sizeof(cfg->high_scores));
}

bool config_default_path(char *out_path, size_t out_size) {
	if (!out_path || out_size < 32) {
		return false;
	}
	out_path[0] = '\0';

	const char *home = getenv("HOME");
#ifdef __APPLE__
	if (!home || home[0] == '\0') {
		return false;
	}
	int n = snprintf(out_path, out_size, "%s/Library/Application Support/Texas/config.ini", home);
	return n > 0 && (size_t)n < out_size;
#else
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && xdg[0] != '\0') {
		int n = snprintf(out_path, out_size, "%s/texas/config.ini", xdg);
		return n > 0 && (size_t)n < out_size;
	}
	if (!home || home[0] == '\0') {
		return false;
	}
	int n = snprintf(out_path, out_size, "%s/.config/texas/config.ini", home);
	return n > 0 && (size_t)n < out_size;
#endif
}

bool config_load(const char *path, Texas_Config *cfg) {
	if (!cfg) {
		return false;
	}
	config_set_defaults(cfg);

	if (!path || path[0] == '\0') {
		return false;
	}

	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}

	int legacy_high_score = -1;
	char line[256];
	while (fgets(line, sizeof(line), f) != NULL) {
		char *s = trim_inplace(line);
		if (*s == '\0' || *s == '#' || *s == ';') {
			continue;
		}
		char *eq = strchr(s, '=');
		if (!eq) {
			continue;
		}
		*eq = '\0';
		char *key = trim_inplace(s);
		char *val = trim_inplace(eq + 1);

		if (eq_nocase(key, "speed") || eq_nocase(key, "level")) {
			int v = 0;
			if (parse_int(val, &v)) {
				cfg->start_speed = clamp_int(v, 0, CFG_MAX_SPEED);
			}
		} else if (eq_nocase(key, "height")) {
			int v = 0;
			if (parse_int(val, &v)) {
				cfg->start_height = clamp_int(v, 0, CFG_MAX_HEIGHT);
			}
		} else if (eq_nocase(key, "music")) {
			Music_Selection sel = MUSIC_SEL_1;
			if (parse_music_selection(val, &sel)) {
				cfg->start_music = sel;
			}
		} else if (eq_nocase(key, "muted") || eq_nocase(key, "mute")) {
			bool b = false;
			if (parse_bool(val, &b)) {
				cfg->muted = b;
			}
		} else if (eq_nocase(key, "fullscreen")) {
			bool b = true;
			if (parse_bool(val, &b)) {
				cfg->fullscreen = b;
			}
		} else if (eq_nocase(key, "high_score")) {
			int v = 0;
			if (parse_int(val, &v)) {
				legacy_high_score = (v > 0) ? v : 0;
			}
		} else if (strncmp(key, "high_score_", 11) == 0) {
			int idx = -1;
			int v = 0;
			if (parse_int(key + 11, &idx) && idx >= 0 && idx < CONFIG_SPEED_COUNT && parse_int(val, &v)) {
				cfg->high_scores[idx] = (v > 0) ? v : 0;
			}
		}
	}

	if (legacy_high_score > 0 && cfg->high_scores[cfg->start_speed] == 0) {
		cfg->high_scores[cfg->start_speed] = legacy_high_score;
	}

	fclose(f);
	return true;
}

bool config_save(const char *path, const Texas_Config *cfg) {
	if (!path || path[0] == '\0' || !cfg) {
		return false;
	}
	if (!ensure_parent_dirs(path)) {
		return false;
	}

	char tmp_path[1040];
	int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
	if (n <= 0 || (size_t)n >= sizeof(tmp_path)) {
		return false;
	}

	FILE *f = fopen(tmp_path, "w");
	if (!f) {
		return false;
	}

	int ok = fprintf(
		f,
		"# Texas settings\n"
		"speed = %d\n"
		"height = %d\n"
		"music = %s\n"
		"muted = %s\n"
		"fullscreen = %s\n",
		clamp_int(cfg->start_speed, 0, CFG_MAX_SPEED),
		clamp_int(cfg->start_height, 0, CFG_MAX_HEIGHT),
		music_selection_config_value(cfg->start_music),
		cfg->muted ? "true" : "false",
		cfg->fullscreen ? "true" : "false"
	);

	for (int i = 0; i < CONFIG_SPEED_COUNT && ok >= 0; i++) {
		int hs = (cfg->high_scores[i] > 0) ? cfg->high_scores[i] : 0;
		ok = fprintf(f, "high_score_%d = %d\n", i, hs);
	}

	if (fflush(f) != 0 || fclose(f) != 0 || ok < 0) {
		remove(tmp_path);
		return false;
	}

	if (rename(tmp_path, path) != 0) {
		remove(tmp_path);
		return false;
	}
	return true;
}
