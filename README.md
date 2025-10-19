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
The `detect_circle` program looks for **`random_circle.png`** and it converts the image to grayscale, applies Gaussian blur and contrast normalization, then detects circles using the Hough Circle Transform.

| Metric          | Value          |
|------------------|----------------|
| Detection time   | 51.5078 ms     |
| Center (x, y)    | (138.6, 166.2) |
| Radius           | 50.88 px       |
