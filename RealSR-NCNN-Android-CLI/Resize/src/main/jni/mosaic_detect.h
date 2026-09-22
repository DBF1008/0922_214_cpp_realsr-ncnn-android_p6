#ifndef MOSAIC_DETECT_H
#define MOSAIC_DETECT_H

#include <vector>
#include <opencv2/opencv.hpp>

// Function to detect mosaic resolution using template matching of grid patterns
int detectMosaicResolution(const unsigned char* pixelData, int width, int height, int channel);

#endif // MOSAIC_DETECT_H
