#include "roi.hpp"
#include <algorithm>

using namespace std;
using namespace cv;

namespace roi

{
    Rect makeSquareROI(
        int cx,
        int cy,
        int squareSize,
        const Size& imageSize)
    {
        int half = squareSize / 2;

        int x = cx - half;
        int y = cy - half;

        int maxWidth  = imageSize.width  - x;
        int maxHeight = imageSize.height - y;

        int size = min({squareSize, maxWidth, maxHeight});

        return Rect(x, y, size, size);
    }

    Mat getSquareROIView(
        const Mat& image,
        int cx,
        int cy,
        int squareSize)
    {
        Rect roiRect = makeSquareROI(cx, cy, squareSize, image.size());
        return image(roiRect); 
    }
}
