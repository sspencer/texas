#include "music.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// Note & Drum Constants
// =============================================================================

#define R 0

enum {
	N_C2 = 36, N_CS2, N_D2, N_DS2, N_E2, N_F2, N_FS2, N_G2, N_GS2, N_A2, N_AS2, N_B2,
	N_C3 = 48, N_CS3, N_D3, N_DS3, N_E3, N_F3, N_FS3, N_G3, N_GS3, N_A3, N_AS3, N_B3,
	N_C4 = 60, N_CS4, N_D4, N_DS4, N_E4, N_F4, N_FS4, N_G4, N_GS4, N_A4, N_AS4, N_B4,
	N_C5 = 72, N_CS5, N_D5, N_DS5, N_E5, N_F5, N_FS5, N_G5, N_GS5, N_A5, N_AS5, N_B5,
	N_C6 = 84, N_CS6, N_D6, N_DS6, N_E6, N_F6, N_FS6, N_G6, N_GS6, N_A6, N_AS6, N_B6,
};

typedef enum Drum_Hit {
	D_NONE = 0,
	D_KICK,
	D_SNARE,
	D_HAT,
	D_OHAT,
	D_RIM,
} Drum_Hit;

typedef struct Music_Event {
	uint8_t note;     // MIDI pitch (1..127), Drum_Hit for drums, or 0 (R) for rest
	uint8_t steps;    // Duration in 16th-note steps (1 = 16th, 2 = 8th, 4 = quarter, 8 = half)
	uint8_t vel;      // 0 = channel default, else 1..15 (NES 4-bit volume scale)
	uint8_t gate_pct; // 0 = channel default, else 10..100 (% of step duration held before release)
} Music_Event;

typedef struct Song_Def {
	const char        *name;
	float              bpm;
	bool               loop;
	float              gain;
	// Pulse 1 timbre
	float              p1_duty;
	float              p1_sustain;
	float              p1_decay;
	uint8_t            p1_default_gate;
	bool               p1_vibrato;
	// Pulse 2 timbre
	float              p2_duty;
	float              p2_sustain;
	float              p2_decay;
	uint8_t            p2_default_gate;
	bool               p2_vibrato;
	// Triangle timbre
	uint8_t            tri_default_gate;
	// Channel mix gains
	float              p1_gain;
	float              p2_gain;
	float              tri_gain;
	float              drum_gain;
	// Event streams
	const Music_Event *p1;
	int                p1_count;
	const Music_Event *p2;
	int                p2_count;
	const Music_Event *tri;
	int                tri_count;
	const Music_Event *drums;
	int                drums_count;
	int                total_steps;
} Song_Def;

#define ARR_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

// Shorthand event constructors
#define N(pitch, len)                {(uint8_t)(pitch), (uint8_t)(len), 0, 0}
#define NV(pitch, len, vel)          {(uint8_t)(pitch), (uint8_t)(len), (uint8_t)(vel), 0}
#define NG(pitch, len, gate)         {(uint8_t)(pitch), (uint8_t)(len), 0, (uint8_t)(gate)}
#define NVG(pitch, len, vel, gate)   {(uint8_t)(pitch), (uint8_t)(len), (uint8_t)(vel), (uint8_t)(gate)}
#define RST(len)                     {0, (uint8_t)(len), 0, 0}
#define DR(hit, len)                 {(uint8_t)(hit), (uint8_t)(len), 0, 0}
#define DRV(hit, len, vel)           {(uint8_t)(hit), (uint8_t)(len), (uint8_t)(vel), 0}

// =============================================================================
// SONG 1: TITLE SCREEN — "Texas Overture" (Original, C Major, 126 BPM, 128 steps)
// Bright, welcoming 8-bar NES menu loop.
// =============================================================================

static const Music_Event title_p1[] = {
	// Bar 1 (C): 16 steps
	N(N_E5, 2), N(N_G5, 2), N(N_C6, 4), N(N_B5, 2), N(N_A5, 2), N(N_G5, 4),
	// Bar 2 (F -> G): 16 steps
	N(N_A5, 2), N(N_F5, 2), N(N_C5, 2), N(N_A5, 2), N(N_G5, 4), RST(2), N(N_E5, 2),
	// Bar 3 (Am -> Em): 16 steps
	N(N_C5, 2), N(N_E5, 2), N(N_A5, 4), N(N_G5, 2), N(N_E5, 2), N(N_B4, 4),
	// Bar 4 (Dm -> G7): 16 steps
	N(N_D5, 2), N(N_F5, 2), N(N_A5, 2), N(N_G5, 2), N(N_F5, 2), N(N_E5, 2), N(N_D5, 4),
	// Bar 5 (C -> E7): 16 steps
	N(N_E5, 2), N(N_G5, 2), N(N_C6, 4), N(N_D6, 2), N(N_C6, 2), N(N_B5, 4),
	// Bar 6 (Am -> F): 16 steps
	N(N_C6, 2), N(N_A5, 2), N(N_E5, 2), N(N_A5, 2), N(N_F5, 4), N(N_A5, 2), N(N_G5, 2),
	// Bar 7 (Dm7 -> G7): 16 steps
	N(N_F5, 2), N(N_E5, 2), N(N_D5, 2), N(N_F5, 2), N(N_E5, 2), N(N_D5, 2), N(N_B4, 4),
	// Bar 8 (C cadence): 16 steps
	N(N_C5, 4), N(N_E5, 2), N(N_G5, 2), N(N_C6, 4), RST(4),
};

static const Music_Event title_p2[] = {
	// Bar 1 (C): crisp offbeat staccato harmony
	RST(2), N(N_C4, 2), RST(2), N(N_E4, 2), RST(2), N(N_G4, 2), RST(2), N(N_E4, 2),
	// Bar 2 (F -> G)
	RST(2), N(N_C4, 2), RST(2), N(N_F4, 2), RST(2), N(N_D4, 2), RST(2), N(N_B3, 2),
	// Bar 3 (Am -> Em)
	RST(2), N(N_A3, 2), RST(2), N(N_C4, 2), RST(2), N(N_B3, 2), RST(2), N(N_G3, 2),
	// Bar 4 (Dm -> G7)
	RST(2), N(N_A3, 2), RST(2), N(N_D4, 2), RST(2), N(N_B3, 2), N(N_G3, 2), N(N_B3, 2),
	// Bar 5 (C -> E7)
	RST(2), N(N_G4, 2), RST(2), N(N_E4, 2), RST(2), N(N_GS4, 2), RST(2), N(N_E4, 2),
	// Bar 6 (Am -> F)
	RST(2), N(N_E4, 2), RST(2), N(N_C4, 2), RST(2), N(N_C4, 2), RST(2), N(N_A3, 2),
	// Bar 7 (Dm7 -> G7)
	RST(2), N(N_A3, 2), RST(2), N(N_C4, 2), RST(2), N(N_B3, 2), RST(2), N(N_G3, 2),
	// Bar 8 (C cadence + pickup)
	N(N_G4, 4), N(N_C4, 2), N(N_E4, 2), N(N_G4, 4), N(N_G4, 2), N(N_F4, 2),
};

static const Music_Event title_tri[] = {
	// Bar 1 (C)
	N(N_C3, 4), N(N_G2, 4), N(N_C3, 4), N(N_E3, 4),
	// Bar 2 (F -> G)
	N(N_F3, 4), N(N_C3, 4), N(N_G2, 4), N(N_B2, 4),
	// Bar 3 (Am -> Em)
	N(N_A2, 4), N(N_E3, 4), N(N_E2, 4), N(N_B2, 4),
	// Bar 4 (Dm -> G7)
	N(N_D3, 4), N(N_A2, 4), N(N_G2, 4), N(N_B2, 4),
	// Bar 5 (C -> E7)
	N(N_C3, 4), N(N_G2, 4), N(N_E2, 4), N(N_GS2, 4),
	// Bar 6 (Am -> F)
	N(N_A2, 4), N(N_E3, 4), N(N_F2, 4), N(N_C3, 4),
	// Bar 7 (Dm7 -> G7)
	N(N_D3, 4), N(N_A2, 4), N(N_G2, 4), N(N_D3, 4),
	// Bar 8 (C)
	N(N_C3, 4), N(N_G2, 4), N(N_C3, 4), N(N_G2, 4),
};

static const Music_Event title_drums[] = {
	// Bars 1..3
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	// Bar 4 fill
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bars 5..7
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	// Bar 8 cadence
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_SNARE, 2),
};

// =============================================================================
// SONG 2: MUSIC 1 — "Sugar Plum"
// P. I. Tchaikovsky, Dance of the Sugar Plum Fairy (1892, Public Domain)
// NES 2A03 arrangement in E minor, 118 BPM, 32 bars of 2/4 = 256 steps.
// =============================================================================

