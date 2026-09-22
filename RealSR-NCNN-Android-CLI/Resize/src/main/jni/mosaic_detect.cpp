#include "mosaic_detect.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

// Function to detect mosaic resolution using template matching of grid patterns
// reference https://github.com/rekaXua/demosaic_project/blob/master/demosaic_project_ESRGAN.py
// It analyzes the input pixel data to find the most likely block size of a mosaic pattern.
// pixelData: Pointer to the raw pixel data (e.g., from stbi_load). Assumed 8-bit per channel.
// width: Width of the image.
// height: Height of the image.
// channel: Number of channels (3 for RGB, 4 for RGBA).
// Returns the detected mosaic block size (e.g., 8 for an 8x8 mosaic).
// Returns a default value (HighRange + 1) if detection is inconclusive.
// Returns -1 if input is invalid or critical error occurs.
int detectMosaicResolution(const unsigned char* pixelData, int width, int height, int channel) {
    // Constants (match Python script)
    const int GBlur_kernel_size = 5; // Gaussian blur kernel size (must be odd)
    const int CannyTr1 = 8;        // Canny lower threshold
    const int CannyTr2 = 30;       // Canny upper threshold
    const int LowRange = 2;        // Minimum mosaic block size to check
    const int HighRange = 25;      // Maximum mosaic block size to check
    const double DetectionTr = 0.29; // Threshold for template matching correlation

    // --- Input Validation ---
    if (!pixelData || width <= 0 || height <= 0 || (channel != 3 && channel != 4)) {
        fprintf(stderr, "Error: Invalid input image data (pixelData=nullptr, width=%d, height=%d, channel=%d).\n", width, height, channel);
        return -1;
    }

    // Validate Gaussian blur kernel size
    if (GBlur_kernel_size % 2 == 0 || GBlur_kernel_size <= 0) {
        fprintf(stderr, "Error: GBlur_kernel_size must be a positive odd number. Got %d.\n", GBlur_kernel_size);
        return -1;
    }

    // Check minimum dimensions required by the pattern generation logic
    // The largest pattern size is calculated with masksize = HighRange + 2
    // Python calculation was 2 + masksize + masksize - 1 + 2
    int max_pattern_size_for_highrange = 2 + (HighRange + 2) + (HighRange + 2) - 1 + 2;
    if (width < max_pattern_size_for_highrange || height < max_pattern_size_for_highrange) {
        fprintf(stderr, "Image too small (%dx%d) for mosaic detection logic based on HighRange constant %d. Needs at least %dx%d.\n",
                width, height, HighRange, max_pattern_size_for_highrange, max_pattern_size_for_highrange);
        // It might still work for smaller mask sizes, but the full range check is not possible.
        // Let's return a default or failure? Python doesn't explicitly handle this.
        // If the loop below skips all patterns, the default HighRange+1 will be returned, which is acceptable.
        // Return -1 if it's ridiculously small, but maybe allow if it's just smaller than max_pattern_size.
        // Let's proceed, as the loops below handle cases where pattern size > image size.
    }


    // --- Pattern Creation ---
    // Create detection patterns (grids of lines) for different mosaic block sizes.
    // Python list size: HighRange+2. Indices 0 to HighRange+1.
    // Python stores patterns at masksize-2 for masksize in [LowRange+2, HighRange+2]. These are indices [LowRange, HighRange].
    // C++ vector needs size HighRange+2 to match Python structure, even if some indices are unused.
    std::vector<cv::Mat> patterns(HighRange + 2);

    // Loop through potential mask sizes (matching Python's range: HighRange+2 down to LowRange+2)
    fprintf(stderr, "Patterns for mask sizes from %d down to %d...\n", HighRange + 2, LowRange + 2);
    for (int masksize = HighRange + 2; masksize >= LowRange + 2; --masksize) {
        int pattern_idx = masksize - 2; // Index in patterns vector [LowRange, HighRange]

        // Pattern size calculation from Python
        int maskimg_size = 2 + masksize + masksize - 1 + 2;

        // Check if pattern size is reasonable relative to image size. If pattern > image, matchTemplate will fail or crash.
        if (maskimg_size > width || maskimg_size > height) {
            fprintf(stderr, "Warning: Pattern size %d for masksize %d exceeds image dimensions (%dx%d). Skipping pattern creation and matching.\n", maskimg_size, masksize, width, height);
            // The patterns vector will have an empty Mat at this index. MatchTemplate loop will check for empty.
            continue;
        }

        cv::Mat img = cv::Mat::zeros(maskimg_size, maskimg_size, CV_8UC3); // Create a BGR image
        img.setTo(cv::Scalar(255, 255, 255)); // Set background to white

        // Draw black lines as per Python logic (starting 2 pixels in, spaced by masksize - 1)
        for (int i = 2; i < maskimg_size; i += masksize - 1) {
            // Line endpoints should be within bounds, already checked maskimg_size vs image size
            cv::line(img, cv::Point(i, 0), cv::Point(i, maskimg_size - 1), cv::Scalar(0, 0, 0), 1); // Black vertical line
        }
        for (int j = 2; j < maskimg_size; j += masksize - 1) {
            // Line endpoints should be within bounds
            cv::line(img, cv::Point(0, j), cv::Point(maskimg_size - 1, j), cv::Scalar(0, 0, 0), 1); // Black horizontal line
        }
        patterns[pattern_idx] = img; // Store the created pattern
    }

//    fprintf(stderr, "Pattern creation complete.\n");

    // --- Image Preprocessing for Detection ---
    // Load image from pixel data and convert to a format suitable for processing (BGRA)
    // Create a Mat that shares the pixelData buffer (no memory copy here)
    cv::Mat img_input(height, width, (channel == 4 ? CV_8UC4 : CV_8UC3), (void*)pixelData);
    cv::Mat img_bgra_detection;
    if (channel == 4) {
        cv::cvtColor(img_input, img_bgra_detection, cv::COLOR_RGBA2BGRA);
    } else {
        cv::cvtColor(img_input, img_bgra_detection, cv::COLOR_RGB2BGRA);
    }

    // Create the mask image (Python's 'card') where detected regions will be marked.
    // Initialize as fully transparent BGRA. This mask is used *after* detection in the Python script,
    // but the detection loop in Python draws on it, so we create it here.
    cv::Mat card_mask = cv::Mat::zeros(height, width, CV_8UC4); // Fully transparent BGRA

    // Convert the BGRA image to grayscale for the core detection pipeline
    cv::Mat img_gray;
    cv::cvtColor(img_bgra_detection, img_gray, cv::COLOR_BGRA2GRAY);

    // Apply Canny edge detection to find grid lines
    cv::Canny(img_gray, img_gray, CannyTr1, CannyTr2);
    img_gray = 255 - img_gray; // Invert: edges become dark (0), non-edges bright (255)

    // Apply Gaussian Blur to smooth the edge map slightly
    cv::GaussianBlur(img_gray, img_gray, cv::Size(GBlur_kernel_size, GBlur_kernel_size), 0);

    fprintf(stderr, "Starting template matching...\n");

    // --- Detection: Perform template matching for each pattern ---
    // resolutions vector stores the count of matches for each potential masksize.
    // Python vector size was HighRange+2 initially (indices 0 to HighRange+1), then appended 0 -> size HighRange+3.
    // Python stored counts at indices masksize-1 for masksize in [LowRange+2, HighRange+2]. These are indices [LowRange+1, HighRange+1].
    // Index HighRange+2 is explicitly set to 0 by Python's append. Indices 0 to LowRange are left as initial -1 (or whatever numpy default is).
    // C++ resolutions vector needs size HighRange+3 (indices 0 to HighRange+2) initialized to 0 to match Python structure for extrema calculation.
    std::vector<int> resolutions(HighRange + 3, 0); // Initialize all counts to 0


    // Loop through potential mask sizes (matching Python's detection loop range: HighRange+2 down to LowRange+2)
    for (int masksize = HighRange + 2; masksize >= LowRange + 2; --masksize) {
        int pattern_idx = masksize - 2;      // Index in patterns vector [LowRange, HighRange]
        int resolution_idx = masksize - 1;   // Index in resolutions vector [LowRange+1, HighRange+1]

        // Ensure indices are within bounds (should be if constants and loops are correct, but defensive check)
        if (pattern_idx < 0 || pattern_idx >= patterns.size() ||
            resolution_idx < 0 || resolution_idx >= resolutions.size()) {
            fprintf(stderr, "Error: Index calculation out of bounds during detection. pattern_idx=%d, resolution_idx=%d. Skipping masksize %d.\n",
                    pattern_idx, resolution_idx, masksize);
            continue; // Skip this masksize
        }

        // Check if pattern was successfully created (skipped if too large or error)
        if (patterns[pattern_idx].empty()) {
            continue; // Skip matching if pattern was not created
        }

        cv::Mat templateImg;
        // Pattern is BGR (white=255, black=0). Convert to grayscale for matching (CV_8U).
        cv::cvtColor(patterns[pattern_idx], templateImg, cv::COLOR_BGR2GRAY);

        int w = templateImg.cols;
        int h = templateImg.rows;

        // Ensure template is smaller than or equal to the image dimensions (redundant with pattern creation check, but safe)
        if (w > img_gray.cols || h > img_gray.rows) {
            continue; // Skip match if template is too large
        }

        cv::Mat img_detection_result; // Result of matchTemplate (CV_32F)
        // Match the (inverted, blurred) edge image against the pattern (dark lines on white background).
        // High correlation (near 1) indicates areas in the image where the processed edges match the pattern grid.
        cv::matchTemplate(img_gray, templateImg, img_detection_result, cv::TM_CCOEFF_NORMED);

        // Threshold the result to find locations above the detection threshold (i.e., good matches)
        cv::Mat detection_locations_mask; // This will be CV_8U, 255 for matches, 0 otherwise
        // Thresholding a CV_32F Mat with THRESH_BINARY works, result is CV_8U.
        cv::threshold(img_detection_result, detection_locations_mask, DetectionTr, 255, cv::THRESH_BINARY);

        // Find all points that are above the threshold (non-zero values in the mask)
        std::vector<cv::Point> points;
        cv::findNonZero(detection_locations_mask, points);

        int rects = points.size(); // Count of locations above threshold for this masksize
        resolutions[resolution_idx] = rects; // Store count at the correct index (masksize - 1)

        // Optional: Draw rectangles on the card_mask (for visualizing detected regions)
        // This part does not affect the resolution detection logic itself.
        // Drawing black opaque rectangles (0,0,0,255)
        for (const auto& pt : points) {
            // Check bounds before drawing rectangle (should be okay if template size check passed, but safe)
            if (pt.x >= 0 && pt.y >= 0 && pt.x + w <= card_mask.cols && pt.y + h <= card_mask.rows) {
                cv::rectangle(card_mask, pt, cv::Point(pt.x + w, pt.y + h), cv::Scalar(0, 0, 0, 255), -1);
            } else {
                // This should ideally not happen if template size is checked correctly
                fprintf(stderr, "Warning: Drawing rectangle out of bounds at (%d, %d) with size (%d, %d) for masksize %d.\n", pt.x, pt.y, w, h, masksize);
            }
        }

        // fprintf(stderr, "Masksize %d (res_idx %d) found %d matches.\n", masksize, resolution_idx, rects);
    }

    // At this point, `resolutions` vector (size HighRange+3) has counts at indices [LowRange+1, HighRange+1].
    // Indices [0, LowRange] and [HighRange+2] are 0 as initialized.

    fprintf(stderr, "Calculating resolution...\n");
    // Debugging: Print populated resolutions vector segment
    fprintf(stderr, "Resolutions counts (indices %d to %d): [", LowRange + 1, HighRange + 1);
    for(int i = LowRange + 1; i <= HighRange + 1; ++i) {
        if (i < resolutions.size()) { // Safety check
            fprintf(stderr, "%d%s", resolutions[i], i == HighRange + 1 ? "" : ", ");
        }
    }
    fprintf(stderr, "]\n");


    // --- Calculate Resolution ---
    // The logic is to find local minima in the `resolutions` counts to define groups of potential resolutions.
    // Then, find the group with the highest cumulative match count, breaking ties with the max count within the group,
    // and finally preferring smaller resolutions in case of a full tie.

    // Find local minima in the resolutions vector (indices 0 to HighRange+2)
    // Python argrelextrema(..., np.less) finds indices i where resolutions[i] < resolutions[i-1] AND resolutions[i] < resolutions[i+1]
    std::vector<int> extremaMIN_indices;

    // Manually add LowRange at the start (as per Python). LowRange is 2. Index 2 in resolutions.
    extremaMIN_indices.push_back(LowRange);

    // Find true local minima (value strictly less than left neighbor AND less than or equal to right neighbor)
    // Check indices from 1 up to size-2. resolutions.size() is HighRange + 3.
    // Loop i from 1 to HighRange + 1.
    // Check neighbors i-1 and i+1. Both are within bounds [0, HighRange+2].
    for (size_t i = 1; i < resolutions.size() - 1; ++i) {
        if (resolutions[i] < resolutions[i - 1] && resolutions[i] <= resolutions[i + 1]) {
            extremaMIN_indices.push_back(static_cast<int>(i));
        }
    }

    // Manually add HighRange+2 at the end (as per Python)
    extremaMIN_indices.push_back(HighRange + 2);

    // Sort and unique the extrema indices
    std::sort(extremaMIN_indices.begin(), extremaMIN_indices.end());
    extremaMIN_indices.erase(std::unique(extremaMIN_indices.begin(), extremaMIN_indices.end()), extremaMIN_indices.end());

    // Debugging: Print final extrema indices
    fprintf(stderr, "Final Extrema indices defining groups: [");
    for(size_t i = 0; i < extremaMIN_indices.size(); ++i) {
        fprintf(stderr, "%d%s", extremaMIN_indices[i], i == extremaMIN_indices.size() - 1 ? "" : ", ");
    }
    fprintf(stderr, "]\n");

    // Find the "biggest extrema group" based on sum and max value preference
    int MosaicResolutionOfImage = HighRange + 1; // Default value if no clear best group is found

    // Variables to track the properties of the best group found so far
    int best_group_sum_score = -1; // Stores sum + int(sum*0.05)
    int best_group_max_val_score = -1; // Stores max_val + int(max_val*0.15)
    int best_original_index_of_max = -1; // The index in `resolutions` where the max count of the best group was found

    if (extremaMIN_indices.size() < 2) {
        fprintf(stderr, "Not enough extrema points (%zu) to form groups. Cannot calculate resolution reliably.\n", extremaMIN_indices.size());
        // Keep default resolution HighRange + 1
    } else {
        // Iterate through pairs of extrema indices to define groups [start_idx, end_idx] inclusive
        for (size_t i = 0; i < extremaMIN_indices.size() - 1; ++i) {
            int group_start_res_index = extremaMIN_indices[i];
            int group_end_res_index = extremaMIN_indices[i+1]; // Inclusive range [start, end]

            // Check bounds for group indices (should be within [0, HighRange+2])
            if (group_start_res_index < 0 || group_start_res_index >= (int)resolutions.size() ||
                group_end_res_index < 0 || group_end_res_index >= (int)resolutions.size() ||
                group_start_res_index > group_end_res_index) {
                fprintf(stderr, "ERROR: Invalid extrema group indices [%d, %d] for resolutions size %zu. Skipping group.\n",
                        group_start_res_index, group_end_res_index, resolutions.size());
                continue; // Skip this invalid group
            }

            int current_group_sum = 0;
            int current_max_val_in_group = -1;
            int current_original_index_of_max_in_group = -1; // Index in `resolutions` for max of this group

            // Calculate sum and find max value + its original index within this group range [start, end]
            for (int res_idx = group_start_res_index; res_idx <= group_end_res_index; ++res_idx) {
                // The index res_idx is guaranteed to be within [group_start_res_index, group_end_res_index]
                // which are already checked to be within [0, resolutions.size()-1].
                current_group_sum += resolutions[res_idx];
                if (current_max_val_in_group == -1 || resolutions[res_idx] > current_max_val_in_group) {
                    current_max_val_in_group = resolutions[res_idx];
                    current_original_index_of_max_in_group = res_idx;
                } else if (resolutions[res_idx] == current_max_val_in_group) {
                    // Tie-breaker for max value within the group: Python's slice index().
                    // If multiple indices have the max value, index() returns the first one.
                    // Our loop iterates res_idx from start to end. The first one found will be stored.
                    // This implicitly matches Python's behavior for max_element finding.
                }
            }

            // If the current group has no positive matches, it's unlikely to be the target resolution peak.
            if (current_max_val_in_group <= 0) {
                // fprintf(stderr, "Skipping group [%d, %d] with no matches.\n", group_start_res_index, group_end_res_index);
                continue;
            }

            // Calculate scores for the current group based on Python's logic
            int current_sum_score = current_group_sum + static_cast<int>(current_group_sum * 0.05);
            int current_max_val_score = current_max_val_in_group + static_cast<int>(current_max_val_in_group * 0.15);

            // Compare current group against the best group found so far (using scores and tie-breakers)
            bool update_best = false;
            if (best_original_index_of_max == -1) { // This is the first valid group found
                update_best = true;
            } else {
                // Compare sum scores first
                if (current_sum_score > best_group_sum_score) {
                    update_best = true; // Current group has a strictly better sum score
                } else if (current_sum_score == best_group_sum_score) {
                    // Sum scores are tied. Compare max value scores as tie-breaker.
                    if (current_max_val_score > best_group_max_val_score) {
                        update_best = true; // Current group has a strictly better max value score
                    } else if (current_max_val_score == best_group_max_val_score) {
                        // Both sum scores and max value scores are tied.
                        // Tie-breaker: Prefer the group whose peak (max value) is at a *smaller* index in the `resolutions` vector.
                        // Smaller index corresponds to a smaller masksize (resolution_idx = masksize - 1).
                        if (current_original_index_of_max_in_group < best_original_index_of_max) {
                            update_best = true; // Prefer smaller masksize on full tie
                        }
                    }
                }
            }

            if (update_best) {
                best_group_sum_score = current_sum_score;
                best_group_max_val_score = current_max_val_score;
                best_original_index_of_max = current_original_index_of_max_in_group;
                // Debugging:
                // fprintf(stderr, "New best group found. Peak at res_idx %d (masksize %d). Sum Score %d, Max Score %d.\n",
                //         best_original_index_of_max, best_original_index_of_max + 1, best_group_sum_score, best_group_max_val_score);
            }

        } // End loop through extrema groups

        // After finding the best group, determine the mosaic resolution
        if (best_original_index_of_max != -1) {
            // The best index in `resolutions` corresponds to masksize = index + 1.
            // This index is expected to be in the range [LowRange+1, HighRange+1] if a match was found.
            // Minimum index possible here is LowRange + 1 = 3. So minimum masksize is 4.
            MosaicResolutionOfImage = best_original_index_of_max + 1;
//            fprintf(stderr, "Detected Mosaic Resolution (from best group peak index): %d\n", MosaicResolutionOfImage);

            // Python has a final check: if MosaicResolutionOfImage == 0, set to HighRange+1.
            // Given how we derive it (index + 1), and indices are >= LowRange+1, it won't be 0 if best_original_index_of_max != -1.
            // Let's include the check anyway to be safe and match Python's final logic state.
            if (MosaicResolutionOfImage == 0) {
                fprintf(stderr, "Calculated MosaicResolutionOfImage was unexpectedly 0. Setting to default HighRange + 1 (%d).\n", HighRange + 1);
                MosaicResolutionOfImage = HighRange + 1;
            }

        } else {
            // No clear best group found (e.g., no points above threshold for any masksize,
            // or all groups were skipped). MosaicResolutionOfImage remains at its default value.
            fprintf(stderr, "No clear best group found during resolution calculation. Using default resolution HighRange + 1 (%d).\n", HighRange + 1);
            // MosaicResolutionOfImage is already initialized to HighRange + 1
        }

    } // End if extremaMIN_indices.size() >= 2

    return MosaicResolutionOfImage;

}
