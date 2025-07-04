package main

import  "core:math/rand"
import rl "vendor:raylib"

Direction :: enum{
    North,
    East,
    South,
    West,
}

// Colors found:
//   https://tetris.wiki/Tetris_Guideline
//   https://tetris.fandom.com/wiki/Tetris_Guideline
Tetro_Color := [8]rl.Color {
    rl.BLACK, // empty
    rl.SKYBLUE, // I
    rl.YELLOW, // O
    rl.DARKPURPLE, // T (or purple)
    rl.DARKGREEN, // S
    rl.RED, // Z
    rl.DARKBLUE, // J
    rl.ORANGE, // L
}

Tetromino :: struct {
    id: int,
    x: int,
    y: int,
    direction: Direction,
    size: int,
    shape: matrix[4, 4]i32, // 4x4 matrix for the piece
}

tetrominos : [8]Tetromino = {
    Tetromino{ },
    // I (4x4 matrix)
    Tetromino{
        size = 4,
        direction = .East,
        shape = matrix[4, 4]i32{
            0, 0, 0, 0,
            1, 1, 1, 1,
            0, 0, 0, 0,
            0, 0, 0, 0,
        }
    },

    // O (2x2 matrix)
    Tetromino{
        size = 2,
        direction = .East,
        shape = matrix[4, 4]i32{
            2, 2, 0, 0,
            2, 2, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        }
    },

    // T (3x3 matrix)
    Tetromino{
        direction = .East,
        size = 3,
        shape = matrix[4, 4]i32{
            0, 0, 0, 0,
            0, 3, 0, 0,
            3, 3, 3, 0,
            0, 0, 0, 0,
        }
    },

    // S (3x3 matrix)
    Tetromino{
        direction = .East,
        size = 3,
        shape = matrix[4, 4]i32{
            0, 0, 0, 0,
            0, 4, 4, 0,
            4, 4, 0, 0,
            0, 0, 0, 0,
        }
    },

    // Z (3x3 matrix)
    Tetromino{
        direction = .East,
        size = 3,
        shape = matrix[4, 4]i32{
            0, 0, 0, 0,
            5, 5, 0, 0,
            0, 5, 5, 0,
            0, 0, 0, 0,
        }
    },

    // J (3x3 matrix)
    Tetromino{
        direction = .East,
        size = 3,
        shape = matrix[4, 4]i32{
            0, 0, 0, 0,
            0, 0, 6, 0,
            6, 6, 6, 0,
            0, 0, 0, 0,
        }
    },

    // L (3x3 matrix)
    Tetromino{
        direction = .East,
        size = 3,
        shape = matrix[4, 4]i32{
            0, 0, 0, 0,
            7, 0, 0, 0,
            7, 7, 7, 0,
            0, 0, 0, 0
        }
    },
}


/*
Super Rotation System (SRS) Implementation

The SRS is the rotation system used in most modern Tetris games. It defines:
1. How pieces rotate (clockwise and counter-clockwise)
2. How wall kicks work (when a rotation would cause a collision)

Key aspects of SRS:
- Each piece has a specific rotation point
- When a rotation would cause a collision, the game tries to "kick" the piece
  into a valid position using predefined offsets
- Different pieces have different wall kick data (I piece vs. others)
- The rotation point is not always at the center of the piece

References:
- https://tetris.wiki/Super_Rotation_System
- https://harddrop.com/wiki/SRS
*/

// Wall kick data for J, L, S, T, Z tetrominos (SRS)
// Format: [rotation_state][test_number][x_offset, y_offset]
// rotation_state: 0=0->R, 1=R->2, 2=2->L, 3=L->0
JLSTZ_WALL_KICK_DATA := [4][5][2]int{
    { { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 } }, // 0->R
    { { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 } }, // R->2
    { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 } }, // 2->L
    { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 } }, // L->0
}

// Wall kick data for I tetromino (SRS)
// Format: [rotation_state][test_number][x_offset, y_offset]
I_WALL_KICK_DATA := [4][5][2]int{
    { { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, -1 }, { 1, 2 } }, // 0->R
    { { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, 2 }, { 2, -1 } }, // R->2
    { { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, 1 }, { -1, -2 } }, // 2->L
    { { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, -2 }, { -2, 1 } }, // L->0
}

rotate :: proc(t: ^Tetromino) {
// Update direction (counter-clockwise rotation)
    switch t.direction {
    case .North: t.direction = .West
    case .East: t.direction = .North
    case .South: t.direction = .East
    case .West: t.direction = .South
    }

    for i in 0 ..< t.size - 1 {
        for j in i + 1 ..< t.size{
        // Swap m[i,j] with m[j,i]
            t.shape[i, j], t.shape[j, i] = t.shape[j, i], t.shape[i, j]
        }
    }

    // Step 2: Reverse each row
    for i in 0 ..< t.size {
    // Reverse all elements in the row
        for j := 0; j < t.size / 2; j += 1 {
            t.shape[i, j], t.shape[i, t.size - 1 - j] = t.shape[i, t.size - 1 - j], t.shape[i, j]
        }
    }


}

