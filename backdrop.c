#include "backdrop.h"

#include <math.h>
#include <stdlib.h>

#define TEXAS_PI  3.14159265358979323846f
#define TEXAS_TAU (2.0f * TEXAS_PI)

// Curated low-contrast, dark muted palettes so patterns remain calm and non-distracting.
static const Backdrop_Palette BACKDROP_PALETTES[6] = {
	// Slate Twilight (homage to original slate blue-gray backdrop)
	{
		.bg_top_left     = {28, 40, 56, 255},
		.bg_top_right    = {34, 48, 66, 255},
		.bg_bottom_left  = {42, 56, 74, 255},
		.bg_bottom_right = {32, 46, 62, 255},
		.accent_a        = {96, 126, 158, 255},
		.accent_b        = {76, 106, 138, 255},
		.accent_c        = {118, 144, 172, 255},
	},
	// Midnight Indigo
	{
		.bg_top_left     = {26, 30, 50, 255},
		.bg_top_right    = {34, 38, 60, 255},
		.bg_bottom_left  = {40, 44, 68, 255},
		.bg_bottom_right = {30, 34, 54, 255},
		.accent_a        = {98, 108, 152, 255},
		.accent_b        = {80, 90, 136, 255},
		.accent_c        = {120, 128, 166, 255},
	},
	// Deep Fjord (muted teal-slate)
	{
		.bg_top_left     = {24, 42, 50, 255},
		.bg_top_right    = {30, 50, 58, 255},
		.bg_bottom_left  = {36, 56, 64, 255},
		.bg_bottom_right = {28, 46, 54, 255},
		.accent_a        = {84, 126, 136, 255},
		.accent_b        = {68, 108, 118, 255},
		.accent_c        = {108, 144, 152, 255},
	},
	// Dusk Plum (dark charcoal-violet)
	{
		.bg_top_left     = {34, 30, 46, 255},
		.bg_top_right    = {42, 36, 54, 255},
		.bg_bottom_left  = {48, 42, 60, 255},
		.bg_bottom_right = {38, 34, 50, 255},
		.accent_a        = {114, 100, 136, 255},
		.accent_b        = {94, 84, 118, 255},
		.accent_c        = {132, 118, 150, 255},
	},
	// Quiet Pine (dark sage-slate)
	{
		.bg_top_left     = {26, 40, 40, 255},
		.bg_top_right    = {32, 48, 46, 255},
		.bg_bottom_left  = {38, 54, 52, 255},
		.bg_bottom_right = {30, 44, 44, 255},
		.accent_a        = {86, 118, 114, 255},
		.accent_b        = {72, 102, 98, 255},
		.accent_c        = {108, 136, 130, 255},
	},
	// Warm Charcoal (smoky graphite)
	{
		.bg_top_left     = {34, 36, 44, 255},
		.bg_top_right    = {42, 44, 52, 255},
		.bg_bottom_left  = {48, 50, 58, 255},
		.bg_bottom_right = {38, 40, 48, 255},
		.accent_a        = {112, 114, 126, 255},
		.accent_b        = {96, 100, 114, 255},
		.accent_c        = {130, 128, 138, 255},
	},
};

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

static float rand_unit_f32(void) {
	return (float)rand() / (float)RAND_MAX;
}

static float backdrop_rand_range(float lo, float hi) {
	return lo + rand_unit_f32() * (hi - lo);
}

static float backdrop_rand_signed(float min_abs, float max_abs) {
	float mag = backdrop_rand_range(min_abs, max_abs);
	return (rand() % 2 == 0) ? mag : -mag;
}

static uint8_t shift_channel(uint8_t v, int delta) {
	int n = (int)v + (rand() % (delta * 2 + 1)) - delta;
	return (uint8_t)clamp_int(n, 14, 180);
}

static Color backdrop_jitter_color(Color c, int delta) {
	return (Color){
		shift_channel(c.r, delta),
		shift_channel(c.g, delta),
		shift_channel(c.b, delta),
		c.a,
	};
}

static Color backdrop_palette_accent(Backdrop_Palette p, uint8_t idx) {
	switch (idx % 3) {
	case 0:
		return p.accent_a;
	case 1:
		return p.accent_b;
	default:
		return p.accent_c;
	}
}

