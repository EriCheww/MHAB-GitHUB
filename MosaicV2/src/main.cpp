#include "mosaic.hpp"
#include <iostream>

using namespace std;

// g++ src/main.cpp src/mosaic.cpp -Iinclude -o mosaic `pkg-config --cflags --libs opencv4`
// ./mosaic data/top_stack.png data/bottom_stack.png

int main(int argc, char** argv)
{
    if (argc < 3) {
        cout << "Usage: mosaic img1 img2 [img3 ...]" << endl;
        return -1;
    }

    vector<string> paths;
    for (int i = 1; i < argc; i++) {
        paths.push_back(argv[i]);
    }

    cv::Mat result;

    if (!mosaic::createMosaic(paths, result)) {
        cout << "Failed to create mosaic." << endl;
        return -1;
    }

    cv::imwrite("output/sun_mosaic.jpg", result);
    cout << "Saved to output/sun_mosaic.jpg" << endl;

    return 0;
}