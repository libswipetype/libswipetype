#pragma once

#include <string>
#include <vector>
#include <cstdint>

/**
 * @file KeyboardLayout.h
 * @brief Keyboard layout descriptor — key positions and dimensions.
 *
 * This is the primary contract between the keyboard app and swipetype-core.
 * The keyboard app (via its adapter) populates this structure with key
 * positions in density-independent pixels (dp).
 */

namespace swipetype {

/**
 * @brief Describes a single key on the keyboard.
 */
struct KeyDescriptor {
    /** Display label (e.g., "a", "shift", "123"). Used for debugging only. */
    std::string label;

    /** Unicode code point for this key's primary character.
     *  E.g., 0x0061 for 'a', 0x0041 for 'A'.
     *  Set to -1 for non-character keys (shift, backspace, space, etc.).
     *  Only keys with codePoint >= 0 participate in gesture recognition. */
    int32_t codePoint = -1;

    /** Key center X coordinate in dp, relative to keyboard top-left. */
    float centerX = 0.0f;

    /** Key center Y coordinate in dp, relative to keyboard top-left. */
    float centerY = 0.0f;

    /** Key width in dp. */
    float width = 0.0f;

    /** Key height in dp. */
    float height = 0.0f;

    KeyDescriptor() = default;
    KeyDescriptor(const std::string& label, int32_t codePoint,
                  float centerX, float centerY, float width, float height)
        : label(label), codePoint(codePoint),
          centerX(centerX), centerY(centerY),
          width(width), height(height) {}

    /** @return true if this key represents a character (participates in gestures). */
    bool isCharacterKey() const { return codePoint >= 0; }
};

/**
 * @brief Complete keyboard layout descriptor.
 *
 * Populated by the adapter from the keyboard app's internal layout representation.
 * The adjacency map is computed internally by GestureEngine during init.
 */
struct KeyboardLayout {
    /** BCP 47 language tag (e.g., "en-US", "de-DE"). */
    std::string languageTag;

    /** All keys on the keyboard, including non-character keys. */
    std::vector<KeyDescriptor> keys;

    /** Total keyboard width in dp. */
    float layoutWidth = 0.0f;

    /** Total keyboard height in dp. */
    float layoutHeight = 0.0f;

    /**
     * @brief Find the index of the key nearest to the given point.
     *
     * Only considers character keys (codePoint >= 0).
     *
     * @param x  X coordinate in dp
     * @param y  Y coordinate in dp
     * @return Index into keys vector, or -1 if no character keys exist.
     */
    int32_t findNearestKey(float x, float y) const;

    /**
     * @brief Find the index of the key with the given code point.
     *
     * @param codePoint  Unicode code point to search for (case-insensitive for ASCII).
     * @return Index into keys vector, or -1 if not found.
     */
    int32_t findKeyByCodePoint(int32_t codePoint) const;

    /** @return true if the layout has at least one character key. */
    bool isValid() const;
};

} // namespace swipetype
