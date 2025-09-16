#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace gol::gpu {

struct RawCoordinate {
    int32_t x;
    int32_t y;
};

struct alignas(16) CellState {
    int32_t x;
    int32_t y;
    int32_t wasAlive;
    int32_t willBeAlive;
};

bool IsAvailable();
void SetEnabled(bool enabled);
bool IsEnabled();
void ResetCaches();

struct GpuTimingStats {
    double prepareMilliseconds = 0.0;
    double uploadMilliseconds = 0.0;
    double dispatchMilliseconds = 0.0;
    double downloadMilliseconds = 0.0;
    double totalMilliseconds = 0.0;
    bool lastUsedGPU = false;
    bool neighborOverflow = false;
};

void ResetTimingStats();
GpuTimingStats GetTimingStats();

namespace detail {

bool CalculateNextGenerationRaw(
    const std::vector<RawCoordinate>& currentActive,
    const std::vector<RawCoordinate>& potentialCells,
    std::vector<CellState>& results,
    std::vector<RawCoordinate>& neighborOutputs,
    bool& usedGPU,
    std::string& errorMessage);

} // namespace detail

template <typename Cell, typename Hash>
bool ComputeCellStates(
    const std::unordered_set<Cell, Hash>& currentActive,
    const std::unordered_set<Cell, Hash>& potentialCells,
    std::vector<CellState>& outStates,
    std::vector<RawCoordinate>& neighborCells,
    bool& usedGPU,
    std::string* errorMessage = nullptr)
{
    thread_local std::vector<RawCoordinate> currentVecStorage;
    auto& currentVec = currentVecStorage;
    currentVec.clear();
    currentVec.reserve(currentActive.size());
    for (const auto& cell : currentActive) {
        currentVec.push_back({cell.x, cell.y});
    }

    thread_local std::vector<RawCoordinate> potentialVecStorage;
    auto& potentialVec = potentialVecStorage;
    potentialVec.clear();
    potentialVec.reserve(potentialCells.size());
    for (const auto& cell : potentialCells) {
        potentialVec.push_back({cell.x, cell.y});
    }

    neighborCells.clear();

    std::string localError;
    bool success = detail::CalculateNextGenerationRaw(
        currentVec, potentialVec, outStates, neighborCells, usedGPU, localError);

    if (!success) {
        if (errorMessage) {
            *errorMessage = localError;
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

template <typename Cell, typename Hash>
bool CalculateNextGeneration(
    const std::unordered_set<Cell, Hash>& currentActive,
    const std::unordered_set<Cell, Hash>& potentialCells,
    std::unordered_set<Cell, Hash>& nextActive,
    std::unordered_set<Cell, Hash>& nextPotential,
    bool& usedGPU,
    std::string* errorMessage = nullptr)
{
    std::vector<CellState> states;
    std::vector<RawCoordinate> neighborCells;
    if (!ComputeCellStates(currentActive, potentialCells, states, neighborCells, usedGPU, errorMessage)) {
        usedGPU = false;
        return false;
    }

    nextActive.clear();
    nextPotential.clear();
    nextActive.reserve(states.size());
    nextPotential.reserve(currentActive.size() + neighborCells.size());

    for (const auto& cell : currentActive) {
        nextPotential.insert(cell);
    }

    size_t changedCount = 0;
    for (const auto& state : states) {
        Cell coord(state.x, state.y);
        const bool wasAlive = state.wasAlive != 0;
        const bool willBeAlive = state.willBeAlive != 0;
        if (willBeAlive) {
            nextActive.insert(coord);
        }
        if (wasAlive != willBeAlive) {
            ++changedCount;
        }
    }

    if (neighborCells.size() >= changedCount * 9) {
        for (const auto& neighbor : neighborCells) {
            nextPotential.insert(Cell(neighbor.x, neighbor.y));
        }
    } else {
        for (const auto& state : states) {
            const bool wasAlive = state.wasAlive != 0;
            const bool willBeAlive = state.willBeAlive != 0;
            if (wasAlive != willBeAlive) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        nextPotential.insert(Cell(state.x + dx, state.y + dy));
                    }
                }
            }
        }
    }

    return true;
}

} // namespace gol::gpu
