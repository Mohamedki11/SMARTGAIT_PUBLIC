#pragma once
#include <atomic>
#include <vector>
#include <cstdint>

#include "readerwriterqueue.h"

using namespace std;
constexpr int SAMPLES_PER_SWEEP = 2048;
constexpr int WORDS_PER_SWEEP = SAMPLES_PER_SWEEP * 2; // 4096 (4 Bytes)(2 Bytes I and 2 Bytes Q)
constexpr int BYTES_PER_SWEEP = WORDS_PER_SWEEP * 2;
constexpr uint16_t HEADER_VAL_TX1 = 49152; //FIND the HEADER FOR TX1

//allocate the buffers for the asynchronus communication (8 transfers of 64KB)
constexpr int NUM_USB_TRANFERS = 8; //number of buckets to store the data coming from the Cypress
constexpr int USB_BUFFER_SIZE =  65536; //64 KB in each bucket (multiple of 512 like the cypress)

extern atomic<bool> radar_run_flag;
extern atomic<bool> radar_record;
extern atomic<bool> soft_restart; //for the restart of the cypress FIFO

extern atomic<bool> radar_error; //Tracking Radar Producer Health

extern moodycamel::ReaderWriterQueue <vector<uint8_t>> radar_queue;

//realtime doctor
extern atomic<bool> realtime_active;
extern moodycamel::ReaderWriterQueue <vector<uint8_t>> realtime_queue;
