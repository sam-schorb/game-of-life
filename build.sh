#!/bin/bash

# Build script for Massive Game of Life
# Requires libpng (install with: brew install libpng)

set -euo pipefail

METAL_SRC="GameOfLife.metal"
AIR_FILE="GameOfLife.air"
METAL_LIB="GameOfLife.metallib"

echo "Compiling Metal shaders..."
xcrun metal -c "${METAL_SRC}" -o "${AIR_FILE}"
xcrun metallib "${AIR_FILE}" -o "${METAL_LIB}"

trap 'rm -f "${AIR_FILE}"' EXIT

echo "Building Massive Game of Life..."

clang++ -std=c++20 -fobjc-arc \
    -I./include \
    -I/opt/homebrew/include \
    MassiveGameOfLife.cpp \
    gpu_calculator.mm \
    -framework Metal \
    -framework MetalKit \
    -framework Foundation \
    -framework OpenGL \
    -framework GLUT \
    -framework Carbon \
    -L/opt/homebrew/lib \
    -lpng \
    -lpthread \
    -o MassiveGameOfLife

echo "Build successful! Run with: ./MassiveGameOfLife"
