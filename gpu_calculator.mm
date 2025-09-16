#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "gpu_calculator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <mach-o/dyld.h>

namespace gol::gpu {
namespace {

constexpr const char* kMetallibName = "GameOfLife.metallib";
constexpr size_t kMaxTableEntries = 1ull << 24; // 16,777,216 entries (~256 MB)

struct alignas(16) HashEntry {
    int32_t x;
    int32_t y;
    uint32_t occupied;
    uint32_t padding;
};

struct alignas(16) DispatchParams {
    uint32_t tableSize;
    uint32_t potentialCount;
    uint32_t hashMask;
    uint32_t maxNeighbors;
};

struct MetalContext {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLLibrary> library = nil;
    id<MTLComputePipelineState> pipeline = nil;
    id<MTLBuffer> tableBuffer = nil;
    size_t tableCapacityBytes = 0;
    id<MTLBuffer> potentialBuffer = nil;
    size_t potentialCapacityBytes = 0;
    id<MTLBuffer> outputBuffer = nil;
    size_t outputCapacityBytes = 0;
    id<MTLBuffer> paramBuffer = nil;
    id<MTLBuffer> neighborBuffer = nil;
    size_t neighborCapacityBytes = 0;
    id<MTLBuffer> neighborCountBuffer = nil;
};

MetalContext& GetContext()
{
    static MetalContext ctx;
    return ctx;
}

std::once_flag gInitFlag;
std::atomic<bool> gRequested{false};
bool gDeviceAvailable = false;
std::string gInitError;
GpuTimingStats gTimingStats;

std::string ToStdString(NSString* value)
{
    if (value == nil) {
        return {};
    }
    const char* cstr = [value UTF8String];
    return cstr ? std::string(cstr) : std::string();
}

std::optional<std::filesystem::path> ExecutableDirectory()
{
    uint32_t pathSize = 0;
    _NSGetExecutablePath(nullptr, &pathSize);
    if (pathSize == 0) {
        return std::nullopt;
    }

    std::string buffer(pathSize, '\0');
    if (_NSGetExecutablePath(buffer.data(), &pathSize) != 0) {
        return std::nullopt;
    }

    std::filesystem::path rawPath(buffer.data());
    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(rawPath, ec);
    if (ec) {
        canonical = rawPath;
    }
    return canonical.parent_path();
}

std::optional<std::filesystem::path> FindMetallib()
{
    const std::filesystem::path fileName(kMetallibName);
    std::vector<std::filesystem::path> candidates;

    auto exeDir = ExecutableDirectory();
    if (exeDir) {
        candidates.push_back(*exeDir);
        candidates.push_back(exeDir->parent_path());
    }
    candidates.push_back(std::filesystem::current_path());

    for (const auto& base : candidates) {
        if (base.empty()) {
            continue;
        }
        std::filesystem::path candidate = base / fileName;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return std::filesystem::canonical(candidate, ec);
        }
    }

    // Fall back to file name in current working directory
    std::error_code ec;
    std::filesystem::path fallback = std::filesystem::current_path() / fileName;
    if (std::filesystem::exists(fallback, ec)) {
        return std::filesystem::canonical(fallback, ec);
    }

