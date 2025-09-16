#!/bin/bash

# Build script for Game of Life tests
echo "Building Game of Life Test Suite..."

set -euo pipefail

METAL_SRC="GameOfLife.metal"
AIR_FILE="GameOfLife_tests.air"
METAL_LIB="GameOfLife.metallib"

echo "Compiling Metal shaders for tests..."
xcrun metal -c "${METAL_SRC}" -o "${AIR_FILE}"
xcrun metallib "${AIR_FILE}" -o "${METAL_LIB}"
trap 'rm -f "${AIR_FILE}"' EXIT

# Compile the tests
clang++ -std=c++20 -fobjc-arc \
    -I. \
    test_game_of_life.cpp \
    gpu_calculator.mm \
    -framework Metal \
    -framework MetalKit \
    -framework Foundation \
    -o test_game_of_life

echo "✓ Test build successful!"
echo ""
echo "Run tests with: ./test_game_of_life"
echo "Or run: ./run_tests.sh"
