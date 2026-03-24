# RAYCAST FPS — Terminal Raycasting Engine in C

A Wolfenstein/Doom-style raycasting FPS game that runs entirely in your terminal
using ANSI escape codes. Pure C, zero external dependencies beyond libc + libm.

## Build & Run

```bash
# Build
make

# Run
./fps

# Or one-liner:
make run
```

**Requirements:** GCC + a terminal that supports 256-color ANSI escape codes
(iTerm2, GNOME Terminal, Konsole, Windows Terminal, etc.). Recommended size: 130×46+.

## Controls

| Key       | Action              |
|-----------|---------------------|
| W / S     | Move forward / back |
| A / D     | Rotate left / right |
| Q / E     | Strafe left / right |
| SPACE     | Shoot               |
| R         | Reload (+20 ammo)   |
| M         | Toggle minimap      |
| 1 / 2 / 3 | Switch map          |
| ESC       | Quit                |

## Features

- **Raycasting engine** — DDA algorithm, correct fisheye correction
- **3 maps** — The Dungeon, Blue Fortress, The Maze (4 wall types each)
- **12 enemies per map** — patrol, alert, shoot back at you
- **Minimap** — toggleable, shows walls + enemies
- **Depth-shaded walls** — 4 shade levels (█▓▒░) per wall type
- **HUD** — HP bar, ammo counter, kill count, current map
- **Damage flash** — red screen edges when hit
- **Muzzle flash** — visual feedback on shooting
- **Death screen** — respawn on the same map

## Architecture

```
fps.c
├── Terminal setup (raw mode, ANSI)
├── Map data (3 maps as char arrays)
├── DDA Raycaster (cast_ray)
├── Sprite renderer (enemies)
├── Game logic (movement, AI, combat)
└── HUD + minimap overlay
```