static Color color_with_alpha(Color c, uint8_t alpha) {
	return (Color){c.r, c.g, c.b, alpha};
}

// Randomizes the generative backdrop style, muted palette, and procedural parameters.
// When avoid_prev_style is true and the backdrop was already initialized, picks a different style.
void init_backdrop(Backdrop *b, bool avoid_prev_style) {
	int style_count = (int)BACKDROP_STYLE_COUNT;
	Backdrop_Style chosen = (Backdrop_Style)(rand() % style_count);
	if (avoid_prev_style && b->initialized && style_count > 1 && chosen == b->style) {
		int offset = 1 + (rand() % (style_count - 1));
		chosen = (Backdrop_Style)(((int)b->style + offset) % style_count);
	}
	init_backdrop_style(b, chosen);
}

// Initializes the generative backdrop with a specific style and freshly randomized palette & shapes.
void init_backdrop_style(Backdrop *b, Backdrop_Style style) {
	b->initialized = true;
	b->style = style;
	b->time = 0.0;

	int palette_count = (int)(sizeof(BACKDROP_PALETTES) / sizeof(BACKDROP_PALETTES[0]));
	Backdrop_Palette base = BACKDROP_PALETTES[rand() % palette_count];
	b->palette = (Backdrop_Palette){
		.bg_top_left     = backdrop_jitter_color(base.bg_top_left, 4),
		.bg_top_right    = backdrop_jitter_color(base.bg_top_right, 4),
		.bg_bottom_left  = backdrop_jitter_color(base.bg_bottom_left, 4),
		.bg_bottom_right = backdrop_jitter_color(base.bg_bottom_right, 4),
		.accent_a        = backdrop_jitter_color(base.accent_a, 8),
		.accent_b        = backdrop_jitter_color(base.accent_b, 8),
		.accent_c        = backdrop_jitter_color(base.accent_c, 8),
	};

	switch (style) {
	case BACKDROP_CONSTELLATION:
		b->node_count = 28 + (rand() % (BACKDROP_MAX_NODES - 28 + 1));
		for (int i = 0; i < b->node_count; i++) {
			b->nodes[i] = (Backdrop_Node){
				.x         = backdrop_rand_range(-0.04f, 1.04f),
				.y         = backdrop_rand_range(-0.04f, 1.04f),
				.vx        = backdrop_rand_signed(0.003f, 0.011f),
				.vy        = backdrop_rand_signed(0.003f, 0.011f),
				.radius    = backdrop_rand_range(1.8f, 3.6f),
				.phase     = backdrop_rand_range(0.0f, TEXAS_TAU),
				.color_idx = (uint8_t)(rand() % 3),
			};
		}
		break;

	case BACKDROP_FLOW_WAVES:
		b->wave_count = 5 + (rand() % (BACKDROP_MAX_WAVES - 5 + 1));
		for (int i = 0; i < b->wave_count; i++) {
			float t = ((float)i + 0.5f) / (float)b->wave_count;
			b->waves[i] = (Backdrop_Wave){
				.base_y    = clamp_f32(t + backdrop_rand_range(-0.06f, 0.06f), 0.08f, 0.92f),
				.amp1      = backdrop_rand_range(0.03f, 0.08f),
				.amp2      = backdrop_rand_range(0.012f, 0.035f),
				.freq1     = backdrop_rand_range(1.1f, 2.4f),
				.freq2     = backdrop_rand_range(2.6f, 4.8f),
				.speed1    = backdrop_rand_signed(0.08f, 0.22f),
				.speed2    = backdrop_rand_signed(0.10f, 0.26f),
				.phase1    = backdrop_rand_range(0.0f, TEXAS_TAU),
				.phase2    = backdrop_rand_range(0.0f, TEXAS_TAU),
				.thickness = backdrop_rand_range(0.06f, 0.14f),
				.tilt      = backdrop_rand_range(-0.10f, 0.10f),
				.color_idx = (uint8_t)(i % 3),
			};
		}
		break;

	case BACKDROP_GEO_DRIFT: {
		static const int sides_pool[6] = {0, 3, 4, 5, 6, 8};
		b->shape_count = 18 + (rand() % (BACKDROP_MAX_SHAPES - 18 + 1));
		for (int i = 0; i < b->shape_count; i++) {
			b->shapes[i] = (Backdrop_Shape){
				.x         = backdrop_rand_range(-0.05f, 1.05f),
				.y         = backdrop_rand_range(-0.05f, 1.05f),
				.vx        = backdrop_rand_signed(0.0025f, 0.009f),
				.vy        = backdrop_rand_signed(0.0025f, 0.009f),
				.size      = backdrop_rand_range(0.035f, 0.105f),
				.angle     = backdrop_rand_range(0.0f, 360.0f),
				.rot_speed = backdrop_rand_signed(1.8f, 6.5f),
				.sides     = sides_pool[rand() % 6],
				.filled    = (rand() % 100) < 42,
				.nested    = (rand() % 100) < 40,
				.color_idx = (uint8_t)(rand() % 3),
			};
		}
		break;
	}

	case BACKDROP_ORBITAL_CONTOURS:
		b->orbit_count = 2 + (rand() % (BACKDROP_MAX_ORBITS - 2 + 1));
		for (int i = 0; i < b->orbit_count; i++) {
			float sector_x = ((float)i + 0.5f) / (float)b->orbit_count;
			b->orbits[i] = (Backdrop_Orbit){
				.cx          = clamp_f32(sector_x + backdrop_rand_range(-0.14f, 0.14f), 0.12f, 0.88f),
				.cy          = backdrop_rand_range(0.20f, 0.80f),
				.orbit_rx    = backdrop_rand_range(0.03f, 0.09f),
				.orbit_ry    = backdrop_rand_range(0.03f, 0.09f),
				.orbit_speed = backdrop_rand_signed(0.05f, 0.13f),
				.orbit_phase = backdrop_rand_range(0.0f, TEXAS_TAU),
				.base_radius = backdrop_rand_range(0.11f, 0.20f),
				.ring_step   = backdrop_rand_range(0.048f, 0.075f),
				.ring_count  = 3 + (rand() % 3),
				.harm_k1     = (float)(2 + (rand() % 4)),
				.harm_k2     = (float)(3 + (rand() % 5)),
				.harm_a1     = backdrop_rand_range(0.04f, 0.10f),
				.harm_a2     = backdrop_rand_range(0.025f, 0.07f),
				.morph_speed = backdrop_rand_signed(0.12f, 0.25f),
				.morph_phase = backdrop_rand_range(0.0f, TEXAS_TAU),
				.color_idx   = (uint8_t)(i % 3),
			};
		}
		break;

	case BACKDROP_TRUCHET_LATTICE:
		b->lattice_motif = (Lattice_Motif)(rand() % (int)LATTICE_MOTIF_COUNT);
		b->lattice_phase = backdrop_rand_range(0.0f, TEXAS_TAU);
		b->lattice_angle = backdrop_rand_range(0.0f, TEXAS_TAU);
		for (int r = 0; r < BACKDROP_LATTICE_ROWS; r++) {
			for (int c = 0; c < BACKDROP_LATTICE_COLS; c++) {
				b->lattice_cells[r][c] = (uint8_t)(rand() % 4);
			}
		}
		break;

	case BACKDROP_STYLE_COUNT:
		break;
	}
}

