#include "test_framework.h"
#include "game_logic.h"

// Global test runner
TestRunner runner;

// Test Conway's Game of Life Rules
void test_conways_rules() {
    GameState current, next;

    // Rule 1: Underpopulation - Living cell with < 2 neighbors dies
    current.clear();
    current.addCell(Vec2i(0, 0)); // Single living cell (0 neighbors)
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_DEAD(next, Vec2i(0, 0), "Rule 1: Single cell dies (underpopulation)");

    current.clear();
    current.addCell(Vec2i(0, 0)); // Center cell
    current.addCell(Vec2i(1, 0)); // One neighbor
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_DEAD(next, Vec2i(0, 0), "Rule 1: Cell with 1 neighbor dies (underpopulation)");

    // Rule 2: Survival - Living cell with 2 or 3 neighbors survives
    current.clear();
    current.addCell(Vec2i(0, 0)); // Center cell
    current.addCell(Vec2i(-1, 0)); // Left neighbor
    current.addCell(Vec2i(1, 0));  // Right neighbor
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_ALIVE(next, Vec2i(0, 0), "Rule 2: Cell with 2 neighbors survives");

    current.clear();
    current.addCell(Vec2i(0, 0)); // Center cell
    current.addCell(Vec2i(-1, 0)); // Left neighbor
    current.addCell(Vec2i(1, 0));  // Right neighbor
    current.addCell(Vec2i(0, 1));  // Bottom neighbor
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_ALIVE(next, Vec2i(0, 0), "Rule 2: Cell with 3 neighbors survives");

    // Rule 3: Overpopulation - Living cell with > 3 neighbors dies
    current.clear();
    current.addCell(Vec2i(0, 0)); // Center cell
    current.addCell(Vec2i(-1, 0)); // Left
    current.addCell(Vec2i(1, 0));  // Right
    current.addCell(Vec2i(0, -1)); // Top
    current.addCell(Vec2i(0, 1));  // Bottom
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_DEAD(next, Vec2i(0, 0), "Rule 3: Cell with 4 neighbors dies (overpopulation)");

    // Rule 4: Birth - Dead cell with exactly 3 neighbors becomes alive
    current.clear();
    current.addCell(Vec2i(-1, 0)); // Left of center
    current.addCell(Vec2i(1, 0));  // Right of center
    current.addCell(Vec2i(0, 1));  // Below center
    // Center (0,0) is dead but has 3 neighbors
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_ALIVE(next, Vec2i(0, 0), "Rule 4: Dead cell with 3 neighbors is born");

    // Dead cell with 2 neighbors stays dead
    current.clear();
    current.addCell(Vec2i(-1, 0)); // Left of center
    current.addCell(Vec2i(1, 0));  // Right of center
    // Center (0,0) is dead with only 2 neighbors
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_DEAD(next, Vec2i(0, 0), "Rule 4: Dead cell with 2 neighbors stays dead");
}

// Test neighbor counting in various configurations
void test_neighbor_counting() {
    GameState current, next;

    // Test all 8 directions
    current.clear();
    Vec2i center(0, 0);
    current.addCell(center);
    // Add all 8 neighbors
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx != 0 || dy != 0) {
                current.addCell(center + Vec2i(dx, dy));
            }
        }
    }
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_DEAD(next, center, "8 neighbors: Center cell dies from overpopulation");

    // Test diagonal neighbors
    current.clear();
    current.addCell(Vec2i(0, 0)); // Center
    current.addCell(Vec2i(-1, -1)); // Top-left
    current.addCell(Vec2i(1, 1));   // Bottom-right
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_ALIVE(next, Vec2i(0, 0), "Diagonal neighbors: Cell with 2 diagonal neighbors survives");

    // Test with large coordinates (edge case)
    current.clear();
    Vec2i large_center(1000000, 1000000);
    current.addCell(large_center);
    current.addCell(large_center + Vec2i(-1, 0));
    current.addCell(large_center + Vec2i(1, 0));
    current.addCell(large_center + Vec2i(0, 1));
    GameLogic::calculateNextGeneration(current, next);
    runner.ASSERT_CELL_ALIVE(next, large_center, "Large coordinates: Cell with 3 neighbors survives");
}