    return std::nullopt;
}

bool EnsureDevice(std::string& error)
{
    std::call_once(gInitFlag, []() {
        MetalContext& ctx = GetContext();
        ctx.device = MTLCreateSystemDefaultDevice();
        if (ctx.device == nil) {
            gInitError = "Metal GPU device unavailable";
            return;
        }
        ctx.queue = [ctx.device newCommandQueue];
        if (ctx.queue == nil) {
            gInitError = "Failed to create Metal command queue";
            ctx.device = nil;
            return;
        }
        gDeviceAvailable = true;
    });

    if (!gDeviceAvailable) {
        error = gInitError.empty() ? std::string("Metal initialization failed") : gInitError;
    }
    return gDeviceAvailable;
}

bool EnsurePipeline(std::string& error)
{
    MetalContext& ctx = GetContext();
    if (ctx.pipeline != nil) {
        return true;
    }

    if (!EnsureDevice(error)) {
        return false;
    }

    auto metallibPath = FindMetallib();
    if (!metallibPath) {
        error = "Unable to locate GameOfLife.metallib";
        return false;
    }

    NSString* nsPath = [NSString stringWithUTF8String:metallibPath->c_str()];
    NSURL* url = [NSURL fileURLWithPath:nsPath];
    NSError* nsError = nil;
    ctx.library = [ctx.device newLibraryWithURL:url error:&nsError];
    if (ctx.library == nil) {
        error = ToStdString(nsError ? nsError.localizedDescription : @"Failed to load Metal library");
        return false;
    }

    id<MTLFunction> function = [ctx.library newFunctionWithName:@"life_step"];
    if (function == nil) {
        error = "Metal function 'life_step' not found";
        return false;
    }

    ctx.pipeline = [ctx.device newComputePipelineStateWithFunction:function error:&nsError];
    if (ctx.pipeline == nil) {
        error = ToStdString(nsError ? nsError.localizedDescription : @"Failed to create Metal pipeline state");
        return false;
    }

    if (ctx.paramBuffer == nil) {
        ctx.paramBuffer = [ctx.device newBufferWithLength:sizeof(DispatchParams) options:MTLResourceStorageModeShared];
        if (ctx.paramBuffer == nil) {
            error = "Failed to allocate parameter buffer";
            return false;
        }
    }

    return true;
}

bool EnsureBuffers(size_t tableEntries, size_t potentialEntries, std::string& error)
{
    MetalContext& ctx = GetContext();

    const size_t tableBytes = std::max<size_t>(tableEntries, 1) * sizeof(HashEntry);
    if (tableBytes > ctx.tableCapacityBytes) {
        ctx.tableBuffer = [ctx.device newBufferWithLength:tableBytes options:MTLResourceStorageModeShared];
        if (ctx.tableBuffer == nil) {
            error = "Failed to allocate hash table buffer";
            return false;
        }
        ctx.tableCapacityBytes = tableBytes;
    }

    const size_t potentialBytes = std::max<size_t>(potentialEntries, 1) * sizeof(RawCoordinate);
    if (potentialBytes > ctx.potentialCapacityBytes) {
        ctx.potentialBuffer = [ctx.device newBufferWithLength:potentialBytes options:MTLResourceStorageModeShared];
        if (ctx.potentialBuffer == nil) {
            error = "Failed to allocate potential cell buffer";
            return false;
        }
        ctx.potentialCapacityBytes = potentialBytes;
    }

    const size_t outputBytes = std::max<size_t>(potentialEntries, 1) * sizeof(CellState);
    if (outputBytes > ctx.outputCapacityBytes) {
        ctx.outputBuffer = [ctx.device newBufferWithLength:outputBytes options:MTLResourceStorageModeShared];
        if (ctx.outputBuffer == nil) {
            error = "Failed to allocate output buffer";
            return false;
        }
        ctx.outputCapacityBytes = outputBytes;
    }

    const size_t neighborEntries = std::max<size_t>(potentialEntries, 1) * 9;
    const size_t neighborBytes = neighborEntries * sizeof(RawCoordinate);
    if (neighborBytes > ctx.neighborCapacityBytes) {
        ctx.neighborBuffer = [ctx.device newBufferWithLength:neighborBytes options:MTLResourceStorageModeShared];
        if (ctx.neighborBuffer == nil) {
            error = "Failed to allocate neighbor buffer";
            return false;
        }
        ctx.neighborCapacityBytes = neighborBytes;
    }

    if (ctx.neighborCountBuffer == nil) {
        ctx.neighborCountBuffer = [ctx.device newBufferWithLength:sizeof(uint32_t) options:MTLResourceStorageModeShared];
        if (ctx.neighborCountBuffer == nil) {
            error = "Failed to allocate neighbor count buffer";
            return false;
        }
    }

    return true;
}

size_t NextPowerOfTwo(size_t value)
{
    if (value <= 1) {
        return 1;
    }

    if (value > (std::numeric_limits<size_t>::max() >> 1)) {
        return 0;
    }

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
#if UINTPTR_MAX > 0xFFFFFFFFull
    value |= value >> 32;
#endif
    value++;
    return value;
}

bool PopulateHashTable(const std::vector<RawCoordinate>& currentActive,
                       std::vector<HashEntry>& table,
                       std::string& error)
{
    if (table.empty()) {
        table.resize(1);
    }

    const size_t mask = table.size() - 1;

    for (const auto& cell : currentActive) {
        uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(cell.x));
        uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(cell.y));
        uint64_t key = (uy << 32) | ux;
        size_t index = static_cast<size_t>(key & mask);
        const size_t start = index;

