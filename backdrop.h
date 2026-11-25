#ifndef TEXAS_BACKDROP_H
#define TEXAS_BACKDROP_H

#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>

#define BACKDROP_MAX_NODES    38
#define BACKDROP_MAX_WAVES    7
#define BACKDROP_MAX_SHAPES   26
#define BACKDROP_MAX_ORBITS   4
#define BACKDROP_LATTICE_COLS 16
#define BACKDROP_LATTICE_ROWS 10
#define BACKDROP_WRAP_MARGIN  0.12f

typedef enum Backdrop_Style {
	BACKDROP_CONSTELLATION = 0,
	BACKDROP_FLOW_WAVES,
	BACKDROP_GEO_DRIFT,
	BACKDROP_ORBITAL_CONTOURS,
	BACKDROP_TRUCHET_LATTICE,
	BACKDROP_STYLE_COUNT,
} Backdrop_Style;

typedef struct Backdrop_Palette {
	Color bg_top_left;
	Color bg_top_right;
	Color bg_bottom_left;
	Color bg_bottom_right;
	Color accent_a;
	Color accent_b;
	Color accent_c;
} Backdrop_Palette;

typedef struct Backdrop_Node {
	float   x, y;
	float   vx, vy;
	float   radius;
	float   phase;
	uint8_t color_idx;
} Backdrop_Node;

typedef struct Backdrop_Wave {
	float   base_y;
	float   amp1;
	float   amp2;
	float   freq1;
	float   freq2;
	float   speed1;
	float   speed2;
	float   phase1;
	float   phase2;
	float   thickness;
	float   tilt;
	uint8_t color_idx;
} Backdrop_Wave;

typedef struct Backdrop_Shape {
	float   x, y;
	float   vx, vy;
	float   size;
	float   angle;
	float   rot_speed;
	int     sides; // 0 = circle/ring, 3..8 = regular polygon
	bool    filled;
	bool    nested;
	uint8_t color_idx;
} Backdrop_Shape;

typedef struct Backdrop_Orbit {
	float   cx, cy;
	float   orbit_rx;
	float   orbit_ry;
	float   orbit_speed;
	float   orbit_phase;
	float   base_radius;
	float   ring_step;
	int     ring_count;
	float   harm_k1;
	float   harm_k2;
	float   harm_a1;
	float   harm_a2;
	float   morph_speed;
	float   morph_phase;
	uint8_t color_idx;
} Backdrop_Orbit;

typedef enum Lattice_Motif {
	LATTICE_QUARTER_ARCS = 0,
	LATTICE_DIAGONAL_WEAVE,
	LATTICE_CORNER_TICKS,
	LATTICE_MOTIF_COUNT,
} Lattice_Motif;

typedef struct Backdrop {
	bool             initialized;
	Backdrop_Style   style;
	Backdrop_Palette palette;
	double           time;
	// Constellation
	Backdrop_Node    nodes[BACKDROP_MAX_NODES];
	int              node_count;
	// Flow_Waves
	Backdrop_Wave    waves[BACKDROP_MAX_WAVES];
	int              wave_count;
	// Geo_Drift
	Backdrop_Shape   shapes[BACKDROP_MAX_SHAPES];
	int              shape_count;
	// Orbital_Contours
	Backdrop_Orbit   orbits[BACKDROP_MAX_ORBITS];
	int              orbit_count;
	// Truchet_Lattice
	Lattice_Motif    lattice_motif;
	uint8_t          lattice_cells[BACKDROP_LATTICE_ROWS][BACKDROP_LATTICE_COLS];
	float            lattice_phase;
	float            lattice_angle;
} Backdrop;

void  init_backdrop(Backdrop *b, bool avoid_prev_style);
void  init_backdrop_style(Backdrop *b, Backdrop_Style style);
void  update_backdrop(Backdrop *b, double frame_dt);
void  draw_backdrop(const Backdrop *b, float width, float height);
float backdrop_wrap_coord(float v);

#endif // TEXAS_BACKDROP_H
