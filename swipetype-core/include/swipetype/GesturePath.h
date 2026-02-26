#pragma once

#include <vector>
#include <cstdint>
#include "GesturePoint.h"

/**
 * @file GesturePath.h
 * @brief Path data structures for raw and processed gesture input.
 */

namespace swipetype {

/**
 * @brief Raw gesture path — unprocessed touch input from the keyboard.
 *
 * Contains the sequence of touch points as captured by the input system.
 * Points are ordered by timestamp. May contain duplicates, noise, and
 * varying density.
 */
struct RawGesturePath {
    std::vector<GesturePoint> points;  ///< Ordered touch points, >= 0 elements

    /** @return true if the path has fewer than MIN_GESTURE_POINTS points. */
    bool isEmpty() const { return points.size() < 2; }

    /** @return Number of points in the raw path. */
    size_t size() const { return points.size(); }
};

/**
 * @brief Normalized gesture path — the input to the scoring algorithm.
 *
 * After processing by PathProcessor::normalize(), this path has exactly
 * RESAMPLE_COUNT (64) points in a [0.0, 1.0] bounding box with
 * preserved aspect ratio.
 */
struct GesturePath {
    /** Exactly RESAMPLE_COUNT normalized points. */
    std::vector<NormalizedPoint> points;

    /** Original aspect ratio (width/height) before normalization.
     *  Used as a scoring heuristic. */
    float aspectRatio = 1.0f;

    /** Total arc length of the original path in dp (before normalization).
     *  Used for word length estimation. */
    float totalArcLength = 0.0f;

    /** Index into KeyboardLayout::keys for the key nearest to the
     *  first raw touch point. -1 if not determined. */
    int32_t startKeyIndex = -1;

    /** Index into KeyboardLayout::keys for the key nearest to the
     *  last raw touch point. -1 if not determined. */
    int32_t endKeyIndex = -1;

    /** @return true if the path has the expected number of points. */
    bool isValid() const {
        return static_cast<int>(points.size()) == 64; // RESAMPLE_COUNT
    }
};

} // namespace swipetype
