// writer_test.cpp
//
// Standalone test for the writer logic (compression + file writing) with a dummy frame.

#include <iostream>
#include <cstring>
#include "writer.hpp"

int main() {
    // Dummy frame parameters (e.g., 640x480 image for quick test)
    uint32_t testWidth = 640;
    uint32_t testHeight = 480;
    uint32_t testBpp = 16;
    uint32_t rawBytes = testWidth * testHeight * (testBpp / 8);

    // Allocate and fill a dummy image buffer
    uint8_t* buffer = new uint8_t[rawBytes];
    if (!buffer) {
        std::cerr << "Allocation failed\n";
        return 1;
    }
    std::memset(buffer, 0xAB, rawBytes);  // fill with a pattern 0xAB

    // Construct a Frame object
    Frame fr;
    fr.ptr    = buffer;
    fr.len    = rawBytes;
    fr.ts_ns  = 1234567890ULL;    // example timestamp
    fr.width  = testWidth;
    fr.height = testHeight;
    fr.bpp    = testBpp;

    try {
        // Compress the frame
        CompressionResult comp = compress_frame(fr);
        // Write the compressed data to disk (in temp_capture/archive dirs) and get the DB row
        Row row = write_and_enqueue(comp, "temp_capture", "archive");
        std::cout << "Test: Compressed " << comp.raw_bytes << " bytes into " 
                  << comp.compressed.size() << " bytes." << std::endl;
        std::cout << "Output file: " << row.filepath << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Writer test error: " << ex.what() << std::endl;
    }

    delete[] buffer;
    return 0;
}
