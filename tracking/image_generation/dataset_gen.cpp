#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
namespace fs = std::filesystem;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

// ---------------- BLOCK TYPES ----------------
enum class BlockType {
    NONE = 0,
    VERTICAL_WIRE,
    CIRCLE,
    ELLIPSE,
    SQUARE
};

const char* blockTypeName(BlockType t) {
    switch (t) {
        case BlockType::NONE: return "NONE";
        case BlockType::VERTICAL_WIRE: return "VERTICAL_WIRE";
        case BlockType::CIRCLE: return "CIRCLE";
        case BlockType::ELLIPSE: return "ELLIPSE";
        case BlockType::SQUARE: return "SQUARE";
        default: return "UNKNOWN";
    }
}

// ---------------- GEOMETRY ----------------
inline void rotatePoint(float x, float y, float a, float& xr, float& yr) {
    float c = std::cos(a), s = std::sin(a);
    xr =  c * x + s * y;
    yr = -s * x + c * y;
}

// ---------------- BLOCK APPLICATION ----------------
void applyBlock(
    std::vector<unsigned char>& image,
    std::vector<bool>& sunMask,
    int width, int height,
    int cx, int cy, int sunRadius,
    BlockType type,
    float offsetPercent,
    int sizeA, int sizeB,
    float rotationDeg
) {
    if (type == BlockType::NONE) return;

    int offsetPx = int((offsetPercent / 100.0f) * sunRadius);
    int ox = cx + offsetPx;
    int oy = cy;

    float theta = rotationDeg * float(M_PI) / 180.0f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!sunMask[y * width + x]) continue;

            float dx = float(x - ox);
            float dy = float(y - oy);

            float rx, ry;
            rotatePoint(dx, dy, theta, rx, ry);

            bool blocked = false;
            switch (type) {
                case BlockType::VERTICAL_WIRE:
                    blocked = std::abs(rx) <= sizeA * 0.5f;
                    break;
                case BlockType::CIRCLE:
                    blocked = (rx*rx + ry*ry) <= sizeA*sizeA;
                    break;
                case BlockType::ELLIPSE:
                    blocked = (rx*rx)/(sizeA*sizeA) +
                              (ry*ry)/(sizeB*sizeB) <= 1.0f;
                    break;
                case BlockType::SQUARE:
                    blocked = std::abs(rx) <= sizeA*0.5f &&
                              std::abs(ry) <= sizeA*0.5f;
                    break;
                default:
                    break;
            }

            if (blocked) {
                int idx = (y * width + x) * 3;
                image[idx] = image[idx+1] = image[idx+2] = 0;
                sunMask[y * width + x] = false;
            }
        }
    }
}

