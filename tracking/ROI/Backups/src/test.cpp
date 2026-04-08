#include <opencv2/opencv.hpp>
#include "roi.hpp"
#include <iostream>

using namespace cv;
using namespace std;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <image_path>" << endl;
        return -1;
    }

    string imagePath = argv[1];
    Mat image = imread(imagePath);
    if (image.empty())
    {
        cerr << "Failed to load image: " << imagePath << endl;
        return -1;
    }

    // Ask user for ROI center and size
    int cx, cy, squareSize;
    cout << "Enter ROI center x coordinate (0 - " << image.cols << "): ";
    cin >> cx;
    cout << "Enter ROI center y coordinate (0 - " << image.rows << "): ";
    cin >> cy;
    cout << "Enter ROI square size: ";
    cin >> squareSize;

    // Validate input
    if (cx < 0 || cx >= image.cols || cy < 0 || cy >= image.rows || squareSize <= 0)
    {
        cerr << "Invalid input values" << endl;
        return -1;
    }

    // Generate ROI using your module
    Mat roiView = roi::getSquareROIView(image, cx, cy, squareSize);

    // Display the original image and ROI
    imshow("Original Image", image);
    imshow("ROI", roiView);
    
    waitKey(0);

    return 0;
}
