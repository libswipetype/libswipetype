#pragma once

#include <cstdint>

/**
 * @file GesturePoint.h
 * @brief Point data structures for raw and normalized gesture paths.
 */

namespace swipetype {

/**
 * @brief A single raw touch point from the keyboard input.
 *
 * Coordinates are in density-independent pixels (dp) relative to the
 * top-left corner of the keyboard view.
 */
struct GesturePoint {
    float x;            ///< X coordinate in dp
    float y;            ///< Y coordinate in dp
    int64_t timestamp;  ///< Milliseconds since gesture start (monotonic, 0-based)

    GesturePoint() : x(0.0f), y(0.0f), timestamp(0) {}
    GesturePoint(float x, float y, int64_t timestamp)
        : x(x), y(y), timestamp(timestamp) {}
};

/**
 * @brief A normalized point after path processing.
 *
 * Coordinates are in [0.0, 1.0] after bounding-box normalization.
 * Time is normalized to [0.0, 1.0] (0 = gesture start, 1 = gesture end).
 */
struct NormalizedPoint {
    float x;  ///< Normalized X in [0.0, 1.0]
    float y;  ///< Normalized Y in [0.0, 1.0]
    float t;  ///< Normalized time in [0.0, 1.0]

    NormalizedPoint() : x(0.0f), y(0.0f), t(0.0f) {}
    NormalizedPoint(float x, float y, float t)
        : x(x), y(y), t(t) {}
};

} // namespace swipetype
