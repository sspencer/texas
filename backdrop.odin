package main

import "core:math"
import "core:math/rand"
import rl "vendor:raylib"

Backdrop_Style :: enum {
	Constellation,
	Flow_Waves,
	Geo_Drift,
	Orbital_Contours,
	Truchet_Lattice,
}

Backdrop_Palette :: struct {
	bg_top_left:     rl.Color,
	bg_top_right:    rl.Color,
	bg_bottom_left:  rl.Color,
	bg_bottom_right: rl.Color,
	accent_a:        rl.Color,
	accent_b:        rl.Color,
	accent_c:        rl.Color,
}

BACKDROP_MAX_NODES :: 38
BACKDROP_MAX_WAVES :: 7
BACKDROP_MAX_SHAPES :: 26
BACKDROP_MAX_ORBITS :: 4
BACKDROP_LATTICE_COLS :: 16
BACKDROP_LATTICE_ROWS :: 10
BACKDROP_WRAP_MARGIN :: 0.12

Backdrop_Node :: struct {
	x, y:      f32,
	vx, vy:    f32,
	radius:    f32,
	phase:     f32,
	color_idx: u8,
}

Backdrop_Wave :: struct {
	base_y:    f32,
	amp1:      f32,
	amp2:      f32,
	freq1:     f32,
	freq2:     f32,
	speed1:    f32,
	speed2:    f32,
	phase1:    f32,
	phase2:    f32,
	thickness: f32,
	tilt:      f32,
	color_idx: u8,
}

Backdrop_Shape :: struct {
	x, y:      f32,
	vx, vy:    f32,
	size:      f32,
	angle:     f32,
	rot_speed: f32,
	sides:     i32, // 0 = circle/ring, 3..8 = regular polygon
	filled:    bool,
	nested:    bool,
	color_idx: u8,
}

Backdrop_Orbit :: struct {
	cx, cy:      f32,
	orbit_rx:    f32,
	orbit_ry:    f32,
	orbit_speed: f32,
	orbit_phase: f32,
	base_radius: f32,
	ring_step:   f32,
	ring_count:  int,
	harm_k1:     f32,
	harm_k2:     f32,
	harm_a1:     f32,
	harm_a2:     f32,
	morph_speed: f32,
	morph_phase: f32,
	color_idx:   u8,
}

Lattice_Motif :: enum {
	Quarter_Arcs,
	Diagonal_Weave,
	Corner_Ticks,
}

Backdrop :: struct {
	initialized:   bool,
	style:         Backdrop_Style,
	palette:       Backdrop_Palette,
	time:          f64,
	// Constellation
	nodes:         [BACKDROP_MAX_NODES]Backdrop_Node,
	node_count:    int,
	// Flow_Waves
	waves:         [BACKDROP_MAX_WAVES]Backdrop_Wave,
	wave_count:    int,
	// Geo_Drift
	shapes:        [BACKDROP_MAX_SHAPES]Backdrop_Shape,
	shape_count:   int,
	// Orbital_Contours
	orbits:        [BACKDROP_MAX_ORBITS]Backdrop_Orbit,
	orbit_count:   int,
	// Truchet_Lattice
	lattice_motif: Lattice_Motif,
	lattice_cells: [BACKDROP_LATTICE_ROWS][BACKDROP_LATTICE_COLS]u8,
	lattice_phase: f32,
	lattice_angle: f32,
}

