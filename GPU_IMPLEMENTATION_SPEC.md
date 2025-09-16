# GPU Acceleration Implementation Specification

## Project Overview
**MassiveGameOfLife** is a Conway's Game of Life implementation using sparse encoding (hash sets) to efficiently handle large, sparse patterns. The project has been refactored to isolate all expensive calculations in a single function: `CalculateNextGeneration()`.

## Current Architecture

### Data Structures
- `std::unordered_set<olc::vi2d, HASH_OLC_VI2D> setActive` - Current living cells
- `std::unordered_set<olc::vi2d, HASH_OLC_VI2D> setPotential` - Cells that might change state
- Hash sets use custom 2D coordinate hashing via `HASH_OLC_VI2D`

### Core Calculation Function
```cpp
void CalculateNextGeneration(
    const std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& currentActive,
    const std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& potentialCells,
    std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& nextActive,
    std::unordered_set<olc::vi2d, HASH_OLC_VI2D>& nextPotential)
```

This function currently processes each potential cell sequentially, performing 8 neighbor lookups per cell and applying Conway's rules.

## What Needs To Be Changed

### 1. GPU Implementation Function
**Requirement**: Create a GPU-accelerated version of `CalculateNextGeneration()` that produces identical results.

**Interface Constraint**: Must accept the same sparse hash set parameters and produce the same sparse hash set outputs. The calling code should not need to change.

**Performance Target**: Handle 100,000+ active cells efficiently (current CPU implementation becomes bottlenecked around this scale).

### 2. Runtime Selection Mechanism
**Requirement**: Implement a way to choose between CPU and GPU calculation at runtime.

**Suggested Approach**: Add a boolean flag or configuration option that determines which implementation to use. Both implementations must coexist and be callable.

**Fallback Strategy**: GPU implementation should gracefully fall back to CPU if GPU resources are unavailable or if errors occur.

### 3. Data Conversion System
**Challenge**: GPUs typically work best with dense, contiguous data structures, while this project uses sparse hash sets for memory efficiency.

**Requirements**:
- Convert sparse coordinate sets to GPU-friendly format
- Perform GPU computation
- Convert results back to sparse hash sets
- Ensure no data is lost in conversion process
- Handle coordinate ranges that may be very large (sparse patterns can span huge areas)

### 4. GPU Programming Model Integration
**Platform**: macOS (user has MacBook Pro) - Metal Shading Language is the primary GPU compute option.

**Requirements**:
- Integrate Metal compute shader compilation and execution
- Handle GPU memory allocation and deallocation
- Manage data transfer between CPU and GPU memory
- Handle Metal framework initialization and error checking

### 5. Neighbor Calculation Parallelization
**Core Challenge**: The expensive operation is counting neighbors for each potential cell (8 lookups per cell).

**Current Behavior**: For each cell in `potentialCells`, count living neighbors and apply Conway's rules:
- Living cell with 2-3 neighbors survives
- Dead cell with exactly 3 neighbors becomes alive
- All other cells die or stay dead
- State changes trigger neighboring cells to be marked as "potential" for next generation

**GPU Requirement**: This neighbor counting must be parallelized such that thousands of cells can be processed simultaneously.

### 6. Memory Layout Considerations
**Sparse vs Dense Trade-off**: The current sparse representation is memory-efficient but may not be optimal for GPU processing.

**Requirements**:
- Determine optimal GPU memory layout (dense grid vs coordinate arrays vs other)
- Handle bounding box calculation for active regions
- Minimize memory allocation/deallocation overhead
- Consider cache-friendly access patterns for GPU

### 7. Synchronization and State Management
**State Update Pattern**: The current implementation uses double-buffering (current generation and next generation sets).

**GPU Requirements**:
- Ensure all GPU computation completes before results are read back
- Handle potential GPU/CPU synchronization issues
- Maintain the double-buffering pattern
- Ensure deterministic results (same input always produces same output)

## Constraints and Requirements

