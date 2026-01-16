#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <csignal>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

// Paths and Constants [cite: 19, 21, 118]
const std::string SOURCE_DIR = R"(/mnt/c/Users/30939/OneDrive/Pictures/Camera Roll)";
const std::string ARCHIVE_DIR = "mhab_archive";

struct Frame {
    cv::Mat image;
    uint64_t ts_ns;
    int id;
};

// Thread Communication Queues (RingQ) [cite: 122, 291]
std::queue<Frame> RingQ; 
std::mutex mtx;
std::condition_variable queue_cv;
std::atomic<bool> running(true);

void signal_handler(int signum) { 
    running = false; 
    queue_cv.notify_all();  
}

// --- 1. CAPTURE THREAD ---
// Goal: Fetch a copy of the image and timestamp it [cite: 134, 135]
void capture_thread() {
    int frame_id = 0;
    while (running) {
        // Updated to 1.5 FPS (approx 667ms period)
        auto next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(667); 

        try {
            for (const auto& entry : fs::directory_iterator(SOURCE_DIR)) {
                if (entry.is_regular_file()) {
                    // Precise Timestamping for scientific alignment [cite: 22, 134]
                    auto now = std::chrono::system_clock::now();
                    auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

                    // Load image to add the visual "Scan Time" overlay
                    cv::Mat img = cv::imread(entry.path().string());
                    if (img.empty()) continue;

                    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
                    char time_str[100];
                    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
                    std::string overlay = "Scan Time: " + std::string(time_str) + "." + std::to_string(ts_ns % 1000000000);
                    cv::putText(img, overlay, cv::Point(50, 50), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 255, 0), 3);

                    // Push to Queue for processing [cite: 67, 135]
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        RingQ.push({img, (uint64_t)ts_ns, ++frame_id});
                    }
                    queue_cv.notify_one(); 

                    // DEMO CHANGE: We no longer remove the original file.
                    // We simply log the "capture" and wait for the next tick.
                    std::cout << "[Capture] Copied & Queued Frame " << frame_id << " (Original remains in Camera Roll)" << std::endl;
                    break; 
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[Capture Error] " << e.what() << std::endl;
        }
        std::this_thread::sleep_until(next_tick);
    }
}

// --- 2. WRITER THREAD ---
// Goal: Save data using the Atomic Write Protocol [cite: 140, 274, 293]
void writer_thread() {
    while (running || !RingQ.empty()) {
        Frame f;
        {
            std::unique_lock<std::mutex> lock(mtx);
            queue_cv.wait(lock, []{ return !RingQ.empty() || !running; }); // in writer_thread
            if (RingQ.empty() && !running) break;

            f = std::move(RingQ.front());
            RingQ.pop();
        }

        // 2.1 COMPRESSION STUB (Requirement: 1.3x size reduction) [cite: 21, 44, 301]
        // teammate_lz4_compress(f.image); 
        
        // 2.2 ATOMIC WRITE PROTOCOL (Section 6.2) [cite: 140, 141]
        std::string base_name = "frame_" + std::to_string(f.id);
        // Change these lines in your writer_thread
        std::string tmp_path = ARCHIVE_DIR + "/" + base_name + ".tmp.jpg";
        std::string final_path = ARCHIVE_DIR + "/" + base_name + ".jpg";

        try {
            // Step 1: Write to temporary file [cite: 140]
            cv::imwrite(tmp_path, f.image); 
            
            // Step 2: Atomic Rename to final path [cite: 72, 140]
            fs::rename(tmp_path, final_path);

            std::cout << "[Writer] Atomically stored: " << final_path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Writer Error] " << e.what() << std::endl;
        }
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    fs::create_directories(ARCHIVE_DIR);

    std::cout << "[Main] Starting Combined Threads at 1.5 FPS. Original files will NOT be deleted.\n";

    std::thread t1(capture_thread);
    std::thread t2(writer_thread);

    t1.join();
    t2.join();
    return 0;
}