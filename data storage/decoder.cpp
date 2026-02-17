// decoder.cpp
//
// Implementation of post-flight decoder for LZ4 compressed frames.
// Reads compressed .lz4 files and outputs raw RAW16 or FITS format.

#include "decoder.hpp"
#include "third_party/lz4/lz4.h"
#include "third_party/sqlite/sqlite3.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>

// Parse the header from a compressed file
static bool parse_header(std::ifstream& ifs, FileHeader& hdr) {
    ifs.read(reinterpret_cast<char*>(&hdr), sizeof(FileHeader));
    if (!ifs) {
        std::cerr << "[Decoder] Failed to read file header.\n";
        return false;
    }
    
    // Verify magic number
    if (hdr.magic != 0x004C5A34) {
        std::cerr << "[Decoder] Invalid magic number: 0x" 
                  << std::hex << hdr.magic << std::dec << "\n";
        return false;
    }
    
    return true;
}

// Decode a single compressed file
bool decode_file(const std::string& input_path, 
                 const std::string& output_path,
                 bool save_as_fits) {
    
    // Open input file
    std::ifstream ifs(input_path, std::ios::binary);
    if (!ifs) {
        std::cerr << "[Decoder] Cannot open file: " << input_path << "\n";
        return false;
    }
    
    // Read and parse header
    FileHeader hdr;
    if (!parse_header(ifs, hdr)) {
        return false;
    }
    
    std::cout << "[Decoder] File: " << input_path << "\n";
    std::cout << "          Size: " << hdr.width << "x" << hdr.height 
              << ", BPP: " << hdr.bpp << "\n";
    std::cout << "          Raw: " << hdr.raw_bytes << " bytes, "
              << "Compressed: " << hdr.cmp_bytes << " bytes\n";
    std::cout << "          Timestamp: " << hdr.ts_ns << " ns\n";
    
    // Read compressed data
    std::vector<char> compressed(hdr.cmp_bytes);
    ifs.read(compressed.data(), hdr.cmp_bytes);
    if (!ifs) {
        std::cerr << "[Decoder] Failed to read compressed data.\n";
        return false;
    }
    ifs.close();
    
    // Allocate buffer for decompressed data
    std::vector<uint8_t> raw(hdr.raw_bytes);
    
    // Decompress using LZ4
    int decompressed_size = LZ4_decompress_safe(
        compressed.data(),
        reinterpret_cast<char*>(raw.data()),
        static_cast<int>(hdr.cmp_bytes),
        static_cast<int>(hdr.raw_bytes)
    );
    
    if (decompressed_size < 0) {
        std::cerr << "[Decoder] LZ4 decompression failed with code: " 
                  << decompressed_size << "\n";
        return false;
    }
    
    if (static_cast<uint32_t>(decompressed_size) != hdr.raw_bytes) {
        std::cerr << "[Decoder] Size mismatch: expected " << hdr.raw_bytes 
                  << " but got " << decompressed_size << "\n";
        return false;
    }
    
    std::cout << "[Decoder] Successfully decompressed " << decompressed_size 
              << " bytes.\n";
    
    // Save output
    if (save_as_fits) {
        return save_as_FITS_or_SER(raw.data(), hdr.width, hdr.height, 
                                   hdr.bpp, hdr.ts_ns, output_path);
    } else {
        // Save as raw binary
        std::ofstream ofs(output_path, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            std::cerr << "[Decoder] Cannot create output file: " 
                      << output_path << "\n";
            return false;
        }
        
        ofs.write(reinterpret_cast<const char*>(raw.data()), hdr.raw_bytes);
        if (!ofs) {
            std::cerr << "[Decoder] Failed to write output file.\n";
            return false;
        }
        
        std::cout << "[Decoder] Saved raw data to: " << output_path << "\n";
        return true;
    }
}

// Simple FITS-like format saver (basic implementation)
bool save_as_FITS_or_SER(const uint8_t* raw_data,
                         uint32_t width,
                         uint32_t height,
                         uint32_t bpp,
                         uint64_t ts_ns,
                         const std::string& output_path) {
    
    // For simplicity, we'll save as a basic FITS-like format
    // A full FITS implementation would require a proper FITS library
    
    std::ofstream ofs(output_path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        std::cerr << "[Decoder] Cannot create FITS output: " << output_path << "\n";
        return false;
    }
    
    // Simple header (not real FITS, just metadata + raw data)
    // For real FITS, use a library like cfitsio
    char header[2880] = {0};  // FITS uses 2880-byte blocks
    
    snprintf(header, sizeof(header),
        "SIMPLE  =                    T / file conforms to FITS standard\n"
        "BITPIX  =                   16 / bits per pixel\n"
        "NAXIS   =                    2 / number of axes\n"
        "NAXIS1  =             %8u / width\n"
        "NAXIS2  =             %8u / height\n"
        "TIMESTMP= %20llu / timestamp in nanoseconds\n"
        "END",
        width, height, (unsigned long long)ts_ns);
    
    ofs.write(header, 2880);
    ofs.write(reinterpret_cast<const char*>(raw_data), width * height * (bpp / 8));
    
    if (!ofs) {
        std::cerr << "[Decoder] Failed to write FITS-like file.\n";
        return false;
    }
    
    std::cout << "[Decoder] Saved FITS-like format to: " << output_path << "\n";
    return true;
}

// Decode all files from database
void decode_all_from_db(const std::string& db_path,
                        const std::string& output_dir,
                        bool save_as_fits) {
    
    std::cout << "[Decoder] Opening database: " << db_path << "\n";
    
    // Open database
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[Decoder] Failed to open database: " << db_path << "\n";
        return;
    }
    
    // Create output directory
    std::filesystem::create_directories(output_dir);
    
    // Query all frames ordered by timestamp
    const char* query = "SELECT filepath, raw_bytes FROM frames ORDER BY ts_ns;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Decoder] Failed to prepare query: " 
                  << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return;
    }
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* filepath = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 0));
        
        if (!filepath) continue;
        
        std::filesystem::path input_path(filepath);
        
        // Generate output filename
        std::string output_filename = input_path.stem().string();
        if (save_as_fits) {
            output_filename += ".fits";
        } else {
            output_filename += ".raw16";
        }
        
        std::filesystem::path output_path = 
            std::filesystem::path(output_dir) / output_filename;
        
        // Decode the file
        if (decode_file(input_path.string(), output_path.string(), save_as_fits)) {
            count++;
        } else {
            std::cerr << "[Decoder] Failed to decode: " << filepath << "\n";
        }
        
        std::cout << "---\n";
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    std::cout << "[Decoder] Successfully decoded " << count << " frames.\n";
}