// Test known patterns - Still lifes (never change)
void test_still_lifes() {
    GameState block, next;

    // Block (2x2 square) - most basic still life
    block.addCell(Vec2i(0, 0));
    block.addCell(Vec2i(1, 0));
    block.addCell(Vec2i(0, 1));
    block.addCell(Vec2i(1, 1));
    runner.ASSERT_PATTERN_STABLE(block, 5, "Block still life");

    // Beehive
    GameState beehive;
    beehive.addCell(Vec2i(1, 0));
    beehive.addCell(Vec2i(2, 0));
    beehive.addCell(Vec2i(0, 1));
    beehive.addCell(Vec2i(3, 1));
    beehive.addCell(Vec2i(1, 2));
    beehive.addCell(Vec2i(2, 2));
    runner.ASSERT_PATTERN_STABLE(beehive, 3, "Beehive still life");
}

// Test oscillators (patterns that cycle)
void test_oscillators() {
    // Blinker (period 2) - simplest oscillator
    GameState blinker;
    blinker.addCell(Vec2i(0, 0));
    blinker.addCell(Vec2i(1, 0));
    blinker.addCell(Vec2i(2, 0));
    runner.ASSERT_PATTERN_CYCLES(blinker, 2, "Blinker oscillator");

    // Test intermediate state of blinker
    GameState next;
    GameLogic::calculateNextGeneration(blinker, next);
    runner.ASSERT_CELL_ALIVE(next, Vec2i(1, -1), "Blinker: Vertical form has top cell");
    runner.ASSERT_CELL_ALIVE(next, Vec2i(1, 0), "Blinker: Vertical form has center cell");
    runner.ASSERT_CELL_ALIVE(next, Vec2i(1, 1), "Blinker: Vertical form has bottom cell");

    // Toad (period 2)
    GameState toad;
    toad.addCell(Vec2i(1, 0));
    toad.addCell(Vec2i(2, 0));
    toad.addCell(Vec2i(3, 0));
    toad.addCell(Vec2i(0, 1));
    toad.addCell(Vec2i(1, 1));
    toad.addCell(Vec2i(2, 1));
    runner.ASSERT_PATTERN_CYCLES(toad, 2, "Toad oscillator");
}

// Test spaceship movement
void test_spaceships() {
    // Glider - moves diagonally
    GameState glider_gen0;
    glider_gen0.addCell(Vec2i(1, 0));
    glider_gen0.addCell(Vec2i(2, 1));
    glider_gen0.addCell(Vec2i(0, 2));
    glider_gen0.addCell(Vec2i(1, 2));
    glider_gen0.addCell(Vec2i(2, 2));

    // Glider should return to same pattern after 4 generations but moved
    GameState current = glider_gen0;
    GameState next;
    for (int i = 0; i < 4; i++) {
        GameLogic::calculateNextGeneration(current, next);
        current = next;
    }

    // After 4 generations, glider should be at position (+1, +1) from original
    runner.ASSERT_CELL_ALIVE(current, Vec2i(2, 1), "Glider: Moved cell after 4 generations");
    runner.ASSERT_CELL_ALIVE(current, Vec2i(3, 2), "Glider: Moved cell after 4 generations");
    runner.ASSERT_CELL_ALIVE(current, Vec2i(1, 3), "Glider: Moved cell after 4 generations");
    runner.ASSERT_CELL_ALIVE(current, Vec2i(2, 3), "Glider: Moved cell after 4 generations");
    runner.ASSERT_CELL_ALIVE(current, Vec2i(3, 3), "Glider: Moved cell after 4 generations");
    runner.ASSERT_POPULATION(current, 5, "Glider: Population unchanged after movement");
}

