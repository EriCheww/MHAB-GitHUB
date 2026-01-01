#include <iostream>
#include <vector>
#include <algorithm>
#include <random>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

// simulate wire, width px, offset from center of image in % px
void applyWireOcclusion(
    std::vector<unsigned char>& image,
    std::vector<bool>& sunMask,
    int width,
    int height,
    int cx,
    int cy,
    int radius,
    int wireWidthPixels,
    float offsetPercent
) {
    if (wireWidthPixels <= 0)
        return;

    offsetPercent = std::max(-100.0f, std::min(100.0f, offsetPercent));
    int offsetPx = static_cast<int>((offsetPercent / 100.0f) * radius);

    int wireX = cx + offsetPx;
    int halfWidth = wireWidthPixels / 2;

    int xMin = std::max(0, wireX - halfWidth);
    int xMax = std::min(width - 1, wireX + halfWidth);

    for (int y = 0; y < height; ++y) {
        for (int x = xMin; x <= xMax; ++x) {

            if (!sunMask[y * width + x])
                continue;

            int idx = (y * width + x) * 3;
            image[idx + 0] = 0;
            image[idx + 1] = 0;
            image[idx + 2] = 0;

            sunMask[y * width + x] = false;
        }
    }
}

int main() {
    int width, height;
    int radiusInput;
    float noisePercent;

    int wireWidthPixels;
    float wireOffsetPercent;

    std::cout << "Enter width (e.g. 3840): ";
    std::cin >> width;
    std::cout << "Enter height (e.g. 2160): ";
    std::cin >> height;

    std::cout << "Enter Sun radius in pixels (0 = random, recommended 140–180): ";
    std::cin >> radiusInput;

    std::cout << "Enter noise percentage (% of image area, 0 recommended): ";
    std::cin >> noisePercent;

    std::cout << "Enter wire width in pixels (e.g. 10): ";
    std::cin >> wireWidthPixels;

    std::cout << "Enter wire offset from Sun center (% of radius, -100 to 100): ";
    std::cin >> wireOffsetPercent;

    if (width <= 0 || height <= 0 ||
        noisePercent < 0 || noisePercent > 100 ||
        wireWidthPixels < 0 ||
        wireOffsetPercent < -100 || wireOffsetPercent > 100) {
        std::cerr << "Invalid input values\n";
        return 1;
    }

    std::vector<unsigned char> image(width * height * 3, 0);
    std::vector<bool> sunMask(width * height, false);

    std::random_device rd;
    std::mt19937 gen(rd());

    int radius;
    if (radiusInput == 0) {
        std::uniform_int_distribution<int> rDist(140, 180);
        radius = rDist(gen);
    } else {
        radius = radiusInput;
    }

    radius = std::min(radius, std::min(width, height) / 2 - 1);
    radius = std::max(radius, 5);

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

    // maping local sun px to desired sun px for gen img
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

    applyWireOcclusion(image, sunMask, width, height, cx, cy, radius, wireWidthPixels, wireOffsetPercent);

    // add noise
    int totalPixels = width * height;
    int targetNoisePixels =
        static_cast<int>((noisePercent / 100.0f) * totalPixels);

    std::uniform_int_distribution<int> blobRadiusDist(3, 15);
    std::uniform_int_distribution<int> xBlobDist(0, width - 1);
    std::uniform_int_distribution<int> yBlobDist(0, height - 1);

    int paintedPixels = 0;

    while (paintedPixels < targetNoisePixels) {
        int bx = xBlobDist(gen);
        int by = yBlobDist(gen);
        int br = blobRadiusDist(gen);
        int br2 = br * br;

        for (int y = std::max(0, by - br); y < std::min(height, by + br); ++y) {
            for (int x = std::max(0, bx - br); x < std::min(width, bx + br); ++x) {
                int dx = x - bx;
                int dy = y - by;

                if (dx * dx + dy * dy <= br2) {
                    if (sunMask[y * width + x])
                        continue;

                    int idx = (y * width + x) * 3;
                    if (image[idx] == 0) {
                        image[idx + 0] = 255;
                        image[idx + 1] = 255;
                        image[idx + 2] = 255;
                        paintedPixels++;
                    }
                }
            }
        }
    }

    if (!stbi_write_png("simulated_sun.png", width, height, 3,
                        image.data(), width * 3)) {
        std::cerr << "Failed to write PNG\n";
        return 1;
    }

    std::cout << "\nImage Saved: simulated_sun.png\n";
    std::cout << "Sun center: (" << cx << ", " << cy << ")\n";
    std::cout << "Radius: " << radius << " px\n";
    std::cout << "Noise: " << noisePercent << "%\n";
    std::cout << "Wire width: " << wireWidthPixels << " px\n";
    std::cout << "Wire offset: " << wireOffsetPercent << "% of radius\n";

    return 0;
}