// Curated low-contrast, dark muted palettes so patterns remain calm and non-distracting.
@(rodata)
BACKDROP_PALETTES := [6]Backdrop_Palette{
	// Slate Twilight (homage to original slate blue-gray backdrop)
	{
		bg_top_left     = {28, 40, 56, 255},
		bg_top_right    = {34, 48, 66, 255},
		bg_bottom_left  = {42, 56, 74, 255},
		bg_bottom_right = {32, 46, 62, 255},
		accent_a        = {96, 126, 158, 255},
		accent_b        = {76, 106, 138, 255},
		accent_c        = {118, 144, 172, 255},
	},
	// Midnight Indigo
	{
		bg_top_left     = {26, 30, 50, 255},
		bg_top_right    = {34, 38, 60, 255},
		bg_bottom_left  = {40, 44, 68, 255},
		bg_bottom_right = {30, 34, 54, 255},
		accent_a        = {98, 108, 152, 255},
		accent_b        = {80, 90, 136, 255},
		accent_c        = {120, 128, 166, 255},
	},
	// Deep Fjord (muted teal-slate)
	{
		bg_top_left     = {24, 42, 50, 255},
		bg_top_right    = {30, 50, 58, 255},
		bg_bottom_left  = {36, 56, 64, 255},
		bg_bottom_right = {28, 46, 54, 255},
		accent_a        = {84, 126, 136, 255},
		accent_b        = {68, 108, 118, 255},
		accent_c        = {108, 144, 152, 255},
	},
	// Dusk Plum (dark charcoal-violet)
	{
		bg_top_left     = {34, 30, 46, 255},
		bg_top_right    = {42, 36, 54, 255},
		bg_bottom_left  = {48, 42, 60, 255},
		bg_bottom_right = {38, 34, 50, 255},
		accent_a        = {114, 100, 136, 255},
		accent_b        = {94, 84, 118, 255},
		accent_c        = {132, 118, 150, 255},
	},
	// Quiet Pine (dark sage-slate)
	{
		bg_top_left     = {26, 40, 40, 255},
		bg_top_right    = {32, 48, 46, 255},
		bg_bottom_left  = {38, 54, 52, 255},
		bg_bottom_right = {30, 44, 44, 255},
		accent_a        = {86, 118, 114, 255},
		accent_b        = {72, 102, 98, 255},
		accent_c        = {108, 136, 130, 255},
	},
	// Warm Charcoal (smoky graphite)
	{
		bg_top_left     = {34, 36, 44, 255},
		bg_top_right    = {42, 44, 52, 255},
		bg_bottom_left  = {48, 50, 58, 255},
		bg_bottom_right = {38, 40, 48, 255},
		accent_a        = {112, 114, 126, 255},
		accent_b        = {96, 100, 114, 255},
		accent_c        = {130, 128, 138, 255},
	},
}

backdrop_rand_range :: proc(lo, hi: f32) -> f32 {
	return lo + rand.float32() * (hi - lo)
}

backdrop_rand_signed :: proc(min_abs, max_abs: f32) -> f32 {
	mag := backdrop_rand_range(min_abs, max_abs)
	return mag if rand.int_max(2) == 0 else -mag
}

backdrop_jitter_color :: proc(c: rl.Color, delta: int) -> rl.Color {
	shift :: proc(v: u8, d: int) -> u8 {
		n := int(v) + rand.int_max(d * 2 + 1) - d
		return u8(clamp(n, 14, 180))
	}
	return rl.Color{shift(c.r, delta), shift(c.g, delta), shift(c.b, delta), c.a}
}

backdrop_palette_accent :: proc(p: Backdrop_Palette, idx: u8) -> rl.Color {
	switch idx % 3 {
	case 0:
		return p.accent_a
	case 1:
		return p.accent_b
	case:
		return p.accent_c
	}
}

color_with_alpha :: proc(c: rl.Color, alpha: u8) -> rl.Color {
	return rl.Color{c.r, c.g, c.b, alpha}
}

// Randomizes the generative backdrop style, muted palette, and procedural parameters.
// When avoid_prev_style is true and the backdrop was already initialized, picks a different style.
init_backdrop :: proc(b: ^Backdrop, avoid_prev_style: bool = false) {
	style_count := len(Backdrop_Style)
	chosen := Backdrop_Style(rand.int_max(style_count))
	if avoid_prev_style && b.initialized && style_count > 1 && chosen == b.style {
		offset := 1 + rand.int_max(style_count - 1)
		chosen = Backdrop_Style((int(b.style) + offset) % style_count)
	}
	init_backdrop_style(b, chosen)
}

