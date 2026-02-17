// writer.hpp
//
// Definitions for the writer stage (Step 2: compression and writing).
// Defines Frame, CompressionResult, Row structures and declares the writer functions.

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
static constexpr int BUF_COUNT = 4;  
// Frame: produced by capture thread, consumed by writer thread.
struct Frame {
    uint8_t*   ptr;     // Pointer to RAW16 frame buffer
    uint32_t   len;     // Length of the buffer in bytes
    uint64_t   ts_ns;   // Timestamp (nanoseconds)
    uint32_t   width;   // Frame width in pixels
    uint32_t   height;  // Frame height in pixels
    uint32_t   bpp;     // Bits per pixel (e.g., 16 for RAW16)
};

// CompressionResult: produced by compress_frame(), contains compressed data and metadata.
struct CompressionResult {
    uint64_t            ts_ns;
    uint32_t            width;
    uint32_t            height;
    uint32_t            bpp;
    uint32_t            raw_bytes;
    std::vector<char>   compressed;  // Compressed payload (LZ4 output)
    std::string         fname;       // Filename for this frame's data (e.g., "frame_<ts>.raw16.lz4")
};

// Row: metadata to be inserted into the database (produced after writing file).
struct Row {
    uint64_t    ts_ns;
    uint32_t    width;
    uint32_t    height;
    uint32_t    bpp;
    uint32_t    raw_bytes;
    uint32_t    compressed_bytes;
    std::string filepath;
};

// Function to push a new frame into the writer queue (called by capture thread).
// Returns false if the stop flag is set and frame was not queued.
bool push_frame(const Frame& fr);

// Compress a raw frame to LZ4 and return the result (no I/O is done here).
CompressionResult compress_frame(const Frame& fr);

// Write the compressed frame to disk (with a header) and enqueue a DB Row.
// Returns the Row (metadata) for potential use by the DB thread.
Row write_and_enqueue(const CompressionResult& comp,
                      const std::filesystem::path& capture_dir,
                      const std::filesystem::path& archive_dir);
