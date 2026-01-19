#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <csignal>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>       // Added for binary file writing
#include <vector>        // Added for data buffers
#include <lz4.h>         // INTEGRATED: LZ4 Library
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

// Paths
const std::string SOURCE_DIR = R"(/mnt/c/Users/30939/OneDrive/Pictures/Camera Roll)";
const std::string ARCHIVE_DIR = "mhab_archive";

// --- HEADER STRUCTURE (From lz4.cpp) ---
// This ensures the binary file is self-describing for post-flight analysis.
#pragma pack(push, 1)
struct Header {
    uint32_t magic;      // 0x004C5A34
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t raw_bytes;
    uint32_t cmp_bytes;
    uint64_t ts_ns;
};
#pragma pack(pop)

struct Frame {
    cv::Mat image;
    uint64_t ts_ns;
    int id;
};

// Thread Communication
std::queue<Frame> RingQ; 
std::mutex mtx;
std::condition_variable queue_cv;
std::atomic<bool> running(true);

void signal_handler(int signum) { 
    running = false; 
    queue_cv.notify_all();  
}

// --- 1. CAPTURE THREAD (Unchanged) ---
void capture_thread() {
    std::vector<fs::path> file_list;
    try {
        for (const auto& entry : fs::directory_iterator(SOURCE_DIR)) {
            if (entry.is_regular_file()) file_list.push_back(entry.path());
        }
    } catch (...) {}

    if (file_list.empty()) {
        std::cerr << "[Capture] No images found!\n";
        return;
    }

    int frame_id = 0;
    size_t current_index = 0;

    while (running) {
        auto next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(667); 
        fs::path current_file = file_list[current_index];
        current_index = (current_index + 1) % file_list.size();

        try {
            auto now = std::chrono::system_clock::now();
            auto ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

            cv::Mat img = cv::imread(current_file.string());
            if (!img.empty()) {
                // Visual Timestamp Overlay
                std::time_t now_c = std::chrono::system_clock::to_time_t(now);
                char time_str[100];
                std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
                std::string overlay = "Scan Time: " + std::string(time_str);
                cv::putText(img, overlay, cv::Point(50, 50), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 255, 0), 3);

                {
                    std::lock_guard<std::mutex> lock(mtx);
                    RingQ.push({img, (uint64_t)ts_ns, ++frame_id});
                }
                queue_cv.notify_one(); 
                std::cout << "[Capture] Processed Frame " << frame_id << std::endl;
            }
        } catch (...) {}

        std::this_thread::sleep_until(next_tick);
    }
}

// --- 2. WRITER THREAD (INTEGRATED WITH LZ4) ---
void writer_thread() {
    std::cout << "[Writer] Thread active. Ready to compress...\n";
    while (running || !RingQ.empty()) {
        Frame f;
        {
            std::unique_lock<std::mutex> lock(mtx);
            queue_cv.wait(lock, []{ return !RingQ.empty() || !running; });
            if (RingQ.empty() && !running) break;

            f = std::move(RingQ.front());
            RingQ.pop();
        }

        // --- INTEGRATION START ---
        
        // 1. Prepare Data for Compression
        // We treat the cv::Mat pixels as the "Raw Buffer" (fr.ptr in lz4.cpp)
        int src_size = f.image.total() * f.image.elemSize();
        int max_dst_size = LZ4_compressBound(src_size);
        std::vector<char> compressed_data(max_dst_size);

        // 2. Perform LZ4 Compression
        int compressed_size = LZ4_compress_default(
            reinterpret_cast<const char*>(f.image.data), // Source pointer
            compressed_data.data(),                      // Dest buffer
            src_size,                                    // Source size
            max_dst_size                                 // Max Dest capacity
        );

        if (compressed_size <= 0) {
            std::cerr << "[Writer Error] LZ4 Compression failed!\n";
            continue;
        }

        // 3. Populate Header
        Header hdr;
        hdr.magic = 0x004C5A34; // LZ4 Magic Number
        hdr.width = f.image.cols;
        hdr.height = f.image.rows;
        hdr.bpp = f.image.elemSize() * 8; // Bits per pixel
        hdr.raw_bytes = src_size;
        hdr.cmp_bytes = compressed_size;
        hdr.ts_ns = f.ts_ns;

        // 4. Atomic Write Protocol
        std::string base_name = "frame_" + std::to_string(f.ts_ns);
        std::string tmp_path = ARCHIVE_DIR + "/" + base_name + ".tmp";
        std::string final_path = ARCHIVE_DIR + "/" + base_name + ".lz4"; // Binary extension

        try {
            // Write to Binary File (Stream instead of cv::imwrite)
            std::ofstream out(tmp_path, std::ios::binary);
            if (!out) throw std::runtime_error("Cannot open tmp file");

            // Write Header
            out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
            // Write Compressed Body
            out.write(compressed_data.data(), compressed_size);
            out.close();

            // Atomic Rename
            fs::rename(tmp_path, final_path);

            std::cout << "[Writer] Saved: " << final_path 
                      << " (Ratio: " << (float)src_size/compressed_size << "x)\n";

        } catch (const std::exception& e) {
            std::cerr << "[Writer Error] " << e.what() << std::endl;
        }
        // --- INTEGRATION END ---
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    fs::create_directories(ARCHIVE_DIR);

    std::cout << "[Main] Starting Pipeline with LZ4 Compression.\n";
    std::thread t1(capture_thread);
    std::thread t2(writer_thread);

    t1.join();
    t2.join();
    return 0;
}
