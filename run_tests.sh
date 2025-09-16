#!/bin/bash

# Convenient test runner script
echo "🧪 Massive Game of Life Test Runner"
echo ""

# Build tests
./build_tests.sh

if [ $? -eq 0 ]; then
    echo ""
    echo "🚀 Running tests..."
    echo ""

    # Run the tests
    ./test_game_of_life

    # Capture exit code
    exit_code=$?

    echo ""
    if [ $exit_code -eq 0 ]; then
        echo "🎉 All tests completed successfully!"
    else
        echo "❌ Some tests failed (exit code: $exit_code)"
    fi

    exit $exit_code
else
    echo "❌ Failed to build tests"
    exit 1
fi