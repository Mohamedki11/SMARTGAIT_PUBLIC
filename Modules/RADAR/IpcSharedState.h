#pragma once
#include "SharedState.h"
#include <atomic>
#include <cstdint>
#include <cstddef>

constexpr const char* SHM_NAME = "/smartgait_radar_shm";

constexpr int CHUNK_SWEEPS = 100;
constexpr int BYTES_PER_CHUNK = CHUNK_SWEEPS * BYTES_PER_SWEEP;

struct SharedRadarBuffer {
    std::atomic<bool> is_data_ready;
    uint8_t payload[BYTES_PER_CHUNK];
};