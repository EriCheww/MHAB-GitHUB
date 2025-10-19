#include <opencv2/opencv.hpp>
#include <random>

cv::Mat createRandomCircleImage(
    int width,
    int height,
    int minRadius = 10,
    int maxRadius = 80,
    int bgLevel = 5,
    const cv::Scalar& circleColor = cv::Scalar(255, 255, 255)
) {
    // Create near-black background
    cv::Mat img(height, width, CV_8UC3, cv::Scalar(bgLevel, bgLevel, bgLevel));

    // Random engine
    std::random_device rd;
    std::mt19937 gen(rd());

    // Random radius and center (keeping circle inside image bounds)
    std::uniform_int_distribution<int> radiusDist(minRadius, maxRadius);
    int r = radiusDist(gen);

    std::uniform_int_distribution<int> xDist(r, width  - 1 - r);
    std::uniform_int_distribution<int> yDist(r, height - 1 - r);
    int cx = xDist(gen);
    int cy = yDist(gen);

    // Draw filled circle
    cv::circle(img, {cx, cy}, r, circleColor, -1, cv::LINE_AA);

    return img;
}

// Example usage
int main() {
    cv::Mat img = createRandomCircleImage(320, 240);
    cv::imwrite("random_circle.png", img);
    cv::imshow("Random Circle", img);
    cv::waitKey(0);
    return 0;
}
