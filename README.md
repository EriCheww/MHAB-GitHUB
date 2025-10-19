## Fine Tracking
 
---

### random_circle_gen
The `random_circle_gen` program randomly generates an image containing a circle over a black background.  
A number of variables can be adjusted to control the output:

- **Image width**  
- **Image height**  
- **Circle minimum radius**  
- **Circle maximum radius**  
- **Circle color**  
- **Background level**

---

### detect_circle
The `detect_circle` program looks for **`random_circle.png`** and it converts the image to grayscale, applies Gaussian blur and contrast normalization, then detects circles using the Hough Circle Transform. This version has backup contour analysis path for tougher or noisier images.

| Run              | Detection Time |         
|------------------|----------------|
| 1                | 51.5078 ms     |
| 2                | 52.7326 ms     |
| 3                | 55.1291 ms     |
| 4                | 51.4253 ms     |
| 5                | 51.8984 ms     |
| 6                | 47.5577 ms     |

When running the .exe the performance is ~2.5ms

---

### detect_circle_v1
The `detect_circle_v1` program looks for **`random_circle.png`** and it converts the image to grayscale, applies Gaussian blur and normalization, then runs the Hough Circle Transform to find the circle. This version does not have any backup analysis path.
Expect the same performance as `detect_circle`.

---

### detect_circle_v2
The `detect_circle_v2` program looks for **`random_circle.png`** and instead of the slower Hough Transform, it downscales the image, applies Otsu thresholding, finds the largest connected component, and estimates its centroid and radius.

| Run              | Detection Time |         
|------------------|----------------|
| 1                | 43.4416 ms     |
| 2                | 46.3882 ms     |
| 3                | 44.7505 ms     |
| 4                | 50.1788 ms     |
| 5                | 44.8603 ms     |
| 6                | 45.0679 ms     |

When running the .exe the performance is ~1ms 
