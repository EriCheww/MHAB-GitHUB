#include "ROI.h"

namespace roi {

cv::Rect getROI(
    const cv::Mat& image,
    const cv::Point2f& center,
    int halfSize
)
{

    cv::Rect roi(
        static_cast<int>(center.x) - halfSize,
        static_cast<int>(center.y) - halfSize,
        2 * halfSize,
        2 * halfSize
    );

    roi &= cv::Rect(0, 0, image.cols, image.rows);

    return roi;
}

}