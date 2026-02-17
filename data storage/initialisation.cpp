// initialisation.cpp
//
// Step 1: Program start / Initialisation
// Sets up directories, opens the SQLite3 database, configures the camera (stubbed), 
// and launches the capture, writer, and DB threads.






#include <iostream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "db.hpp"



// Constants for image dimensions and buffer count (from project specs)
static constexpr int WIDTH  = 1936;
static constexpr int HEIGHT = 1216;
static constexpr int BPP    = 16;                       // RAW16 => 16 bits per pixel
static constexpr int RAW_BYTES = WIDTH * HEIGHT * (BPP / 8);
                 // using 2–4 buffers for ~2 fps

// Data storage paths (using relative paths for local testing on Windows)
static const std::filesystem::path CAPTURE_DIR = "temp_capture";
static const std::filesystem::path ARCHIVE_DIR = "archive";
static const char* DB_PATH = "archive/frames.sqlite3";

// Global flag to signal threads to stop
std::atomic<bool> g_stop(false);

// Thread function declarations (implemented in other files)
extern void capture_thread(int fd);
extern void writer_thread();
extern void db_thread(sqlite3* db);


// Condition variables to coordinate the frame queue (for stop notifications)
extern std::condition_variable frame_cv_nonempty;
extern std::condition_variable frame_cv_notfull;

// Utility: ensure a directory exists (create if missing)
static void ensure_dir(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    if (ec) {
        std::cerr << "Warning: Could not create " << p 
                  << ": " << ec.message() << std::endl;
    }
}

int main() {
    // ----- 1.1 Create folders and open database -----
    ensure_dir(CAPTURE_DIR);
    ensure_dir(ARCHIVE_DIR);

    sqlite3* db = nullptr;
    if (!db_open(&db, DB_PATH)) {
        std::cerr << "DB open failed: " << DB_PATH << "\n";
        return 1;
    }


    // ----- 1.2 Open camera (stub) and prepare V4L2 buffers -----
    int fd = -1;
    // (Camera calls are placeholders since no real camera is connected)
    // fd = v4l2_open("/dev/videoX", O_RDWR | O_NONBLOCK);
    // v4l2_set_format(fd, RAW16, WIDTH, HEIGHT, /*fps=*/2);
    // v4l2_mmap_buffers(fd, BUF_COUNT);
    // v4l2_stream_on(fd);

    // ----- 1.3 Launch the capture, writer, and DB threads -----
    std::thread t_capture(capture_thread, fd);
    std::thread t_writer(writer_thread);
    std::thread t_db(db_thread, db);

    // Wait for a shutdown signal (here, we use a console input to stop)
    std::cout << "Press Enter to stop capture...\n";
    std::cin.get();

    // ----- 1.4 Orderly shutdown -----
std::cout << "Stopping threads...\n";
g_stop = true;
frame_cv_nonempty.notify_all();
frame_cv_notfull.notify_all();

// Ensure DB thread wakes and exits
Row s{};
s.ts_ns = 0;
push_row(s);

t_capture.join();
t_writer.join();
t_db.join();

    // Cleanup: stop camera streaming and close handles (if this were real)
    // v4l2_stream_off(fd);
    // v4l2_close(fd);
    db_close(db);

    std::cout << "Shutdown complete.\n";
    return 0;
}
