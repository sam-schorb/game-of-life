#!/bin/bash

# Build script for Massive Game of Life
# Requires libpng (install with: brew install libpng)

echo "Building Massive Game of Life..."

clang++ -std=c++20 \
    -I./include \
    -I/opt/homebrew/include \
    MassiveGameOfLife.cpp \
    -framework OpenGL \
    -framework GLUT \
    -framework Carbon \
    -L/opt/homebrew/lib \
    -lpng \
    -lpthread \
    -o MassiveGameOfLife

if [ $? -eq 0 ]; then
    echo "Build successful! Run with: ./MassiveGameOfLife"
else
    echo "Build failed!"
    exit 1
fi