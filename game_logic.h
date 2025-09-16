#pragma once

#include <unordered_set>
#include <cstdint>

// Simple 2D vector for coordinates (independent of olcPixelGameEngine)
struct Vec2i {
    int32_t x, y;

    Vec2i() : x(0), y(0) {}
    Vec2i(int32_t x, int32_t y) : x(x), y(y) {}

    Vec2i operator+(const Vec2i& other) const {
        return Vec2i(x + other.x, y + other.y);
    }

    bool operator==(const Vec2i& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Vec2i& other) const {
        return !(*this == other);
    }
};

// Hash function for Vec2i
struct Vec2iHash {
    std::size_t operator()(const Vec2i& v) const {
        return int64_t(v.y) << 32 | int64_t(v.x);
    }
};

// Game state representation
struct GameState {
    std::unordered_set<Vec2i, Vec2iHash> active;
    std::unordered_set<Vec2i, Vec2iHash> potential;

    GameState() = default;

    // Copy constructor
    GameState(const GameState& other) : active(other.active), potential(other.potential) {}

    // Assignment operator
    GameState& operator=(const GameState& other) {
        if (this != &other) {
            active = other.active;
            potential = other.potential;
        }
        return *this;
    }

    // Check if cell is alive
    bool isAlive(const Vec2i& cell) const {
        return active.contains(cell);
    }

    // Get number of living cells
    size_t getPopulation() const {
        return active.size();
    }

    // Clear all cells
    void clear() {
        active.clear();
        potential.clear();
    }

    // Add a living cell and mark neighbors as potential
    void addCell(const Vec2i& cell) {
        active.insert(cell);
        // Mark all 8 neighbors as potential for next generation
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                potential.insert(cell + Vec2i(dx, dy));
            }
        }
    }

    // Remove a living cell
    void removeCell(const Vec2i& cell) {
        active.erase(cell);
    }

    // Check if two game states are identical
    bool operator==(const GameState& other) const {
        return active == other.active;
    }

    bool operator!=(const GameState& other) const {
        return !(*this == other);
    }
};

// Core Game of Life calculation logic
class GameLogic {
public:
    // Calculate next generation using Conway's rules
    static void calculateNextGeneration(const GameState& current, GameState& next) {
        next.active.clear();
        next.potential.clear();
        next.potential.reserve(current.active.size());

        // All living cells are always potential for next generation
        for (const auto& cell : current.active) {
            next.potential.insert(cell);
        }

        // Process each potential cell
        for (const auto& cell : current.potential) {
            int neighborCount = countLivingNeighbors(current, cell);

            if (current.isAlive(cell)) {
                // Living cell rules
                if (neighborCount == 2 || neighborCount == 3) {
                    // Survives
                    next.active.insert(cell);
                } else {
                    // Dies - mark neighbors as potential for next generation
                    markNeighborsAsPotential(next, cell);
                }
            } else {
                // Dead cell rules
                if (neighborCount == 3) {
                    // Birth
                    next.active.insert(cell);
                    // Mark neighbors as potential for next generation
                    markNeighborsAsPotential(next, cell);
                }
            }
        }
    }

    // Add cells with brush (circle pattern)
    static void addCellsWithBrush(GameState& state, const Vec2i& center, int brushSize) {
        for (int dy = -brushSize + 1; dy < brushSize; dy++) {
            for (int dx = -brushSize + 1; dx < brushSize; dx++) {
                state.addCell(center + Vec2i(dx, dy));
            }
        }
    }

    // Add random cluster of cells
    static void addRandomCluster(GameState& state, const Vec2i& center, int clusterSize, unsigned int seed = 0) {
        if (seed != 0) {
            srand(seed); // Use seed for reproducible tests
        }

        for (int dy = -clusterSize; dy <= clusterSize; dy++) {
            for (int dx = -clusterSize; dx <= clusterSize; dx++) {
                if (rand() % 3 == 0) { // 33% density
                    state.addCell(center + Vec2i(dx, dy));
                }
            }
        }
    }

private:
    // Count living neighbors for a cell
    static int countLivingNeighbors(const GameState& state, const Vec2i& cell) {
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue; // Skip the cell itself
                if (state.isAlive(cell + Vec2i(dx, dy))) {
                    count++;
                }
            }
        }
        return count;
    }

    // Mark all neighbors of a cell as potential
    static void markNeighborsAsPotential(GameState& state, const Vec2i& cell) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                state.potential.insert(cell + Vec2i(dx, dy));
            }
        }
    }
};