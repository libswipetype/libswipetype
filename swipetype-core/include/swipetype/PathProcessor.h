#pragma once

#include "GesturePath.h"
#include "KeyboardLayout.h"
#include "SwipeTypeTypes.h"

/**
 * @file PathProcessor.h
 * @brief Path normalization — converts raw touch input to normalized gesture paths.
 *
 * The PathProcessor is responsible for:
 * 1. Removing duplicate/near-duplicate consecutive points
 * 2. Resampling to exactly RESAMPLE_COUNT equidistant points
 * 3. Normalizing coordinates to [0.0, 1.0] bounding box
 * 4. Determining start/end keys from the gesture endpoints
 *
 * Thread safety: NOT thread-safe. Use one instance per thread.
 */

namespace swipetype {

/**
 * @brief Converts raw gesture input to a normalized, resampled path.
 *
 * Uses the pImpl pattern to hide implementation details.
 */
class PathProcessor {
public:
    PathProcessor();
    ~PathProcessor();

    // Non-copyable, movable
    PathProcessor(const PathProcessor&) = delete;
    PathProcessor& operator=(const PathProcessor&) = delete;
    PathProcessor(PathProcessor&&) noexcept;
    PathProcessor& operator=(PathProcessor&&) noexcept;

    /**
     * @brief Normalize a raw gesture path.
     *
     * Performs deduplication, resampling, bounding-box normalization,
     * and start/end key detection.
     *
     * @param raw     Raw gesture path (must contain >= 2 points)
     * @param layout  Keyboard layout for start/end key detection
     * @return Normalized path with exactly RESAMPLE_COUNT points.
     *         Returns empty GesturePath if raw.isEmpty().
     *
     * @pre raw.points.size() >= MIN_GESTURE_POINTS
     * @post result.points.size() == RESAMPLE_COUNT || result.points.empty()
     */
    GesturePath normalize(const RawGesturePath& raw,
                          const KeyboardLayout& layout) const;

    /**
     * @brief Configure the minimum point distance for deduplication.
     *
     * @param distanceDp  Minimum distance in dp. Default: MIN_POINT_DISTANCE_DP.
     */
    void setMinPointDistance(float distanceDp);

    /**
     * @brief Configure the resample count.
     *
     * @param count  Number of points in output. Default: RESAMPLE_COUNT.
     *               Must be >= 2.
     */
    void setResampleCount(int count);

private:
    struct Impl;
    Impl* pImpl;
};

} // namespace swipetype