static const Music_Event sugar_p1[] = {
	// Intro (4 bars of 2/4 = 32 steps): pizzicato vamp
	RST(8), RST(8), RST(8), RST(8),

	// Section A1 (Bars 5..8 = 32 steps): Iconic celesta theme (Em -> B7)
	RST(2), NG(N_G5, 1, 65), NG(N_E5, 1, 65), NG(N_G5, 2, 70), NG(N_FS5, 2, 70),
	NG(N_DS5, 2, 70), NG(N_E5, 2, 70), NG(N_D5, 1, 60), NG(N_D5, 1, 60), NG(N_D5, 2, 65),
	NG(N_CS5, 1, 60), NG(N_CS5, 1, 60), NG(N_CS5, 2, 65), NG(N_C5, 1, 60), NG(N_C5, 1, 60), NG(N_C5, 2, 65),
	NG(N_B4, 1, 65), NG(N_E5, 1, 65), NG(N_C5, 1, 65), NG(N_E5, 1, 65), NG(N_B4, 2, 70), RST(2),

	// Section A2 (Bars 9..12 = 32 steps): Modulation to B minor (Em -> F#7 -> Bm -> F#)
	RST(2), NG(N_G5, 1, 65), NG(N_E5, 1, 65), NG(N_G5, 2, 70), NG(N_FS5, 2, 70),
	NG(N_AS5, 2, 75), NG(N_B5, 2, 75), NG(N_A5, 1, 60), NG(N_A5, 1, 60), NG(N_A5, 2, 65),
	NG(N_GS5, 1, 60), NG(N_GS5, 1, 60), NG(N_GS5, 2, 65), NG(N_G5, 1, 60), NG(N_G5, 1, 60), NG(N_G5, 2, 65),
	NG(N_FS5, 1, 65), NG(N_B5, 1, 65), NG(N_G5, 1, 65), NG(N_B5, 1, 65), NG(N_FS5, 2, 70), RST(2),

	// Section B (Bars 13..20 = 64 steps): Sparkling G major -> Am -> B7 celesta dance
	RST(2), NG(N_B5, 1, 65), NG(N_G5, 1, 65), NG(N_B5, 2, 70), NG(N_A5, 2, 70),
	NG(N_FS5, 2, 70), NG(N_G5, 2, 70), NG(N_E5, 2, 65), NG(N_D5, 2, 65),
	NG(N_C5, 1, 70), NG(N_E5, 1, 70), NG(N_G5, 1, 70), NG(N_C6, 1, 75), NG(N_B5, 2, 70), NG(N_A5, 2, 70),
	NG(N_G5, 2, 70), NG(N_FS5, 2, 65), NG(N_G5, 2, 75), RST(2),

	RST(2), NG(N_A5, 1, 65), NG(N_F5, 1, 65), NG(N_A5, 2, 70), NG(N_G5, 2, 70),
	NG(N_E5, 2, 70), NG(N_FS5, 2, 70), NG(N_DS5, 2, 70), NG(N_B4, 2, 70),
	NG(N_E5, 1, 75), NG(N_FS5, 1, 75), NG(N_G5, 1, 75), NG(N_A5, 1, 75),
	NG(N_B5, 1, 75), NG(N_A5, 1, 75), NG(N_G5, 1, 75), NG(N_FS5, 1, 75),
	NG(N_G5, 1, 70), NG(N_FS5, 1, 70), NG(N_E5, 1, 70), NG(N_DS5, 1, 70), NG(N_B4, 2, 75), RST(2),

	// Section A1' (Bars 21..24 = 32 steps): Return of main theme
	RST(2), NG(N_G5, 1, 65), NG(N_E5, 1, 65), NG(N_G5, 2, 70), NG(N_FS5, 2, 70),
	NG(N_DS5, 2, 70), NG(N_E5, 2, 70), NG(N_D5, 1, 60), NG(N_D5, 1, 60), NG(N_D5, 2, 65),
	NG(N_CS5, 1, 60), NG(N_CS5, 1, 60), NG(N_CS5, 2, 65), NG(N_C5, 1, 60), NG(N_C5, 1, 60), NG(N_C5, 2, 65),
	NG(N_B4, 1, 65), NG(N_E5, 1, 65), NG(N_C5, 1, 65), NG(N_E5, 1, 65), NG(N_B4, 2, 70), RST(2),

	// Section A3 (Bars 25..28 = 32 steps): Climactic E minor resolution
	RST(2), NG(N_G5, 1, 65), NG(N_E5, 1, 65), NG(N_G5, 2, 70), NG(N_FS5, 2, 70),
	NG(N_B5, 2, 75), NG(N_G5, 2, 75), NG(N_A5, 1, 65), NG(N_G5, 1, 65), NG(N_FS5, 2, 70),
	NG(N_G5, 1, 75), NG(N_FS5, 1, 75), NG(N_E5, 1, 75), NG(N_DS5, 1, 75),
	NG(N_E5, 1, 75), NG(N_FS5, 1, 75), NG(N_G5, 1, 75), NG(N_A5, 1, 75),
	NG(N_B5, 2, 75), NG(N_DS5, 2, 75), NG(N_E5, 2, 80), RST(2),

	// Coda / Celesta Cascade (Bars 29..32 = 32 steps)
	NG(N_E6, 1, 65), NG(N_B5, 1, 65), NG(N_G5, 1, 65), NG(N_E5, 1, 65),
	NG(N_DS6, 1, 65), NG(N_B5, 1, 65), NG(N_FS5, 1, 65), NG(N_DS5, 1, 65),
	NG(N_E6, 1, 65), NG(N_B5, 1, 65), NG(N_G5, 1, 65), NG(N_E5, 1, 65),
	NG(N_C6, 1, 65), NG(N_A5, 1, 65), NG(N_E5, 1, 65), NG(N_C5, 1, 65),
	NG(N_B5, 2, 70), NG(N_G5, 2, 70), NG(N_FS5, 2, 70), NG(N_DS5, 2, 70),
	NG(N_E5, 2, 75), RST(2), NG(N_B4, 2, 65), RST(2),
};

static const Music_Event sugar_p2[] = {
	// Intro (Bars 1..4 = 32 steps): pizzicato offbeat chords
	RST(2), N(N_G3, 2), RST(2), N(N_B3, 2),
	RST(2), N(N_A3, 2), RST(2), N(N_FS3, 2),
	RST(2), N(N_G3, 2), RST(2), N(N_B3, 2),
	RST(2), N(N_FS3, 2), RST(2), N(N_DS3, 2),

	// Section A1 (Bars 5..8 = 32 steps): pizzicato chords + bass-clarinet answer in bar 8
	RST(2), N(N_G3, 2), RST(2), N(N_A3, 2),
	RST(2), N(N_G3, 2), RST(2), N(N_G3, 2),
	RST(2), N(N_G3, 2), RST(2), N(N_E3, 2),
	RST(2), N(N_G3, 2), N(N_DS3, 1), N(N_FS3, 1), N(N_B3, 1), N(N_FS3, 1),

	// Section A2 (Bars 9..12 = 32 steps)
	RST(2), N(N_G3, 2), RST(2), N(N_A3, 2),
	RST(2), N(N_D4, 2), RST(2), N(N_D4, 2),
	RST(2), N(N_D4, 2), RST(2), N(N_B3, 2),
	RST(2), N(N_D4, 2), N(N_AS3, 1), N(N_CS4, 1), N(N_FS4, 1), N(N_CS4, 1),

	// Section B (Bars 13..20 = 64 steps): G major -> C -> D7 -> G -> Am -> B7
	RST(2), N(N_D4, 2), RST(2), N(N_C4, 2),
	RST(2), N(N_B3, 2), RST(2), N(N_B3, 2),
	RST(2), N(N_G3, 2), RST(2), N(N_C4, 2),
	RST(2), N(N_A3, 2), N(N_B3, 2), N(N_D4, 2),

	RST(2), N(N_C4, 2), RST(2), N(N_B3, 2),
	RST(2), N(N_A3, 2), RST(2), N(N_FS3, 2),
	N(N_G3, 2), N(N_B3, 2), N(N_E4, 2), N(N_C4, 2),
	N(N_B3, 2), N(N_FS3, 2), N(N_DS3, 2), N(N_FS3, 2),

	// Section A1' (Bars 21..24 = 32 steps): richer staccato counterpoint
	N(N_B3, 2), N(N_G3, 2), N(N_B3, 2), N(N_A3, 2),
	N(N_FS3, 2), N(N_G3, 2), N(N_B3, 2), N(N_G3, 2),
	N(N_E3, 2), N(N_G3, 2), N(N_E3, 2), N(N_G3, 2),
	N(N_DS3, 2), N(N_G3, 2), N(N_DS3, 1), N(N_FS3, 1), N(N_B3, 1), N(N_FS3, 1),

	// Section A3 (Bars 25..28 = 32 steps)
	N(N_B3, 2), N(N_G3, 2), N(N_B3, 2), N(N_A3, 2),
	N(N_G4, 2), N(N_E4, 2), N(N_C4, 2), N(N_A3, 2),
	N(N_B3, 2), N(N_G3, 2), N(N_C4, 2), N(N_A3, 2),
	N(N_G3, 2), N(N_FS3, 2), N(N_G3, 2), N(N_B3, 2),

	// Coda (Bars 29..32 = 32 steps)
	N(N_E4, 2), N(N_G4, 2), N(N_DS4, 2), N(N_FS4, 2),
	N(N_E4, 2), N(N_G4, 2), N(N_C4, 2), N(N_E4, 2),
	N(N_G3, 2), N(N_B3, 2), N(N_A3, 2), N(N_FS3, 2),
	N(N_G3, 2), RST(2), N(N_DS3, 2), RST(2),
};

static const Music_Event sugar_tri[] = {
	// Intro (Bars 1..4 = 32 steps)
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),

	// Section A1 (Bars 5..8 = 32 steps): iconic descending chromatic bass E -> D -> C# -> C -> B
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),
	N(N_B2, 2), N(N_E2, 2), N(N_D3, 2), RST(2),
	N(N_CS3, 2), RST(2), N(N_C3, 2), RST(2),
	N(N_B2, 2), N(N_C3, 2), N(N_B2, 2), RST(2),

	// Section A2 (Bars 9..12 = 32 steps): Em -> F#7 -> Bm -> A -> G# -> G -> F#
	N(N_E2, 2), RST(2), N(N_FS2, 2), RST(2),
	N(N_FS2, 2), N(N_B2, 2), N(N_A2, 2), RST(2),
	N(N_GS2, 2), RST(2), N(N_G2, 2), RST(2),
	N(N_FS2, 2), N(N_G2, 2), N(N_FS2, 2), RST(2),

	// Section B (Bars 13..20 = 64 steps)
	N(N_G2, 2), RST(2), N(N_D3, 2), RST(2),
	N(N_D2, 2), RST(2), N(N_G2, 2), N(N_E2, 2),
	N(N_C3, 2), RST(2), N(N_D3, 2), RST(2),
	N(N_G2, 2), N(N_D2, 2), N(N_G2, 2), RST(2),

	N(N_A2, 2), RST(2), N(N_E2, 2), RST(2),
	N(N_C3, 2), RST(2), N(N_B2, 2), RST(2),
	N(N_E2, 2), N(N_G2, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_B2, 2), N(N_FS2, 2), N(N_B2, 2), RST(2),

	// Section A1' (Bars 21..24 = 32 steps)
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),
	N(N_B2, 2), N(N_E2, 2), N(N_D3, 2), RST(2),
	N(N_CS3, 2), RST(2), N(N_C3, 2), RST(2),
	N(N_B2, 2), N(N_C3, 2), N(N_B2, 2), RST(2),

	// Section A3 (Bars 25..28 = 32 steps)
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),
	N(N_E2, 2), N(N_G2, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_B2, 2), N(N_E2, 2), N(N_A2, 2), N(N_C3, 2),
	N(N_B2, 2), N(N_B2, 2), N(N_E2, 2), RST(2),

	// Coda (Bars 29..32 = 32 steps)
	N(N_E2, 4), N(N_B2, 4),
	N(N_E2, 4), N(N_C3, 4),
	N(N_B2, 4), N(N_B2, 4),
	N(N_E2, 2), RST(2), N(N_B2, 2), RST(2),
};

static const Music_Event sugar_drums[] = {
	// Light, crisp clockwork/fairy percussion across 32 bars of 2/4 (256 steps)
	// Bars 1..4 (Intro: delicate rim & hat tick-tock)
	DR(D_RIM, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_RIM, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_RIM, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_RIM, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_SNARE, 2),
	// Bars 5..8 (A1)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	// Bars 9..12 (A2)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bars 13..20 (B)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_RIM, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bars 21..24 (A1')
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	// Bars 25..28 (A3)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_KICK, 2), DR(D_HAT, 2),
	// Bars 29..32 (Coda)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_RIM, 2), DR(D_HAT, 2),
};

// =============================================================================
// SONG 3: MUSIC 2 — "Tetra March"
// Original NES-style driving baroque/arcade chiptune in A minor (138 BPM, 256 steps)
// Inspired by the energetic polyphonic spirit of NES Tetris Music 2.
// =============================================================================

