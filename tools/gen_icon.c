/*
 * Procedural macOS App Icon generator for Texas.
 * Renders a 1024x1024 4x-supersampled RGBA icon matching Texas's dark slate
 * playfield aesthetic and Guideline tetromino palette, written as an
 * uncompressed 32-bit top-down TGA (converted to PNG/.icns via sips/iconutil).
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ICON_SIZE 1024
#define SS        4 // 4x4 = 16x supersampling for razor-sharp anti-aliasing

typedef struct RGBA {
	float r, g, b, a;
} RGBA;

static float clampf(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static float smoothstep(float edge0, float edge1, float x) {
	float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

static RGBA rgba_over(RGBA dst, RGBA src) {
	float sa = clampf(src.a, 0.0f, 1.0f);
	float da = clampf(dst.a, 0.0f, 1.0f);
	float out_a = sa + da * (1.0f - sa);
	if (out_a <= 1e-6f) {
		return (RGBA){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return (RGBA){
		(src.r * sa + dst.r * da * (1.0f - sa)) / out_a,
		(src.g * sa + dst.g * da * (1.0f - sa)) / out_a,
		(src.b * sa + dst.b * da * (1.0f - sa)) / out_a,
		out_a,
	};
}

// Signed distance to a rounded rectangle centered at (cx, cy) with half-extents (hx, hy) and radius r.
static float sd_round_rect(float x, float y, float cx, float cy, float hx, float hy, float r) {
	float dx = fabsf(x - cx) - (hx - r);
	float dy = fabsf(y - cy) - (hy - r);
	float ox = (dx > 0.0f) ? dx : 0.0f;
	float oy = (dy > 0.0f) ? dy : 0.0f;
	float outside = sqrtf(ox * ox + oy * oy);
	float inside = (dx > dy) ? dx : dy;
	if (inside > 0.0f) inside = 0.0f;
	return outside + inside - r;
}

// Signed distance to a macOS-style superellipse squircle (n = 4.6).
static float sd_squircle(float x, float y, float cx, float cy, float half_size) {
	const float n = 4.6f;
	float nx = fabsf(x - cx) / half_size;
	float ny = fabsf(y - cy) / half_size;
	float p = powf(nx, n) + powf(ny, n);
	float dist_norm = powf(p, 1.0f / n) - 1.0f;
	return dist_norm * half_size;
}

// 5x5 symmetric Tetris composition:
//   1 = Purple T-piece (floating hero "T" for Texas!)
//   2 = Cyan L-piece (left pocket wall)
//   3 = Orange J-piece (right pocket wall)
//   4 = Ghost T-piece (glowing landing preview in the T-slot!)
static const int BOARD_GRID[5][5] = {
	{0, 1, 1, 1, 0},
	{0, 0, 1, 0, 0},
	{2, 0, 0, 0, 3},
	{2, 4, 4, 4, 3},
	{2, 2, 4, 3, 3},
};

static RGBA piece_color(int id) {
	switch (id) {
	case 1:  return (RGBA){182.0f / 255.0f,  74.0f / 255.0f, 245.0f / 255.0f, 1.0f}; // T (Vivid Purple)
	case 2:  return (RGBA){  0.0f / 255.0f, 210.0f / 255.0f, 240.0f / 255.0f, 1.0f}; // Cyan
	case 3:  return (RGBA){255.0f / 255.0f, 152.0f / 255.0f,  32.0f / 255.0f, 1.0f}; // Orange
	case 4:  return (RGBA){182.0f / 255.0f,  74.0f / 255.0f, 245.0f / 255.0f, 1.0f}; // Ghost Purple
	default: return (RGBA){0.0f, 0.0f, 0.0f, 0.0f};
	}
}

static RGBA eval_pixel(float x, float y) {
	const float cx = 512.0f;
	const float cy = 512.0f;
	const float sq_half = 412.0f; // Apple HIG: 824x824 icon body inside 1024x1024 canvas

	RGBA col = {0.0f, 0.0f, 0.0f, 0.0f};

	// Soft drop shadow beneath macOS squircle
	float d_shadow = sd_squircle(x, y, cx, cy + 16.0f, sq_half + 4.0f);
	if (d_shadow < 48.0f) {
		float sh_a = (1.0f - smoothstep(-8.0f, 44.0f, d_shadow)) * 0.42f;
		col = rgba_over(col, (RGBA){0.02f, 0.03f, 0.06f, sh_a});
	}

	float d_sq = sd_squircle(x, y, cx, cy, sq_half);
	if (d_sq > 1.5f) {
		return col;
	}
	float body_mask = 1.0f - smoothstep(-1.0f, 1.0f, d_sq);

	// Deep twilight-slate vertical + radial gradient matching Texas's aesthetic
	float gy = clampf((y - (cy - sq_half)) / (2.0f * sq_half), 0.0f, 1.0f);
	float gx = clampf((x - (cx - sq_half)) / (2.0f * sq_half), 0.0f, 1.0f);
	RGBA bg = {
		0.075f + 0.045f * (1.0f - gy) + 0.018f * gx,
		0.095f + 0.060f * (1.0f - gy) + 0.022f * (1.0f - gx),
		0.155f + 0.085f * (1.0f - gy) + 0.030f * gx,
		1.0f,
	};

	// Subtle ambient glow behind the hero T-piece & pocket
	float dx_t = (x - cx) / 360.0f;
	float dy_t = (y - (cy - 60.0f)) / 320.0f;
	float r_t = sqrtf(dx_t * dx_t + dy_t * dy_t);
	float glow_p = expf(-r_t * r_t * 2.2f) * 0.22f;
	bg.r += 0.55f * glow_p;
	bg.g += 0.20f * glow_p;
	bg.b += 0.85f * glow_p;

	// Subtle bottom cyan/amber bounce glow
	float dx_l = (x - (cx - 190.0f)) / 260.0f;
	float dy_l = (y - (cy + 200.0f)) / 240.0f;
	float glow_c = expf(-(dx_l * dx_l + dy_l * dy_l) * 2.5f) * 0.14f;
	bg.g += 0.70f * glow_c;
	bg.b += 0.90f * glow_c;

	float dx_r = (x - (cx + 190.0f)) / 260.0f;
	float dy_r = (y - (cy + 200.0f)) / 240.0f;
	float glow_o = expf(-(dx_r * dx_r + dy_r * dy_r) * 2.5f) * 0.14f;
	bg.r += 0.95f * glow_o;
	bg.g += 0.52f * glow_o;

	// Inner dark playfield card recess
	const float cell = 114.0f;
	const float gap = 10.0f;
	const float stride = cell + gap;
	const float grid_total = 5.0f * cell + 4.0f * gap; // 610px
	const float gx0 = cx - grid_total * 0.5f;
	const float gy0 = cy - grid_total * 0.5f + 4.0f;

	float d_well = sd_round_rect(x, y, cx, gy0 + grid_total * 0.5f, grid_total * 0.5f + 22.0f, grid_total * 0.5f + 22.0f, 44.0f);
	if (d_well < 2.0f) {
		float wm = 1.0f - smoothstep(-1.0f, 1.5f, d_well);
		bg = rgba_over(bg, (RGBA){0.045f, 0.055f, 0.095f, 0.65f * wm});
	}
	// Subtle recess rim
	if (fabsf(d_well) < 3.0f) {
		float rim = 1.0f - smoothstep(0.0f, 2.5f, fabsf(d_well));
		bg = rgba_over(bg, (RGBA){0.42f, 0.52f, 0.72f, 0.28f * rim});
	}

	// Render 5x5 cells: subtle empty grid slots + glossy beveled tetromino blocks + ghost T
	for (int r = 0; r < 5; r++) {
		for (int c = 0; c < 5; c++) {
			float bx = gx0 + (float)c * stride + cell * 0.5f;
			float by = gy0 + (float)r * stride + cell * 0.5f;
			int id = BOARD_GRID[r][c];

			if (id == 0) {
				// Faint playfield cell recess
				float d_cell = sd_round_rect(x, y, bx, by, cell * 0.48f, cell * 0.48f, 18.0f);
				if (d_cell < 1.5f) {
					float m = 1.0f - smoothstep(-1.0f, 1.0f, d_cell);
					bg = rgba_over(bg, (RGBA){0.11f, 0.14f, 0.22f, 0.35f * m});
				}
				continue;
			}

			if (id == 4) {
				// Glowing Ghost Piece (matching Texas's draw_ghost style!)
				float d_g = sd_round_rect(x, y, bx, by, cell * 0.485f, cell * 0.485f, 22.0f);
				RGBA pc = piece_color(id);
				if (d_g < 1.5f) {
					float fill_m = 1.0f - smoothstep(-1.0f, 1.0f, d_g);
					bg = rgba_over(bg, (RGBA){pc.r, pc.g, pc.b, 0.22f * fill_m});
				}
				if (d_g > -9.0f && d_g < 2.0f) {
					float edge = (1.0f - smoothstep(-1.0f, 1.5f, d_g)) * smoothstep(-9.0f, -6.5f, d_g);
					bg = rgba_over(bg, (RGBA){
						clampf(pc.r + 0.18f, 0.0f, 1.0f),
						clampf(pc.g + 0.22f, 0.0f, 1.0f),
						clampf(pc.b + 0.10f, 0.0f, 1.0f),
						0.88f * edge,
					});
				}
				continue;
			}

			// Solid glossy beveled Tetromino block
			// Soft block drop shadow
			float d_bsh = sd_round_rect(x, y, bx, by + 7.0f, cell * 0.49f, cell * 0.49f, 22.0f);
			if (d_bsh < 14.0f) {
				float sm = (1.0f - smoothstep(-2.0f, 13.0f, d_bsh)) * 0.48f;
				bg = rgba_over(bg, (RGBA){0.0f, 0.0f, 0.02f, sm});
			}

			float d_blk = sd_round_rect(x, y, bx, by, cell * 0.49f, cell * 0.49f, 22.0f);
			if (d_blk < 1.5f) {
				float bm = 1.0f - smoothstep(-1.0f, 1.0f, d_blk);
				RGBA base = piece_color(id);
				float ly = clampf((y - (by - cell * 0.5f)) / cell, 0.0f, 1.0f);
				float lx = clampf((x - (bx - cell * 0.5f)) / cell, 0.0f, 1.0f);

				// Vertical sheen gradient (brighter top, deeper bottom)
				float shade = 1.12f - 0.28f * ly - 0.06f * lx;
				RGBA blk = {
					clampf(base.r * shade, 0.0f, 1.0f),
					clampf(base.g * shade, 0.0f, 1.0f),
					clampf(base.b * shade, 0.0f, 1.0f),
					bm,
				};

				// Crisp inner top-bar highlight (matches Texas's draw_block_px!)
				float d_hi = sd_round_rect(x, y, bx, by - cell * 0.23f, cell * 0.35f, cell * 0.125f, 12.0f);
				if (d_hi < 1.5f) {
					float hm = (1.0f - smoothstep(-1.0f, 1.0f, d_hi)) * 0.46f;
					blk = rgba_over(blk, (RGBA){1.0f, 1.0f, 1.0f, hm * bm});
				}

				// Crisp subtle inner dark bottom bevel
				if (d_blk > -7.0f && ly > 0.65f) {
					float bevel = smoothstep(-7.0f, -1.5f, d_blk) * smoothstep(0.65f, 0.95f, ly) * 0.28f;
					blk = rgba_over(blk, (RGBA){0.02f, 0.02f, 0.06f, bevel * bm});
				}

				bg = rgba_over(bg, blk);
			}
		}
	}

	// Subtle top glass rim & outer squircle edge polish
	if (d_sq > -7.0f) {
		float edge = smoothstep(-7.0f, -1.2f, d_sq);
		float top_lit = smoothstep(0.55f, 0.0f, gy);
		bg = rgba_over(bg, (RGBA){0.75f, 0.85f, 1.0f, (0.08f + 0.26f * top_lit) * edge});
	}

	bg.a = body_mask;
	return rgba_over(col, bg);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <output.tga>\n", argv[0]);
		return 1;
	}

	size_t px_bytes = (size_t)ICON_SIZE * ICON_SIZE * 4;
	uint8_t *pixels = (uint8_t *)malloc(px_bytes);
	if (!pixels) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	const float inv_ss = 1.0f / (float)SS;
	const float inv_samples = 1.0f / (float)(SS * SS);

	for (int py = 0; py < ICON_SIZE; py++) {
		for (int px = 0; px < ICON_SIZE; px++) {
			float r_acc = 0.0f, g_acc = 0.0f, b_acc = 0.0f, a_acc = 0.0f;
			for (int sy = 0; sy < SS; sy++) {
				for (int sx = 0; sx < SS; sx++) {
					float fx = (float)px + ((float)sx + 0.5f) * inv_ss;
					float fy = (float)py + ((float)sy + 0.5f) * inv_ss;
					RGBA s = eval_pixel(fx, fy);
					r_acc += s.r * s.a;
					g_acc += s.g * s.a;
					b_acc += s.b * s.a;
					a_acc += s.a;
				}
			}
			float a = clampf(a_acc * inv_samples, 0.0f, 1.0f);
			float r = (a_acc > 1e-6f) ? clampf(r_acc / a_acc, 0.0f, 1.0f) : 0.0f;
			float g = (a_acc > 1e-6f) ? clampf(g_acc / a_acc, 0.0f, 1.0f) : 0.0f;
			float b = (a_acc > 1e-6f) ? clampf(b_acc / a_acc, 0.0f, 1.0f) : 0.0f;

			// 32-bit TGA stores BGRA
			size_t idx = ((size_t)py * ICON_SIZE + (size_t)px) * 4;
			pixels[idx + 0] = (uint8_t)lrintf(b * 255.0f);
			pixels[idx + 1] = (uint8_t)lrintf(g * 255.0f);
			pixels[idx + 2] = (uint8_t)lrintf(r * 255.0f);
			pixels[idx + 3] = (uint8_t)lrintf(a * 255.0f);
		}
	}

	FILE *f = fopen(argv[1], "wb");
	if (!f) {
		perror("fopen");
		free(pixels);
		return 1;
	}

	// Uncompressed 32-bit BGRA top-down TGA header (18 bytes)
	uint8_t header[18] = {0};
	header[2]  = 2;                                // Uncompressed true-color
	header[12] = (uint8_t)(ICON_SIZE & 0xFF);      // Width LSB
	header[13] = (uint8_t)((ICON_SIZE >> 8) & 0xFF);
	header[14] = (uint8_t)(ICON_SIZE & 0xFF);      // Height LSB
	header[15] = (uint8_t)((ICON_SIZE >> 8) & 0xFF);
	header[16] = 32;                               // 32 bits per pixel
	header[17] = 0x28;                             // Top-left origin + 8 alpha bits

	fwrite(header, 1, sizeof(header), f);
	fwrite(pixels, 1, px_bytes, f);
	fclose(f);
	free(pixels);
	return 0;
}
