package backdrop

import (
	"cmp"
	"math"
)

const (
	pi  = float32(math.Pi)
	tau = 2 * pi
)

func sin(x float32) float32     { return float32(math.Sin(float64(x))) }
func cos(x float32) float32     { return float32(math.Cos(float64(x))) }
func sqrt(x float32) float32    { return float32(math.Sqrt(float64(x))) }
func ceil(x float32) float32    { return float32(math.Ceil(float64(x))) }
func floor(x float32) float32   { return float32(math.Floor(float64(x))) }
func fmod(x, y float32) float32 { return float32(math.Mod(float64(x), float64(y))) }

func clamp[T cmp.Ordered](v, lo, hi T) T {
	return min(max(v, lo), hi)
}
