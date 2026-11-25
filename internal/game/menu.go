package game

// Field is a selectable row on the title screen.
type Field uint8

// Title screen fields.
const (
	FieldSpeed Field = iota
	FieldHeight
)

// TargetKind classifies a pointer target on the title screen.
type TargetKind uint8

// Title screen pointer targets.
const (
	TargetNone TargetKind = iota
	TargetSpeed
	TargetHeight
	TargetStart
)

// Target identifies a clickable title-screen control: a speed or height tile
// (with its Value) or the start button.
type Target struct {
	Kind  TargetKind
	Value int
}

// Menu is the state of the title screen.
type Menu struct {
	Focus  Field  // field adjusted by the keyboard
	Speed  int    // selected starting level, 0..MaxStartSpeed
	Height int    // selected starting garbage height, 0..MaxStartHeight
	Hover  Target // control under the pointer
}

func (m *Menu) toggleFocus() {
	if m.Focus == FieldSpeed {
		m.Focus = FieldHeight
	} else {
		m.Focus = FieldSpeed
	}
}

// adjust nudges the focused field by delta, clamped to its range.
func (m *Menu) adjust(delta int) {
	switch m.Focus {
	case FieldSpeed:
		m.Speed = clamp(m.Speed+delta, 0, MaxStartSpeed)
	case FieldHeight:
		m.Height = clamp(m.Height+delta, 0, MaxStartHeight)
	}
}

// set selects value for the focused field, ignoring out-of-range values.
func (m *Menu) set(value int) {
	switch m.Focus {
	case FieldSpeed:
		if value >= 0 && value <= MaxStartSpeed {
			m.Speed = value
		}
	case FieldHeight:
		if value >= 0 && value <= MaxStartHeight {
			m.Height = value
		}
	}
}

func (g *Game) handleTitleInput(in Input) {
	g.pointTitle(in.Pointer, in.Click)
	if g.state != StateTitle {
		return
	}

	p := in.Pressed
	if p.Has(MenuSwitchField) {
		g.menu.toggleFocus()
	}
	if p.Has(MenuDecrease) {
		g.menu.adjust(-1)
	}
	if p.Has(MenuIncrease) {
		g.menu.adjust(1)
	}
	for d := range 10 {
		if p.Has(MenuDigit(d)) {
			g.menu.set(d)
		}
	}
	if p.Has(MenuConfirm) {
		g.startNewGame()
	}
}

// pointTitle updates hover state for the control under the pointer and acts
// on it when clicked.
func (g *Game) pointTitle(target Target, click bool) {
	g.menu.Hover = target
	if !click {
		return
	}
	switch target.Kind {
	case TargetSpeed:
		if target.Value >= 0 && target.Value <= MaxStartSpeed {
			g.menu.Speed = target.Value
			g.menu.Focus = FieldSpeed
		}
	case TargetHeight:
		if target.Value >= 0 && target.Value <= MaxStartHeight {
			g.menu.Height = target.Value
			g.menu.Focus = FieldHeight
		}
	case TargetStart:
		g.startNewGame()
	case TargetNone:
	}
}
