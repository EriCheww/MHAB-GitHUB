#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cmath>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

// block types
enum class BlockType {
    NONE = 0,
    VERTICAL_WIRE,
    CIRCLE,
    ELLIPSE,
    SQUARE
};

// rotation 
inline void rotatePoint(
    float x, float y,
    float angleRad,
    float& xr, float& yr
) {
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    xr =  c * x + s * y;
    yr = -s * x + c * y;
}

// apply block
void applyBlock(
    std::vector<unsigned char>& image,
    std::vector<bool>& sunMask,
    int width,
    int height,
    int cx,
    int cy,
    int sunRadius,
    BlockType type,
    float offsetPercent,
    int sizeA,
    int sizeB,
    float rotationDeg
) {
    if (type == BlockType::NONE)
        return;

    int offsetPx = int((offsetPercent / 100.0f) * sunRadius);
    int ox = cx + offsetPx;
    int oy = cy;

    float theta = rotationDeg * float(M_PI) / 180.0f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {

            if (!sunMask[y * width + x])
                continue;

            // translate to block space
            float dx = float(x - ox);
            float dy = float(y - oy);

            // rotate into local frame
            float rx, ry;
            rotatePoint(dx, dy, theta, rx, ry);

            bool blocked = false;

            switch (type) {
                case BlockType::VERTICAL_WIRE:
                    blocked = std::abs(rx) <= sizeA * 0.5f;
                    break;

                case BlockType::CIRCLE:
                    blocked = (rx * rx + ry * ry) <= sizeA * sizeA;
                    break;

                case BlockType::ELLIPSE:
                    blocked = (rx * rx) / (sizeA * sizeA) +
                              (ry * ry) / (sizeB * sizeB) <= 1.0f;
                    break;

                case BlockType::SQUARE:
                    blocked = std::abs(rx) <= sizeA * 0.5f &&
                              std::abs(ry) <= sizeA * 0.5f;
                    break;

                default:
                    break;
            }

            if (blocked) {
                int idx = (y * width + x) * 3;
                image[idx + 0] = 0;
                image[idx + 1] = 0;
                image[idx + 2] = 0;
                sunMask[y * width + x] = false;
            }
        }
    }
}



int main() {
    int width, height;
    int radiusInput;
    float noisePercent;

    std::cout << "Image width (rec 3840): ";
    std::cin >> width;
    std::cout << "Image height (rec 2160): ";
    std::cin >> height;

    std::cout << "Sun radius (0 = random rec 180): ";
    std::cin >> radiusInput;

    std::cout << "Noise percentage (rec 0): ";
    std::cin >> noisePercent;

    std::cout << "\nBlock type:\n";
    std::cout << "0 = None\n";
    std::cout << "1 = Vertical wire\n";
    std::cout << "2 = Circle\n";
    std::cout << "3 = Ellipse\n";
    std::cout << "4 = Square\n";
    std::cout << "Select: ";

    int shapeChoice;
    std::cin >> shapeChoice;
    BlockType block = static_cast<BlockType>(shapeChoice);

    float offsetPercent;
    std::cout << "Block offset (% of sun radius): ";
    std::cin >> offsetPercent;

    int sizeA = 0, sizeB = 0;
    float rotationDeg = 0.0f;

    if (block == BlockType::VERTICAL_WIRE) {
        std::cout << "Wire width (px): ";
        std::cin >> sizeA;
        std::cout << "Rotation (deg): ";
        std::cin >> rotationDeg;
    }
    else if (block == BlockType::CIRCLE) {
        std::cout << "Circle radius (px): ";
        std::cin >> sizeA;
    }
    else if (block == BlockType::ELLIPSE) {
        std::cout << "Ellipse rx ry (px): ";
        std::cin >> sizeA >> sizeB;
        std::cout << "Rotation (deg): ";
        std::cin >> rotationDeg;
    }
    else if (block == BlockType::SQUARE) {
        std::cout << "Square width (px): ";
        std::cin >> sizeA;
        std::cout << "Rotation (deg): ";
        std::cin >> rotationDeg;
    }

    std::vector<unsigned char> image(width * height * 3, 0);
    std::vector<bool> sunMask(width * height, false);

    std::mt19937 gen(std::random_device{}());

    int radius = radiusInput == 0
        ? std::uniform_int_distribution<int>(140, 180)(gen)
        : radiusInput;

    radius = std::min(radius, std::min(width, height) / 2 - 1);

    std::uniform_int_distribution<int> xDist(radius, width - radius - 1);
    std::uniform_int_distribution<int> yDist(radius, height - radius - 1);

    int cx = xDist(gen);
    int cy = yDist(gen);

    int sunW, sunH, sunC;
    unsigned char* sunImg = stbi_load("sun.png", &sunW, &sunH, &sunC, 4);
    if (!sunImg) {
        std::cerr << "Failed to load sun.png\n";
        return 1;
    }

    int sunSize = radius * 2;

    for (int sy = 0; sy < sunSize; ++sy) {
        for (int sx = 0; sx < sunSize; ++sx) {
            int imgX = cx - radius + sx;
            int imgY = cy - radius + sy;

            if (imgX < 0 || imgX >= width || imgY < 0 || imgY >= height)
                continue;

            int srcX = sx * sunW / sunSize;
            int srcY = sy * sunH / sunSize;
            int sidx = (srcY * sunW + srcX) * 4;

            unsigned char r = sunImg[sidx + 0];
            unsigned char g = sunImg[sidx + 1];
            unsigned char b = sunImg[sidx + 2];
            unsigned char a = sunImg[sidx + 3];

            int brightness = (r + g + b) / 3;
            if (a > 30 || brightness > 30) {
                int didx = (imgY * width + imgX) * 3;
                image[didx + 0] = r;
                image[didx + 1] = g;
                image[didx + 2] = b;
                sunMask[imgY * width + imgX] = true;
            }
        }
    }

    stbi_image_free(sunImg);

    applyBlock(
        image, sunMask,
        width, height,
        cx, cy, radius,
        block,
        offsetPercent,
        sizeA, sizeB,
        rotationDeg
    );

    int totalPixels = width * height;
    int targetNoisePixels = int((noisePercent / 100.0f) * totalPixels);
    std::uniform_int_distribution<int> px(0, width - 1);
    std::uniform_int_distribution<int> py(0, height - 1);

    for (int i = 0; i < targetNoisePixels; ++i) {
        int x = px(gen);
        int y = py(gen);
        if (!sunMask[y * width + x]) {
            int idx = (y * width + x) * 3;
            image[idx] = image[idx + 1] = image[idx + 2] = 255;
        }
    }

    stbi_write_png(
        "simulated_sun.png",
        width, height,
        3,
        image.data(),
        width * 3
    );

    std::cout << "\nSaved simulated_sun.png\n";
    std::cout << "Sun center: (" << cx << ", " << cy << ")\n";
    std::cout << "Radius: " << radius << " px\n";

    return 0;
}