### Must Preserve
- **Identical behavior**: GPU version must produce exactly the same results as CPU version
- **Sparse data structures**: Keep using hash sets for memory efficiency on CPU side
- **Infinite canvas**: Must handle arbitrarily large coordinate ranges
- **Real-time interaction**: User can add/remove cells during simulation

### Performance Goals
- Handle 100,000+ active cells in real-time (60+ FPS)
- GPU calculation should be significantly faster than current CPU implementation for large cell counts
- Graceful performance scaling with cell count

### Platform Requirements
- macOS compatibility (Metal)
- Must work on MacBook Pro internal GPU
- Should not require external GPU libraries

## Implementation Boundaries

### What Should NOT Be Changed
- The sparse hash set data structures (`setActive`, `setPotential`, etc.)
- The main simulation loop structure
- The rendering system (olcPixelGameEngine)
- User interface and controls
- File I/O or project structure

### What MUST Be Changed
- The `CalculateNextGeneration()` function (add GPU version)
- Add Metal shader compilation and execution
- Add CPU/GPU selection mechanism
- Add data conversion utilities (sparse ↔ dense)

## Available Test Suite

A comprehensive test suite has been implemented to validate GPU implementation:

### Test Infrastructure
- **Core Logic Extraction**: `game_logic.h` contains platform-independent Game of Life logic
- **Test Framework**: `test_framework.h` provides game-specific assertions and pattern testing utilities
- **Test Suite**: `test_game_of_life.cpp` contains 33 comprehensive tests
- **Build Scripts**: `build_tests.sh` and `run_tests.sh` for easy execution

### Test Coverage (33 tests, all must pass)
1. **Conway's Four Rules** (7 tests)
   - Underpopulation: cells with < 2 neighbors die
   - Survival: cells with 2-3 neighbors survive
   - Overpopulation: cells with > 3 neighbors die
   - Birth: dead cells with exactly 3 neighbors become alive

2. **Neighbor Counting** (3 tests)
   - All 8 directions correctly detected
   - Diagonal neighbor recognition
   - Large coordinate handling (edge cases)

3. **Known Pattern Validation** (8 tests)
   - **Still Lifes**: Block (2×2), Beehive - must remain stable
   - **Oscillators**: Blinker (period 2), Toad (period 2) - must cycle correctly
   - **Spaceships**: Glider movement and population preservation

4. **User Interaction Simulation** (4 tests)
   - Brush painting with various sizes
   - Random cluster generation (with reproducible seeds)
   - Population counting accuracy

5. **Edge Cases & Stress Tests** (11 tests)
   - Empty world behavior
   - Extreme coordinates (INT32_MAX/2)
   - Negative coordinates
   - Large patterns (1000+ cells, 10+ generations)

### GPU Validation Process

**Critical Requirement**: Both CPU and GPU implementations must pass identical test suite.

```bash
# Validate CPU implementation
./run_tests.sh
# Expected: 🎉 ALL TESTS PASSED! (33/33)

# Validate GPU implementation (after implementation)
./run_tests.sh --gpu
# Must produce: 🎉 ALL TESTS PASSED! (33/33)
```

### Test-Driven Development Approach
1. **Before GPU Implementation**: Ensure all 33 tests pass on CPU version
2. **During GPU Development**: Run tests frequently to catch regressions
3. **GPU Completion**: Both CPU and GPU must produce identical results on all tests
4. **Future Changes**: Any modification must maintain test suite compatibility

The test suite provides **mathematical proof** that GPU implementation is behaviorally identical to the CPU version, eliminating guesswork and ensuring correctness.

## Success Criteria
1. **Functional**: GPU implementation produces identical results to CPU version
2. **Performance**: Significant speedup for 100,000+ cell simulations
3. **Reliability**: Graceful fallback to CPU if GPU fails
4. **Maintainable**: Clean separation between CPU and GPU code paths
5. **User-transparent**: Existing gameplay and user experience unchanged
6. **Test Validation**: Both CPU and GPU implementations pass all 33 tests identically

The implementation should result in a drop-in replacement for the calculation function that can be toggled between CPU and GPU modes while maintaining all existing functionality and behavior.