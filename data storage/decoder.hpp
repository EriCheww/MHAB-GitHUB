// decoder.hpp
//
// Post-flight decoder utility for decompressing LZ4 frames back to raw format.
// Reads the database, retrieves compressed files, and decodes them.

#pragma once
#include <string>
#include <cstdint>

// Header structure (must match writer.cpp)
#pragma pack(push, 1)
struct FileHeader {
    uint32_t magic;      // 'LZ4' tag (0x004C5A34)
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t raw_bytes;
    uint32_t cmp_bytes;
    uint64_t ts_ns;
};
#pragma pack(pop)

// Decode a single compressed file and save as raw or FITS
bool decode_file(const std::string& input_path, 
                 const std::string& output_path,
                 bool save_as_fits = false);

// Decode all files from the database
void decode_all_from_db(const std::string& db_path,
                        const std::string& output_dir,
                        bool save_as_fits = false);

// Save raw data as FITS or SER format
bool save_as_FITS_or_SER(const uint8_t* raw_data,
                         uint32_t width,
                         uint32_t height,
                         uint32_t bpp,
                         uint64_t ts_ns,
                         const std::string& output_path);