// Initializes the generative backdrop with a specific style and freshly randomized palette & shapes.
init_backdrop_style :: proc(b: ^Backdrop, style: Backdrop_Style) {
	b.initialized = true
	b.style = style
	b.time = 0

	base := BACKDROP_PALETTES[rand.int_max(len(BACKDROP_PALETTES))]
	b.palette = Backdrop_Palette{
		bg_top_left     = backdrop_jitter_color(base.bg_top_left, 4),
		bg_top_right    = backdrop_jitter_color(base.bg_top_right, 4),
		bg_bottom_left  = backdrop_jitter_color(base.bg_bottom_left, 4),
		bg_bottom_right = backdrop_jitter_color(base.bg_bottom_right, 4),
		accent_a        = backdrop_jitter_color(base.accent_a, 8),
		accent_b        = backdrop_jitter_color(base.accent_b, 8),
		accent_c        = backdrop_jitter_color(base.accent_c, 8),
	}

	switch style {
	case .Constellation:
		b.node_count = 28 + rand.int_max(BACKDROP_MAX_NODES - 28 + 1)
		for i in 0 ..< b.node_count {
			b.nodes[i] = Backdrop_Node{
				x         = backdrop_rand_range(-0.04, 1.04),
				y         = backdrop_rand_range(-0.04, 1.04),
				vx        = backdrop_rand_signed(0.003, 0.011),
				vy        = backdrop_rand_signed(0.003, 0.011),
				radius    = backdrop_rand_range(1.8, 3.6),
				phase     = backdrop_rand_range(0, math.TAU),
				color_idx = u8(rand.int_max(3)),
			}
		}

	case .Flow_Waves:
		b.wave_count = 5 + rand.int_max(BACKDROP_MAX_WAVES - 5 + 1)
		for i in 0 ..< b.wave_count {
			t := (f32(i) + 0.5) / f32(b.wave_count)
			b.waves[i] = Backdrop_Wave{
				base_y    = clamp(t + backdrop_rand_range(-0.06, 0.06), 0.08, 0.92),
				amp1      = backdrop_rand_range(0.03, 0.08),
				amp2      = backdrop_rand_range(0.012, 0.035),
				freq1     = backdrop_rand_range(1.1, 2.4),
				freq2     = backdrop_rand_range(2.6, 4.8),
				speed1    = backdrop_rand_signed(0.08, 0.22),
				speed2    = backdrop_rand_signed(0.10, 0.26),
				phase1    = backdrop_rand_range(0, math.TAU),
				phase2    = backdrop_rand_range(0, math.TAU),
				thickness = backdrop_rand_range(0.06, 0.14),
				tilt      = backdrop_rand_range(-0.10, 0.10),
				color_idx = u8(i % 3),
			}
		}

	case .Geo_Drift:
		sides_pool := [6]i32{0, 3, 4, 5, 6, 8}
		b.shape_count = 18 + rand.int_max(BACKDROP_MAX_SHAPES - 18 + 1)
		for i in 0 ..< b.shape_count {
			b.shapes[i] = Backdrop_Shape{
				x         = backdrop_rand_range(-0.05, 1.05),
				y         = backdrop_rand_range(-0.05, 1.05),
				vx        = backdrop_rand_signed(0.0025, 0.009),
				vy        = backdrop_rand_signed(0.0025, 0.009),
				size      = backdrop_rand_range(0.035, 0.105),
				angle     = backdrop_rand_range(0, 360),
				rot_speed = backdrop_rand_signed(1.8, 6.5),
				sides     = sides_pool[rand.int_max(len(sides_pool))],
				filled    = rand.int_max(100) < 42,
				nested    = rand.int_max(100) < 40,
				color_idx = u8(rand.int_max(3)),
			}
		}

	case .Orbital_Contours:
		b.orbit_count = 2 + rand.int_max(BACKDROP_MAX_ORBITS - 2 + 1)
		for i in 0 ..< b.orbit_count {
			// Spread focal centers across different regions of the viewport
			sector_x := (f32(i) + 0.5) / f32(b.orbit_count)
			b.orbits[i] = Backdrop_Orbit{
				cx          = clamp(sector_x + backdrop_rand_range(-0.14, 0.14), 0.12, 0.88),
				cy          = backdrop_rand_range(0.20, 0.80),
				orbit_rx    = backdrop_rand_range(0.03, 0.09),
				orbit_ry    = backdrop_rand_range(0.03, 0.09),
				orbit_speed = backdrop_rand_signed(0.05, 0.13),
				orbit_phase = backdrop_rand_range(0, math.TAU),
				base_radius = backdrop_rand_range(0.11, 0.20),
				ring_step   = backdrop_rand_range(0.048, 0.075),
				ring_count  = 3 + rand.int_max(3),
				harm_k1     = f32(2 + rand.int_max(4)),
				harm_k2     = f32(3 + rand.int_max(5)),
				harm_a1     = backdrop_rand_range(0.04, 0.10),
				harm_a2     = backdrop_rand_range(0.025, 0.07),
				morph_speed = backdrop_rand_signed(0.12, 0.25),
				morph_phase = backdrop_rand_range(0, math.TAU),
				color_idx   = u8(i % 3),
			}
		}

	case .Truchet_Lattice:
		b.lattice_motif = Lattice_Motif(rand.int_max(len(Lattice_Motif)))
		b.lattice_phase = backdrop_rand_range(0, math.TAU)
		b.lattice_angle = backdrop_rand_range(0, math.TAU)
		for r in 0 ..< BACKDROP_LATTICE_ROWS {
			for c in 0 ..< BACKDROP_LATTICE_COLS {
				b.lattice_cells[r][c] = u8(rand.int_max(4))
			}
		}
	}
}