        while (table[index].occupied != 0) {
            if (table[index].x == cell.x && table[index].y == cell.y) {
                break;
            }
            index = (index + 1) & mask;
            if (index == start) {
                error = "Hash table is full";
                return false;
            }
        }

        table[index].x = cell.x;
        table[index].y = cell.y;
        table[index].occupied = 1;
        table[index].padding = 0;
    }

    return true;
}

} // namespace

bool IsAvailable()
{
    std::string error;
    return EnsureDevice(error);
}

void SetEnabled(bool enabled)
{
    gRequested.store(enabled, std::memory_order_relaxed);
}

bool IsEnabled()
{
    if (!gRequested.load(std::memory_order_relaxed)) {
        return false;
    }

    std::string error;
    return EnsureDevice(error);
}

void ResetCaches()
{
    MetalContext& ctx = GetContext();
    ctx.tableBuffer = nil;
    ctx.tableCapacityBytes = 0;
    ctx.potentialBuffer = nil;
    ctx.potentialCapacityBytes = 0;
    ctx.outputBuffer = nil;
    ctx.outputCapacityBytes = 0;
    ctx.paramBuffer = nil;
    ctx.pipeline = nil;
    ctx.library = nil;
    ctx.neighborBuffer = nil;
    ctx.neighborCapacityBytes = 0;
    ctx.neighborCountBuffer = nil;
}

void ResetTimingStats()
{
    gTimingStats = {};
}

GpuTimingStats GetTimingStats()
{
    return gTimingStats;
}

