#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>

using namespace std;
using namespace cv;
using namespace std::chrono;

int main(int argc, char** argv) {

    if (argc < 2) {
        cout << "Usage: ./sun_center <image_path>" << endl;
        return -1;
    }

    Mat img = imread(argv[1]);
    if (img.empty()) {
        cout << "Failed to load image\n";
        return -1;
    }

    auto start = high_resolution_clock::now();

    // Convert image to HSV
    Mat hsv, mask;
    cvtColor(img, hsv, COLOR_BGR2HSV);

    // Orange colour range (adjust if needed)
    Scalar lower_orange(5, 50, 50); 
    Scalar upper_orange(25, 255, 255);
    inRange(hsv, lower_orange, upper_orange, mask);

    // Find contours
    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // Return areas of all detected contours
    cout << "[Detected Contour Areas]" << endl;
    for (int i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        cout << "Contour " << i << ": Area = " << area << endl;
    }

    if (contours.empty()) {
        cout << "No contours detected\n";
        return -1;
    }

    // Area thresholds
    int minArea = 20000;   // ignore tiny blobs
    int maxArea = 1e7;    // ignore huge blobs if needed

    // Combine only contours within the area range
    vector<Point> conbinedPoints;
    for (vector<Point> &contour : contours) {
        double area = contourArea(contour);
        if (area >= minArea && area <= maxArea) {
            conbinedPoints.insert(conbinedPoints.end(), contour.begin(), contour.end());
        }
    }

    if (conbinedPoints.empty()) {
        cout << "No valid contours after area filtering\n";
        return -1;
    }

    // Use minEnclosingCircle on filtered points
    Point2f center;
    float radius;
    minEnclosingCircle(conbinedPoints, center, radius);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    cout << "[Sun Detection using minEnclosingCircle]" << endl;
    cout << "Estimated Center: (" << center.x << ", " << center.y << ")\n";
    cout << "Radius: " << radius << endl;
    cout << "Time: " << duration.count() << " ms\n";

    // Draw results
    circle(img, center, radius, Scalar(0, 255, 0), 2); 
    circle(img, center, 3, Scalar(0, 0, 255), -1);    

    imshow("Detected Sun", img);
    waitKey(0);

    return 0;
}
