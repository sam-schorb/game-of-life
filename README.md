# Massive Game of Life

A high-performance implementation of Conway's Game of Life using sparse encoding for massive simulations.

## Features
- **Sparse Encoding** - Only tracks active cells for efficient memory usage
- **Infinite World** - Pan and zoom through unlimited space
- **Interactive Editing** - Add cells and patterns in real-time
- **Laptop-Friendly Controls** - Designed for trackpad and keyboard use

## Controls

### Navigation
- **Arrow Keys** - Pan around the world (Up/Down/Left/Right)
- **Q Key** - Zoom out (centered on screen)
- **E Key** - Zoom in (centered on screen)
- **Mouse Wheel** - Zoom in/out (if available)

### Simulation
- **Spacebar** - Toggle between EDIT mode and LIVE simulation mode

### Editing (Edit Mode Only)
- **Click & Drag** - Paint cells with current brush size
- **Right Click** or **R Key** - Add a random cluster of cells
- **C Key** - Clear all cells
- **+/= Key** - Increase brush size (1-10)
- **-/_ Key** - Decrease brush size

### Visual Features
- **Mode Indicator** - Shows whether you're in EDIT or LIVE mode
- **Brush Preview** - Yellow outline shows current brush size and position
- **Precise Placement** - Cells appear exactly where you click

## Building

Requires:
- C++20 compatible compiler
- libpng (via Homebrew: `brew install libpng`)
- OpenGL and GLUT frameworks (included on macOS)

```bash
clang++ -std=c++20 -I./include -I/opt/homebrew/include MassiveGameOfLife.cpp \
    -framework OpenGL -framework GLUT -framework Carbon \
    -L/opt/homebrew/lib -lpng -lpthread -o MassiveGameOfLife
```

## Running

```bash
./MassiveGameOfLife
```

## Credits

Based on the original implementation by javidx9 (OneLoneCoder)
- YouTube: https://www.youtube.com/javidx9
- GitHub: https://www.github.com/onelonecoder
- Original video: https://youtu.be/OqfHIujOvnE

Modified for laptop-friendly controls and improved usability.