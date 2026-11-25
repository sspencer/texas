#ifndef TEXAS_MUSIC_H
#define TEXAS_MUSIC_H

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

#define MUSIC_SAMPLE_RATE    44100
#define MUSIC_CHANNELS       2
#define MUSIC_BUFFER_FRAMES  1024
#define MUSIC_MAX_SFX_VOICES 6

// Selectable gameplay background tracks (NES Tetris style: 1, 2, 3, Off).
typedef enum Music_Selection {
	MUSIC_SEL_1 = 0, // Sugar Plum (Tchaikovsky, NES 2A03 arrangement)
	MUSIC_SEL_2,     // Tetra March (Original baroque/arcade chiptune)
	MUSIC_SEL_3,     // Korobeiniki (Traditional Russian folk, NES 2A03 arrangement)
	MUSIC_SEL_OFF,   // Silent
	MUSIC_SEL_COUNT,
} Music_Selection;

// Internal playable songs managed by the chiptune synthesizer.
typedef enum Music_Song {
	SONG_NONE = 0,
	SONG_TITLE,
	SONG_SUGAR_PLUM,
	SONG_TETRA_MARCH,
	SONG_KOROBEINIKI,
	SONG_GAME_OVER,
	SONG_COUNT,
} Music_Song;

// NES-style procedural sound effects mixed alongside the soundtrack.
typedef enum Music_Sfx {
	SFX_NONE = 0,
	SFX_MOVE,       // Subtle horizontal shift tick
	SFX_ROTATE,     // Crisp 2-step rotation blip
	SFX_LOCK,       // Piece landing / lock thud
	SFX_HARD_DROP,  // Punchy hard-drop slam
	SFX_HOLD,       // Warm swap chime
	SFX_LINE_CLEAR, // 1-3 rows cleared sweep + ascending arpeggio
	SFX_TETRIS,     // 4 rows cleared triumphant NES fanfare
	SFX_TSPIN,      // T-Spin harmonic shimmer
	SFX_LEVEL_UP,   // Level increment fanfare
	SFX_PAUSE,      // Classic NES pause chime
	SFX_MENU_MOVE,  // Title screen selection tick
	SFX_MENU_START, // Game start confirmation chime
	SFX_COUNTDOWN,  // Restart countdown beep
	SFX_COUNT,
} Music_Sfx;

typedef struct Music_Sfx_Voice {
	Music_Sfx kind;
	bool      active;
	double    age;
	double    duration;
	double    phase1;
	double    phase2;
	uint16_t  lfsr;
	double    lfsr_phase;
	float     noise_val;
} Music_Sfx_Voice;

typedef struct Music_Pulse_Voice {
	double  phase;
	double  freq;
	float   duty;
	float   vel;
	float   env;
	float   sustain;
	float   decay_rate;
	double  note_age;
	double  gate_time;
	bool    active;
	bool    releasing;
	bool    vibrato;
	int     ev_index;
	int     steps_left;
} Music_Pulse_Voice;

typedef struct Music_Tri_Voice {
	double phase;
	double freq;
	float  env;
	double note_age;
	double gate_time;
	bool   active;
	bool   releasing;
	float  lp_state;
	int    ev_index;
	int    steps_left;
} Music_Tri_Voice;

typedef struct Music_Drum_Voice {
	uint16_t lfsr;
	double   lfsr_phase;
	double   lfsr_rate;
	float    noise_val;
	float    noise_env;
	float    noise_decay;
	float    hp_prev_in;
	float    hp_prev_out;
	bool     use_hp;
	double   tone_phase;
	double   tone_freq;
	double   tone_end_freq;
	double   tone_sweep_rate;
	float    tone_env;
	float    tone_decay;
	int      ev_index;
	int      steps_left;
} Music_Drum_Voice;

typedef struct Music_Engine {
	bool              initialized;
	bool              audio_open;
	AudioStream       stream;
	int16_t           mix_buf[MUSIC_BUFFER_FRAMES * MUSIC_CHANNELS];

	Music_Song        song;
	bool              looping;
	bool              finished;
	bool              paused;
	bool              muted;
	bool              danger;
	float             tempo_scale;        // Smoothly tracks 1.0f (normal) or ~1.28f (danger)
	float             target_tempo_scale;
	float             master_volume;
	float             song_gain;          // Gentle gain trim (e.g. softer on title screen)
	float             music_env;          // Smooth pause/song fade for music bus
	float             mute_env;           // Smooth master mute fade (silences music + SFX)
	float             duck_env;           // Gentle music ducking during prominent SFX

	double            step_accum;         // Samples accumulated toward next 16th-note step
	int               current_step;       // Current 16th-note step in song

	Music_Pulse_Voice pulse1;
	Music_Pulse_Voice pulse2;
	Music_Tri_Voice   tri;
	Music_Drum_Voice  drum;

	Music_Sfx_Voice   sfx[MUSIC_MAX_SFX_VOICES];
	Music_Sfx         last_sfx;           // Most recently triggered SFX (useful for testing/UI)
	uint32_t          sfx_trigger_count;  // Monotonic counter of triggered SFX

	// Master NES-style analog output conditioning filters (per stereo channel)
	float             dc_x[2];
	float             dc_y[2];
	float             lp_y[2];
} Music_Engine;

// Pure-C synthesizer & sequencer lifecycle (safe to call without an audio device)
void        music_init(Music_Engine *m);
void        music_play_song(Music_Engine *m, Music_Song song);
void        music_stop(Music_Engine *m);
void        music_play_sfx(Music_Engine *m, Music_Sfx sfx);
void        music_clear_sfx(Music_Engine *m);
void        music_set_paused(Music_Engine *m, bool paused);
void        music_set_muted(Music_Engine *m, bool muted);
void        music_set_danger(Music_Engine *m, bool danger);
void        music_set_volume(Music_Engine *m, float volume);
void        music_render(Music_Engine *m, int16_t *out_stereo, int frames);

// Helpers for UI & track metadata
const char *music_selection_short_label(Music_Selection sel);
const char *music_selection_name(Music_Selection sel);
Music_Song  music_song_for_selection(Music_Selection sel);
int         music_song_total_steps(Music_Song song);
bool        music_validate_song_lengths(Music_Song song);

// Raylib AudioStream bridge (called from main.c when audio device is active)
void        music_open_audio(Music_Engine *m);
void        music_update_audio(Music_Engine *m);
void        music_close_audio(Music_Engine *m);

#endif // TEXAS_MUSIC_H