static const Music_Event march_p1[] = {
	// Section A (Bars 1..4 = 64 steps)
	N(N_A4, 2), N(N_C5, 2), N(N_E5, 4), N(N_D5, 2), N(N_C5, 2), N(N_B4, 2), N(N_A4, 2),
	N(N_F5, 4), N(N_E5, 2), N(N_D5, 2), N(N_E5, 2), N(N_D5, 2), N(N_C5, 2), N(N_B4, 2),
	N(N_D5, 2), N(N_F5, 2), N(N_A5, 4), N(N_G5, 2), N(N_F5, 2), N(N_E5, 4),
	N(N_E5, 2), N(N_D5, 2), N(N_C5, 2), N(N_B4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E5, 4),

	// Section A' (Bars 5..8 = 64 steps)
	N(N_A4, 2), N(N_C5, 2), N(N_E5, 2), N(N_A5, 2), N(N_G5, 2), N(N_F5, 2), N(N_E5, 4),
	N(N_F5, 2), N(N_E5, 2), N(N_D5, 2), N(N_F5, 2), N(N_E5, 2), N(N_D5, 2), N(N_C5, 4),
	N(N_B4, 2), N(N_D5, 2), N(N_F5, 2), N(N_E5, 2), N(N_D5, 2), N(N_C5, 2), N(N_B4, 2), N(N_GS4, 2),
	N(N_A4, 2), N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_A4, 4), RST(4),

	// Section B (Bars 9..12 = 64 steps): Triumphant C Major -> G -> Am -> E7
	N(N_G5, 3), N(N_E5, 1), N(N_C5, 2), N(N_E5, 2), N(N_G5, 2), N(N_A5, 2), N(N_G5, 4),
	N(N_F5, 3), N(N_D5, 1), N(N_B4, 2), N(N_D5, 2), N(N_G5, 2), N(N_F5, 2), N(N_E5, 2), N(N_D5, 2),
	N(N_E5, 2), N(N_C5, 2), N(N_A4, 2), N(N_C5, 2), N(N_F5, 2), N(N_E5, 2), N(N_D5, 2), N(N_C5, 2),
	N(N_B4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E5, 2), N(N_GS5, 2), N(N_FS5, 2), N(N_E5, 4),

	// Section C (Bars 13..16 = 64 steps): Baroque 16th-note drive back to A minor
	N(N_A5, 2), N(N_F5, 2), N(N_D5, 2), N(N_F5, 2), N(N_A5, 1), N(N_G5, 1), N(N_F5, 1), N(N_E5, 1), N(N_D5, 4),
	N(N_E5, 2), N(N_C5, 2), N(N_A4, 2), N(N_C5, 2), N(N_E5, 1), N(N_D5, 1), N(N_C5, 1), N(N_B4, 1), N(N_A4, 4),
	N(N_F5, 2), N(N_D5, 2), N(N_B4, 2), N(N_D5, 2), N(N_E5, 2), N(N_GS5, 2), N(N_B5, 4),
	N(N_A5, 2), N(N_E5, 2), N(N_C5, 2), N(N_A4, 2), N(N_E5, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E5, 2),
};

static const Music_Event march_p2[] = {
	// Section A (Bars 1..4 = 64 steps): lively baroque harmonic arpeggios
	N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_A4, 2), N(N_B4, 2), N(N_A4, 2), N(N_GS4, 2), N(N_E4, 2),
	N(N_A4, 2), N(N_C5, 2), N(N_F4, 2), N(N_A4, 2), N(N_G4, 2), N(N_B4, 2), N(N_D4, 2), N(N_G4, 2),
	N(N_F4, 2), N(N_A4, 2), N(N_D5, 2), N(N_A4, 2), N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_A4, 2),
	N(N_GS4, 2), N(N_B4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_GS4, 2),

	// Section A' (Bars 5..8 = 64 steps)
	N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_E5, 2), N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_A4, 2),
	N(N_D4, 2), N(N_A4, 2), N(N_F4, 2), N(N_A4, 2), N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_E4, 2),
	N(N_D4, 2), N(N_F4, 2), N(N_B4, 2), N(N_F4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E4, 2),
	N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_E4, 2), N(N_G4, 2), N(N_B4, 2),

	// Section B (Bars 9..12 = 64 steps): C -> G -> Am/F -> E7
	N(N_E5, 2), N(N_C5, 2), N(N_G4, 2), N(N_C5, 2), N(N_E5, 2), N(N_F5, 2), N(N_E5, 4),
	N(N_D5, 2), N(N_B4, 2), N(N_G4, 2), N(N_B4, 2), N(N_D5, 2), N(N_B4, 2), N(N_G4, 4),
	N(N_C5, 2), N(N_A4, 2), N(N_E4, 2), N(N_A4, 2), N(N_A4, 2), N(N_C5, 2), N(N_F4, 2), N(N_A4, 2),
	N(N_GS4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E5, 2), N(N_D5, 2), N(N_B4, 4),

	// Section C (Bars 13..16 = 64 steps): energetic 8th counterpoint
	N(N_F5, 2), N(N_D5, 2), N(N_A4, 2), N(N_D5, 2), N(N_F5, 2), N(N_E5, 2), N(N_D5, 2), N(N_A4, 2),
	N(N_C5, 2), N(N_A4, 2), N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_B4, 2), N(N_A4, 2), N(N_E4, 2),
	N(N_D5, 2), N(N_B4, 2), N(N_F4, 2), N(N_B4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E5, 2), N(N_D5, 2),
	N(N_C5, 2), N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_B4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2),
};

static const Music_Event march_tri[] = {
	// Pumping NES octave bass (16 bars = 256 steps)
	// Bars 1..4: Am | F -> G | Dm -> Am | E7
	N(N_A2, 2), N(N_A3, 2), N(N_E2, 2), N(N_E3, 2), N(N_A2, 2), N(N_A3, 2), N(N_G2, 2), N(N_E2, 2),
	N(N_F2, 2), N(N_F3, 2), N(N_C3, 2), N(N_F2, 2), N(N_G2, 2), N(N_G3, 2), N(N_D3, 2), N(N_G2, 2),
	N(N_D2, 2), N(N_D3, 2), N(N_A2, 2), N(N_D3, 2), N(N_A2, 2), N(N_E3, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_E2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_B2, 2), N(N_E2, 2), N(N_B2, 2), N(N_GS2, 2), N(N_E2, 2),

	// Bars 5..8: Am | Dm -> C | Bdim -> E7 | Am
	N(N_A2, 2), N(N_A3, 2), N(N_E2, 2), N(N_E3, 2), N(N_A2, 2), N(N_C3, 2), N(N_E3, 2), N(N_A2, 2),
	N(N_D2, 2), N(N_D3, 2), N(N_A2, 2), N(N_D3, 2), N(N_C3, 2), N(N_G2, 2), N(N_C3, 2), N(N_E3, 2),
	N(N_B2, 2), N(N_F2, 2), N(N_D3, 2), N(N_B2, 2), N(N_E2, 2), N(N_E3, 2), N(N_B2, 2), N(N_GS2, 2),
	N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_C3, 2), N(N_A2, 4), N(N_G2, 4),

	// Bars 9..12: C | G | Am -> F | E7
	N(N_C3, 2), N(N_G2, 2), N(N_C3, 2), N(N_E3, 2), N(N_C3, 2), N(N_G2, 2), N(N_C3, 2), N(N_E2, 2),
	N(N_G2, 2), N(N_D3, 2), N(N_G2, 2), N(N_B2, 2), N(N_G2, 2), N(N_D3, 2), N(N_B2, 2), N(N_G2, 2),
	N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_C3, 2), N(N_F2, 2), N(N_C3, 2), N(N_F2, 2), N(N_A2, 2),
	N(N_E2, 2), N(N_B2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_E2, 2), N(N_B2, 2), N(N_GS2, 2), N(N_E2, 2),

	// Bars 13..16: Dm | Am | Dm -> E7 | Am -> E7
	N(N_D2, 2), N(N_D3, 2), N(N_A2, 2), N(N_D3, 2), N(N_F2, 2), N(N_D3, 2), N(N_A2, 2), N(N_F2, 2),
	N(N_A2, 2), N(N_A3, 2), N(N_E2, 2), N(N_A2, 2), N(N_C3, 2), N(N_A2, 2), N(N_E2, 2), N(N_A2, 2),
	N(N_D2, 2), N(N_A2, 2), N(N_F2, 2), N(N_D3, 2), N(N_E2, 2), N(N_B2, 2), N(N_GS2, 2), N(N_E3, 2),
	N(N_A2, 2), N(N_E3, 2), N(N_C3, 2), N(N_A2, 2), N(N_E2, 2), N(N_GS2, 2), N(N_B2, 2), N(N_E2, 2),
};

static const Music_Event march_drums[] = {
	// Bars 1..3
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	// Bar 4 fill
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 1), DR(D_SNARE, 1), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bars 5..7
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	// Bar 8 fill
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_KICK, 2), DR(D_OHAT, 2),
	// Bars 9..11 (Section B: upbeat ride/open-hat energy)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_OHAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_OHAT, 2), DR(D_KICK, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_OHAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	// Bar 12 fill
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_SNARE, 2), DR(D_KICK, 2), DR(D_OHAT, 2),
	// Bars 13..15 (Section C: driving 16th energy)
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bar 16 turnaround fill
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 1), DR(D_SNARE, 1), DR(D_SNARE, 2), DR(D_SNARE, 2),
};

// =============================================================================
// SONG 4: MUSIC 3 — "Korobeiniki"
// Traditional Russian Folk Song (1861, Public Domain)
// NES 2A03 arrangement in A minor, 144 BPM, 24 bars = 384 steps (A + A2 + A' + A2' + B)
// =============================================================================