// ---------------- MAIN ----------------
int main() {
    const int NUM_IMAGES = 1000;
    const int WIDTH = 3840;
    const int HEIGHT = 2160;
    const int SUN_RADIUS = 180;
    const float NOISE_PERCENT = 0.0f;

    std::mt19937 gen(1337);

    fs::create_directories("output/images");

    // Load sun texture
    int sunW, sunH, sunC;
    unsigned char* sunImg = stbi_load("sun.png", &sunW, &sunH, &sunC, 4);
    if (!sunImg) {
        std::cerr << "Failed to load sun.png\n";
        return 1;
    }

    std::ofstream json("metadata.json");
    json << "[\n";

    for (int i = 0; i < NUM_IMAGES; ++i) {

        std::ostringstream id;
        id << std::setw(3) << std::setfill('0') << (i+1);

        std::vector<unsigned char> image(WIDTH * HEIGHT * 3, 0);
        std::vector<bool> sunMask(WIDTH * HEIGHT, false);

        std::uniform_int_distribution<int> xDist(SUN_RADIUS, WIDTH - SUN_RADIUS - 1);
        std::uniform_int_distribution<int> yDist(SUN_RADIUS, HEIGHT - SUN_RADIUS - 1);

        int cx = xDist(gen);
        int cy = yDist(gen);

        // Render sun
        int sunSize = SUN_RADIUS * 2;
        int sunMin = std::min(sunW, sunH);
        int xo = (sunW - sunMin)/2;
        int yo = (sunH - sunMin)/2;

        for (int sy = 0; sy < sunSize; ++sy) {
            for (int sx = 0; sx < sunSize; ++sx) {
                int x = cx - SUN_RADIUS + sx;
                int y = cy - SUN_RADIUS + sy;
                if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) continue;

                int srcX = xo + (sx * sunMin) / sunSize;
                int srcY = yo + (sy * sunMin) / sunSize;
                int sidx = (srcY * sunW + srcX) * 4;

                unsigned char r = sunImg[sidx];
                unsigned char g = sunImg[sidx+1];
                unsigned char b = sunImg[sidx+2];
                unsigned char a = sunImg[sidx+3];

                if (a > 30) {
                    int didx = (y * WIDTH + x) * 3;
                    image[didx] = r;
                    image[didx+1] = g;
                    image[didx+2] = b;
                    sunMask[y * WIDTH + x] = true;
                }
            }
        }

        // Random block
        std::uniform_int_distribution<int> blockDist(0, 4);
        BlockType block = (BlockType)blockDist(gen);

        std::uniform_real_distribution<float> offsetDist(-30.f, 30.f);
        std::uniform_real_distribution<float> rotDist(0.f, 180.f);
        std::uniform_int_distribution<int> sizeADist(
            int(0.2f * SUN_RADIUS),   // 20% of radius
            int(1.2f * SUN_RADIUS)    // 80% of radius
        );

        std::uniform_int_distribution<int> sizeBDist(
            int(0.2f * SUN_RADIUS),
            int(1.2f * SUN_RADIUS)
        );


        float offset = offsetDist(gen);
        float rot = rotDist(gen);
        int sizeA = sizeADist(gen);
        int sizeB = sizeBDist(gen);


        applyBlock(
            image, sunMask,
            WIDTH, HEIGHT,
            cx, cy, SUN_RADIUS,
            block,
            offset,
            sizeA, sizeB,
            rot
        );

        // Noise
        int noisePx = int(NOISE_PERCENT/100.0f * WIDTH * HEIGHT);
        std::uniform_int_distribution<int> px(0, WIDTH-1), py(0, HEIGHT-1);

        for (int n = 0; n < noisePx; ++n) {
            int x = px(gen), y = py(gen);
            if (!sunMask[y*WIDTH + x]) {
                int idx = (y*WIDTH + x)*3;
                image[idx] = image[idx+1] = image[idx+2] = 255;
            }
        }

        std::string fname = "output/images/" + id.str() + ".png";
        stbi_write_png(fname.c_str(), WIDTH, HEIGHT, 3, image.data(), WIDTH*3);

        // JSON entry
        json << "  {\n";
        json << "    \"id\": \"" << id.str() << "\",\n";
        json << "    \"center\": [" << cx << ", " << cy << "],\n";
        json << "    \"radius\": " << SUN_RADIUS << ",\n";
        json << "    \"block\": {\n";
        json << "      \"type\": \"" << blockTypeName(block) << "\",\n";
        json << "      \"offset_percent\": " << offset << ",\n";
        json << "      \"sizeA\": " << sizeA << ",\n";
        json << "      \"sizeB\": " << sizeB << ",\n";
        json << "      \"rotation_deg\": " << rot << "\n";
        json << "    },\n";
        json << "    \"noise_percent\": " << NOISE_PERCENT << "\n";
        json << "  }" << (i+1 < NUM_IMAGES ? "," : "") << "\n";
    }

    json << "]\n";
    json.close();
    stbi_image_free(sunImg);

    std::cout << "Generated " << NUM_IMAGES << " images + metadata.json\n";
    return 0;
}