// Test user interaction simulation
void test_user_interactions() {
    GameState state;

    // Test brush painting
    GameLogic::addCellsWithBrush(state, Vec2i(0, 0), 1);
    runner.ASSERT_POPULATION(state, 1, "Brush size 1: Single cell");

    state.clear();
    GameLogic::addCellsWithBrush(state, Vec2i(0, 0), 2);
    // Brush size 2 creates a 3x3 square: (-1,-1) to (1,1) = 9 cells
    runner.ASSERT_POPULATION(state, 9, "Brush size 2: 3x3 square");

    // Test random cluster with seed for reproducibility
    state.clear();
    GameLogic::addRandomCluster(state, Vec2i(0, 0), 2, 12345);
    size_t population1 = state.getPopulation();

    state.clear();
    GameLogic::addRandomCluster(state, Vec2i(0, 0), 2, 12345);
    size_t population2 = state.getPopulation();

    runner.ASSERT_EQ(population1, population2, "Random cluster: Reproducible with same seed");
    runner.ASSERT_TRUE(population1 > 0, "Random cluster: Generated some cells");
}

// Test edge cases and error conditions
void test_edge_cases() {
    GameState state, next;

    // Empty world
    GameLogic::calculateNextGeneration(state, next);
    runner.ASSERT_POPULATION(next, 0, "Empty world stays empty");

    // Single cell at origin
    state.addCell(Vec2i(0, 0));
    GameLogic::calculateNextGeneration(state, next);
    runner.ASSERT_POPULATION(next, 0, "Single cell dies");

    // Extreme coordinates
    state.clear();
    Vec2i extreme(INT32_MAX / 2, INT32_MAX / 2);
    state.addCell(extreme);
    state.addCell(extreme + Vec2i(-1, 0));
    state.addCell(extreme + Vec2i(1, 0));
    GameLogic::calculateNextGeneration(state, next);
    runner.ASSERT_CELL_ALIVE(next, extreme, "Extreme coordinates: Cell with 2 neighbors survives");

    // Negative coordinates
    state.clear();
    Vec2i negative(-100, -200);
    state.addCell(negative);
    state.addCell(negative + Vec2i(-1, 0));
    state.addCell(negative + Vec2i(1, 0));
    state.addCell(negative + Vec2i(0, 1));
    GameLogic::calculateNextGeneration(state, next);
    runner.ASSERT_CELL_ALIVE(next, negative, "Negative coordinates: Cell with 3 neighbors survives");
}

// Performance/stress test
void test_large_patterns() {
    GameState state;

    // Create a large scattered pattern
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            if ((i + j) % 7 == 0) { // Sparse pattern
                state.addCell(Vec2i(i, j));
            }
        }
    }

    size_t initial_population = state.getPopulation();
    runner.ASSERT_TRUE(initial_population > 100, "Large pattern: Generated sufficient cells");

    // Run several generations
    GameState next;
    for (int gen = 0; gen < 10; gen++) {
        GameLogic::calculateNextGeneration(state, next);
        state = next;
    }

    runner.ASSERT_TRUE(state.getPopulation() > 0, "Large pattern: Some cells survive after 10 generations");
}

int main() {
    std::cout << "Massive Game of Life - Test Suite" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    runner.RUN_TEST(test_conways_rules, "Conway's Rules");
    runner.RUN_TEST(test_neighbor_counting, "Neighbor Counting");
    runner.RUN_TEST(test_still_lifes, "Still Life Patterns");
    runner.RUN_TEST(test_oscillators, "Oscillator Patterns");
    runner.RUN_TEST(test_spaceships, "Spaceship Patterns");
    runner.RUN_TEST(test_user_interactions, "User Interactions");
    runner.RUN_TEST(test_edge_cases, "Edge Cases");
    runner.RUN_TEST(test_large_patterns, "Large Patterns");

    runner.report_results();

    return runner.all_passed() ? 0 : 1;
}