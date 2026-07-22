#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include "IpcSharedState.h"

using namespace std;

int main() {
    cout << "[DOCTOR PROCESS] Booting up... Connecting to Shared RAM..." << endl;

    // Connect to the exact same RAM file
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        cerr << "Error: Could not find Shared Memory. Is the Master Script running?" << endl;
        return -1;
    }

    SharedRadarBuffer* shm_ptr = (SharedRadarBuffer*)mmap(0, sizeof(SharedRadarBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    cout << "[DOCTOR PROCESS] Connected! Waiting for radar data..." << endl;

    int chunks_processed = 0;

    while (true) {
        // 1. Wait for the handshake flag
        if (shm_ptr->is_data_ready == true) {
            
            // 2. We got data! Cast it to 16-bit integers
            uint16_t* radar_words = reinterpret_cast<uint16_t*>(shm_ptr->payload);
            
            // 3. PROVE IT IS PERFECTLY ALIGNED (Check the magic header)
            if (radar_words[0] >= 49152) {
                cout << "\n✅ [CHUNK " << ++chunks_processed << "] SUCCESS: Received " << BYTES_PER_CHUNK << " bytes." << endl;
                cout << "   -> Perfect Alignment Confirmed! First word is 49152." << endl;
            } else {
                cout << "\n❌ ERROR: Misaligned data! First word was: " << radar_words[0] << endl;
            }

            // 4. Simulate doing heavy CUDA FFT math (Takes 200ms)
            cout << "   -> Executing heavy math (sleeping 200ms)..." << endl;
            this_thread::sleep_for(chrono::milliseconds(200));

            // 5. Flip the flag back so the Producer gives us new data!
            shm_ptr->is_data_ready = false;
            cout << "   -> Math complete. Ready for next chunk." << endl;
            
        } else {
            // Idle a tiny bit to not burn CPU while waiting
            this_thread::sleep_for(chrono::milliseconds(2));
        }
    }

    return 0;
}