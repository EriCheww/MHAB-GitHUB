// decode_main.cpp
//
// Standalone decoder application - run on laptop after flight.
// Usage: ./decoder <db_path> <output_dir> [--fits]
//
// Example: ./decoder archive/frames.sqlite3 decoded_frames
//          ./decoder archive/frames.sqlite3 decoded_frames --fits

#include <iostream>
#include <cstring>
#include "decoder.hpp"

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <db_path> <output_dir> [--fits]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  db_path     - Path to the SQLite database (e.g., archive/frames.sqlite3)\n";
    std::cout << "  output_dir  - Directory where decoded frames will be saved\n";
    std::cout << "  --fits      - (Optional) Save as FITS-like format instead of raw binary\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog_name << " archive/frames.sqlite3 decoded_frames\n";
    std::cout << "  " << prog_name << " archive/frames.sqlite3 decoded_frames --fits\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== LZ4 Frame Decoder (Post-Flight Utility) ===\n\n";
    
    // Parse command-line arguments
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string db_path = argv[1];
    std::string output_dir = argv[2];
    bool save_as_fits = false;
    
    // Check for --fits flag
    if (argc >= 4 && std::strcmp(argv[3], "--fits") == 0) {
        save_as_fits = true;
        std::cout << "[Decoder] Output format: FITS-like\n";
    } else {
        std::cout << "[Decoder] Output format: Raw binary (.raw16)\n";
    }
    
    std::cout << "[Decoder] Database: " << db_path << "\n";
    std::cout << "[Decoder] Output directory: " << output_dir << "\n\n";
    
    // Run the decoder
    decode_all_from_db(db_path, output_dir, save_as_fits);
    
    std::cout << "\n=== Decoding complete ===\n";
    return 0;
}