static const Music_Event koro_p1[] = {
	// A (Bars 1..4 = 64 steps)
	N(N_E5, 4), N(N_B4, 2), N(N_C5, 2), N(N_D5, 4), N(N_C5, 2), N(N_B4, 2),
	N(N_A4, 4), N(N_A4, 2), N(N_C5, 2), N(N_E5, 4), N(N_D5, 2), N(N_C5, 2),
	N(N_B4, 6), N(N_C5, 2), N(N_D5, 4), N(N_E5, 4),
	N(N_C5, 4), N(N_A4, 4), N(N_A4, 8),

	// A2 (Bars 5..8 = 64 steps)
	RST(2), N(N_D5, 4), N(N_F5, 2), N(N_A5, 4), N(N_G5, 2), N(N_F5, 2),
	N(N_E5, 6), N(N_C5, 2), N(N_E5, 4), N(N_D5, 2), N(N_C5, 2),
	N(N_B4, 4), N(N_B4, 2), N(N_C5, 2), N(N_D5, 4), N(N_E5, 4),
	N(N_C5, 4), N(N_A4, 4), N(N_A4, 8),

	// A' (Bars 9..12 = 64 steps): energetic repeat
	N(N_E5, 4), N(N_B4, 2), N(N_C5, 2), N(N_D5, 4), N(N_C5, 2), N(N_B4, 2),
	N(N_A4, 4), N(N_A4, 2), N(N_C5, 2), N(N_E5, 4), N(N_D5, 2), N(N_C5, 2),
	N(N_B4, 6), N(N_C5, 2), N(N_D5, 4), N(N_E5, 4),
	N(N_C5, 4), N(N_A4, 4), N(N_A4, 8),

	// A2' (Bars 13..16 = 64 steps)
	RST(2), N(N_D5, 4), N(N_F5, 2), N(N_A5, 4), N(N_G5, 2), N(N_F5, 2),
	N(N_E5, 6), N(N_C5, 2), N(N_E5, 4), N(N_D5, 2), N(N_C5, 2),
	N(N_B4, 4), N(N_B4, 2), N(N_C5, 2), N(N_D5, 4), N(N_E5, 4),
	N(N_C5, 4), N(N_A4, 4), N(N_A4, 8),

	// B (Bars 17..24 = 128 steps): Stately half-note counterpoint bridge
	NG(N_E5, 8, 92), NG(N_C5, 8, 92),
	NG(N_D5, 8, 92), NG(N_B4, 8, 92),
	NG(N_C5, 8, 92), NG(N_A4, 8, 92),
	NG(N_GS4, 8, 92), NG(N_B4, 8, 85),
	NG(N_E5, 8, 92), NG(N_C5, 8, 92),
	NG(N_D5, 8, 92), NG(N_B4, 8, 92),
	NG(N_C5, 4, 88), NG(N_E5, 4, 88), NG(N_A5, 8, 92),
	NG(N_GS5, 12, 90), RST(4),
};

static const Music_Event koro_p2[] = {
	// A (Bars 1..4 = 64 steps): classic Russian third/sixth harmony
	N(N_B4, 4), N(N_GS4, 2), N(N_A4, 2), N(N_B4, 4), N(N_A4, 2), N(N_GS4, 2),
	N(N_E4, 4), N(N_E4, 2), N(N_A4, 2), N(N_C5, 4), N(N_B4, 2), N(N_A4, 2),
	N(N_GS4, 6), N(N_A4, 2), N(N_B4, 4), N(N_C5, 4),
	N(N_A4, 4), N(N_E4, 4), N(N_E4, 4), RST(4),

	// A2 (Bars 5..8 = 64 steps)
	RST(2), N(N_A4, 4), N(N_D5, 2), N(N_F5, 4), N(N_E5, 2), N(N_D5, 2),
	N(N_C5, 6), N(N_A4, 2), N(N_C5, 4), N(N_B4, 2), N(N_A4, 2),
	N(N_GS4, 4), N(N_GS4, 2), N(N_A4, 2), N(N_B4, 4), N(N_C5, 4),
	N(N_A4, 4), N(N_E4, 4), N(N_E4, 4), RST(4),

	// A' (Bars 9..12 = 64 steps): lively 8th-note chord arpeggios
	N(N_GS4, 2), N(N_B4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_GS4, 2), N(N_E4, 2), N(N_GS4, 2),
	N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_E4, 2), N(N_C5, 2), N(N_A4, 2), N(N_E4, 2), N(N_A4, 2),
	N(N_GS4, 2), N(N_B4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2),
	N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_E4, 2),

	// A2' (Bars 13..16 = 64 steps)
	N(N_F4, 2), N(N_A4, 2), N(N_D5, 2), N(N_A4, 2), N(N_F5, 2), N(N_D5, 2), N(N_A4, 2), N(N_D5, 2),
	N(N_C5, 2), N(N_A4, 2), N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_B4, 2), N(N_A4, 2), N(N_E4, 2),
	N(N_GS4, 2), N(N_E4, 2), N(N_GS4, 2), N(N_A4, 2), N(N_B4, 2), N(N_GS4, 2), N(N_C5, 2), N(N_B4, 2),
	N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_E4, 2), N(N_A4, 4), N(N_E4, 4),

	// B (Bars 17..24 = 128 steps): flowing 8th-note balalaika accompaniment
	N(N_A3, 2), N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_A3, 2), N(N_C4, 2),
	N(N_GS3, 2), N(N_B3, 2), N(N_E4, 2), N(N_GS4, 2), N(N_E4, 2), N(N_B3, 2), N(N_GS3, 2), N(N_B3, 2),
	N(N_A3, 2), N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_A3, 2), N(N_C4, 2),
	N(N_E3, 2), N(N_GS3, 2), N(N_B3, 2), N(N_E4, 2), N(N_GS4, 2), N(N_E4, 2), N(N_B3, 2), N(N_GS3, 2),
	N(N_A3, 2), N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_E4, 2), N(N_C4, 2), N(N_A3, 2), N(N_C4, 2),
	N(N_GS3, 2), N(N_B3, 2), N(N_E4, 2), N(N_GS4, 2), N(N_E4, 2), N(N_B3, 2), N(N_GS3, 2), N(N_B3, 2),
	N(N_A3, 2), N(N_C4, 2), N(N_E4, 2), N(N_A4, 2), N(N_C5, 2), N(N_A4, 2), N(N_E4, 2), N(N_C4, 2),
	N(N_E4, 2), N(N_B3, 2), N(N_GS3, 2), N(N_B3, 2), N(N_E4, 2), N(N_GS4, 2), N(N_B4, 2), N(N_E5, 2),
};

static const Music_Event koro_tri[] = {
	// A (Bars 1..4 = 64 steps): bouncing oom-pah octaves
	N(N_E2, 2), N(N_E3, 2), N(N_E2, 2), N(N_E3, 2), N(N_E2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_B2, 2),
	N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_E3, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_GS2, 2), N(N_E3, 2), N(N_E2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_E3, 2), N(N_B2, 2), N(N_GS2, 2),
	N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_B2, 2), N(N_C3, 2), N(N_CS3, 2),

	// A2 (Bars 5..8 = 64 steps)
	N(N_D2, 2), N(N_D3, 2), N(N_D2, 2), N(N_D3, 2), N(N_D2, 2), N(N_A2, 2), N(N_F2, 2), N(N_D2, 2),
	N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_E3, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_E2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_E3, 2), N(N_B2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_E2, 2),
	N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_G2, 2), N(N_F2, 2), N(N_E2, 2),

	// A' (Bars 9..12 = 64 steps)
	N(N_E2, 2), N(N_E3, 2), N(N_E2, 2), N(N_E3, 2), N(N_E2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_B2, 2),
	N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_E3, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_GS2, 2), N(N_E3, 2), N(N_E2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_E3, 2), N(N_B2, 2), N(N_GS2, 2),
	N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_B2, 2), N(N_C3, 2), N(N_CS3, 2),

	// A2' (Bars 13..16 = 64 steps)
	N(N_D2, 2), N(N_D3, 2), N(N_D2, 2), N(N_D3, 2), N(N_D2, 2), N(N_A2, 2), N(N_F2, 2), N(N_D2, 2),
	N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_A3, 2), N(N_A2, 2), N(N_E3, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_E2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_E3, 2), N(N_B2, 2), N(N_E3, 2), N(N_GS2, 2), N(N_E2, 2),
	N(N_A2, 2), N(N_E3, 2), N(N_A2, 2), N(N_E3, 2), N(N_A2, 4), N(N_E2, 4),

	// B (Bars 17..24 = 128 steps)
	N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2),
	N(N_GS2, 2), N(N_E2, 2), N(N_GS2, 2), N(N_E2, 2), N(N_GS2, 2), N(N_E2, 2), N(N_B2, 2), N(N_GS2, 2),
	N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_E2, 2), N(N_B2, 2), N(N_E2, 2), N(N_B2, 2), N(N_E2, 2), N(N_GS2, 2), N(N_B2, 2), N(N_E2, 2),
	N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_E2, 2),
	N(N_GS2, 2), N(N_E2, 2), N(N_GS2, 2), N(N_E2, 2), N(N_GS2, 2), N(N_E2, 2), N(N_B2, 2), N(N_GS2, 2),
	N(N_A2, 2), N(N_E2, 2), N(N_A2, 2), N(N_C3, 2), N(N_A2, 2), N(N_E3, 2), N(N_C3, 2), N(N_A2, 2),
	N(N_E2, 2), N(N_B2, 2), N(N_E2, 2), N(N_B2, 2), N(N_E2, 2), N(N_GS2, 2), N(N_B2, 2), N(N_E3, 2),
};

static const Music_Event koro_drums[] = {
	// Bars 1..4 (A)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bars 5..8 (A2)
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 1), DR(D_SNARE, 1), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bars 9..12 (A')
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	// Bars 13..16 (A2')
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 1), DR(D_HAT, 1), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_OHAT, 4),
	// Bars 17..24 (B: stately half-time feel building back into A)
	DR(D_KICK, 4), DR(D_RIM, 4), DR(D_KICK, 4), DR(D_SNARE, 4),
	DR(D_KICK, 4), DR(D_RIM, 4), DR(D_KICK, 4), DR(D_SNARE, 4),
	DR(D_KICK, 4), DR(D_RIM, 4), DR(D_KICK, 4), DR(D_SNARE, 4),
	DR(D_KICK, 4), DR(D_SNARE, 4), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2), DR(D_KICK, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_OHAT, 2),
	DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_KICK, 2), DR(D_SNARE, 2), DR(D_SNARE, 2), DR(D_SNARE, 1), DR(D_SNARE, 1), DR(D_SNARE, 2), DR(D_OHAT, 2),
};

// =============================================================================
// SONG 5: GAME OVER JINGLE — "Curtain Call" (One-shot, A minor, 132 BPM, 32 steps)
// =============================================================================

static const Music_Event gameover_p1[] = {
	N(N_E5, 2), N(N_DS5, 2), N(N_D5, 2), N(N_CS5, 2),
	N(N_C5, 4), N(N_B4, 4),
	NG(N_A4, 12, 95), RST(4),
};

static const Music_Event gameover_p2[] = {
	N(N_C5, 2), N(N_B4, 2), N(N_AS4, 2), N(N_A4, 2),
	N(N_A4, 4), N(N_GS4, 4),
	NG(N_E4, 12, 95), RST(4),
};

static const Music_Event gameover_tri[] = {
	N(N_A2, 2), N(N_G2, 2), N(N_F2, 2), N(N_E2, 2),
	N(N_D2, 4), N(N_E2, 4),
	NG(N_A2, 12, 95), RST(4),
};

static const Music_Event gameover_drums[] = {
	DR(D_KICK, 2), DR(D_HAT, 2), DR(D_SNARE, 2), DR(D_HAT, 2),
	DR(D_KICK, 4), DR(D_SNARE, 4),
	DR(D_KICK, 4), RST(12),
};

// =============================================================================
// Song Registry
// =============================================================================