backdrop_wrap_coord :: proc(v: f32) -> f32 {
	lo :: f32(-BACKDROP_WRAP_MARGIN)
	hi :: f32(1.0 + BACKDROP_WRAP_MARGIN)
	span := hi - lo
	res := v
	for res < lo {
		res += span
	}
	for res > hi {
		res -= span
	}
	return res
}

// Advances the generative backdrop's gentle ambient motion.
update_backdrop :: proc(b: ^Backdrop, frame_dt: f64) {
	if !b.initialized || frame_dt <= 0 {
		return
	}
	dt := f32(frame_dt)
	b.time += frame_dt

	switch b.style {
	case .Constellation:
		for i in 0 ..< b.node_count {
			n := &b.nodes[i]
			n.x = backdrop_wrap_coord(n.x + n.vx * dt)
			n.y = backdrop_wrap_coord(n.y + n.vy * dt)
		}

	case .Geo_Drift:
		for i in 0 ..< b.shape_count {
			s := &b.shapes[i]
			s.x = backdrop_wrap_coord(s.x + s.vx * dt)
			s.y = backdrop_wrap_coord(s.y + s.vy * dt)
			s.angle = math.mod(s.angle + s.rot_speed * dt, 360.0)
			if s.angle < 0 {
				s.angle += 360.0
			}
		}

	case .Flow_Waves, .Orbital_Contours, .Truchet_Lattice:
		// Purely time-driven parametric motion using b.time
	}
}

