#include <metal_stdlib>
using namespace metal;

struct HashEntry {
    int x;
    int y;
    uint occupied;
    uint padding0;
};

struct CellState {
    int x;
    int y;
    int wasAlive;
    int willBeAlive;
};

struct DispatchParams {
    uint tableSize;
    uint potentialCount;
    uint hashMask;
    uint maxNeighbors;
};

constant int2 kNeighborOffsets[8] = {
    int2(-1, -1), int2(0, -1), int2(1, -1),
    int2(-1, 0),                  int2(1, 0),
    int2(-1, 1),  int2(0, 1),  int2(1, 1)
};

inline bool is_alive(int2 cell,
                     const device HashEntry* table,
                     uint mask)
{
    ulong key = (static_cast<ulong>(static_cast<uint>(cell.y)) << 32) |
                static_cast<ulong>(static_cast<uint>(cell.x));

    ulong hashMask = static_cast<ulong>(mask);
    uint index = static_cast<uint>(key & hashMask);
    uint start = index;

    while (true) {
        const device HashEntry& entry = table[index];
        if (entry.occupied == 0) {
            return false;
        }
        if (entry.x == cell.x && entry.y == cell.y) {
            return true;
        }
        index = (index + 1) & mask;
        if (index == start) {
            return false;
        }
    }
}

kernel void life_step(const device HashEntry* table [[buffer(0)]],
                      const device int2* potentialCells [[buffer(1)]],
                      device CellState* results [[buffer(2)]],
                      constant DispatchParams& params [[buffer(3)]],
                      device int2* neighborOutput [[buffer(4)]],
                      device atomic_uint* neighborCount [[buffer(5)]],
                      uint gid [[thread_position_in_grid]])
{
    if (gid >= params.potentialCount) {
        return;
    }

    int2 cell = potentialCells[gid];
    CellState state;
    state.x = cell.x;
    state.y = cell.y;
    bool alive = is_alive(cell, table, params.hashMask);
    state.wasAlive = alive ? 1 : 0;

    int neighborCountLocal = 0;
    for (uint i = 0; i < 8; ++i) {
        int2 neighbor = cell + kNeighborOffsets[i];
        neighborCountLocal += is_alive(neighbor, table, params.hashMask) ? 1 : 0;
    }

    bool willLive;
    if (alive) {
        willLive = (neighborCountLocal == 2 || neighborCountLocal == 3);
    } else {
        willLive = (neighborCountLocal == 3);
    }

    state.willBeAlive = willLive ? 1 : 0;
    results[gid] = state;

    if ((state.wasAlive != 0) != (state.willBeAlive != 0)) {
        uint base = atomic_fetch_add_explicit(neighborCount, 9u, memory_order_relaxed);
        if (base + 9u <= params.maxNeighbors) {
            for (uint i = 0; i < 9; ++i) {
                neighborOutput[base + i] = cell + kNeighborOffsets[i];
            }
        }
    }
}