float backdrop_wrap_coord(float v) {
	const float lo = -BACKDROP_WRAP_MARGIN;
	const float hi = 1.0f + BACKDROP_WRAP_MARGIN;
	const float span = hi - lo;
	float res = v;
	while (res < lo) {
		res += span;
	}
	while (res > hi) {
		res -= span;
	}
	return res;
}

// Advances the generative backdrop's gentle ambient motion.
void update_backdrop(Backdrop *b, double frame_dt) {
	if (!b->initialized || frame_dt <= 0.0) {
		return;
	}
	float dt = (float)frame_dt;
	b->time += frame_dt;

	switch (b->style) {
	case BACKDROP_CONSTELLATION:
		for (int i = 0; i < b->node_count; i++) {
			Backdrop_Node *n = &b->nodes[i];
			n->x = backdrop_wrap_coord(n->x + n->vx * dt);
			n->y = backdrop_wrap_coord(n->y + n->vy * dt);
		}
		break;

	case BACKDROP_GEO_DRIFT:
		for (int i = 0; i < b->shape_count; i++) {
			Backdrop_Shape *s = &b->shapes[i];
			s->x = backdrop_wrap_coord(s->x + s->vx * dt);
			s->y = backdrop_wrap_coord(s->y + s->vy * dt);
			s->angle = fmodf(s->angle + s->rot_speed * dt, 360.0f);
			if (s->angle < 0.0f) {
				s->angle += 360.0f;
			}
		}
		break;

	case BACKDROP_FLOW_WAVES:
	case BACKDROP_ORBITAL_CONTOURS:
	case BACKDROP_TRUCHET_LATTICE:
	case BACKDROP_STYLE_COUNT:
		// Purely time-driven parametric motion using b->time
		break;
	}
}

