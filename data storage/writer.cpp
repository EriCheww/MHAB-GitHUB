// writer.cpp
//
// Implementation of writer thread functions (Step 2: Compression and Write).
// This uses the LZ4 library for compression and writes output files to disk.
// ---- LZ4 STUB (local testing only) ----
#include "db.hpp"

#include <cstring>  // for memcpy
#include <iostream> // for std::cout / std::cerr
#include "third_party/lz4/lz4.h"
#include <atomic>
#include <vector>
#include <filesystem>
#include <cstdint>

#include "writer.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstring> // for memcpy, etc.

// External signals and DB-queue function
extern std::atomic<bool> g_stop;
extern void push_row(const Row &row);

// Queue and synchronization primitives for frames (capture -> writer)
std::mutex frame_mtx;
std::condition_variable frame_cv_nonempty;
std::condition_variable frame_cv_notfull;
static std::queue<Frame> frame_queue;

// Helper: build filename for a given timestamp (ns)
static std::string build_filename(uint64_t ts_ns)
{
    std::ostringstream oss;
    oss << "frame_" << ts_ns << ".raw16.lz4";
    return oss.str();
}

// push_frame: enqueue a new Frame into the writer's queue (blocks if full)
bool push_frame(const Frame &fr)
{
    std::unique_lock<std::mutex> lock(frame_mtx);
    // Wait until queue has space or stop is signaled
    frame_cv_notfull.wait(lock, []
                          { return g_stop.load() || frame_queue.size() < static_cast<size_t>(BUF_COUNT); });
    if (g_stop.load())
    {
        // Stop flag set, do not enqueue new frame
        return false;
    }
    frame_queue.push(fr);
    // Notify writer_thread that a frame is available
    frame_cv_nonempty.notify_one();
    return true;
}

// compress_frame: compress the raw frame data using LZ4
CompressionResult compress_frame(const Frame &fr)
{
    CompressionResult res;
    res.ts_ns = fr.ts_ns;
    res.width = fr.width;
    res.height = fr.height;
    res.bpp = fr.bpp;
    res.raw_bytes = fr.len;
    res.fname = build_filename(fr.ts_ns);

    // Compute max compressed size and allocate buffer
    int srcSize = static_cast<int>(fr.len);
    int cap = LZ4_compressBound(srcSize);

    std::vector<char> cmp_buf(static_cast<size_t>(cap));

    int cmp_len = LZ4_compress_default(
        reinterpret_cast<const char *>(fr.ptr),
        cmp_buf.data(),
        srcSize,
        cap);

    if (cmp_len <= 0)
    {
        throw std::runtime_error("LZ4_compress_default failed");
    }

    cmp_buf.resize(static_cast<size_t>(cmp_len));
    res.compressed = std::move(cmp_buf);
    return res;
}

// write_and_enqueue: write compressed frame to disk and prepare DB row
Row write_and_enqueue(const CompressionResult &comp,
                      const std::filesystem::path &capture_dir,
                      const std::filesystem::path &archive_dir)
{
    // Ensure output directories exist
    std::filesystem::create_directories(capture_dir);
    std::filesystem::create_directories(archive_dir);

    // Prepare file paths (.tmp in capture_dir, final in archive_dir for atomic rename)
    std::filesystem::path tmp_path = capture_dir / (comp.fname + ".tmp");
    std::filesystem::path final_path = archive_dir / comp.fname;

    // Prepare a small header with metadata (for offline decoding of .lz4 file)
#pragma pack(push, 1)
    struct Header
    {
        uint32_t magic; // 'LZ4' tag
        uint32_t width;
        uint32_t height;
        uint32_t bpp;
        uint32_t raw_bytes;
        uint32_t cmp_bytes;
        uint64_t ts_ns;
    };
#pragma pack(pop)

    Header hdr{};
    hdr.magic = 0x004C5A34; // 'LZ4' (low 3 bytes), safe
    hdr.width = comp.width;
    hdr.height = comp.height;
    hdr.bpp = comp.bpp;
    hdr.raw_bytes = comp.raw_bytes;
    hdr.cmp_bytes = static_cast<uint32_t>(comp.compressed.size());
    hdr.ts_ns = comp.ts_ns;

    // Write header + compressed data to a temporary file
    {
        std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
        if (!ofs)
        {
            throw std::runtime_error("Cannot write temp file: " + tmp_path.string());
        }
        ofs.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
        ofs.write(comp.compressed.data(), static_cast<std::streamsize>(comp.compressed.size()));
        ofs.flush();
        if (!ofs)
        {
            // If write failed, remove the temp file and throw
            ofs.close();
            std::filesystem::remove(tmp_path);
            throw std::runtime_error("Write failed for file: " + tmp_path.string());
        }
    }
    // Atomically rename the temp file to the final destination (on same filesystem)
    std::filesystem::rename(tmp_path, final_path);

    // Build the Row for database logging
    Row row;
    row.ts_ns = comp.ts_ns;
    row.width = comp.width;
    row.height = comp.height;
    row.bpp = comp.bpp;
    row.raw_bytes = comp.raw_bytes;
    row.compressed_bytes = static_cast<uint32_t>(comp.compressed.size());
    row.filepath = final_path.string();

    // Enqueue the Row into the DB thread's queue
    push_row(row);
    return row;
}

// writer_thread: main loop that consumes frames, compresses, and writes them
void writer_thread()
{
    std::cout << "[Writer] Thread started.\n";
    while (true)
    {
        std::unique_lock<std::mutex> lock(frame_mtx);
        // Wait for a frame to be available, or stop signal
        frame_cv_nonempty.wait(lock, []
                               { return g_stop.load() || !frame_queue.empty(); });
        // If stop was signaled and no frames remain, exit the loop
        if (g_stop.load() && frame_queue.empty())
        {
            lock.unlock();
            break;
        }
        // Get the next frame from the queue
        Frame fr = frame_queue.front();
        frame_queue.pop();
        // Notify capture thread that a buffer slot is free
        frame_cv_notfull.notify_one();
        lock.unlock();

        try
        {
            // Compress the frame and write to disk
            CompressionResult comp = compress_frame(fr);
            Row dbRow = write_and_enqueue(comp, "temp_capture", "archive");
            // Free the raw frame buffer now that it's written
            delete[] fr.ptr;
            std::cout << "[Writer] Compressed frame ts=" << fr.ts_ns
                      << " (" << dbRow.raw_bytes << " -> "
                      << dbRow.compressed_bytes << " bytes)" << std::endl;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "[Writer] Error: " << ex.what() << std::endl;
            // Continue loop even if one frame fails, or break if needed
        }
    }
    // Push a sentinel row to tell the DB thread to stop, then exit
    Row sentinel;
    sentinel.ts_ns = 0;
    push_row(sentinel); // enqueue sentinel
    std::cout << "[Writer] Thread exiting.\n";
}
