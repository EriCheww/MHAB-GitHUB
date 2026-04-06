
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

namespace fs = std::filesystem;

struct Image {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char> data;
};

struct Settings {
    std::string basePath = "sun.png";
    std::string outputRoot = "gen_images";
    int imagesPerBatch = 7;
    int cropSize = 1200;
    int verticalOffset = 200;
    int horizontalShift = 0;
    unsigned int seed = std::random_device{}();
};

static void usage(const char* exe) {
    std::cout
        << "Usage: " << exe << " [--base <png>] [--output <dir>] [--count <n>] [--crop <px>] [--offset <px>] [--shift <px>] [--seed <n>]\n";
}

static bool parseArgs(int argc, char** argv, Settings& s) {
    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto need = [&](const std::string& name) -> std::string {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for " + name);
                return argv[++i];
            };

            if (arg == "--help" || arg == "-h") {
                usage(argv[0]);
                return false;
            } else if (arg == "--base") {
                s.basePath = need(arg);
            } else if (arg == "--output") {
                s.outputRoot = need(arg);
            } else if (arg == "--count") {
                s.imagesPerBatch = std::stoi(need(arg));
            } else if (arg == "--crop") {
                s.cropSize = std::stoi(need(arg));
            } else if (arg == "--offset") {
                s.verticalOffset = std::stoi(need(arg));
            } else if (arg == "--shift") {
                s.horizontalShift = std::stoi(need(arg));
            } else if (arg == "--seed") {
                s.seed = static_cast<unsigned int>(std::stoul(need(arg)));
            } else {
                throw std::runtime_error("Unknown option: " + arg);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        usage(argv[0]);
        return false;
    }
    return true;
}

static Image loadImage(const std::string& path) {
    int w = 0, h = 0, c = 0;
    unsigned char* raw = stbi_load(path.c_str(), &w, &h, &c, 3);
    if (!raw) {
        throw std::runtime_error("Failed to load image: " + path + " | reason: " + stbi_failure_reason());
    }

    Image img;
    img.width = w;
    img.height = h;
    img.channels = 3;
    img.data.assign(raw, raw + (w * h * 3));
    stbi_image_free(raw);
    return img;
}

static Image cropSquare(const Image& src, int cx, int cy, int size) {
    Image out;
    out.width = size;
    out.height = size;
    out.channels = 3;
    out.data.resize(size * size * 3, 0);

    int half = size / 2;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int sx = cx - half + x;
            int sy = cy - half + y;
            int di = (y * size + x) * 3;

            if (sx >= 0 && sy >= 0 && sx < src.width && sy < src.height) {
                int si = (sy * src.width + sx) * 3;
                out.data[di + 0] = src.data[si + 0];
                out.data[di + 1] = src.data[si + 1];
                out.data[di + 2] = src.data[si + 2];
            }
        }
    }
    return out;
}

static float randRange(std::mt19937& rng, float a, float b) {
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng);
}

static float bilinearSample(const Image& img, float x, float y, int channel) {
    x = std::clamp(x, 0.0f, static_cast<float>(img.width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(img.height - 1));

    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = std::min(x0 + 1, img.width - 1);
    int y1 = std::min(y0 + 1, img.height - 1);

    float fx = x - x0;
    float fy = y - y0;

    auto at = [&](int px, int py) -> float {
        return static_cast<float>(img.data[(py * img.width + px) * 3 + channel]);
    };

    float v00 = at(x0, y0);
    float v10 = at(x1, y0);
    float v01 = at(x0, y1);
    float v11 = at(x1, y1);

    float v0 = v00 * (1.0f - fx) + v10 * fx;
    float v1 = v01 * (1.0f - fx) + v11 * fx;
    return v0 * (1.0f - fy) + v1 * fy;
}

static void applyTurbulenceWarp(Image& img, unsigned int seed, float amp1, float amp2, float freq1, float freq2) {
    Image src = img;
    std::mt19937 rng(seed);
    float p1 = randRange(rng, 0.0f, 6.2831853f);
    float p2 = randRange(rng, 0.0f, 6.2831853f);
    float p3 = randRange(rng, 0.0f, 6.2831853f);
    float p4 = randRange(rng, 0.0f, 6.2831853f);

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            float xf = static_cast<float>(x);
            float yf = static_cast<float>(y);

            float dx = amp1 * std::sin(yf * freq1 + p1) + amp2 * std::sin(xf * freq2 + yf * freq2 * 0.7f + p2);
            float dy = amp1 * std::sin(xf * freq1 + p3) + amp2 * std::sin(yf * freq2 + xf * freq2 * 0.7f + p4);

            float sx = xf + dx;
            float sy = yf + dy;

            int di = (y * img.width + x) * 3;
            for (int c = 0; c < 3; ++c) {
                float v = bilinearSample(src, sx, sy, c);
                img.data[di + c] = static_cast<unsigned char>(std::clamp<int>(static_cast<int>(std::lround(v)), 0, 255));
            }
        }
    }
}

