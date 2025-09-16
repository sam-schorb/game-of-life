#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "game_logic.h"

// Simple lightweight test framework
class TestRunner {
private:
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;

public:
    // Basic assertion macros
    template<typename T1, typename T2>
    void ASSERT_EQ(const T1& expected, const T2& actual, const std::string& test_name) {
        if (expected == actual) {
            passed++;
            std::cout << "✓ " << test_name << std::endl;
        } else {
            failed++;
            std::stringstream ss;
            ss << "✗ FAILED: " << test_name << "\n";
            ss << "    Expected: " << expected << "\n";
            ss << "    Actual: " << actual;
            failures.push_back(ss.str());
            std::cout << failures.back() << std::endl;
        }
    }

    void ASSERT_TRUE(bool condition, const std::string& test_name) {
        if (condition) {
            passed++;
            std::cout << "✓ " << test_name << std::endl;
        } else {
            failed++;
            std::string failure = "✗ FAILED: " + test_name + "\n    Condition was false";
            failures.push_back(failure);
            std::cout << failure << std::endl;
        }
    }

    void ASSERT_FALSE(bool condition, const std::string& test_name) {
        ASSERT_TRUE(!condition, test_name);
    }

    // Game-specific assertions
    void ASSERT_CELL_ALIVE(const GameState& state, const Vec2i& cell, const std::string& test_name) {
        ASSERT_TRUE(state.isAlive(cell), test_name + " - Cell should be alive at (" +
                   std::to_string(cell.x) + "," + std::to_string(cell.y) + ")");
    }

    void ASSERT_CELL_DEAD(const GameState& state, const Vec2i& cell, const std::string& test_name) {
        ASSERT_FALSE(state.isAlive(cell), test_name + " - Cell should be dead at (" +
                    std::to_string(cell.x) + "," + std::to_string(cell.y) + ")");
    }

    void ASSERT_POPULATION(const GameState& state, size_t expected, const std::string& test_name) {
        ASSERT_EQ(expected, state.getPopulation(), test_name + " - Population count");
    }

    void ASSERT_STATES_EQUAL(const GameState& expected, const GameState& actual, const std::string& test_name) {
        ASSERT_TRUE(expected == actual, test_name + " - Game states should be identical");
    }

    // Test pattern stability over multiple generations
    void ASSERT_PATTERN_STABLE(GameState state, int generations, const std::string& test_name) {
        GameState original = state;
        GameState next;

        for (int i = 0; i < generations; i++) {
            GameLogic::calculateNextGeneration(state, next);
            state = next;
        }

        ASSERT_STATES_EQUAL(original, state, test_name + " - Pattern should be stable after " +
                           std::to_string(generations) + " generations");
    }

    // Test pattern oscillation
    void ASSERT_PATTERN_CYCLES(GameState state, int period, const std::string& test_name) {
        GameState original = state;
        GameState next;

        // Run for one complete period
        for (int i = 0; i < period; i++) {
            GameLogic::calculateNextGeneration(state, next);
            state = next;
        }

        ASSERT_STATES_EQUAL(original, state, test_name + " - Pattern should cycle with period " +
                           std::to_string(period));
    }

    // Run a test function and catch any exceptions
    void RUN_TEST(void (*test_func)(), const std::string& test_suite_name) {
        std::cout << "\n--- " << test_suite_name << " ---" << std::endl;
        try {
            test_func();
        } catch (const std::exception& e) {
            failed++;
            std::string failure = "✗ EXCEPTION in " + test_suite_name + ": " + e.what();
            failures.push_back(failure);
            std::cout << failure << std::endl;
        }
    }

    // Report final results
    void report_results() {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        if (failed == 0) {
            std::cout << "🎉 ALL TESTS PASSED! (" << passed << "/" << (passed + failed) << ")" << std::endl;
        } else {
            std::cout << "Tests passed: " << passed << std::endl;
            std::cout << "Tests failed: " << failed << std::endl;
            std::cout << "Success rate: " << (100.0 * passed / (passed + failed)) << "%" << std::endl;

            std::cout << "\nFAILED TESTS:" << std::endl;
            for (const auto& failure : failures) {
                std::cout << failure << std::endl;
            }
        }

        std::cout << std::string(50, '=') << std::endl;
    }

    // Get pass/fail counts
    int get_passed() const { return passed; }
    int get_failed() const { return failed; }
    bool all_passed() const { return failed == 0; }
};