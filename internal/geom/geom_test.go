package geom

import "testing"

func TestRectContains(t *testing.T) {
	r := R(10, 20, 30, 40)
	tests := []struct {
		name string
		p    Vec2
		want bool
	}{
		{"inside", Pt(25, 35), true},
		{"top-left edge", Pt(10, 20), true},
		{"bottom-right edge", Pt(40, 60), true},
		{"left of", Pt(9.9, 30), false},
		{"below", Pt(20, 60.1), false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := r.Contains(tt.p); got != tt.want {
				t.Errorf("%v.Contains(%v) = %v, want %v", r, tt.p, got, tt.want)
			}
		})
	}
}

func TestRectCenterAndInset(t *testing.T) {
	r := R(0, 0, 100, 50)
	if got, want := r.Center(), Pt(50, 25); got != want {
		t.Errorf("Center() = %v, want %v", got, want)
	}
	if got, want := r.Inset(8), R(8, 8, 84, 34); got != want {
		t.Errorf("Inset(8) = %v, want %v", got, want)
	}
}