// Draws a filled triangle ensuring counter-clockwise screen winding (+x right, +y down).
static void draw_triangle_ccw(Vector2 a, Vector2 b, Vector2 c, Color color) {
	float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
	if (cross < 0.0f) {
		DrawTriangle(a, b, c, color);
	} else {
		DrawTriangle(a, c, b, color);
	}
}

// Draws a smooth arc polyline using standard radians (0 = +x, pi/2 = +y).
static void draw_arc_lines(
	Vector2 center,
	float radius,
	float start_rad,
	float end_rad,
	int segments,
	float thick,
	Color color
) {
	if (segments <= 0 || radius <= 0.0f) {
		return;
	}
	Vector2 prev = {
		center.x + cosf(start_rad) * radius,
		center.y + sinf(start_rad) * radius,
	};
	float step = (end_rad - start_rad) / (float)segments;
	for (int i = 1; i <= segments; i++) {
		float ang = start_rad + (float)i * step;
		Vector2 cur = {
			center.x + cosf(ang) * radius,
			center.y + sinf(ang) * radius,
		};
		DrawLineEx(prev, cur, thick, color);
		prev = cur;
	}
}

static void draw_backdrop_constellation(const Backdrop *b, float width, float height) {
	float min_dim = (width < height) ? width : height;
	float link_dist = clamp_f32(min_dim * 0.25f, 110.0f, 260.0f);
	float link_dist_sq = link_dist * link_dist;
	float tri_dist = link_dist * 0.72f;
	float tri_dist_sq = tri_dist * tri_dist;

	Vector2 pos[BACKDROP_MAX_NODES];
	for (int i = 0; i < b->node_count; i++) {
		pos[i] = (Vector2){b->nodes[i].x * width, b->nodes[i].y * height};
	}

	// Subtle translucent mesh facets (capped to keep composition airy)
	int tri_drawn = 0;
	for (int i = 0; i < b->node_count && tri_drawn < 20; i++) {
		for (int j = i + 1; j < b->node_count && tri_drawn < 20; j++) {
			float dx_ij = pos[j].x - pos[i].x;
			float dy_ij = pos[j].y - pos[i].y;
			float d2_ij = dx_ij * dx_ij + dy_ij * dy_ij;
			if (d2_ij > tri_dist_sq) {
				continue;
			}
			for (int k = j + 1; k < b->node_count && tri_drawn < 20; k++) {
				float dx_ik = pos[k].x - pos[i].x;
				float dy_ik = pos[k].y - pos[i].y;
				float d2_ik = dx_ik * dx_ik + dy_ik * dy_ik;
				if (d2_ik > tri_dist_sq) {
					continue;
				}
				float dx_jk = pos[k].x - pos[j].x;
				float dy_jk = pos[k].y - pos[j].y;
				float d2_jk = dx_jk * dx_jk + dy_jk * dy_jk;
				if (d2_jk > tri_dist_sq) {
					continue;
				}
				float max_d2 = (d2_ik > d2_jk) ? d2_ik : d2_jk;
				if (d2_ij > max_d2) {
					max_d2 = d2_ij;
				}
				float fade = clamp_f32(1.0f - sqrtf(max_d2) / tri_dist, 0.0f, 1.0f);
				uint8_t alpha = (uint8_t)(fade * fade * 14.0f);
				if (alpha > 0) {
					Color c = color_with_alpha(backdrop_palette_accent(b->palette, b->nodes[i].color_idx), alpha);
					draw_triangle_ccw(pos[i], pos[j], pos[k], c);
					tri_drawn++;
				}
			}
		}
	}

	// Connecting edges fading smoothly with distance
	for (int i = 0; i < b->node_count; i++) {
		for (int j = i + 1; j < b->node_count; j++) {
			float dx = pos[j].x - pos[i].x;
			float dy = pos[j].y - pos[i].y;
			float d2 = dx * dx + dy * dy;
			if (d2 >= link_dist_sq) {
				continue;
			}
			float fade = 1.0f - sqrtf(d2) / link_dist;
			uint8_t alpha = (uint8_t)(fade * fade * 40.0f);
			if (alpha > 1) {
				Color c = color_with_alpha(backdrop_palette_accent(b->palette, b->nodes[i].color_idx), alpha);
				DrawLineEx(pos[i], pos[j], 1.1f, c);
			}
		}
	}

	// Nodes with whisper-soft halos
	float t = (float)b->time;
	for (int i = 0; i < b->node_count; i++) {
		Backdrop_Node n = b->nodes[i];
		float pulse = 0.8f + 0.2f * sinf(t * 0.65f + n.phase);
		Color accent = backdrop_palette_accent(b->palette, n.color_idx);
		DrawCircleV(pos[i], n.radius * 2.3f, color_with_alpha(accent, (uint8_t)(12.0f * pulse)));
		DrawCircleV(pos[i], n.radius, color_with_alpha(accent, (uint8_t)(36.0f * pulse)));
	}
}

