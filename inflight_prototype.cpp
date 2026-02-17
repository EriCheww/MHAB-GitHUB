// MHAB In-Flight Data Archival System
// linux cmd:  g++ -std=c++17 mhab_inflight_code.cpp -o mhab_inflight_code -lsqlite3 -llz4 `pkg-config --cflags --libs opencv4`
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <csignal>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cstring> 

// LIBRARIES
#include <lz4.h>         
#include <sqlite3.h>     
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

// CONFIGURATION
const std::string SOURCE_DIR = "gray16bit_linear"; 
const std::string ARCHIVE_DIR = "mhab_archive";
const std::string DB_PATH = "mhab_archive/mission_data.db";

// --- DATA STRUCTURES ---

#pragma pack(push, 1)
struct FileHeader {
    uint32_t magic; uint32_t width; uint32_t height; uint32_t bpp;
    uint32_t raw_bytes; uint32_t cmp_bytes; uint64_t ts_ns;
};
#pragma pack(pop)

struct Frame {
    cv::Mat image;
    uint64_t ts_ns;
    int id;
};

struct Row {
    int frame_id;
    std::string timestamp; // String format: "02/09/12/30/05/500"
    int width;
    int height;
    int bpp;
    int raw_bytes;
    int compressed_bytes;
    std::string filepath;
};

// --- GLOBALS & SYNC ---
std::atomic<bool> running(true);

std::queue<Frame> ring_q; 
std::mutex ring_mtx;
std::condition_variable ring_cv;

std::queue<Row> db_q;
std::mutex db_mtx;
std::condition_variable db_cv;

// --- ROBUST SIGNAL HANDLER ---
void signal_handler(int signum) { 
    std::cout << "\n[System] Interrupt Received! Initiating Shutdown Sequence...\n";
    running = false; 
    
    // Wake up everyone so they can check the 'running' flag and exit
    ring_cv.notify_all();
    db_cv.notify_all();
}

// --- HELPER ---
std::vector<uint8_t> apply_byte_shuffle(const cv::Mat& img) {
    int total_pixels = img.total();
    std::vector<uint8_t> shuffled(total_pixels * 2);
    const uint16_t* input_data = reinterpret_cast<const uint16_t*>(img.data);
    uint8_t* out_high = shuffled.data();
    uint8_t* out_low = shuffled.data() + total_pixels;
    for (int i = 0; i < total_pixels; ++i) {
        uint16_t pixel = input_data[i];
        out_high[i] = (pixel >> 8) & 0xFF;
        out_low[i]  = pixel & 0xFF;
    }
    return shuffled;
}