static void blur3x3(Image& img) {
    Image src = img;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            int sum[3] = {0, 0, 0};
            int count = 0;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    int sx = std::clamp(x + ox, 0, img.width - 1);
                    int sy = std::clamp(y + oy, 0, img.height - 1);
                    int si = (sy * img.width + sx) * 3;
                    sum[0] += src.data[si + 0];
                    sum[1] += src.data[si + 1];
                    sum[2] += src.data[si + 2];
                    ++count;
                }
            }
            int di = (y * img.width + x) * 3;
            img.data[di + 0] = static_cast<unsigned char>(sum[0] / count);
            img.data[di + 1] = static_cast<unsigned char>(sum[1] / count);
            img.data[di + 2] = static_cast<unsigned char>(sum[2] / count);
        }
    }
}

static void applyThinCloudVeil(Image& img, unsigned int seed) {
    std::mt19937 rng(seed);
    float angle = randRange(rng, -0.8f, 0.8f);
    float period = randRange(rng, 80.0f, 180.0f);
    float opacity = randRange(rng, 0.06f, 0.14f);
    float phase = randRange(rng, 0.0f, 6.2831853f);

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            float xr =  std::cos(angle) * x + std::sin(angle) * y;
            float band = 0.5f + 0.5f * std::sin(xr / period * 6.2831853f + phase);
            float local = 1.0f - opacity * band;

            int i = (y * img.width + x) * 3;
            img.data[i + 0] = static_cast<unsigned char>(std::clamp<int>(static_cast<int>(img.data[i + 0] * local), 0, 255));
            img.data[i + 1] = static_cast<unsigned char>(std::clamp<int>(static_cast<int>(img.data[i + 1] * local), 0, 255));
            img.data[i + 2] = static_cast<unsigned char>(std::clamp<int>(static_cast<int>(img.data[i + 2] * local), 0, 255));
        }
    }
}

static void save(const Image& img, const fs::path& p) {
    if (!stbi_write_png(p.string().c_str(), img.width, img.height, 3, img.data.data(), img.width * 3)) {
        throw std::runtime_error("Failed to save image: " + p.string());
    }
}

static int nextBatchNumber(const fs::path& root, const std::string& prefix) {
    int highest = 0;
    if (!fs::exists(root)) return 1;

    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0) continue;
        std::string suffix = name.substr(prefix.size());
        if (suffix.empty()) continue;
        bool allDigits = std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) { return std::isdigit(ch); });
        if (!allDigits) continue;
        highest = std::max(highest, std::stoi(suffix));
    }
    return highest + 1;
}

int main(int argc, char** argv) {
    try {
        Settings s;
        if (!parseArgs(argc, argv, s)) return 0;

        Image base = loadImage(s.basePath);

        if (s.cropSize <= 0 || s.cropSize > std::min(base.width, base.height)) {
            throw std::runtime_error("Crop size must be between 1 and " + std::to_string(std::min(base.width, base.height)));
        }
        if (s.imagesPerBatch <= 0) {
            throw std::runtime_error("Image count must be positive.");
        }

        int cx = std::clamp(base.width / 2 + s.horizontalShift, s.cropSize / 2, base.width - s.cropSize / 2);
        int topCy = std::clamp(base.height / 2 - s.verticalOffset, s.cropSize / 2, base.height - s.cropSize / 2);
        int bottomCy = std::clamp(base.height / 2 + s.verticalOffset, s.cropSize / 2, base.height - s.cropSize / 2);

        Image top = cropSquare(base, cx, topCy, s.cropSize);
        Image bottom = cropSquare(base, cx, bottomCy, s.cropSize);

        fs::create_directories(s.outputRoot);
        int batchNumber = std::max(nextBatchNumber(s.outputRoot, "top_"),
                                   nextBatchNumber(s.outputRoot, "bottom_"));

        fs::path topDir = fs::path(s.outputRoot) / ("top_" + std::to_string(batchNumber));
        fs::path bottomDir = fs::path(s.outputRoot) / ("bottom_" + std::to_string(batchNumber));
        fs::create_directories(topDir);
        fs::create_directories(bottomDir);

        std::mt19937 batchRng(s.seed);
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);

        for (int i = 0; i < s.imagesPerBatch; ++i) {
            Image t = top;
            Image b = bottom;

            unsigned int pairSeed = batchRng();
            applyTurbulenceWarp(t, pairSeed, 1.6f, 0.9f, 0.012f, 0.028f);
            applyTurbulenceWarp(b, pairSeed, 1.6f, 0.9f, 0.012f, 0.028f);

            if (chance(batchRng) < 0.35f) {
                applyThinCloudVeil(t, pairSeed + 101u);
                applyThinCloudVeil(b, pairSeed + 101u);
            }

            if (chance(batchRng) < 0.25f) {
                blur3x3(t);
                blur3x3(b);
            }

            fs::path topPath = topDir / ("sun_top_" + std::to_string(i + 1) + ".png");
            fs::path bottomPath = bottomDir / ("sun_bottom_" + std::to_string(i + 1) + ".png");
            save(t, topPath);
            save(b, bottomPath);
            std::cout << "Saved " << topPath.string() << "\n";
            std::cout << "Saved " << bottomPath.string() << "\n";
        }

        std::cout << "\nDone.\n";
        std::cout << "Top batch folder: " << topDir.string() << "\n";
        std::cout << "Bottom batch folder: " << bottomDir.string() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