static void draw_backdrop_flow_waves(const Backdrop *b, float width, float height) {
	enum { WAVE_SAMPLES = 44 };
	float t = (float)b->time;

	Vector2 strip[WAVE_SAMPLES * 2];
	Vector2 crest[WAVE_SAMPLES];

	for (int w_idx = 0; w_idx < b->wave_count; w_idx++) {
		Backdrop_Wave w = b->waves[w_idx];
		Color accent = backdrop_palette_accent(b->palette, w.color_idx);
		float half_thick = w.thickness * height * 0.5f;

		for (int s = 0; s < WAVE_SAMPLES; s++) {
			float u = -0.02f + 1.04f * ((float)s / (float)(WAVE_SAMPLES - 1));
			float x = u * width;
			float y_norm = w.base_y +
				(u - 0.5f) * w.tilt +
				w.amp1 * sinf(u * w.freq1 * TEXAS_TAU + t * w.speed1 + w.phase1) +
				w.amp2 * cosf(u * w.freq2 * TEXAS_TAU + t * w.speed2 + w.phase2);
			float cy = y_norm * height;
			float thick_mod = 0.78f + 0.22f * sinf(u * 2.3f * TEXAS_TAU - t * 0.18f + w.phase1);
			float ht = half_thick * thick_mod;

			// Strip order: top then bottom gives CCW triangles in screen space (+y down)
			strip[s * 2] = (Vector2){x, cy - ht};
			strip[s * 2 + 1] = (Vector2){x, cy + ht};
			crest[s] = (Vector2){x, cy - ht};
		}

		DrawTriangleStrip(strip, WAVE_SAMPLES * 2, color_with_alpha(accent, 14));

		Color line_color = color_with_alpha(accent, 30);
		for (int s = 0; s < WAVE_SAMPLES - 1; s++) {
			DrawLineEx(crest[s], crest[s + 1], 1.2f, line_color);
		}
	}
}

static void draw_backdrop_geo_drift(const Backdrop *b, float width, float height) {
	float min_dim = (width < height) ? width : height;

	for (int i = 0; i < b->shape_count; i++) {
		Backdrop_Shape s = b->shapes[i];
		Vector2 center = {s.x * width, s.y * height};
		float radius = s.size * min_dim;
		Color accent = backdrop_palette_accent(b->palette, s.color_idx);

		if (s.sides == 0) {
			if (s.filled) {
				DrawCircleV(center, radius, color_with_alpha(accent, 14));
			}
			DrawCircleLinesV(center, radius, color_with_alpha(accent, 30));
			if (s.nested) {
				DrawCircleLinesV(center, radius * 0.58f, color_with_alpha(accent, 20));
			}
		} else {
			if (s.filled) {
				DrawPoly(center, s.sides, radius, s.angle, color_with_alpha(accent, 14));
			}
			DrawPolyLinesEx(center, s.sides, radius, s.angle, 1.3f, color_with_alpha(accent, 32));
			if (s.nested) {
				DrawPolyLinesEx(
					center,
					s.sides,
					radius * 0.58f,
					s.angle + 180.0f / (float)s.sides,
					1.0f,
					color_with_alpha(accent, 20)
				);
			}
		}
	}
}