std::string get_formatted_time() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%m/%d/%H/%M/%S/") 
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// =========================================================
// THREAD 1: DATABASE (SQLite Logger)
// =========================================================
void db_thread() {
    sqlite3* db = nullptr;
    if (sqlite3_open(DB_PATH.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[DB Error] Cannot open database.\n"; return;
    }

    // Optimization settings
    sqlite3_exec(db, "PRAGMA journal_mode=DELETE;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    // Create Table
    const char* schema = 
        "CREATE TABLE IF NOT EXISTS frames ("
        "  frame_id INTEGER PRIMARY KEY,"
        "  timestamp TEXT NOT NULL,"       
        "  width INTEGER NOT NULL,"
        "  height INTEGER NOT NULL,"
        "  bpp INTEGER NOT NULL,"
        "  raw_bytes INTEGER NOT NULL,"
        "  compressed_bytes INTEGER NOT NULL,"
        "  filepath TEXT NOT NULL"
        ");";
    sqlite3_exec(db, schema, nullptr, nullptr, nullptr);

    const char* insert_sql = "INSERT INTO frames VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);

    std::cout << "[DB] Thread active.\n";

    while (true) {
        Row row;
        {
            std::unique_lock<std::mutex> lock(db_mtx);
            db_cv.wait(lock, []{ return !db_q.empty() || !running; });
            
            if (db_q.empty() && !running) break;
            
            row = db_q.front();
            db_q.pop();
        }

        // DB Insert Logic
        sqlite3_reset(stmt);
        sqlite3_bind_int(stmt, 1, row.frame_id);
        sqlite3_bind_text(stmt, 2, row.timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, row.width);
        sqlite3_bind_int(stmt, 4, row.height);
        sqlite3_bind_int(stmt, 5, row.bpp);
        sqlite3_bind_int(stmt, 6, row.raw_bytes);
        sqlite3_bind_int(stmt, 7, row.compressed_bytes);
        sqlite3_bind_text(stmt, 8, row.filepath.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "[DB Error] " << sqlite3_errmsg(db) << "\n";
        }
    }

    // CLEANUP
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    std::cout << "[DB] Database Closed Safely.\n";
}

// =========================================================
// THREAD 2: CAPTURE
// =========================================================
void capture_thread() {
    std::vector<fs::path> file_list;
    try {
        for (const auto& entry : fs::directory_iterator(SOURCE_DIR)) {
            if (entry.is_regular_file()) file_list.push_back(entry.path());
        }
    } catch (...) {}

    if (file_list.empty()) return;
    int frame_id = 0;
    size_t current_index = 0;

    while (running) {
        auto next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(667); 
        fs::path current_file = file_list[current_index];
        current_index = (current_index + 1) % file_list.size();

        try {
            auto now = std::chrono::system_clock::now();
            uint64_t ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

            cv::Mat img = cv::imread(current_file.string(), cv::IMREAD_UNCHANGED);
            
            if (!img.empty()) {
                {
                    std::lock_guard<std::mutex> lock(ring_mtx);
                    ring_q.push({img, ts_ns, ++frame_id});
                }
                ring_cv.notify_one(); 
                std::cout << "[Capture] Frame " << frame_id << "\n";
            }
        } catch (...) {}
        std::this_thread::sleep_until(next_tick);
    }
    std::cout << "[Capture] Stopped.\n";
}

// =========================================================
// THREAD 3: WRITER
// =========================================================
void writer_thread() {
    while (true) {
        Frame f;
        {
            std::unique_lock<std::mutex> lock(ring_mtx);
            ring_cv.wait(lock, []{ return !ring_q.empty() || !running; });
            
            // EXIT CONDITION: Shutdown AND empty queue
            if (ring_q.empty() && !running) break;
            
            f = std::move(ring_q.front());
            ring_q.pop();
        }

        // 1. Compression
        std::vector<uint8_t> raw_buffer;
        if (f.image.depth() == CV_16U) raw_buffer = apply_byte_shuffle(f.image);
        else {
            int size = f.image.total() * f.image.elemSize();
            raw_buffer.resize(size);
            std::memcpy(raw_buffer.data(), f.image.data, size);
        }

        int src_size = raw_buffer.size();
        int max_dst_size = LZ4_compressBound(src_size);
        std::vector<char> compressed_data(max_dst_size);
        int compressed_size = LZ4_compress_default(reinterpret_cast<const char*>(raw_buffer.data()), compressed_data.data(), src_size, max_dst_size);

        if (compressed_size <= 0) continue;

        // 2. Save to Disk
        FileHeader hdr = {0x004C5A34, (uint32_t)f.image.cols, (uint32_t)f.image.rows, (uint32_t)(f.image.elemSize()*8), (uint32_t)src_size, (uint32_t)compressed_size, f.ts_ns};
        std::string filename = "frame_" + std::to_string(f.id) + ".lz4";
        std::string tmp_path = ARCHIVE_DIR + "/" + filename + ".tmp";
        std::string final_path = ARCHIVE_DIR + "/" + filename;

        std::ofstream out(tmp_path, std::ios::binary);
        out.write((char*)&hdr, sizeof(hdr));
        out.write(compressed_data.data(), compressed_size);
        out.close();
        fs::rename(tmp_path, final_path);

        // 3. Queue for DB (Corrected to use Formatted String)
        {
            std::lock_guard<std::mutex> lock(db_mtx);
            Row r;
            r.frame_id = f.id;
            r.timestamp = get_formatted_time(); // Uses the helper function
            r.width = f.image.cols;
            r.height = f.image.rows;
            r.bpp = hdr.bpp;
            r.raw_bytes = src_size;
            r.compressed_bytes = compressed_size;
            r.filepath = final_path;
            db_q.push(r);
        }
        db_cv.notify_one();
    }
    std::cout << "[Writer] Queue Flushed & Stopped.\n";
}

// =========================================================
// THREAD 4: SENSORS
// =========================================================
void sensor_thread() {
    std::string dirs[] = {"imu", "pressure", "humidity", "current", "temperature"};
    for(const auto& d : dirs) fs::create_directories(ARCHIVE_DIR + "/sensors/" + d);
    std::ofstream log_curr(ARCHIVE_DIR + "/sensors/current/power_log.csv", std::ios::app);
    
    while (running) {
        auto next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(333);
        
        auto now = std::chrono::system_clock::now();
        uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        log_curr << ts << ",500.0,120.0,10.0\n"; 
        
        std::this_thread::sleep_until(next_tick);
    }
    std::cout << "[Sensors] Stopped.\n";
}

// =========================================================
// MAIN
// =========================================================
int main() {
    std::signal(SIGINT, signal_handler);
    
    if (fs::exists(ARCHIVE_DIR)) fs::remove_all(ARCHIVE_DIR);
    fs::create_directories(ARCHIVE_DIR);

    std::cout << "[Main] System Started. Press Ctrl+C to stop.\n";

    std::thread t_db(db_thread);
    std::thread t_cap(capture_thread);
    std::thread t_wri(writer_thread);
    std::thread t_sen(sensor_thread);

    t_cap.join();
    t_sen.join();

    ring_cv.notify_all(); 
    t_wri.join();
    
    db_cv.notify_all();
    t_db.join();

    std::cout << "[Main] All threads joined. System Exit.\n";
    return 0;
}
