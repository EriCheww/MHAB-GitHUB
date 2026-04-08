#pragma once
#include <opencv2/opencv.hpp>

using namespace cv;

namespace roi

{

    Rect makeSquareROI(
        int cx,
        int cy,
        int squareSize,
        const Size& imageSize);

    Mat getSquareROIView(
        const Mat& image,
        int cx,
        int cy,
        int squareSize);
}
