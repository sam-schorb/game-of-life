#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <tuple>
#include <vector>

#include "game_logic.h"

struct BenchmarkScenario {
    std::string name;
    int generations = 10;
    int width = 256;
    int height = 256;
    int densityDivisor = 3; // 1/divisor chance a cell is alive
};

struct BenchmarkResult {
    BenchmarkScenario scenario;
    double cpuMilliseconds = 0.0;
    double gpuMilliseconds = 0.0;
    size_t initialPopulation = 0;
    size_t finalPopulationCPU = 0;
    size_t finalPopulationGPU = 0;
    bool gpuUsed = false;
    std::string gpuError;
    gol::gpu::GpuTimingStats timings;
};

inline GameState GenerateScenarioState(const BenchmarkScenario& scenario) {
    GameState state;
    for (int y = 0; y < scenario.height; ++y) {
        for (int x = 0; x < scenario.width; ++x) {
            if ((x + y * scenario.width) % scenario.densityDivisor == 0) {
                state.addCell(Vec2i(x, y));
            }
        }
    }
    return state;
}

inline double RunTimedGenerations(GameState startState, int generations, bool useGPU, bool& gpuUsed, std::string& gpuError, size_t& finalPopulation) {
    GameLogic::setUseGPU(useGPU);
    GameLogic::clearGPUError();
    GameState current = std::move(startState);
    GameState next;

    auto begin = std::chrono::steady_clock::now();
    for (int gen = 0; gen < generations; ++gen) {
        GameLogic::calculateNextGeneration(current, next);
        current = next;
    }
    auto end = std::chrono::steady_clock::now();

    finalPopulation = current.getPopulation();
    gpuUsed = useGPU ? GameLogic::lastStepUsedGPU() : false;
    gpuError = GameLogic::lastGPUError();

    GameLogic::setUseGPU(false);

    return std::chrono::duration<double, std::milli>(end - begin).count();
}

inline BenchmarkResult RunBenchmark(const BenchmarkScenario& scenario) {
    BenchmarkResult result;
    result.scenario = scenario;

    GameState initial = GenerateScenarioState(scenario);
    result.initialPopulation = initial.getPopulation();

    bool gpuUsed = false;
    std::string gpuError;
    size_t finalPopulation = 0;

    GameState cpuState = initial;
    result.cpuMilliseconds = RunTimedGenerations(cpuState, scenario.generations, false, gpuUsed, gpuError, result.finalPopulationCPU);

    GameState gpuState = initial;
    gol::gpu::ResetTimingStats();
    result.gpuMilliseconds = RunTimedGenerations(gpuState, scenario.generations, true, gpuUsed, gpuError, result.finalPopulationGPU);
    result.gpuUsed = gpuUsed;
    result.gpuError = gpuError;
    result.timings = gol::gpu::GetTimingStats();

    return result;
}

inline std::string FormatBenchmarkResult(const BenchmarkResult& result) {
    std::string output;
    output += "Scenario: " + result.scenario.name + "\n";
    output += "  Generations: " + std::to_string(result.scenario.generations) + "\n";
    output += "  Dimensions: " + std::to_string(result.scenario.width) + "x" + std::to_string(result.scenario.height) + "\n";
    output += "  Initial population: " + std::to_string(result.initialPopulation) + "\n";
    output += "  CPU time: " + std::to_string(result.cpuMilliseconds) + " ms\n";
    output += "  GPU time: " + std::to_string(result.gpuMilliseconds) + " ms\n";
    if (!result.gpuUsed) {
        output += "  GPU result: Fallback (" + result.gpuError + ")\n";
    }
    output += "  CPU final population: " + std::to_string(result.finalPopulationCPU) + "\n";
    output += "  GPU final population: " + std::to_string(result.finalPopulationGPU) + "\n";
    if (result.gpuMilliseconds > 0.0) {
        double speedup = result.cpuMilliseconds / result.gpuMilliseconds;
        output += "  Speedup (CPU/GPU): " + std::to_string(speedup) + "x\n";
    }
    if (result.gpuUsed) {
        output += "  GPU prepare: " + std::to_string(result.timings.prepareMilliseconds) + " ms\n";
        output += "  GPU upload: " + std::to_string(result.timings.uploadMilliseconds) + " ms\n";
        output += "  GPU dispatch: " + std::to_string(result.timings.dispatchMilliseconds) + " ms\n";
        output += "  GPU download: " + std::to_string(result.timings.downloadMilliseconds) + " ms\n";
        output += "  GPU total (module): " + std::to_string(result.timings.totalMilliseconds) + " ms\n";
        if (result.timings.neighborOverflow) {
            output += "  GPU neighbor buffer overflow detected (CPU fallback engaged)\n";
        }
    }
    return output;
}