// Draws a filled triangle ensuring counter-clockwise screen winding (+x right, +y down).
draw_triangle_ccw :: proc(a, b, c: rl.Vector2, color: rl.Color) {
	cross := (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
	if cross < 0 {
		rl.DrawTriangle(a, b, c, color)
	} else {
		rl.DrawTriangle(a, c, b, color)
	}
}

// Draws a smooth arc polyline using standard radians (0 = +x, pi/2 = +y).
draw_arc_lines :: proc(
	center: rl.Vector2,
	radius: f32,
	start_rad, end_rad: f32,
	segments: int,
	thick: f32,
	color: rl.Color,
) {
	if segments <= 0 || radius <= 0 {
		return
	}
	prev := rl.Vector2{
		center.x + math.cos(start_rad) * radius,
		center.y + math.sin(start_rad) * radius,
	}
	step := (end_rad - start_rad) / f32(segments)
	for i in 1 ..= segments {
		ang := start_rad + f32(i) * step
		cur := rl.Vector2{
			center.x + math.cos(ang) * radius,
			center.y + math.sin(ang) * radius,
		}
		rl.DrawLineEx(prev, cur, thick, color)
		prev = cur
	}
}

// Renders the full-viewport generative backdrop in logical screen coordinates.
draw_backdrop :: proc(b: ^Backdrop, width, height: f32) {
	if !b.initialized || width <= 0 || height <= 0 {
		return
	}

	// 1. Subtle 4-corner atmospheric gradient base
	full_rec := rl.Rectangle{0, 0, width, height}
	rl.DrawRectangleGradientEx(
		full_rec,
		b.palette.bg_top_left,
		b.palette.bg_bottom_left,
		b.palette.bg_bottom_right,
		b.palette.bg_top_right,
	)

	// 2. Active generative art pattern
	switch b.style {
	case .Constellation:
		draw_backdrop_constellation(b, width, height)
	case .Flow_Waves:
		draw_backdrop_flow_waves(b, width, height)
	case .Geo_Drift:
		draw_backdrop_geo_drift(b, width, height)
	case .Orbital_Contours:
		draw_backdrop_orbital_contours(b, width, height)
	case .Truchet_Lattice:
		draw_backdrop_truchet_lattice(b, width, height)
	}

	// 3. Soft edge framing so outer margins gently recede behind the gameboard
	edge_dark := rl.Color{8, 12, 20, 48}
	edge_clear := rl.Color{8, 12, 20, 0}
	band_y := math.max(height * 0.14, 24.0)
	rl.DrawRectangleGradientV(0, 0, i32(math.ceil(width)), i32(math.ceil(band_y)), edge_dark, edge_clear)
	rl.DrawRectangleGradientV(
		0,
		i32(math.floor(height - band_y)),
		i32(math.ceil(width)),
		i32(math.ceil(band_y)) + 1,
		edge_clear,
		edge_dark,
	)
}

draw_backdrop_constellation :: proc(b: ^Backdrop, width, height: f32) {
	min_dim := math.min(width, height)
	link_dist := clamp(min_dim * 0.25, 110.0, 260.0)
	link_dist_sq := link_dist * link_dist
	tri_dist_sq := (link_dist * 0.72) * (link_dist * 0.72)

	pos: [BACKDROP_MAX_NODES]rl.Vector2
	for i in 0 ..< b.node_count {
		pos[i] = rl.Vector2{b.nodes[i].x * width, b.nodes[i].y * height}
	}

	// Subtle translucent mesh facets (capped to keep composition airy)
	tri_drawn := 0
	for i in 0 ..< b.node_count {
		if tri_drawn >= 20 {
			break
		}
		for j := i + 1; j < b.node_count && tri_drawn < 20; j += 1 {
			dx_ij := pos[j].x - pos[i].x
			dy_ij := pos[j].y - pos[i].y
			d2_ij := dx_ij * dx_ij + dy_ij * dy_ij
			if d2_ij > tri_dist_sq {
				continue
			}
			for k := j + 1; k < b.node_count && tri_drawn < 20; k += 1 {
				dx_ik := pos[k].x - pos[i].x
				dy_ik := pos[k].y - pos[i].y
				d2_ik := dx_ik * dx_ik + dy_ik * dy_ik
				if d2_ik > tri_dist_sq {
					continue
				}
				dx_jk := pos[k].x - pos[j].x
				dy_jk := pos[k].y - pos[j].y
				d2_jk := dx_jk * dx_jk + dy_jk * dy_jk
				if d2_jk > tri_dist_sq {
					continue
				}
				max_d := math.sqrt(math.max(d2_ij, math.max(d2_ik, d2_jk)))
				fade := clamp(1.0 - max_d / (link_dist * 0.72), 0.0, 1.0)
				alpha := u8(fade * fade * 14.0)
				if alpha > 0 {
					c := color_with_alpha(backdrop_palette_accent(b.palette, b.nodes[i].color_idx), alpha)
					draw_triangle_ccw(pos[i], pos[j], pos[k], c)
					tri_drawn += 1
				}
			}
		}
	}

	// Connecting edges fading smoothly with distance
	for i in 0 ..< b.node_count {
		for j := i + 1; j < b.node_count; j += 1 {
			dx := pos[j].x - pos[i].x
			dy := pos[j].y - pos[i].y
			d2 := dx * dx + dy * dy
			if d2 >= link_dist_sq {
				continue
			}
			fade := 1.0 - math.sqrt(d2) / link_dist
			alpha := u8(fade * fade * 40.0)
			if alpha > 1 {
				c := color_with_alpha(backdrop_palette_accent(b.palette, b.nodes[i].color_idx), alpha)
				rl.DrawLineEx(pos[i], pos[j], 1.1, c)
			}
		}
	}

	// Nodes with whisper-soft halos
	t := f32(b.time)
	for i in 0 ..< b.node_count {
		n := b.nodes[i]
		pulse := 0.8 + 0.2 * math.sin(t * 0.65 + n.phase)
		accent := backdrop_palette_accent(b.palette, n.color_idx)
		rl.DrawCircleV(pos[i], n.radius * 2.3, color_with_alpha(accent, u8(12.0 * pulse)))
		rl.DrawCircleV(pos[i], n.radius, color_with_alpha(accent, u8(36.0 * pulse)))
	}
}

draw_backdrop_flow_waves :: proc(b: ^Backdrop, width, height: f32) {
	WAVE_SAMPLES :: 44
	t := f32(b.time)

	strip: [WAVE_SAMPLES * 2]rl.Vector2
	crest: [WAVE_SAMPLES]rl.Vector2

	for w_idx in 0 ..< b.wave_count {
		w := b.waves[w_idx]
		accent := backdrop_palette_accent(b.palette, w.color_idx)
		half_thick := w.thickness * height * 0.5

		for s in 0 ..< WAVE_SAMPLES {
			u := -0.02 + 1.04 * (f32(s) / f32(WAVE_SAMPLES - 1))
			x := u * width
			y_norm := w.base_y +
				(u - 0.5) * w.tilt +
				w.amp1 * math.sin(u * w.freq1 * math.TAU + t * w.speed1 + w.phase1) +
				w.amp2 * math.cos(u * w.freq2 * math.TAU + t * w.speed2 + w.phase2)
			cy := y_norm * height
			thick_mod := 0.78 + 0.22 * math.sin(u * 2.3 * math.TAU - t * 0.18 + w.phase1)
			ht := half_thick * thick_mod

			// Strip order: top then bottom gives CCW triangles in screen space (+y down)
			strip[s * 2] = rl.Vector2{x, cy - ht}
			strip[s * 2 + 1] = rl.Vector2{x, cy + ht}
			crest[s] = rl.Vector2{x, cy - ht}
		}

		rl.DrawTriangleStrip(&strip[0], WAVE_SAMPLES * 2, color_with_alpha(accent, 14))

		// Delicate crest line along the top edge of each ribbon
		line_color := color_with_alpha(accent, 30)
		for s in 0 ..< WAVE_SAMPLES - 1 {
			rl.DrawLineEx(crest[s], crest[s + 1], 1.2, line_color)
		}
	}
}

draw_backdrop_geo_drift :: proc(b: ^Backdrop, width, height: f32) {
	min_dim := math.min(width, height)

	for i in 0 ..< b.shape_count {
		s := b.shapes[i]
		center := rl.Vector2{s.x * width, s.y * height}
		radius := s.size * min_dim
		accent := backdrop_palette_accent(b.palette, s.color_idx)

		if s.sides == 0 {
			if s.filled {
				rl.DrawCircleV(center, radius, color_with_alpha(accent, 14))
			}
			rl.DrawCircleLinesV(center, radius, color_with_alpha(accent, 30))
			if s.nested {
				rl.DrawCircleLinesV(center, radius * 0.58, color_with_alpha(accent, 20))
			}
		} else {
			if s.filled {
				rl.DrawPoly(center, s.sides, radius, s.angle, color_with_alpha(accent, 14))
			}
			rl.DrawPolyLinesEx(center, s.sides, radius, s.angle, 1.3, color_with_alpha(accent, 32))
			if s.nested {
				rl.DrawPolyLinesEx(
					center,
					s.sides,
					radius * 0.58,
					s.angle + 180.0 / f32(s.sides),
					1.0,
					color_with_alpha(accent, 20),
				)
			}
		}
	}
}

draw_backdrop_orbital_contours :: proc(b: ^Backdrop, width, height: f32) {
	CONTOUR_SEGMENTS :: 72
	t := f32(b.time)
	min_dim := math.min(width, height)

	pts: [CONTOUR_SEGMENTS + 1]rl.Vector2

	for o_idx in 0 ..< b.orbit_count {
		o := b.orbits[o_idx]
		cx := (o.cx + math.cos(t * o.orbit_speed + o.orbit_phase) * o.orbit_rx) * width
		cy := (o.cy + math.sin(t * o.orbit_speed * 0.83 + o.orbit_phase) * o.orbit_ry) * height
		accent := backdrop_palette_accent(b.palette, o.color_idx)

		// Soft ambient radial glow at the focal point
		glow_r := (o.base_radius + f32(o.ring_count) * o.ring_step) * min_dim * 0.85
		rl.DrawCircleGradient(
			rl.Vector2{cx, cy},
			glow_r,
			color_with_alpha(accent, 16),
			color_with_alpha(accent, 0),
		)

		for ring in 0 ..< o.ring_count {
			r_base := (o.base_radius + f32(ring) * o.ring_step) * min_dim
			ring_phase := o.morph_phase + f32(ring) * 0.65
			alpha := u8(clamp(32 - ring * 4, 14, 34))
			col := color_with_alpha(accent, alpha)

			for s in 0 ..= CONTOUR_SEGMENTS {
				ang := (f32(s) / f32(CONTOUR_SEGMENTS)) * math.TAU
				wobble := 1.0 +
					o.harm_a1 * math.sin(o.harm_k1 * ang + t * o.morph_speed + ring_phase) +
					o.harm_a2 * math.cos(o.harm_k2 * ang - t * o.morph_speed * 0.72 + ring_phase)
				r := r_base * wobble
				pts[s] = rl.Vector2{cx + math.cos(ang) * r, cy + math.sin(ang) * r}
			}

			for s in 0 ..< CONTOUR_SEGMENTS {
				rl.DrawLineEx(pts[s], pts[s + 1], 1.2, col)
			}
		}
	}
}

draw_backdrop_truchet_lattice :: proc(b: ^Backdrop, width, height: f32) {
	t := f32(b.time)
	min_dim := math.min(width, height)
	cell := clamp(min_dim / 8.5, 64.0, 128.0)

	cols := int(math.ceil(width / cell)) + 1
	rows := int(math.ceil(height / cell)) + 1
	ox := (width - f32(cols) * cell) * 0.5
	oy := (height - f32(rows) * cell) * 0.5

	dir_x := math.cos(b.lattice_angle)
	dir_y := math.sin(b.lattice_angle)

	for r in 0 ..< rows {
		for c in 0 ..< cols {
			x0 := ox + f32(c) * cell
			y0 := oy + f32(r) * cell
			x1 := x0 + cell
			y1 := y0 + cell
			half := cell * 0.5

			orient := b.lattice_cells[r % BACKDROP_LATTICE_ROWS][c % BACKDROP_LATTICE_COLS]
			wave := 0.5 + 0.5 * math.sin((f32(c) * dir_x + f32(r) * dir_y) * 0.45 - t * 0.35 + b.lattice_phase)
			alpha := u8(12.0 + wave * 22.0)
			accent := backdrop_palette_accent(b.palette, orient)
			col := color_with_alpha(accent, alpha)

			switch b.lattice_motif {
			case .Quarter_Arcs:
				if orient % 2 == 0 {
					// Top-left & bottom-right quarter circles
					draw_arc_lines(rl.Vector2{x0, y0}, half, 0, math.PI * 0.5, 14, 1.4, col)
					draw_arc_lines(rl.Vector2{x1, y1}, half, math.PI, math.PI * 1.5, 14, 1.4, col)
				} else {
					// Top-right & bottom-left quarter circles
					draw_arc_lines(rl.Vector2{x1, y0}, half, math.PI * 0.5, math.PI, 14, 1.4, col)
					draw_arc_lines(rl.Vector2{x0, y1}, half, math.PI * 1.5, math.TAU, 14, 1.4, col)
				}

			case .Diagonal_Weave:
				if orient % 2 == 0 {
					rl.DrawLineEx(rl.Vector2{x0, y0}, rl.Vector2{x1, y1}, 1.3, col)
					rl.DrawLineEx(rl.Vector2{x0 + half, y0}, rl.Vector2{x1, y0 + half}, 1.0, color_with_alpha(accent, alpha / 2))
				} else {
					rl.DrawLineEx(rl.Vector2{x1, y0}, rl.Vector2{x0, y1}, 1.3, col)
					rl.DrawLineEx(rl.Vector2{x0, y0 + half}, rl.Vector2{x0 + half, y1}, 1.0, color_with_alpha(accent, alpha / 2))
				}

			case .Corner_Ticks:
				// Minimalist architectural corner brackets & center dot
				arm := cell * 0.30
				switch orient % 4 {
				case 0:
					rl.DrawLineEx(rl.Vector2{x0 + arm, y0}, rl.Vector2{x0, y0}, 1.3, col)
					rl.DrawLineEx(rl.Vector2{x0, y0}, rl.Vector2{x0, y0 + arm}, 1.3, col)
				case 1:
					rl.DrawLineEx(rl.Vector2{x1 - arm, y0}, rl.Vector2{x1, y0}, 1.3, col)
					rl.DrawLineEx(rl.Vector2{x1, y0}, rl.Vector2{x1, y0 + arm}, 1.3, col)
				case 2:
					rl.DrawLineEx(rl.Vector2{x1 - arm, y1}, rl.Vector2{x1, y1}, 1.3, col)
					rl.DrawLineEx(rl.Vector2{x1, y1}, rl.Vector2{x1, y1 - arm}, 1.3, col)
				case:
					rl.DrawLineEx(rl.Vector2{x0 + arm, y1}, rl.Vector2{x0, y1}, 1.3, col)
					rl.DrawLineEx(rl.Vector2{x0, y1}, rl.Vector2{x0, y1 - arm}, 1.3, col)
				}
				rl.DrawCircleV(rl.Vector2{x0 + half, y0 + half}, 1.8, color_with_alpha(accent, alpha))
			}
		}
	}
}
