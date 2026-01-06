#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>

using namespace std;
using namespace cv;
using namespace std::chrono;

int main(int argc, char** argv) {

    if (argc < 2) {
        cout << "Usage: ./filename <image_path>" << endl;
        return -1;
    }

    Mat img = imread(argv[1]);
    if (img.empty()) {
        cout << "Failed to load image\n";
        return -1;
    }

    // Ask the user if they want to zoom in or out before processing
    char choice;
    cout << "\nDo you want to zoom in or out? (i = in / o = out):\n";
    cin >> choice;

    // Copy the current image to a new variable
    Mat processedImg = img.clone();

    // Either zoom in or zoom out or neither
    if (choice == 'o') {
        double zoomFactor = 2.0; 
        resize(img, processedImg, Size(), zoomFactor, zoomFactor, INTER_LINEAR);
    } else if (choice == 'i') {
        double zoomFactor = 0.5;
        resize(img, processedImg, Size(), zoomFactor, zoomFactor, INTER_LINEAR);
    } else {
        cout << "No zoom applied. Processing original image.\n\n";
    }

    // Print current resolution
    cout << "-------Processed Image Resolution-------\n";
    cout << processedImg.cols << "x" << processedImg.rows << "\n\n";

    // Start timer
    auto start = high_resolution_clock::now();

    // Convert image to HSV
    Mat hsv, mask;
    cvtColor(processedImg, hsv, COLOR_BGR2HSV);

    // Orange colour range (adjust if needed)
    Scalar lower_orange(5, 50, 50); 
    Scalar upper_orange(25, 255, 255);
    inRange(hsv, lower_orange, upper_orange, mask);

    // Find contours
    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // Return areas of all detected contours
    cout << "-------Detected Contour Areas-------\\n";

    for (int i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        cout << "Contour " << i << ": Area = " << area << endl;
    }

    cout << "\n";

    // Return this if no contours were detected
    if (contours.empty()) {
        cout << "No contours detected\n";
        return -1;
    }

    // Adjust Area for different sized images
    // Base line image, so originally i tested the area with a 980 by 732 image so now it will adjust the area based on the image
    // size
    double oldMinArea = 20000.0;
    double oldWidth = 980.0;
    double oldHeight = 732.0;

    // Adjust as needed, this is just how I came about to getting about 2% as the minimum area. Any other constant could work here
    double minAreaFraction = oldMinArea / (oldWidth * oldHeight);
    double maxAreaFraction = 0.5;

    // Current image resolution areas
    double imgArea = processedImg.cols * processedImg.rows;
    double minArea = minAreaFraction * imgArea;
    double maxArea = maxAreaFraction * imgArea;

    cout << "-------Image Size Area-------\n";
    cout << "minArea = " << minArea << "\nmaxArea = " << maxArea << "\n\n";

    // Loop over each contour and conbine the points of each that are within the allowable area
    vector<Point> combinedPoints;
    for (vector<Point> &contour : contours) {
        double area = contourArea(contour);
        if (area >= minArea && area <= maxArea) {
            combinedPoints.insert(combinedPoints.end(), contour.begin(), contour.end());
        }
    }

    // No contours that fit within the area restriction
    if (combinedPoints.empty()) {
        cout << "No valid contours after area filtering\n";
        return -1;
    }

    // Use minEnclosingCircle on filtered points
    Point2f center;
    float radius;
    minEnclosingCircle(combinedPoints, center, radius);

    // End timer and calculate the duration
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    cout << "-------Sun Detection using minEnclosingCircle-------\n";
    cout << "Estimated Center: (" << center.x << ", " << center.y << ")\n";
    cout << "Radius: " << radius << endl;
    cout << "Time: " << duration.count() << " ms\n\n";

    // Draw results on processed image
    circle(processedImg, center, radius, Scalar(0, 255, 0), 2); 
    circle(processedImg, center, 3, Scalar(0, 0, 255), -1);    

    // Makes it so that the window doesnt lag your computer out, still the same size but just a small window
    namedWindow("Detected Sun", WINDOW_NORMAL);
    setWindowProperty("Detected Sun", WND_PROP_AUTOSIZE, WINDOW_NORMAL); 
    
    imshow("Detected Sun", processedImg);
    waitKey(0);

    return 0;
}
