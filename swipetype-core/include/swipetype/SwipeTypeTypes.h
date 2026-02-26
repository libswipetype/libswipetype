#pragma once

#include <cstdint>
#include <string>
#include <functional>

/**
 * @file SwipeTypeTypes.h
 * @brief Shared type definitions, constants, and enums for the swipetype-core library.
 *
 * This file contains all fundamental types used across the library.
 * It has no dependencies beyond the C++ standard library.
 */

namespace swipetype {

// ============================================================================
// Path Processing Constants
// ============================================================================

/** Number of points after resampling. All normalized paths have exactly this many points. */
static constexpr int RESAMPLE_COUNT = 64;

/** Minimum Euclidean distance (in dp) between consecutive points to keep. */
static constexpr float MIN_POINT_DISTANCE_DP = 2.0f;

/** Minimum number of points for a valid gesture. */
static constexpr int MIN_GESTURE_POINTS = 2;

/** Maximum number of raw input points accepted. */
static constexpr int MAX_GESTURE_POINTS = 10000;

// ============================================================================
// Scoring Constants
// ============================================================================

/** Sakoe-Chiba band width as a fraction of RESAMPLE_COUNT. */
static constexpr float DTW_BANDWIDTH_RATIO = 0.10f;

/** Absolute Sakoe-Chiba band width: ceil(RESAMPLE_COUNT * DTW_BANDWIDTH_RATIO). */
static constexpr int DTW_BANDWIDTH = 6;

/** Weight of dictionary frequency in final score (α). Range [0.0, 1.0].
 *  finalScore = (1 - α) * dtwScore + α * freqScore */
static constexpr float FREQUENCY_WEIGHT = 0.30f;

/** Default maximum candidates returned by recognize(). */
static constexpr int DEFAULT_MAX_CANDIDATES = 8;

/** Hard upper limit for maxCandidates parameter. */
static constexpr int MAX_MAX_CANDIDATES = 20;

/** Word length estimate tolerance (±). Used for candidate filtering.
 *  With key-transition estimation this can be tighter than the old arc-length heuristic. */
static constexpr float LENGTH_FILTER_TOLERANCE = 3.0f;

/** Floor for maxDTW normalization. Prevents single-candidate results from
 *  always receiving normalizedDTW=1.0 and thus near-zero confidence.
 *  A good gesture match typically yields DTW ~0.2–0.5; poor ~2–4. */
static constexpr float MAX_DTW_FLOOR = 3.0f;

// ============================================================================
// Dictionary Constants
// ============================================================================

/** Magic bytes for .glide dictionary files: ASCII "GLID". */
static constexpr uint32_t DICT_MAGIC = 0x474C4944;

/** Current dictionary format version. */
static constexpr uint16_t DICT_VERSION = 1;

/** Fixed size of the dictionary file header in bytes. */
static constexpr uint32_t DICT_HEADER_SIZE = 32;

/** Maximum allowed word length in UTF-8 bytes. */
static constexpr uint32_t MAX_WORD_LENGTH = 64;

// ============================================================================
// Candidate Source Flags (bitmask)
// ============================================================================

static constexpr uint32_t SOURCE_MAIN_DICT = 0x01;
static constexpr uint32_t SOURCE_USER_DICT = 0x02;
static constexpr uint32_t SOURCE_COMPLETION = 0x04;

// ============================================================================
// Dictionary Entry Flags (bitmask)
// ============================================================================

static constexpr uint8_t DICT_FLAG_PROPER_NOUN = 0x01;
static constexpr uint8_t DICT_FLAG_PROFANITY = 0x02;

// ============================================================================
// Error Types
// ============================================================================

/**
 * @brief Error codes used throughout the library.
 */
enum class ErrorCode : int {
    NONE = 0,
    DICT_NOT_FOUND = 1,
    DICT_CORRUPT = 2,
    DICT_VERSION_MISMATCH = 3,
    LAYOUT_INVALID = 4,
    PATH_TOO_SHORT = 5,
    ENGINE_NOT_INITIALIZED = 6,
    OUT_OF_MEMORY = 7
};

/**
 * @brief Error information structure for callback-based error reporting.
 */
struct ErrorInfo {
    ErrorCode code = ErrorCode::NONE;
    std::string message;
};

/**
 * @brief Error callback function type.
 *
 * Set via GestureEngine::setErrorCallback() to receive error notifications.
 * Called synchronously from the thread that encounters the error.
 */
using ErrorCallback = std::function<void(const ErrorInfo& error)>;

// ============================================================================
// Scoring Configuration
// ============================================================================

/**
 * @brief Tunable parameters for the scoring algorithm.
 *
 * All fields have sensible defaults. Override via GestureEngine::configure().
 */
struct ScoringConfig {
    int resampleCount = RESAMPLE_COUNT;
    float minPointDistance = MIN_POINT_DISTANCE_DP;
    float dtwBandwidthRatio = DTW_BANDWIDTH_RATIO;
    float frequencyWeight = FREQUENCY_WEIGHT;
    int maxCandidatesEvaluated = MAX_MAX_CANDIDATES;
    float lengthFilterTolerance = LENGTH_FILTER_TOLERANCE;
    float maxDTWFloor = MAX_DTW_FLOOR;
};

} // namespace swipetype
