#include "SharedState.h"
#include "IpcSharedState.h" // The struct we made in the previous message
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

void realtime_thread_worker() {
    cout << "[RADAR REALTIME] Thread Started! Setting up Shared Memory..." << endl;

    // 1. Setup the Shared Memory Block
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedRadarBuffer));
    SharedRadarBuffer* shm_ptr = (SharedRadarBuffer*)mmap(0, sizeof(SharedRadarBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    shm_ptr->is_data_ready = false;

    vector<uint8_t> incoming_packet;
    vector<uint8_t> leftover;
    vector<uint8_t> doctor_accumulator;
    bool is_sync = false;

    while (realtime_active || realtime_queue.size_approx() > 0) {

        if(shm_ptr->is_data_ready == true)
        {
            while(realtime_queue.try_dequeue(incoming_packet)){}
            doctor_accumulator.clear();
            leftover.clear();
            is_sync = false;
            this_thread::sleep_for(chrono::milliseconds(2));
            continue;

        }


        if (realtime_queue.try_dequeue(incoming_packet)) {
            
            // Combine leftover bytes
            if (!leftover.empty()) {
                incoming_packet.insert(incoming_packet.begin(), leftover.begin(), leftover.end());
                leftover.clear();
            }

            size_t byte_offset = 0;

            // Phase Alignment (Just like the SSD thread)
            if (!is_sync) {
                uint16_t* words = reinterpret_cast<uint16_t*>(incoming_packet.data());
                size_t num_words = incoming_packet.size() / 2;
                int sync_word_idx = -1;

                for (size_t i = 0; i < num_words; i++) {
                    if (words[i] >= HEADER_VAL_TX1) { 
                        if (i + WORDS_PER_SWEEP < num_words) {
                            if (words[i + WORDS_PER_SWEEP] >= HEADER_VAL_TX1) {
                                sync_word_idx = i;
                                break;
                            }
                        } else {
                            sync_word_idx = i;
                            break;
                        }
                    }
                }

                if (sync_word_idx != -1) {
                    is_sync = true;
                    byte_offset = sync_word_idx * 2;
                } else {
                    continue; // Skip this packet if no header found
                }
            }

            // Grab the aligned bytes
            size_t bytes = incoming_packet.size() - byte_offset;
            if (bytes % 2 != 0) {
                leftover.push_back(incoming_packet.back());
                bytes -= 1;
            }

            // Put aligned bytes into the Doctor's bucket
            doctor_accumulator.insert(doctor_accumulator.end(), 
                                      incoming_packet.begin() + byte_offset, 
                                      incoming_packet.begin() + byte_offset + bytes);

            // THROTTLING / HANDSHAKE LOGIC
            while (doctor_accumulator.size() >= BYTES_PER_CHUNK) {
                
                if (shm_ptr->is_data_ready == false) {
                    // DOCTOR IS READY! Give him the data.
                    memcpy(shm_ptr->payload, doctor_accumulator.data(), BYTES_PER_CHUNK);
                    shm_ptr->is_data_ready = true; // Tell him to wake up!
                }
                // If flag is TRUE, he is still working. We intentionally drop the chunk for him!

                // Always delete the processed chunk from our accumulator so memory stays flat
                doctor_accumulator.clear();
                is_sync = false;
            }
            
        } else {
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }
    munmap(shm_ptr, sizeof(SharedRadarBuffer));
    close(shm_fd);
    cout << "[RADAR REALTIME] Thread Safely Stopped." << endl;
}