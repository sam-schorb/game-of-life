#!/bin/bash

# Build script for Game of Life tests
echo "Building Game of Life Test Suite..."

# Compile the tests
clang++ -std=c++20 \
    -I. \
    test_game_of_life.cpp \
    -o test_game_of_life

if [ $? -eq 0 ]; then
    echo "✓ Test build successful!"
    echo ""
    echo "Run tests with: ./test_game_of_life"
    echo "Or run: ./run_tests.sh"
else
    echo "✗ Test build failed!"
    exit 1
fi