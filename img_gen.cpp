#include <iostream>
#include <vector>
#include <algorithm>
#include <random>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

int main() {
    int width, height;
    float sunSizePercentInput, noisePercent;

    std::cout << "Enter width: ";
    std::cin >> width;
    std::cout << "Enter height: ";
    std::cin >> height;

    std::cout << "Enter sun size (% of image height, 0 = random): ";
    std::cin >> sunSizePercentInput;

    std::cout << "Enter noise percentage (% of image area): ";
    std::cin >> noisePercent;

    if (width <= 0 || height <= 0 ||
        sunSizePercentInput < 0 || sunSizePercentInput > 100 ||
        noisePercent < 0 || noisePercent > 100) {
        std::cerr << "Invalid input values\n";
        return 1;
    }

    // black image (RGB)
    std::vector<unsigned char> image(width * height * 3, 0);

    std::random_device rd;
    std::mt19937 gen(rd());

    // sun size 
    float sunSizePercent = sunSizePercentInput;
    if (sunSizePercentInput == 0.0f) {
        std::uniform_real_distribution<float> sizeDist(10.0f, 90.0f);
        sunSizePercent = sizeDist(gen);
    }

    float diameter = (sunSizePercent / 100.0f) * height;
    int radius = static_cast<int>(diameter / 2.0f);

    radius = std::min(radius, std::min(width, height) / 2 - 1);
    radius = std::max(radius, 2);

    // sun position
    std::uniform_int_distribution<int> xDist(radius, width - radius - 1);
    std::uniform_int_distribution<int> yDist(radius, height - radius - 1);

    int cx = xDist(gen);
    int cy = yDist(gen);
    int r2 = radius * radius;

    // draw sun 
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= r2) {
                int idx = (y * width + x) * 3;
                image[idx + 0] = 255;
                image[idx + 1] = 255;
                image[idx + 2] = 255;
            }
        }
    }

    // blob noise
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

                    // avoid overwriting the Sun
                    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r2)
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

    // save img
    if (!stbi_write_png("circle.png", width, height, 3,
                        image.data(), width * 3)) {
        std::cerr << "Failed to write PNG\n";
        return 1;
    }

    std::cout << "Saved circle.png\n";
    std::cout << "Sun center: (" << cx << ", " << cy << ")\n";
    std::cout << "Radius: " << radius << "\n";
    std::cout << "Sun size used: " << sunSizePercent << "%\n";
    std::cout << "Noise: " << noisePercent << "%\n";

    return 0;
}