static const Song_Def SONGS[SONG_COUNT] = {
	[SONG_NONE] = {
		.name = "Off",
		.bpm = 120.0f,
		.loop = false,
		.gain = 0.0f,
	},
	[SONG_TITLE] = {
		.name             = "Texas Overture",
		.bpm              = 126.0f,
		.loop             = true,
		.gain             = 0.78f,
		.p1_duty          = 0.25f,
		.p1_sustain       = 0.55f,
		.p1_decay         = 7.0f,
		.p1_default_gate  = 78,
		.p1_vibrato       = true,
		.p2_duty          = 0.125f,
		.p2_sustain       = 0.40f,
		.p2_decay         = 11.0f,
		.p2_default_gate  = 58,
		.p2_vibrato       = false,
		.tri_default_gate = 78,
		.p1_gain          = 0.34f,
		.p2_gain          = 0.20f,
		.tri_gain         = 0.44f,
		.drum_gain        = 0.28f,
		.p1               = title_p1,
		.p1_count         = ARR_LEN(title_p1),
		.p2               = title_p2,
		.p2_count         = ARR_LEN(title_p2),
		.tri              = title_tri,
		.tri_count        = ARR_LEN(title_tri),
		.drums            = title_drums,
		.drums_count      = ARR_LEN(title_drums),
		.total_steps      = 128,
	},
	[SONG_SUGAR_PLUM] = {
		.name             = "Sugar Plum",
		.bpm              = 118.0f,
		.loop             = true,
		.gain             = 0.96f,
		.p1_duty          = 0.25f,
		.p1_sustain       = 0.36f,
		.p1_decay         = 12.5f,
		.p1_default_gate  = 66,
		.p1_vibrato       = false,
		.p2_duty          = 0.125f,
		.p2_sustain       = 0.30f,
		.p2_decay         = 14.0f,
		.p2_default_gate  = 52,
		.p2_vibrato       = false,
		.tri_default_gate = 62,
		.p1_gain          = 0.38f,
		.p2_gain          = 0.21f,
		.tri_gain         = 0.46f,
		.drum_gain        = 0.24f,
		.p1               = sugar_p1,
		.p1_count         = ARR_LEN(sugar_p1),
		.p2               = sugar_p2,
		.p2_count         = ARR_LEN(sugar_p2),
		.tri              = sugar_tri,
		.tri_count        = ARR_LEN(sugar_tri),
		.drums            = sugar_drums,
		.drums_count      = ARR_LEN(sugar_drums),
		.total_steps      = 256,
	},
	[SONG_TETRA_MARCH] = {
		.name             = "Tetra March",
		.bpm              = 138.0f,
		.loop             = true,
		.gain             = 0.94f,
		.p1_duty          = 0.25f,
		.p1_sustain       = 0.62f,
		.p1_decay         = 8.0f,
		.p1_default_gate  = 82,
		.p1_vibrato       = true,
		.p2_duty          = 0.50f,
		.p2_sustain       = 0.42f,
		.p2_decay         = 11.0f,
		.p2_default_gate  = 68,
		.p2_vibrato       = false,
		.tri_default_gate = 80,
		.p1_gain          = 0.36f,
		.p2_gain          = 0.21f,
		.tri_gain         = 0.45f,
		.drum_gain        = 0.32f,
		.p1               = march_p1,
		.p1_count         = ARR_LEN(march_p1),
		.p2               = march_p2,
		.p2_count         = ARR_LEN(march_p2),
		.tri              = march_tri,
		.tri_count        = ARR_LEN(march_tri),
		.drums            = march_drums,
		.drums_count      = ARR_LEN(march_drums),
		.total_steps      = 256,
	},
	[SONG_KOROBEINIKI] = {
		.name             = "Korobeiniki",
		.bpm              = 144.0f,
		.loop             = true,
		.gain             = 0.94f,
		.p1_duty          = 0.25f,
		.p1_sustain       = 0.64f,
		.p1_decay         = 7.5f,
		.p1_default_gate  = 84,
		.p1_vibrato       = true,
		.p2_duty          = 0.125f,
		.p2_sustain       = 0.44f,
		.p2_decay         = 10.5f,
		.p2_default_gate  = 70,
		.p2_vibrato       = false,
		.tri_default_gate = 78,
		.p1_gain          = 0.36f,
		.p2_gain          = 0.21f,
		.tri_gain         = 0.46f,
		.drum_gain        = 0.30f,
		.p1               = koro_p1,
		.p1_count         = ARR_LEN(koro_p1),
		.p2               = koro_p2,
		.p2_count         = ARR_LEN(koro_p2),
		.tri              = koro_tri,
		.tri_count        = ARR_LEN(koro_tri),
		.drums            = koro_drums,
		.drums_count      = ARR_LEN(koro_drums),
		.total_steps      = 384,
	},
	[SONG_GAME_OVER] = {
		.name             = "Game Over",
		.bpm              = 132.0f,
		.loop             = false,
		.gain             = 0.92f,
		.p1_duty          = 0.50f,
		.p1_sustain       = 0.55f,
		.p1_decay         = 5.5f,
		.p1_default_gate  = 85,
		.p1_vibrato       = true,
		.p2_duty          = 0.25f,
		.p2_sustain       = 0.50f,
		.p2_decay         = 6.0f,
		.p2_default_gate  = 85,
		.p2_vibrato       = false,
		.tri_default_gate = 88,
		.p1_gain          = 0.36f,
		.p2_gain          = 0.24f,
		.tri_gain         = 0.46f,
		.drum_gain        = 0.26f,
		.p1               = gameover_p1,
		.p1_count         = ARR_LEN(gameover_p1),
		.p2               = gameover_p2,
		.p2_count         = ARR_LEN(gameover_p2),
		.tri              = gameover_tri,
		.tri_count        = ARR_LEN(gameover_tri),
		.drums            = gameover_drums,
		.drums_count      = ARR_LEN(gameover_drums),
		.total_steps      = 32,
	},
};

// =============================================================================
// DSP Helpers
// =============================================================================

static double midi_to_hz(uint8_t midi) {
	return 440.0 * pow(2.0, ((double)midi - 69.0) / 12.0);
}

// Band-limited PolyBLEP correction for crisp, alias-free NES pulse waves.
static double poly_blep(double t, double dt) {
	if (dt <= 0.0) {
		return 0.0;
	}
	if (t < dt) {
		double x = t / dt;
		return x + x - x * x - 1.0;
	}
	if (t > 1.0 - dt) {
		double x = (t - 1.0) / dt;
		return x * x + x + x + 1.0;
	}
	return 0.0;
}

