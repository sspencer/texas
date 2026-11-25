package tetromino

import "math/rand/v2"

// Bag is the Guideline "7-bag" randomizer: each run of seven pieces contains
// every kind exactly once, in random order.
//
// A Bag is not safe for concurrent use.
type Bag struct {
	rng    *rand.Rand
	pieces [Count]Kind
	next   int
}

// NewBag returns a freshly shuffled bag drawing randomness from rng.
func NewBag(rng *rand.Rand) *Bag {
	b := &Bag{rng: rng}
	b.refill()
	return b
}

func (b *Bag) refill() {
	b.pieces = [Count]Kind{I, O, T, S, Z, J, L}
	b.rng.Shuffle(len(b.pieces), func(i, j int) {
		b.pieces[i], b.pieces[j] = b.pieces[j], b.pieces[i]
	})
	b.next = 0
}

// Next draws the next kind, reshuffling a new bag when the current one is
// exhausted.
func (b *Bag) Next() Kind {
	if b.next >= len(b.pieces) {
		b.refill()
	}
	k := b.pieces[b.next]
	b.next++
	return k
}
