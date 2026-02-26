#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "GesturePath.h"
#include "KeyboardLayout.h"
#include "SwipeTypeTypes.h"

/**
 * @file IdealPathGenerator.h
 * @brief Generates ideal (reference) gesture paths for dictionary words.
 *
 * For each word, the ideal path connects the key centers of each character
 * in sequence, then resamples and normalizes the result to match the format
 * of a normalized gesture path.
 *
 * Ideal paths are cached after first generation for performance.
 *
 * Thread safety: NOT thread-safe.
 */

namespace swipetype {

/**
 * @brief Generates and caches ideal swipe paths for words given a keyboard layout.
 */
class IdealPathGenerator {
public:
    IdealPathGenerator();
    ~IdealPathGenerator();

    // Non-copyable, movable
    IdealPathGenerator(const IdealPathGenerator&) = delete;
    IdealPathGenerator& operator=(const IdealPathGenerator&) = delete;
    IdealPathGenerator(IdealPathGenerator&&) noexcept;
    IdealPathGenerator& operator=(IdealPathGenerator&&) noexcept;

    /**
     * @brief Set the keyboard layout used for path generation.
     *
     * Clears the path cache (since key positions have changed).
     *
     * @param layout  Keyboard layout with character key positions.
     */
    void setLayout(const KeyboardLayout& layout);

    /**
     * @brief Generate or retrieve the ideal path for a word.
     *
     * If the path has been generated before for the current layout,
     * returns the cached version. Otherwise generates, caches, and returns it.
     *
     * @param word  UTF-8 encoded word string. Only ASCII lowercase letters
     *              are used for path generation; other characters are skipped.
     * @return Normalized ideal path. Empty path if word has no mappable characters
     *         or layout not set.
     */
    GesturePath getIdealPath(const std::string& word);

    /**
     * @brief Pre-generate ideal paths for a batch of words.
     *
     * Useful for warming up the cache during initialization.
     *
     * @param words  List of words to pre-generate paths for.
     */
    void pregenerate(const std::vector<std::string>& words);

    /**
     * @brief Clear the path cache.
     *
     * Call when the keyboard layout changes or to free memory.
     */
    void clearCache();

    /**
     * @return Number of cached paths.
     */
    size_t cacheSize() const;

private:
    struct Impl;
    Impl* pImpl;
};

} // namespace swipetype
