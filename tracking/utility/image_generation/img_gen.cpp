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

// apply motionblur
void applyMotionBlur(
    std::vector<unsigned char>& image,
    int width,
    int height,
    float angleDeg,
    int strength
) {
    if (strength <= 1)
        return;

    std::vector<unsigned char> original = image;

    float angle = angleDeg * float(M_PI) / 180.0f;
    float dx = std::cos(angle);
    float dy = std::sin(angle);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {

            float r = 0, g = 0, b = 0;
            int samples = 0;

            for (int i = -strength; i <= strength; ++i) {
                // Clamp coordinates to image edges to avoid cutting off blur
                int sx = std::clamp(int(x + dx * i), 0, width - 1);
                int sy = std::clamp(int(y + dy * i), 0, height - 1);

                int idx = (sy * width + sx) * 3;
                r += original[idx + 0];
                g += original[idx + 1];
                b += original[idx + 2];
                samples++;
            }

            int didx = (y * width + x) * 3;
            image[didx + 0] = static_cast<unsigned char>(r / samples);
            image[didx + 1] = static_cast<unsigned char>(g / samples);
            image[didx + 2] = static_cast<unsigned char>(b / samples);
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

    int blurStrength;
    float blurAngle;

    std::cout << "Motion blur strength (0 = none, rec 8-20): ";
    std::cin >> blurStrength;
    std::cout << "Motion blur angle (deg), determines the direction of the smear: ";
    std::cin >> blurAngle;

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

            int sunMin = std::min(sunW, sunH);
            int xOffset = (sunW - sunMin) / 2;
            int yOffset = (sunH - sunMin) / 2;

            int srcX = xOffset + (sx * sunMin) / sunSize;
            int srcY = yOffset + (sy * sunMin) / sunSize;

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

    applyMotionBlur(
        image,
        width,
        height,
        blurAngle,
        blurStrength
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