// Rotates a tetromino counter-clockwise using the Super Rotation System (SRS)
// Includes wall kick tests to handle collisions during rotation
// If no valid position is found after wall kicks, the rotation is reverted
rotate_with_kick :: proc(g: ^Game) {
    t := &g.tetro
    // Save the original shape and position for wall kick tests
    original_shape := t.shape
    original_x := t.x
    original_y := t.y
    original_dir := t.direction

    rotate(t)

    // Apply wall kicks if needed
    // Determine which wall kick data to use
    wall_kick_data: [4][5][2]int
    if t.size == 4 {
        wall_kick_data = I_WALL_KICK_DATA
    } else {
        wall_kick_data = JLSTZ_WALL_KICK_DATA
    }

    // Get the rotation state index (0=0->R, 1=R->2, 2=2->L, 3=L->0)
    // For counter-clockwise rotation, we need to map the original direction to the correct rotation state
    rotation_state: int
    switch original_dir {
    case .North: rotation_state = 3  // North->West (L->0)
    case .East: rotation_state = 0   // East->North (0->R)
    case .South: rotation_state = 1  // South->East (R->2)
    case .West: rotation_state = 2   // West->South (2->L)
    }

    // Try each test position
    kick_applied := false
    for test_idx in 0 ..< 5 {
        test_offset_x := wall_kick_data[rotation_state][test_idx][0]
        test_offset_y := wall_kick_data[rotation_state][test_idx][1]

        // Apply offset
        test_x := original_x + test_offset_x
        test_y := original_y + test_offset_y

        // Check if this position is valid by ensuring it's within bounds and doesn't collide with the board
        t.x = test_x
        t.y = test_y
        if !collide(g) {
            kick_applied = true
            break
        }
    }

    // If no valid position found, revert to original state
    if !kick_applied {
        t.shape = original_shape
        t.x = original_x
        t.y = original_y
        t.direction = original_dir
    }

}


rotate_ccw :: proc(t: ^Tetromino) {
// Update direction (clockwise rotation)
    switch t.direction {
    case .North: t.direction = .East
    case .East: t.direction = .South
    case .South: t.direction = .West
    case .West: t.direction = .North
    }

    // Step 1: Transpose the matrix
    for i in 0 ..< t.size - 1 {
        for j in i + 1 ..< t.size {
        // Swap m[i,j] with m[j,i]
            t.shape[i, j], t.shape[j, i] = t.shape[j, i], t.shape[i, j]
        }
    }

    // Step 2: Reverse each column
    for j in 0 ..< t.size {
    // Reverse all elements in the column
        for i := 0; i < t.size / 2; i += 1 {
            t.shape[i, j], t.shape[t.size - 1 - i, j] = t.shape[t.size - 1 - i, j], t.shape[i, j]
        }
    }
}

// Rotates a tetromino clockwise using the Super Rotation System (SRS)
// Includes wall kick tests to handle collisions during rotation
// If no valid position is found after wall kicks, the rotation is reverted
rotate_ccw_with_kick :: proc(g: ^Game) {
// Save the original shape and position for wall kick tests
    t := &g.tetro
    original_shape := t.shape
    original_x := t.x
    original_y := t.y
    original_dir := t.direction

    rotate_ccw(t)

    // Apply wall kicks if needed
    // Determine which wall kick data to use
    wall_kick_data: [4][5][2]int
    if t.size == 4 {
        wall_kick_data = I_WALL_KICK_DATA
    } else {
        wall_kick_data = JLSTZ_WALL_KICK_DATA
    }

    // Get the rotation state index (0=0->R, 1=R->2, 2=2->L, 3=L->0)
    // For clockwise rotation, we need to map the original direction to the correct rotation state
    rotation_state: int
    switch original_dir {
    case .North: rotation_state = 0  // North->East (0->R)
    case .East: rotation_state = 1   // East->South (R->2)
    case .South: rotation_state = 2  // South->West (2->L)
    case .West: rotation_state = 3   // West->North (L->0)
    }

    // Try each test position
    kick_applied := false
    for test_idx in 0 ..< 5 {
        test_offset_x := wall_kick_data[rotation_state][test_idx][0]
        test_offset_y := wall_kick_data[rotation_state][test_idx][1]

        // Apply offset
        test_x := original_x + test_offset_x
        test_y := original_y + test_offset_y

        // Check if this position is valid by ensuring it's within bounds and doesn't collide with the board
        t.x = test_x
        t.y = test_y
        if !collide(g) {
            kick_applied = true
            break
        }
    }

    // If no valid position found, revert to original state
    if !kick_applied {
        t.shape = original_shape
        t.x = original_x
        t.y = original_y
        t.direction = original_dir
    }
}

set_tetromino_start :: proc(t: ^Tetromino) {
    t.x = GAME_WIDTH / 2 - t.size / 2
    if t.size == 3 {
        t.x -= 1
        t.y = -1
    } else {
        t.y = 0
    }
}

// Sets a tetromino to a specific shape from the predefined tetrominos array
// Also positions the tetromino at the top center of the game board
// Parameter n specifies which tetromino shape to use (1=I, 2=O, 3=T, 4=S, 5=Z, 6=J, 7=L)
set_tetromino :: proc(t: ^Tetromino, n: int) {
    n := clamp(n, 0, len(tetrominos))
    tetro := tetrominos[n]

    for i in 0 ..< 4 {
        t.shape[i, 0] = tetro.shape[i, 0]
        t.shape[i, 1] = tetro.shape[i, 1]
        t.shape[i, 2] = tetro.shape[i, 2]
        t.shape[i, 3] = tetro.shape[i, 3]
    }

    t.direction = tetro.direction
    t.size = tetro.size
    t.id = n

    set_tetromino_start(t)
}

copy_shape :: proc(src, dst: ^Tetromino) {
    for i in 0 ..< 4 {
        dst.shape[i, 0] = src.shape[i, 0]
        dst.shape[i, 1] = src.shape[i, 1]
        dst.shape[i, 2] = src.shape[i, 2]
        dst.shape[i, 3] = src.shape[i, 3]
    }

    dst.x = src.x
    dst.y = src.y
}

// Creates a random tetromino by selecting a random shape from the tetrominos array
// The tetromino is positioned at the top center of the game board
random_tetromino :: proc(t: ^Tetromino) {
    n := rand.int_max(len(tetrominos) - 1) + 1
    set_tetromino(t, n)
}