static float clamp_f32(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static void reset_voices(Music_Engine *m) {
	memset(&m->pulse1, 0, sizeof(m->pulse1));
	memset(&m->pulse2, 0, sizeof(m->pulse2));
	memset(&m->tri, 0, sizeof(m->tri));
	memset(&m->drum, 0, sizeof(m->drum));
	m->drum.lfsr = 0x4001u;
}

static double current_step_seconds(const Music_Engine *m, const Song_Def *s) {
	float eff_bpm = s->bpm * m->tempo_scale;
	if (eff_bpm < 30.0f) {
		eff_bpm = 30.0f;
	}
	// 16th-note step duration: 60 / (BPM * 4)
	return 60.0 / ((double)eff_bpm * 4.0);
}

static void trigger_pulse(
	Music_Pulse_Voice *v,
	const Music_Event *ev,
	float duty,
	float sustain,
	float decay_rate,
	uint8_t default_gate,
	bool vibrato,
	double step_sec
) {
	if (ev->note == R) {
		v->releasing = true;
		return;
	}
	v->freq = midi_to_hz(ev->note);
	v->duty = duty;
	v->sustain = sustain;
	v->decay_rate = decay_rate;
	v->vibrato = vibrato;
	v->vel = (ev->vel > 0) ? ((float)ev->vel / 15.0f) : 1.0f;
	v->note_age = 0.0;
	uint8_t gate = (ev->gate_pct > 0) ? ev->gate_pct : default_gate;
	if (gate < 15) gate = 15;
	if (gate > 100) gate = 100;
	v->gate_time = step_sec * (double)ev->steps * ((double)gate / 100.0);
	v->active = true;
	v->releasing = false;
}

static void trigger_tri(Music_Tri_Voice *v, const Music_Event *ev, uint8_t default_gate, double step_sec) {
	if (ev->note == R) {
		v->releasing = true;
		return;
	}
	v->freq = midi_to_hz(ev->note);
	v->note_age = 0.0;
	uint8_t gate = (ev->gate_pct > 0) ? ev->gate_pct : default_gate;
	if (gate < 15) gate = 15;
	if (gate > 100) gate = 100;
	v->gate_time = step_sec * (double)ev->steps * ((double)gate / 100.0);
	v->active = true;
	v->releasing = false;
}

static void trigger_drum(Music_Drum_Voice *d, const Music_Event *ev) {
	float vel = (ev->vel > 0) ? ((float)ev->vel / 15.0f) : 1.0f;
	switch ((Drum_Hit)ev->note) {
	case D_NONE:
		break;
	case D_KICK:
		d->tone_phase = 0.0;
		d->tone_freq = 158.0;
		d->tone_end_freq = 44.0;
		d->tone_sweep_rate = 38.0;
		d->tone_env = 0.92f * vel;
		d->tone_decay = 16.0f;
		// Tiny click transient
		d->lfsr_rate = 14000.0;
		d->noise_env = 0.18f * vel;
		d->noise_decay = 180.0f;
		d->use_hp = false;
		break;
	case D_SNARE:
		d->lfsr_rate = 9500.0;
		d->noise_env = 0.52f * vel;
		d->noise_decay = 15.0f;
		d->use_hp = true;
		d->tone_phase = 0.0;
		d->tone_freq = 205.0;
		d->tone_end_freq = 115.0;
		d->tone_sweep_rate = 42.0;
		d->tone_env = 0.42f * vel;
		d->tone_decay = 34.0f;
		break;
	case D_HAT:
		d->lfsr_rate = 24000.0;
		d->noise_env = 0.24f * vel;
		d->noise_decay = 52.0f;
		d->use_hp = true;
		break;
	case D_OHAT:
		d->lfsr_rate = 22000.0;
		d->noise_env = 0.26f * vel;
		d->noise_decay = 12.0f;
		d->use_hp = true;
		break;
	case D_RIM:
		d->tone_phase = 0.0;
		d->tone_freq = 860.0;
		d->tone_end_freq = 540.0;
		d->tone_sweep_rate = 60.0;
		d->tone_env = 0.30f * vel;
		d->tone_decay = 68.0f;
		d->lfsr_rate = 19000.0;
		d->noise_env = 0.14f * vel;
		d->noise_decay = 95.0f;
		d->use_hp = true;
		break;
	}
}

static void advance_sequencer_step(Music_Engine *m, const Song_Def *s) {
	if (m->current_step >= s->total_steps) {
		if (s->loop && m->looping) {
			m->current_step = 0;
			m->pulse1.ev_index = 0;
			m->pulse1.steps_left = 0;
			m->pulse2.ev_index = 0;
			m->pulse2.steps_left = 0;
			m->tri.ev_index = 0;
			m->tri.steps_left = 0;
			m->drum.ev_index = 0;
			m->drum.steps_left = 0;
		} else {
			m->finished = true;
			m->pulse1.releasing = true;
			m->pulse2.releasing = true;
			m->tri.releasing = true;
			return;
		}
	}

	double step_sec = current_step_seconds(m, s);

	// Pulse 1
	if (s->p1 && s->p1_count > 0) {
		if (m->pulse1.steps_left <= 0 && m->pulse1.ev_index < s->p1_count) {
			const Music_Event *ev = &s->p1[m->pulse1.ev_index++];
			trigger_pulse(&m->pulse1, ev, s->p1_duty, s->p1_sustain, s->p1_decay, s->p1_default_gate, s->p1_vibrato, step_sec);
			m->pulse1.steps_left = ev->steps;
		}
		if (m->pulse1.steps_left > 0) {
			m->pulse1.steps_left--;
		}
	}

	// Pulse 2
	if (s->p2 && s->p2_count > 0) {
		if (m->pulse2.steps_left <= 0 && m->pulse2.ev_index < s->p2_count) {
			const Music_Event *ev = &s->p2[m->pulse2.ev_index++];
			trigger_pulse(&m->pulse2, ev, s->p2_duty, s->p2_sustain, s->p2_decay, s->p2_default_gate, s->p2_vibrato, step_sec);
			m->pulse2.steps_left = ev->steps;
		}
		if (m->pulse2.steps_left > 0) {
			m->pulse2.steps_left--;
		}
	}

	// Triangle
	if (s->tri && s->tri_count > 0) {
		if (m->tri.steps_left <= 0 && m->tri.ev_index < s->tri_count) {
			const Music_Event *ev = &s->tri[m->tri.ev_index++];
			trigger_tri(&m->tri, ev, s->tri_default_gate, step_sec);
			m->tri.steps_left = ev->steps;
		}
		if (m->tri.steps_left > 0) {
			m->tri.steps_left--;
		}
	}

	// Drums
	if (s->drums && s->drums_count > 0) {
		if (m->drum.steps_left <= 0 && m->drum.ev_index < s->drums_count) {
			const Music_Event *ev = &s->drums[m->drum.ev_index++];
			trigger_drum(&m->drum, ev);
			m->drum.steps_left = ev->steps;
		}
		if (m->drum.steps_left > 0) {
			m->drum.steps_left--;
		}
	}

	m->current_step++;
}

static float sample_pulse(Music_Pulse_Voice *v, double dt_sec) {
	if (!v->active && v->env <= 1e-5f) {
		return 0.0f;
	}

	v->note_age += dt_sec;
	if (v->active && !v->releasing && v->note_age >= v->gate_time) {
		v->releasing = true;
	}

	// Envelope: fast 3ms anti-click attack, musical exponential decay to sustain, smooth 6ms release
	if (!v->releasing) {
		float target;
		if (v->note_age < 0.003) {
			target = (float)(v->note_age / 0.003);
		} else {
			double decay_t = v->note_age - 0.003;
			target = v->sustain + (1.0f - v->sustain) * expf(-(float)decay_t * v->decay_rate);
		}
		// Smooth approach to prevent sudden amplitude steps on re-trigger
		v->env += (target - v->env) * 0.25f;
	} else {
		v->env -= (float)(dt_sec / 0.007);
		if (v->env <= 0.0f) {
			v->env = 0.0f;
			v->active = false;
			return 0.0f;
		}
	}

	double freq = v->freq;
	if (v->vibrato && v->note_age > 0.16) {
		double vib_env = (v->note_age - 0.16) * 4.0;
		if (vib_env > 1.0) vib_env = 1.0;
		// Subtle +/- 9 cents at 5.6 Hz
		double vib = sin(2.0 * M_PI * 5.6 * (v->note_age - 0.16)) * 0.0052 * vib_env;
		freq *= (1.0 + vib);
	}

	double phase_inc = freq * dt_sec;
	v->phase += phase_inc;
	if (v->phase >= 1.0) {
		v->phase -= floor(v->phase);
	}

	double duty = (double)v->duty;
	double raw = (v->phase < duty) ? 1.0 : -1.0;
	raw += poly_blep(v->phase, phase_inc);
	double fall_t = v->phase - duty;
	if (fall_t < 0.0) {
		fall_t += 1.0;
	}
	raw -= poly_blep(fall_t, phase_inc);
	// Remove DC offset for asymmetric duty cycles (12.5%, 25%)
	raw -= (2.0 * duty - 1.0);

	return (float)raw * v->env * v->vel;
}

static float sample_tri(Music_Tri_Voice *v, double dt_sec) {
	if (!v->active && v->env <= 1e-5f) {
		return 0.0f;
	}

	v->note_age += dt_sec;
	if (v->active && !v->releasing && v->note_age >= v->gate_time) {
		v->releasing = true;
	}

	if (!v->releasing) {
		v->env += (float)(dt_sec / 0.004);
		if (v->env > 1.0f) v->env = 1.0f;
	} else {
		v->env -= (float)(dt_sec / 0.006);
		if (v->env <= 0.0f) {
			v->env = 0.0f;
			v->active = false;
			return 0.0f;
		}
	}

	v->phase += v->freq * dt_sec;
	if (v->phase >= 1.0) {
		v->phase -= floor(v->phase);
	}

	// Authentic NES 2A03 4-bit 32-step quantized triangle table
	int idx = ((int)(v->phase * 32.0)) & 31;
	int step = (idx < 16) ? (15 - idx) : (idx - 16);
	float raw = ((float)step / 7.5f) - 1.0f;

	// Gentle 1-pole low-pass to smooth high-frequency quantization hash
	v->lp_state += 0.42f * (raw - v->lp_state);
	return v->lp_state * v->env;
}

static float sample_drum(Music_Drum_Voice *d, double dt_sec) {
	float out = 0.0f;

	if (d->noise_env > 1e-4f) {
		d->lfsr_phase += d->lfsr_rate * dt_sec;
		while (d->lfsr_phase >= 1.0) {
			d->lfsr_phase -= 1.0;
			uint16_t bit = (uint16_t)((d->lfsr ^ (d->lfsr >> 1)) & 1u);
			d->lfsr = (uint16_t)((d->lfsr >> 1) | (bit << 14));
			if (d->lfsr == 0) {
				d->lfsr = 0x4001u;
			}
			d->noise_val = (d->lfsr & 1u) ? 1.0f : -1.0f;
		}

		float n = d->noise_val;
		if (d->use_hp) {
			float hp = 0.72f * (d->hp_prev_out + n - d->hp_prev_in);
			d->hp_prev_in = n;
			d->hp_prev_out = hp;
			n = hp;
		}
		out += n * d->noise_env;
		d->noise_env *= expf(-(float)dt_sec * d->noise_decay);
		if (d->noise_env < 1e-4f) {
			d->noise_env = 0.0f;
		}
	}

	if (d->tone_env > 1e-4f) {
		d->tone_freq += (d->tone_end_freq - d->tone_freq) * (1.0 - exp(-dt_sec * d->tone_sweep_rate));
		d->tone_phase += d->tone_freq * dt_sec;
		if (d->tone_phase >= 1.0) {
			d->tone_phase -= floor(d->tone_phase);
		}
		float body = sinf((float)(2.0 * M_PI * d->tone_phase));
		out += body * d->tone_env;
		d->tone_env *= expf(-(float)dt_sec * d->tone_decay);
		if (d->tone_env < 1e-4f) {
			d->tone_env = 0.0f;
		}
	}

	return out;
}

// =============================================================================
// NES 2A03 Sound Effect Synthesizer
// =============================================================================

static double sfx_duration_for(Music_Sfx kind) {
	switch (kind) {
	case SFX_MOVE:       return 0.032;
	case SFX_ROTATE:     return 0.062;
	case SFX_LOCK:       return 0.075;
	case SFX_HARD_DROP:  return 0.105;
	case SFX_HOLD:       return 0.095;
	case SFX_LINE_CLEAR: return 0.360;
	case SFX_TETRIS:     return 0.740;
	case SFX_TSPIN:      return 0.240;
	case SFX_LEVEL_UP:   return 0.460;
	case SFX_PAUSE:      return 0.190;
	case SFX_MENU_MOVE:  return 0.028;
	case SFX_MENU_START: return 0.210;
	case SFX_COUNTDOWN:  return 0.110;
	case SFX_NONE:
	case SFX_COUNT:      break;
	}
	return 0.0;
}

static bool sfx_is_prominent(Music_Sfx kind) {
	return kind == SFX_LINE_CLEAR || kind == SFX_TETRIS || kind == SFX_LEVEL_UP || kind == SFX_PAUSE;
}

static float sfx_pulse_osc(double *phase, double freq, double duty, double dt_sec) {
	double inc = freq * dt_sec;
	*phase += inc;
	if (*phase >= 1.0) {
		*phase -= floor(*phase);
	}
	double raw = (*phase < duty) ? 1.0 : -1.0;
	raw += poly_blep(*phase, inc);
	double fall_t = *phase - duty;
	if (fall_t < 0.0) {
		fall_t += 1.0;
	}
	raw -= poly_blep(fall_t, inc);
	raw -= (2.0 * duty - 1.0);
	return (float)raw;
}

static float sfx_tri_osc(double *phase, double freq, double dt_sec) {
	*phase += freq * dt_sec;
	if (*phase >= 1.0) {
		*phase -= floor(*phase);
	}
	int idx = ((int)(*phase * 32.0)) & 31;
	int step = (idx < 16) ? (15 - idx) : (idx - 16);
	return ((float)step / 7.5f) - 1.0f;
}

static float sfx_noise_osc(Music_Sfx_Voice *v, double rate, double dt_sec) {
	v->lfsr_phase += rate * dt_sec;
	while (v->lfsr_phase >= 1.0) {
		v->lfsr_phase -= 1.0;
		uint16_t bit = (uint16_t)((v->lfsr ^ (v->lfsr >> 1)) & 1u);
		v->lfsr = (uint16_t)((v->lfsr >> 1) | (bit << 14));
		if (v->lfsr == 0) {
			v->lfsr = 0x5A31u;
		}
		v->noise_val = (v->lfsr & 1u) ? 1.0f : -1.0f;
	}
	return v->noise_val;
}

// Smooth attack/release envelope helper to guarantee zero clicks on SFX boundaries.
static float sfx_window(double age, double duration, double attack, double release) {
	float w = 1.0f;
	if (attack > 0.0 && age < attack) {
		w *= (float)(age / attack);
	}
	double rem = duration - age;
	if (release > 0.0 && rem < release) {
		w *= (float)(rem / release);
	}
	return clamp_f32(w, 0.0f, 1.0f);
}

static float sample_sfx_voice(Music_Sfx_Voice *v, double dt_sec) {
	if (!v->active) {
		return 0.0f;
	}

	double t = v->age;
	double dur = v->duration;
	if (t >= dur) {
		v->active = false;
		return 0.0f;
	}
	v->age += dt_sec;

	float out = 0.0f;
	switch (v->kind) {
	case SFX_MOVE: {
		// Subtle, crisp NES tick on horizontal shift
		double freq = 720.0 + (t / dur) * 210.0;
		float env = sfx_window(t, dur, 0.002, 0.012) * expf(-(float)t * 48.0f);
		out = sfx_pulse_osc(&v->phase1, freq, 0.25, dt_sec) * env * 0.24f;
		break;
	}
	case SFX_ROTATE: {
		// Classic NES 2-step upward chirp
		double freq = (t < 0.024) ? 523.25 : 783.99;
		float env = sfx_window(t, dur, 0.002, 0.014) * expf(-(float)t * 22.0f);
		out = sfx_pulse_osc(&v->phase1, freq, 0.25, dt_sec) * env * 0.34f;
		break;
	}
	case SFX_LOCK: {
		// Tactile low-mid landing thud
		double freq = 82.0 + 115.0 * exp(-t * 45.0);
		float tri = sfx_tri_osc(&v->phase1, freq, dt_sec) * expf(-(float)t * 26.0f);
		float n = (t < 0.018) ? sfx_noise_osc(v, 9000.0, dt_sec) * expf(-(float)t * 140.0f) * 0.25f : 0.0f;
		out = (tri * 0.46f + n) * sfx_window(t, dur, 0.002, 0.010);
		break;
	}
	case SFX_HARD_DROP: {
		// Punchy downward slam + crisp impact
		double freq = 58.0 + 280.0 * exp(-t * 52.0);
		float body = sfx_tri_osc(&v->phase1, freq, dt_sec) * expf(-(float)t * 20.0f);
		float snap = sfx_pulse_osc(&v->phase2, 130.0 * exp(-t * 24.0) + 55.0, 0.50, dt_sec) * expf(-(float)t * 40.0f);
		float n = (t < 0.032) ? sfx_noise_osc(v, 11000.0, dt_sec) * expf(-(float)t * 95.0f) : 0.0f;
		out = (body * 0.54f + snap * 0.24f + n * 0.28f) * sfx_window(t, dur, 0.002, 0.014);
		break;
	}
	case SFX_HOLD: {
		// Clean 2-note swap chime (A4 -> E5)
		double freq = (t < 0.042) ? 440.0 : 659.25;
		float env = sfx_window(t, dur, 0.003, 0.020) * expf(-(float)t * 14.0f);
		out = sfx_pulse_osc(&v->phase1, freq, 0.50, dt_sec) * env * 0.34f;
		break;
	}
	case SFX_LINE_CLEAR: {
		// 1-3 rows cleared: NES noise sweep + bright 4-note ascending major arpeggio
		static const double lead[4] = {523.25, 659.25, 783.99, 1046.50}; // C5 E5 G5 C6
		static const double harm[4] = {392.00, 523.25, 659.25, 783.99};  // G4 C5 E5 G5
		int step = (int)(t / 0.068);
		if (step > 3) step = 3;
		double local_t = t - (double)step * 0.068;
		float note_env = (step < 3) ? expf(-(float)local_t * 11.0f) : expf(-(float)local_t * 7.5f);
		float p1 = sfx_pulse_osc(&v->phase1, lead[step], 0.25, dt_sec);
		float p2 = sfx_pulse_osc(&v->phase2, harm[step], 0.50, dt_sec);
		float noise_rate = 18000.0 - 11000.0 * clamp_f32((float)(t / 0.14), 0.0f, 1.0f);
		float n = (t < 0.14) ? sfx_noise_osc(v, noise_rate, dt_sec) * expf(-(float)t * 24.0f) * 0.28f : 0.0f;
		out = ((p1 * 0.44f + p2 * 0.20f) * note_env + n) * sfx_window(t, dur, 0.003, 0.035);
		break;
	}
	case SFX_TETRIS: {
		// 4 rows cleared (TETRIS!): distinct, triumphant 2-octave NES fanfare + chord shimmer
		static const double lead[6] = {523.25, 783.99, 659.25, 1046.50, 783.99, 1318.51}; // C5 G5 E5 C6 G5 E6
		static const double harm[6] = {392.00, 523.25, 523.25, 783.99,  659.25, 1046.50}; // G4 C5 C5 G5 E5 C6
		double f1, f2;
		float note_env;
		if (t < 0.330) {
			int step = (int)(t / 0.055);
			if (step > 5) step = 5;
			double local_t = t - (double)step * 0.055;
			f1 = lead[step];
			f2 = harm[step];
			note_env = 0.72f + 0.28f * expf(-(float)local_t * 16.0f);
		} else {
			// Shimmering NES arpeggio trill sustain (E6 <-> G6 over C6) with sparkle vibrato
			double hold_t = t - 0.330;
			int trill = ((int)(hold_t / 0.042)) & 1;
			double vib = 1.0 + 0.007 * sin(2.0 * M_PI * 10.5 * hold_t);
			f1 = (trill == 0 ? 1567.98 : 1318.51) * vib; // G6 / E6
			f2 = 1046.50 * vib;                          // C6
			note_env = expf(-(float)hold_t * 3.8f);
		}
		float p1 = sfx_pulse_osc(&v->phase1, f1, 0.25, dt_sec);
		float p2 = sfx_pulse_osc(&v->phase2, f2, 0.50, dt_sec);
		float noise_rate = 22000.0 - 13000.0 * clamp_f32((float)(t / 0.20), 0.0f, 1.0f);
		float n = (t < 0.20) ? sfx_noise_osc(v, noise_rate, dt_sec) * expf(-(float)t * 17.0f) * 0.34f : 0.0f;
		out = ((p1 * 0.48f + p2 * 0.28f) * note_env + n) * sfx_window(t, dur, 0.003, 0.050);
		break;
	}
	case SFX_TSPIN: {
		// Crisp harmonic twist shimmer (A4 -> C#5 -> E5 -> A5)
		static const double notes[4] = {440.0, 554.37, 659.25, 880.0};
		int step = (int)(t / 0.048);
		if (step > 3) step = 3;
		float p1 = sfx_pulse_osc(&v->phase1, notes[step], 0.125, dt_sec);
		float p2 = sfx_pulse_osc(&v->phase2, notes[step] * 0.5, 0.50, dt_sec);
		float env = sfx_window(t, dur, 0.003, 0.030) * expf(-(float)t * 7.0f);
		out = (p1 * 0.38f + p2 * 0.18f) * env;
		break;
	}
	case SFX_LEVEL_UP: {
		// Upbeat 6-note rising NES fanfare (G4 B4 D5 G5 B5 D6)
		static const double notes[6] = {392.00, 493.88, 587.33, 783.99, 987.77, 1174.66};
		int step = (int)(t / 0.060);
		if (step > 5) step = 5;
		double local_t = t - (double)step * 0.060;
		float env = (step < 5) ? (0.75f + 0.25f * expf(-(float)local_t * 18.0f)) : expf(-(float)local_t * 8.0f);
		float p1 = sfx_pulse_osc(&v->phase1, notes[step], 0.25, dt_sec);
		float p2 = sfx_pulse_osc(&v->phase2, notes[step] * 0.5, 0.50, dt_sec);
		out = (p1 * 0.42f + p2 * 0.18f) * env * sfx_window(t, dur, 0.003, 0.035);
		break;
	}
	case SFX_PAUSE: {
		// Iconic 4-tone NES pause chime (E5 C5 E5 C5)
		static const double notes[4] = {659.25, 523.25, 659.25, 523.25};
		int step = (int)(t / 0.045);
		if (step > 3) step = 3;
		double local_t = t - (double)step * 0.045;
		float env = expf(-(float)local_t * 16.0f);
		float p1 = sfx_pulse_osc(&v->phase1, notes[step], 0.50, dt_sec);
		out = p1 * env * 0.38f * sfx_window(t, dur, 0.002, 0.015);
		break;
	}
	case SFX_MENU_MOVE: {
		double freq = 660.0 + (t / dur) * 140.0;
		float env = sfx_window(t, dur, 0.002, 0.010) * expf(-(float)t * 55.0f);
		out = sfx_pulse_osc(&v->phase1, freq, 0.25, dt_sec) * env * 0.22f;
		break;
	}
	case SFX_MENU_START: {
		static const double notes[4] = {523.25, 659.25, 783.99, 1046.50};
		int step = (int)(t / 0.042);
		if (step > 3) step = 3;
		float p1 = sfx_pulse_osc(&v->phase1, notes[step], 0.50, dt_sec);
		float env = sfx_window(t, dur, 0.002, 0.025) * expf(-(float)t * 8.5f);
		out = p1 * env * 0.38f;
		break;
	}
	case SFX_COUNTDOWN: {
		float p1 = sfx_pulse_osc(&v->phase1, 440.0, 0.50, dt_sec);
		float env = sfx_window(t, dur, 0.003, 0.025) * expf(-(float)t * 16.0f);
		out = p1 * env * 0.36f;
		break;
	}
	case SFX_NONE:
	case SFX_COUNT:
		v->active = false;
		break;
	}

	return out;
}

// =============================================================================
// Public Pure-C API
// =============================================================================

void music_init(Music_Engine *m) {
	memset(m, 0, sizeof(*m));
	m->initialized = true;
	m->song = SONG_NONE;
	m->looping = true;
	m->finished = true;
	m->tempo_scale = 1.0f;
	m->target_tempo_scale = 1.0f;
	m->master_volume = 0.62f;
	m->song_gain = 1.0f;
	m->music_env = 0.0f;
	m->mute_env = 1.0f;
	m->duck_env = 1.0f;
	reset_voices(m);
}

void music_play_song(Music_Engine *m, Music_Song song) {
	if (!m->initialized) {
		music_init(m);
	}
	if (song < SONG_NONE || song >= SONG_COUNT) {
		song = SONG_NONE;
	}
	if (song == SONG_NONE) {
		music_stop(m);
		return;
	}

	const Song_Def *s = &SONGS[song];
	m->song = song;
	m->looping = s->loop;
	m->finished = false;
	m->song_gain = s->gain;
	m->current_step = 0;
	m->step_accum = 0.0;
	reset_voices(m);

	// Apply danger tempo only to looping gameplay tracks
	bool allow_danger = (song == SONG_SUGAR_PLUM || song == SONG_TETRA_MARCH || song == SONG_KOROBEINIKI);
	m->target_tempo_scale = (allow_danger && m->danger) ? 1.28f : 1.0f;
	m->tempo_scale = m->target_tempo_scale;

	// Trigger step 0 immediately so audio starts right away with zero dead air
	advance_sequencer_step(m, s);
}

void music_stop(Music_Engine *m) {
	m->song = SONG_NONE;
	m->finished = true;
	m->current_step = 0;
	m->step_accum = 0.0;
	reset_voices(m);
}

void music_play_sfx(Music_Engine *m, Music_Sfx sfx) {
	if (!m || !m->initialized || sfx <= SFX_NONE || sfx >= SFX_COUNT) {
		return;
	}

	// Reuse an existing slot of the same short rapid-fire SFX (e.g. move/menu) to avoid pile-up,
	// otherwise pick an inactive slot or steal the oldest active voice.
	int slot = -1;
	if (sfx == SFX_MOVE || sfx == SFX_MENU_MOVE) {
		for (int i = 0; i < MUSIC_MAX_SFX_VOICES; i++) {
			if (m->sfx[i].active && m->sfx[i].kind == sfx) {
				slot = i;
				break;
			}
		}
	}
	if (slot < 0) {
		for (int i = 0; i < MUSIC_MAX_SFX_VOICES; i++) {
			if (!m->sfx[i].active) {
				slot = i;
				break;
			}
		}
	}
	if (slot < 0) {
		double oldest = -1.0;
		slot = 0;
		for (int i = 0; i < MUSIC_MAX_SFX_VOICES; i++) {
			if (m->sfx[i].age > oldest) {
				oldest = m->sfx[i].age;
				slot = i;
			}
		}
	}

	Music_Sfx_Voice *v = &m->sfx[slot];
	memset(v, 0, sizeof(*v));
	v->kind = sfx;
	v->active = true;
	v->duration = sfx_duration_for(sfx);
	v->lfsr = (uint16_t)(0x4001u ^ (uint16_t)(m->sfx_trigger_count * 257u + (uint32_t)sfx * 17u));
	if (v->lfsr == 0) {
		v->lfsr = 0x5A31u;
	}
	m->last_sfx = sfx;
	m->sfx_trigger_count++;
}

void music_clear_sfx(Music_Engine *m) {
	if (!m) {
		return;
	}
	memset(m->sfx, 0, sizeof(m->sfx));
}

void music_set_paused(Music_Engine *m, bool paused) {
	m->paused = paused;
}

void music_set_muted(Music_Engine *m, bool muted) {
	m->muted = muted;
}

void music_set_danger(Music_Engine *m, bool danger) {
	m->danger = danger;
	bool allow_danger = (m->song == SONG_SUGAR_PLUM || m->song == SONG_TETRA_MARCH || m->song == SONG_KOROBEINIKI);
	m->target_tempo_scale = (allow_danger && danger) ? 1.28f : 1.0f;
}

void music_set_volume(Music_Engine *m, float volume) {
	m->master_volume = clamp_f32(volume, 0.0f, 1.0f);
}

void music_render(Music_Engine *m, int16_t *out_stereo, int frames) {
	if (!out_stereo || frames <= 0) {
		return;
	}
	if (!m || !m->initialized) {
		memset(out_stereo, 0, (size_t)frames * MUSIC_CHANNELS * sizeof(int16_t));
		return;
	}

	const bool has_song = (m->song > SONG_NONE && m->song < SONG_COUNT);
	const Song_Def *s = has_song ? &SONGS[m->song] : NULL;
	const double dt_sec = 1.0 / (double)MUSIC_SAMPLE_RATE;
	const float target_music_env = (has_song && !m->paused) ? 1.0f : 0.0f;
	const float target_mute_env = m->muted ? 0.0f : 1.0f;
	const float env_step = (float)(dt_sec / 0.015); // 15ms click-free fade

	for (int i = 0; i < frames; i++) {
		// Smooth music pause/stop envelope
		if (m->music_env < target_music_env) {
			m->music_env += env_step;
			if (m->music_env > target_music_env) m->music_env = target_music_env;
		} else if (m->music_env > target_music_env) {
			m->music_env -= env_step;
			if (m->music_env < target_music_env) m->music_env = target_music_env;
		}

		// Smooth master mute envelope (applies to both music and SFX)
		if (m->mute_env < target_mute_env) {
			m->mute_env += env_step;
			if (m->mute_env > target_mute_env) m->mute_env = target_mute_env;
		} else if (m->mute_env > target_mute_env) {
			m->mute_env -= env_step;
			if (m->mute_env < target_mute_env) m->mute_env = target_mute_env;
		}

		// Mix active SFX voices (still plays when music is OFF or during pause chime)
		float sfx_sum = 0.0f;
		bool prominent_sfx = false;
		for (int v = 0; v < MUSIC_MAX_SFX_VOICES; v++) {
			if (m->sfx[v].active) {
				if (sfx_is_prominent(m->sfx[v].kind)) {
					prominent_sfx = true;
				}
				sfx_sum += sample_sfx_voice(&m->sfx[v], dt_sec);
			}
		}

		// Gentle music ducking during prominent SFX (line clear, Tetris, level up)
		float target_duck = prominent_sfx ? 0.66f : 1.0f;
		m->duck_env += (target_duck - m->duck_env) * 0.003f;

		// Fast path: fully silent (paused/no-song and no SFX, or fully muted)
		if ((m->mute_env <= 0.0f) || (m->music_env <= 0.0f && fabsf(sfx_sum) < 1e-6f && fabsf(m->lp_y[0]) < 1e-6f && fabsf(m->lp_y[1]) < 1e-6f)) {
			out_stereo[i * 2] = 0;
			out_stereo[i * 2 + 1] = 0;
			continue;
		}

		float music_ch[2] = {0.0f, 0.0f};
		if (has_song && !(m->paused && m->music_env <= 0.0f)) {
			// Smooth danger-mode tempo transition
			m->tempo_scale += (m->target_tempo_scale - m->tempo_scale) * 0.00025f;

			if (!m->finished && !m->paused) {
				double samples_per_step = current_step_seconds(m, s) * (double)MUSIC_SAMPLE_RATE;
				m->step_accum += 1.0;
				while (m->step_accum >= samples_per_step && !m->finished) {
					m->step_accum -= samples_per_step;
					advance_sequencer_step(m, s);
				}
			}

			float p1 = sample_pulse(&m->pulse1, dt_sec) * s->p1_gain;
			float p2 = sample_pulse(&m->pulse2, dt_sec) * s->p2_gain;
			float tr = sample_tri(&m->tri, dt_sec) * s->tri_gain;
			float dr = sample_drum(&m->drum, dt_sec) * s->drum_gain;

			// Sit background music gently below sound effects for clear gameplay audio cues
			const float music_bus_gain = 0.68f;
			float m_gain = s->gain * music_bus_gain * m->music_env * m->duck_env;
			music_ch[0] = (p1 * 1.12f + p2 * 0.84f + tr + dr * 0.96f) * m_gain;
			music_ch[1] = (p1 * 0.88f + p2 * 1.16f + tr + dr * 1.04f) * m_gain;
		}

		float sfx_gain = 1.22f;
		float master = m->master_volume * m->mute_env;

		for (int c = 0; c < 2; c++) {
			float x = (music_ch[c] + sfx_sum * sfx_gain) * master;
			// NES-style analog output conditioning: ~45 Hz DC blocker + ~11.5 kHz warmth low-pass
			float hp = 0.9935f * (m->dc_y[c] + x - m->dc_x[c]);
			m->dc_x[c] = x;
			m->dc_y[c] = hp;

			m->lp_y[c] += 0.62f * (hp - m->lp_y[c]);
			// Smooth soft limiter prevents harsh clipping on dense chords + SFX
			float limited = tanhf(m->lp_y[c] * 1.15f) * 0.92f;
			int32_t pcm = (int32_t)lrintf(limited * 32767.0f);
			if (pcm > 32767) pcm = 32767;
			if (pcm < -32768) pcm = -32768;
			out_stereo[i * 2 + c] = (int16_t)pcm;
		}
	}
}

// =============================================================================
// Track Metadata & Validation Helpers
// =============================================================================

const char *music_selection_short_label(Music_Selection sel) {
	switch (sel) {
	case MUSIC_SEL_1:   return "1";
	case MUSIC_SEL_2:   return "2";
	case MUSIC_SEL_3:   return "3";
	case MUSIC_SEL_OFF: return "OFF";
	case MUSIC_SEL_COUNT: break;
	}
	return "OFF";
}

const char *music_selection_name(Music_Selection sel) {
	switch (sel) {
	case MUSIC_SEL_1:   return "Sugar Plum";
	case MUSIC_SEL_2:   return "Tetra March";
	case MUSIC_SEL_3:   return "Korobeiniki";
	case MUSIC_SEL_OFF: return "Off";
	case MUSIC_SEL_COUNT: break;
	}
	return "Off";
}

Music_Song music_song_for_selection(Music_Selection sel) {
	switch (sel) {
	case MUSIC_SEL_1:   return SONG_SUGAR_PLUM;
	case MUSIC_SEL_2:   return SONG_TETRA_MARCH;
	case MUSIC_SEL_3:   return SONG_KOROBEINIKI;
	case MUSIC_SEL_OFF: return SONG_NONE;
	case MUSIC_SEL_COUNT: break;
	}
	return SONG_NONE;
}

int music_song_total_steps(Music_Song song) {
	if (song < SONG_NONE || song >= SONG_COUNT) {
		return 0;
	}
	return SONGS[song].total_steps;
}

static int sum_event_steps(const Music_Event *evs, int count) {
	int sum = 0;
	for (int i = 0; i < count; i++) {
		sum += evs[i].steps;
	}
	return sum;
}

bool music_validate_song_lengths(Music_Song song) {
	if (song <= SONG_NONE || song >= SONG_COUNT) {
		return false;
	}
	const Song_Def *s = &SONGS[song];
	if (s->total_steps <= 0) {
		return false;
	}
	if (sum_event_steps(s->p1, s->p1_count) != s->total_steps) return false;
	if (sum_event_steps(s->p2, s->p2_count) != s->total_steps) return false;
	if (sum_event_steps(s->tri, s->tri_count) != s->total_steps) return false;
	if (sum_event_steps(s->drums, s->drums_count) != s->total_steps) return false;
	return true;
}

// =============================================================================
// Raylib AudioStream Bridge
// =============================================================================

void music_open_audio(Music_Engine *m) {
	if (!m || m->audio_open || !IsAudioDeviceReady()) {
		return;
	}
	SetAudioStreamBufferSizeDefault(MUSIC_BUFFER_FRAMES);
	m->stream = LoadAudioStream(MUSIC_SAMPLE_RATE, 16, MUSIC_CHANNELS);
	if (!IsAudioStreamValid(m->stream)) {
		return;
	}
	m->audio_open = true;
	PlayAudioStream(m->stream);
	// Pre-fill initial buffer immediately
	if (IsAudioStreamProcessed(m->stream)) {
		music_render(m, m->mix_buf, MUSIC_BUFFER_FRAMES);
		UpdateAudioStream(m->stream, m->mix_buf, MUSIC_BUFFER_FRAMES);
	}
}

void music_update_audio(Music_Engine *m) {
	if (!m || !m->audio_open) {
		return;
	}
	// Refill any consumed AudioStream buffers
	int refills = 0;
	while (IsAudioStreamProcessed(m->stream) && refills < 4) {
		music_render(m, m->mix_buf, MUSIC_BUFFER_FRAMES);
		UpdateAudioStream(m->stream, m->mix_buf, MUSIC_BUFFER_FRAMES);
		refills++;
	}
}

void music_close_audio(Music_Engine *m) {
	if (!m || !m->audio_open) {
		return;
	}
	StopAudioStream(m->stream);
	UnloadAudioStream(m->stream);
	m->audio_open = false;
}
