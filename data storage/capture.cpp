// capture.cpp
//
// Simulated capture thread (dummy camera data producer).
// Generates fake frames and pushes them to the writer queue.

#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include "writer.hpp"  // for Frame definition and push_frame()

// External stop flag and constants
extern std::atomic<bool> g_stop;
static constexpr int WIDTH  = 1936;
static constexpr int HEIGHT = 1216;
static constexpr int BPP    = 16;
static constexpr int RAW_BYTES = WIDTH * HEIGHT * (BPP / 8);
static constexpr int FPS    = 2;  // target ~2 frames per second

void capture_thread(int fd) {
    // We ignore the camera file descriptor since this is a simulation
    (void)fd;
    std::cout << "[Capture] Thread started, generating dummy frames..." << std::endl;
    using Clock = std::chrono::steady_clock;

    while (!g_stop.load()) {
        // Allocate a new raw frame buffer
        uint8_t* buffer = new uint8_t[RAW_BYTES];
        if (!buffer) {
            std::cerr << "[Capture] Frame buffer allocation failed, stopping capture.\n";
            break;
        }
        // Fill the buffer with a dummy pattern (e.g., repeating byte values)
        for (int i = 0; i < RAW_BYTES; ++i) {
            buffer[i] = static_cast<uint8_t>(i & 0xFF);
        }
        // Timestamp in nanoseconds
        uint64_t ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             Clock::now().time_since_epoch()).count();
        // Create Frame object
        Frame fr;
        fr.ptr    = buffer;
        fr.len    = RAW_BYTES;
        fr.ts_ns  = ts_ns;
        fr.width  = WIDTH;
        fr.height = HEIGHT;
        fr.bpp    = BPP;
        // Push the frame to the writer queue. If the queue is full, this call will block until space is available.
        if (!push_frame(fr)) {
            // If push_frame returns false, it means a stop signal was received.
            delete[] buffer;  // free buffer (since writer thread won't take ownership in this case)
            break;
        }
        std::cout << "[Capture] Captured frame ts=" << ts_ns 
                  << " (" << fr.len << " bytes)" << std::endl;
        // Sleep ~500ms to simulate 2 FPS capture rate
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "[Capture] Thread exiting.\n";
}
