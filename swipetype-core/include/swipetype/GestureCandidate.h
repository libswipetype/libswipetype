#pragma once

#include <string>
#include <cstdint>

/**
 * @file GestureCandidate.h
 * @brief Word candidate produced by the gesture recognition pipeline.
 */

namespace swipetype {

/**
 * @brief A word candidate with confidence score and metadata.
 *
 * Candidates are returned sorted by confidence descending (best first).
 */
struct GestureCandidate {
    /** UTF-8 encoded word string. */
    std::string word;

    /** Confidence score in [0.0, 1.0]. 1.0 = highest confidence.
     *  Computed as: 1.0 - finalScore, where finalScore combines DTW and frequency. */
    float confidence = 0.0f;

    /** Source flags bitmask:
     *  - SOURCE_MAIN_DICT (0x01): word from main dictionary
     *  - SOURCE_USER_DICT (0x02): word from user dictionary (future)
     *  - SOURCE_COMPLETION (0x04): prefix completion (future) */
    uint32_t sourceFlags = 0;

    /** Raw DTW distance (for debugging/tuning). Lower = better match.
     *  Not normalized — depends on path length and scoring config. */
    float dtwScore = 0.0f;

    /** Dictionary frequency contribution to final score. Higher = more common word.
     *  Normalized to [0.0, 1.0] within the candidate set. */
    float frequencyScore = 0.0f;

    GestureCandidate() = default;
    GestureCandidate(const std::string& word, float confidence, uint32_t sourceFlags)
        : word(word), confidence(confidence), sourceFlags(sourceFlags) {}
};

} // namespace swipetype