namespace detail {

bool CalculateNextGenerationRaw(
    const std::vector<RawCoordinate>& currentActive,
    const std::vector<RawCoordinate>& potentialCells,
    std::vector<CellState>& results,
    std::vector<RawCoordinate>& neighborOutputs,
    bool& usedGPU,
    std::string& errorMessage)
{
    using clock = std::chrono::steady_clock;
    auto totalStart = clock::now();
    auto stageStart = totalStart;
    gTimingStats = {};

    results.clear();
    neighborOutputs.clear();
    usedGPU = false;
    errorMessage.clear();

    if (potentialCells.size() > std::numeric_limits<uint32_t>::max()) {
        errorMessage = "Too many potential cells for GPU dispatch";
        return false;
    }

    if (!EnsurePipeline(errorMessage)) {
        return false;
    }

    const size_t potentialCount = potentialCells.size();
    if (potentialCount == 0) {
        usedGPU = true;
        gTimingStats.lastUsedGPU = true;
        return true;
    }

    size_t desiredTableSize = NextPowerOfTwo(std::max<size_t>(1, currentActive.size() * 2));
    if (desiredTableSize == 0 || desiredTableSize > kMaxTableEntries) {
        errorMessage = "Active cell count is too large for GPU hash table";
        return false;
    }

    std::vector<HashEntry> table(desiredTableSize);
    if (!PopulateHashTable(currentActive, table, errorMessage)) {
        return false;
    }

    auto prepareEnd = clock::now();
    gTimingStats.prepareMilliseconds = std::chrono::duration<double, std::milli>(prepareEnd - stageStart).count();
    stageStart = prepareEnd;

    if (!EnsureBuffers(desiredTableSize, potentialCount, errorMessage)) {
        return false;
    }

    MetalContext& ctx = GetContext();

    std::memcpy([ctx.tableBuffer contents], table.data(), table.size() * sizeof(HashEntry));
    std::memcpy([ctx.potentialBuffer contents], potentialCells.data(), potentialCells.size() * sizeof(RawCoordinate));
    std::memset([ctx.outputBuffer contents], 0, potentialCells.size() * sizeof(CellState));
    auto* neighborCountPtr = static_cast<uint32_t*>([ctx.neighborCountBuffer contents]);
    *neighborCountPtr = 0;

    DispatchParams params{};
    params.tableSize = static_cast<uint32_t>(desiredTableSize);
    params.potentialCount = static_cast<uint32_t>(potentialCount);
    params.hashMask = static_cast<uint32_t>(desiredTableSize - 1);
    params.maxNeighbors = static_cast<uint32_t>(ctx.neighborCapacityBytes / sizeof(RawCoordinate));
    std::memcpy([ctx.paramBuffer contents], &params, sizeof(params));

    auto uploadEnd = clock::now();
    gTimingStats.uploadMilliseconds = std::chrono::duration<double, std::milli>(uploadEnd - stageStart).count();
    stageStart = uploadEnd;

    id<MTLCommandBuffer> commandBuffer = [ctx.queue commandBuffer];
    if (commandBuffer == nil) {
        errorMessage = "Failed to create command buffer";
        return false;
    }

    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    if (encoder == nil) {
        errorMessage = "Failed to create compute command encoder";
        return false;
    }

    [encoder setComputePipelineState:ctx.pipeline];
    [encoder setBuffer:ctx.tableBuffer offset:0 atIndex:0];
    [encoder setBuffer:ctx.potentialBuffer offset:0 atIndex:1];
    [encoder setBuffer:ctx.outputBuffer offset:0 atIndex:2];
    [encoder setBuffer:ctx.paramBuffer offset:0 atIndex:3];
    [encoder setBuffer:ctx.neighborBuffer offset:0 atIndex:4];
    [encoder setBuffer:ctx.neighborCountBuffer offset:0 atIndex:5];

    const NSUInteger threadCount = static_cast<NSUInteger>(potentialCount);
    NSUInteger threadsPerGroup = std::min<NSUInteger>(ctx.pipeline.maxTotalThreadsPerThreadgroup, threadCount);
    if (threadsPerGroup == 0) {
        threadsPerGroup = 1;
    }
    MTLSize threadgroupSize = MTLSizeMake(threadsPerGroup, 1, 1);
    MTLSize gridSize = MTLSizeMake(threadCount, 1, 1);
    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
    [encoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    auto dispatchEnd = clock::now();
    gTimingStats.dispatchMilliseconds = std::chrono::duration<double, std::milli>(dispatchEnd - stageStart).count();
    stageStart = dispatchEnd;

    if (commandBuffer.error != nil) {
        errorMessage = ToStdString(commandBuffer.error.localizedDescription);
        return false;
    }

    const auto* gpuResults = static_cast<const CellState*>([ctx.outputBuffer contents]);
    results.assign(gpuResults, gpuResults + potentialCount);
    uint32_t rawNeighborCount = *static_cast<const uint32_t*>([ctx.neighborCountBuffer contents]);
    const uint32_t maxNeighbors = static_cast<uint32_t>(ctx.neighborCapacityBytes / sizeof(RawCoordinate));
    if (rawNeighborCount > maxNeighbors) {
        gTimingStats.neighborOverflow = true;
    }
    uint32_t neighborCount = std::min<uint32_t>(rawNeighborCount, maxNeighbors);
    const auto* neighborData = static_cast<const RawCoordinate*>([ctx.neighborBuffer contents]);
    neighborOutputs.assign(neighborData, neighborData + neighborCount);
    usedGPU = true;

    auto downloadEnd = clock::now();
    gTimingStats.downloadMilliseconds = std::chrono::duration<double, std::milli>(downloadEnd - stageStart).count();
    gTimingStats.totalMilliseconds = std::chrono::duration<double, std::milli>(downloadEnd - totalStart).count();
    gTimingStats.lastUsedGPU = true;
    return true;
}

} // namespace detail

} // namespace gol::gpu