static void draw_backdrop_orbital_contours(const Backdrop *b, float width, float height) {
	enum { CONTOUR_SEGMENTS = 72 };
	float t = (float)b->time;
	float min_dim = (width < height) ? width : height;

	Vector2 pts[CONTOUR_SEGMENTS + 1];

	for (int o_idx = 0; o_idx < b->orbit_count; o_idx++) {
		Backdrop_Orbit o = b->orbits[o_idx];
		float cx = (o.cx + cosf(t * o.orbit_speed + o.orbit_phase) * o.orbit_rx) * width;
		float cy = (o.cy + sinf(t * o.orbit_speed * 0.83f + o.orbit_phase) * o.orbit_ry) * height;
		Color accent = backdrop_palette_accent(b->palette, o.color_idx);

		float glow_r = (o.base_radius + (float)o.ring_count * o.ring_step) * min_dim * 0.85f;
		DrawCircleGradient(
			(Vector2){cx, cy},
			glow_r,
			color_with_alpha(accent, 16),
			color_with_alpha(accent, 0)
		);

		for (int ring = 0; ring < o.ring_count; ring++) {
			float r_base = (o.base_radius + (float)ring * o.ring_step) * min_dim;
			float ring_phase = o.morph_phase + (float)ring * 0.65f;
			uint8_t alpha = (uint8_t)clamp_int(32 - ring * 4, 14, 34);
			Color col = color_with_alpha(accent, alpha);

			for (int s = 0; s <= CONTOUR_SEGMENTS; s++) {
				float ang = ((float)s / (float)CONTOUR_SEGMENTS) * TEXAS_TAU;
				float wobble = 1.0f +
					o.harm_a1 * sinf(o.harm_k1 * ang + t * o.morph_speed + ring_phase) +
					o.harm_a2 * cosf(o.harm_k2 * ang - t * o.morph_speed * 0.72f + ring_phase);
				float r = r_base * wobble;
				pts[s] = (Vector2){cx + cosf(ang) * r, cy + sinf(ang) * r};
			}

			for (int s = 0; s < CONTOUR_SEGMENTS; s++) {
				DrawLineEx(pts[s], pts[s + 1], 1.2f, col);
			}
		}
	}
}

