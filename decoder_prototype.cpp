#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <lz4.h>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

const std::string ARCHIVE_DIR = "mhab_archive";
const std::string OUTPUT_DIR = "mhab_decoded";

// --- HEADER STRUCTURE (Matches your Inflight Code) ---
#pragma pack(push, 1)
struct FileHeader {
    uint32_t magic;      // 0x004C5A34
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t raw_bytes;
    uint32_t cmp_bytes;
    uint64_t ts_ns;
};
#pragma pack(pop)

// --- HELPER: UN-SHUFFLE 16-BIT DATA ---
void reverse_byte_shuffle_16bit(const std::vector<uint8_t>& raw_data, cv::Mat& img) {
    int total_pixels = img.total();
    const uint8_t* in_high = raw_data.data();
    const uint8_t* in_low = raw_data.data() + total_pixels;
    uint16_t* out_data = reinterpret_cast<uint16_t*>(img.data);

    for (int i = 0; i < total_pixels; ++i) {
        // EXACT RECONSTRUCTION: Combine High Byte and Low Byte
        uint16_t high = static_cast<uint16_t>(in_high[i]);
        uint16_t low  = static_cast<uint16_t>(in_low[i]);
        out_data[i] = (high << 8) | low;
    }
}

void process_file(const fs::path& filepath) {
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return;

    // 1. Read Header
    FileHeader hdr;
    in.read(reinterpret_cast<char*>(&hdr), sizeof(FileHeader));

    if (hdr.magic != 0x004C5A34) {
        std::cerr << "[SKIP] " << filepath.filename() << " (Invalid Magic Header)\n";
        return;
    }

    // 2. Load Compressed Data
    std::vector<char> compressed_data(hdr.cmp_bytes);
    in.read(compressed_data.data(), hdr.cmp_bytes);
    in.close();

    // 3. Decompress
    std::vector<uint8_t> raw_buffer(hdr.raw_bytes);
    int decompressed_size = LZ4_decompress_safe(
        compressed_data.data(), 
        reinterpret_cast<char*>(raw_buffer.data()), 
        hdr.cmp_bytes, 
        hdr.raw_bytes
    );

    if (decompressed_size < 0) {
        std::cerr << "[ERROR] " << filepath.filename() << " (Decompression Failed)\n";
        return;
    }

    // 4. Reconstruct Image
    cv::Mat img;
    if (hdr.bpp == 16) {
        img = cv::Mat(hdr.height, hdr.width, CV_16UC1);
        reverse_byte_shuffle_16bit(raw_buffer, img);
    } else {
        img = cv::Mat(hdr.height, hdr.width, CV_8UC1);
        std::memcpy(img.data, raw_buffer.data(), hdr.raw_bytes);
    }

    // 5. Save with ORIGINAL NAME
    // filepath.stem() removes the ".lz4" extension
    // Example: "frame_1.lz4" -> "frame_1" -> "frame_1.png"
    std::string out_name = filepath.stem().string() + ".png"; 
    fs::path out_path = fs::path(OUTPUT_DIR) / out_name;
    
    // Disable PNG compression (0) to prove no data loss/fastest write
    std::vector<int> params;
    params.push_back(cv::IMWRITE_PNG_COMPRESSION);
    params.push_back(0); 

    cv::imwrite(out_path.string(), img, params);
    std::cout << "[Decoded] " << filepath.filename() << " -> " << out_name << "\n";
}

int main() {
    // Clean old files so you don't see the confusing timestamps anymore
    if (fs::exists(OUTPUT_DIR)) fs::remove_all(OUTPUT_DIR);
    fs::create_directories(OUTPUT_DIR);

    std::cout << "Starting Decoder...\n";

    for (const auto& entry : fs::directory_iterator(ARCHIVE_DIR)) {
        if (entry.path().extension() == ".lz4") {
            process_file(entry.path());
        }
    }
    
    std::cout << "Done.\n";
    return 0;
}
