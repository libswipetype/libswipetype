#pragma once

#include <string>
#include <vector>
#include "GesturePath.h"
#include "GestureCandidate.h"
#include "KeyboardLayout.h"
#include "SwipeTypeTypes.h"

/**
 * @file GestureEngine.h
 * @brief Main entry point for gesture recognition.
 *
 * GestureEngine orchestrates the entire recognition pipeline:
 * 1. Path normalization (PathProcessor)
 * 2. Candidate generation (DictionaryLoader + start/end key filtering)
 * 3. Ideal path generation (IdealPathGenerator)
 * 4. DTW scoring (Scorer)
 * 5. Ranking and pruning
 *
 * Usage:
 * @code
 *   GestureEngine engine;
 *   engine.init(layout, "/path/to/dictionary.glide");
 *
 *   RawGesturePath raw;
 *   raw.points = { {100, 200, 0}, {110, 210, 10}, ... };
 *   auto candidates = engine.recognize(raw, 5);
 *   for (const auto& c : candidates) {
 *       printf("%s (%.2f)\n", c.word.c_str(), c.confidence);
 *   }
 *
 *   engine.shutdown();
 * @endcode
 *
 * Thread safety: NOT thread-safe. External synchronization required.
 * Callers must not call recognize() concurrently on the same instance.
 *
 * Ownership: Caller retains ownership of all passed objects.
 * The engine copies layout and dictionary data internally.
 */

namespace swipetype {

class GestureEngine {
public:
    GestureEngine();
    ~GestureEngine();

    // Non-copyable, movable
    GestureEngine(const GestureEngine&) = delete;
    GestureEngine& operator=(const GestureEngine&) = delete;
    GestureEngine(GestureEngine&&) noexcept;
    GestureEngine& operator=(GestureEngine&&) noexcept;

    /**
     * @brief Initialize the engine with a keyboard layout and dictionary.
     *
     * Must be called before any recognize() invocation. Can be called
     * again to change layout or dictionary (re-initialization).
     *
     * @param layout    Keyboard layout descriptor. Must have at least one
     *                  character key. Coordinates in dp units.
     * @param dictPath  Absolute path to the binary .glide dictionary file.
     * @return true on success; false if dictionary file not found, corrupt,
     *         or layout is invalid.
     *
     * @post isInitialized() == true on success
     */
    bool init(const KeyboardLayout& layout, const std::string& dictPath);

    /**
     * @brief Initialize with a pre-loaded dictionary from memory.
     *
     * @param layout  Keyboard layout descriptor.
     * @param dictData  Pointer to dictionary file contents in memory.
     * @param dictSize  Size of dictionary data in bytes.
     * @return true on success.
     */
    bool initWithData(const KeyboardLayout& layout,
                      const uint8_t* dictData, size_t dictSize);

    /**
     * @brief Recognize a gesture path and return ranked word candidates.
     *
     * Pipeline: normalize → filter candidates → score → rank → return.
     *
     * @param rawPath        Raw gesture path. Must contain >= 2 points.
     * @param maxCandidates  Maximum results to return. Clamped to [1, 20].
     *                       Default: 8.
     * @return Ranked candidates, best first (highest confidence).
     *         Empty vector if engine not initialized, path too short,
     *         or no candidates found.
     *
     * @pre isInitialized() == true
     * @pre rawPath.points.size() >= MIN_GESTURE_POINTS
     */
    std::vector<GestureCandidate> recognize(const RawGesturePath& rawPath,
                                             int maxCandidates = DEFAULT_MAX_CANDIDATES);

    /**
     * @brief Shut down the engine and free all resources.
     *
     * Safe to call multiple times. After shutdown, isInitialized() returns false
     * and recognize() returns empty results.
     *
     * @post isInitialized() == false
     */
    void shutdown();

    /**
     * @brief Check whether the engine is initialized and ready.
     * @return true if init() succeeded and shutdown() has not been called.
     */
    bool isInitialized() const;

    /**
     * @brief Update the keyboard layout without reloading the dictionary.
     *
     * Clears cached ideal paths (since key positions changed).
     * The engine must already be initialized.
     *
     * @param layout  New keyboard layout.
     * @return true on success; false if layout is invalid.
     */
    bool updateLayout(const KeyboardLayout& layout);

    /**
     * @brief Configure scoring parameters.
     *
     * Can be called before or after init(). Parameters take effect
     * on the next recognize() call.
     *
     * @param config  Scoring configuration.
     */
    void configure(const ScoringConfig& config);

    /**
     * @brief Set an error callback for asynchronous error reporting.
     *
     * The callback is invoked synchronously from the thread that
     * encounters the error.
     *
     * @param callback  Error callback function. Pass nullptr to clear.
     */
    void setErrorCallback(ErrorCallback callback);

    /**
     * @brief Get the last error that occurred.
     * @return ErrorInfo with code and message.
     */
    ErrorInfo getLastError() const;

private:
    struct Impl;
    Impl* pImpl;
};

} // namespace swipetype