static void draw_backdrop_truchet_lattice(const Backdrop *b, float width, float height) {
	float t = (float)b->time;
	float min_dim = (width < height) ? width : height;
	float cell = clamp_f32(min_dim / 8.5f, 64.0f, 128.0f);

	int cols = (int)ceilf(width / cell) + 1;
	int rows = (int)ceilf(height / cell) + 1;
	float ox = (width - (float)cols * cell) * 0.5f;
	float oy = (height - (float)rows * cell) * 0.5f;

	float dir_x = cosf(b->lattice_angle);
	float dir_y = sinf(b->lattice_angle);

	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols; c++) {
			float x0 = ox + (float)c * cell;
			float y0 = oy + (float)r * cell;
			float x1 = x0 + cell;
			float y1 = y0 + cell;
			float half = cell * 0.5f;

			uint8_t orient = b->lattice_cells[r % BACKDROP_LATTICE_ROWS][c % BACKDROP_LATTICE_COLS];
			float wave = 0.5f + 0.5f * sinf(((float)c * dir_x + (float)r * dir_y) * 0.45f - t * 0.35f + b->lattice_phase);
			uint8_t alpha = (uint8_t)(12.0f + wave * 22.0f);
			Color accent = backdrop_palette_accent(b->palette, orient);
			Color col = color_with_alpha(accent, alpha);

			switch (b->lattice_motif) {
			case LATTICE_QUARTER_ARCS:
				if (orient % 2 == 0) {
					draw_arc_lines((Vector2){x0, y0}, half, 0.0f, TEXAS_PI * 0.5f, 14, 1.4f, col);
					draw_arc_lines((Vector2){x1, y1}, half, TEXAS_PI, TEXAS_PI * 1.5f, 14, 1.4f, col);
				} else {
					draw_arc_lines((Vector2){x1, y0}, half, TEXAS_PI * 0.5f, TEXAS_PI, 14, 1.4f, col);
					draw_arc_lines((Vector2){x0, y1}, half, TEXAS_PI * 1.5f, TEXAS_TAU, 14, 1.4f, col);
				}
				break;

			case LATTICE_DIAGONAL_WEAVE:
				if (orient % 2 == 0) {
					DrawLineEx((Vector2){x0, y0}, (Vector2){x1, y1}, 1.3f, col);
					DrawLineEx((Vector2){x0 + half, y0}, (Vector2){x1, y0 + half}, 1.0f, color_with_alpha(accent, alpha / 2));
				} else {
					DrawLineEx((Vector2){x1, y0}, (Vector2){x0, y1}, 1.3f, col);
					DrawLineEx((Vector2){x0, y0 + half}, (Vector2){x0 + half, y1}, 1.0f, color_with_alpha(accent, alpha / 2));
				}
				break;

			case LATTICE_CORNER_TICKS: {
				float arm = cell * 0.30f;
				switch (orient % 4) {
				case 0:
					DrawLineEx((Vector2){x0 + arm, y0}, (Vector2){x0, y0}, 1.3f, col);
					DrawLineEx((Vector2){x0, y0}, (Vector2){x0, y0 + arm}, 1.3f, col);
					break;
				case 1:
					DrawLineEx((Vector2){x1 - arm, y0}, (Vector2){x1, y0}, 1.3f, col);
					DrawLineEx((Vector2){x1, y0}, (Vector2){x1, y0 + arm}, 1.3f, col);
					break;
				case 2:
					DrawLineEx((Vector2){x1 - arm, y1}, (Vector2){x1, y1}, 1.3f, col);
					DrawLineEx((Vector2){x1, y1}, (Vector2){x1, y1 - arm}, 1.3f, col);
					break;
				default:
					DrawLineEx((Vector2){x0 + arm, y1}, (Vector2){x0, y1}, 1.3f, col);
					DrawLineEx((Vector2){x0, y1}, (Vector2){x0, y1 - arm}, 1.3f, col);
					break;
				}
				DrawCircleV((Vector2){x0 + half, y0 + half}, 1.8f, color_with_alpha(accent, alpha));
				break;
			}

			case LATTICE_MOTIF_COUNT:
				break;
			}
		}
	}
}

// Renders the full-viewport generative backdrop in logical screen coordinates.
void draw_backdrop(const Backdrop *b, float width, float height) {
	if (!b->initialized || width <= 0.0f || height <= 0.0f) {
		return;
	}

	// 1. Subtle 4-corner atmospheric gradient base
	Rectangle full_rec = {0.0f, 0.0f, width, height};
	DrawRectangleGradientEx(
		full_rec,
		b->palette.bg_top_left,
		b->palette.bg_bottom_left,
		b->palette.bg_bottom_right,
		b->palette.bg_top_right
	);

	// 2. Active generative art pattern
	switch (b->style) {
	case BACKDROP_CONSTELLATION:
		draw_backdrop_constellation(b, width, height);
		break;
	case BACKDROP_FLOW_WAVES:
		draw_backdrop_flow_waves(b, width, height);
		break;
	case BACKDROP_GEO_DRIFT:
		draw_backdrop_geo_drift(b, width, height);
		break;
	case BACKDROP_ORBITAL_CONTOURS:
		draw_backdrop_orbital_contours(b, width, height);
		break;
	case BACKDROP_TRUCHET_LATTICE:
		draw_backdrop_truchet_lattice(b, width, height);
		break;
	case BACKDROP_STYLE_COUNT:
		break;
	}

	// 3. Soft edge framing so outer margins gently recede behind the gameboard
	Color edge_dark = {8, 12, 20, 48};
	Color edge_clear = {8, 12, 20, 0};
	float band_y = height * 0.14f;
	if (band_y < 24.0f) {
		band_y = 24.0f;
	}
	int wi = (int)ceilf(width);
	int bi = (int)ceilf(band_y);
	DrawRectangleGradientV(0, 0, wi, bi, edge_dark, edge_clear);
	DrawRectangleGradientV(0, (int)floorf(height - band_y), wi, bi + 1, edge_clear, edge_dark);
}
