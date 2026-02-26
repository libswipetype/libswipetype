#pragma once

#include <vector>
#include "GesturePath.h"
#include "GestureCandidate.h"
#include "SwipeTypeTypes.h"

/**
 * @file Scorer.h
 * @brief DTW-based scoring for comparing gesture paths against ideal paths.
 *
 * Uses Dynamic Time Warping (DTW) with Sakoe-Chiba band constraint
 * (Sakoe & Chiba, 1978) to compute similarity between a user's gesture
 * and the ideal swipe path for each candidate word.
 *
 * Thread safety: NOT thread-safe. Use one instance per thread.
 */

namespace swipetype {

/**
 * @brief Scores gesture paths against ideal reference paths using DTW.
 */
class Scorer {
public:
    Scorer();
    ~Scorer();

    // Non-copyable, movable
    Scorer(const Scorer&) = delete;
    Scorer& operator=(const Scorer&) = delete;
    Scorer(Scorer&&) noexcept;
    Scorer& operator=(Scorer&&) noexcept;

    /**
     * @brief Configure the scorer with custom parameters.
     *
     * @param config  Scoring configuration. See ScoringConfig for defaults.
     */
    void configure(const ScoringConfig& config);

    /**
     * @brief Compute the DTW distance between two normalized paths.
     *
     * Uses Sakoe-Chiba band constraint with bandwidth = DTW_BANDWIDTH.
     * Both paths must have exactly RESAMPLE_COUNT points.
     *
     * @param gesture      Normalized gesture path from user input
     * @param idealPath    Normalized ideal path for a candidate word
     * @return DTW distance (>= 0.0). Lower = better match.
     *         Returns FLT_MAX if either path is invalid.
     *
     * @pre gesture.points.size() == RESAMPLE_COUNT
     * @pre idealPath.points.size() == RESAMPLE_COUNT
     */
    float computeDTWDistance(const GesturePath& gesture,
                             const GesturePath& idealPath) const;

    /**
     * @brief Score a candidate by combining DTW distance with frequency.
     *
     * finalScore = (1 - α) * normalizedDTW + α * (1 - normalizedFreq)
     * confidence = 1.0 - finalScore
     *
     * @param dtwDistance      Raw DTW distance (from computeDTWDistance)
     * @param maxDTWDistance   Maximum DTW distance in the candidate set (for normalization)
     * @param frequency        Dictionary frequency of the word
     * @param maxFrequency     Maximum frequency in the dictionary (for normalization)
     * @return Confidence score in [0.0, 1.0].
     */
    float computeConfidence(float dtwDistance, float maxDTWDistance,
                            uint32_t frequency, uint32_t maxFrequency) const;

private:
    struct Impl;
    Impl* pImpl;
};

} // namespace swipetype
