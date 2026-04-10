#include "mosaic.hpp"
#include <opencv2/stitching.hpp>
#include <iostream>

using namespace std;
using namespace cv;

namespace mosaic {

    bool loadImages(const vector<string>& paths, vector<Mat>& images)
    {
        images.clear();

        for (const auto& path : paths) {
            Mat img = imread(path);
            if (img.empty()) {
                cout << "Cannot read image: " << path << endl;
                return false;
            }
            images.push_back(img);
        }

        return true;
    }

    bool stitchImages(const vector<Mat>& images, Mat& result)
    {
        if (images.size() < 2) {
            cout << "Need at least 2 images to stitch." << endl;
            return false;
        }

        Ptr<Stitcher> stitcher = Stitcher::create(Stitcher::SCANS);

        Stitcher::Status status = stitcher->stitch(images, result);

        if (status != Stitcher::OK) {
            cout << "Error during stitching! Status: " << int(status) << endl;
            return false;
        }

        return true;
    }

    bool createMosaic(const vector<string>& paths, Mat& result)
    {
        vector<Mat> images;

        if (!loadImages(paths, images))
            return false;

        return stitchImages(images, result);
    }

}