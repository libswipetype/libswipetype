#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "SwipeTypeTypes.h"

/**
 * @file DictionaryLoader.h
 * @brief Loads binary .glide dictionary files.
 *
 * The dictionary loader reads the custom binary format produced by
 * scripts/gen_dict.py. It validates the file header, reads all entries,
 * and provides lookup by word and iteration over all entries.
 *
 * Thread safety: After loading, read-only operations (lookup, iteration)
 * are thread-safe. Loading/unloading are NOT thread-safe.
 */

namespace swipetype {

/**
 * @brief A single dictionary entry.
 */
struct DictionaryEntry {
    std::string word;        ///< UTF-8 encoded word string
    uint32_t frequency = 0;  ///< Frequency (higher = more common)
    uint8_t flags = 0;       ///< Bitmask: DICT_FLAG_PROPER_NOUN, DICT_FLAG_PROFANITY
};

/**
 * @brief Parsed dictionary file header.
 */
struct DictionaryHeader {
    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t flags = 0;
    uint32_t entryCount = 0;
    std::string languageTag;
};

/**
 * @brief Loads and provides access to a binary .glide dictionary.
 *
 * Usage:
 * @code
 *   DictionaryLoader loader;
 *   if (loader.load("/path/to/en-us.glide")) {
 *       auto entries = loader.getEntriesStartingWith('h');
 *       // ... use entries ...
 *   }
 *   loader.unload();
 * @endcode
 */
class DictionaryLoader {
public:
    DictionaryLoader();
    ~DictionaryLoader();

    // Non-copyable, movable
    DictionaryLoader(const DictionaryLoader&) = delete;
    DictionaryLoader& operator=(const DictionaryLoader&) = delete;
    DictionaryLoader(DictionaryLoader&&) noexcept;
    DictionaryLoader& operator=(DictionaryLoader&&) noexcept;

    /**
     * @brief Load a dictionary from a binary .glide file.
     *
     * Validates the header (magic, version) and reads all entries into memory.
     * If a dictionary is already loaded, it is unloaded first.
     *
     * @param filePath  Absolute path to the .glide dictionary file.
     * @return true on success; false on file not found, corrupt header,
     *         or version mismatch. Check getLastError() for details.
     */
    bool load(const std::string& filePath);

    /**
     * @brief Load a dictionary from a memory buffer.
     *
     * @param data  Pointer to the buffer containing the dictionary file contents.
     * @param size  Size of the buffer in bytes.
     * @return true on success; false on invalid data.
     */
    bool loadFromMemory(const uint8_t* data, size_t size);

    /**
     * @brief Unload the current dictionary and free memory.
     */
    void unload();

    /**
     * @brief Check if a dictionary is currently loaded.
     * @return true if a dictionary is loaded and ready for queries.
     */
    bool isLoaded() const;

    /**
     * @brief Get the dictionary header information.
     * @return Header struct. Fields are zero/empty if no dictionary is loaded.
     */
    DictionaryHeader getHeader() const;

    /**
     * @brief Get the total number of entries in the loaded dictionary.
     * @return Entry count, or 0 if no dictionary is loaded.
     */
    uint32_t getEntryCount() const;

    /**
     * @brief Get the maximum frequency value in the dictionary.
     *
     * Used for frequency normalization in scoring.
     * @return Maximum frequency, or 0 if no dictionary loaded.
     */
    uint32_t getMaxFrequency() const;

    /**
     * @brief Get all dictionary entries.
     * @return Reference to the internal entry vector. Empty if not loaded.
     */
    const std::vector<DictionaryEntry>& getAllEntries() const;

    /**
     * @brief Get entries whose word starts with the given character.
     *
     * @param startChar  Starting character (lowercase ASCII expected).
     * @return Vector of matching entries. Empty if none found or not loaded.
     */
    std::vector<const DictionaryEntry*> getEntriesStartingWith(char startChar) const;

    /**
     * @brief Get entries whose word starts with startChar and ends with endChar.
     *
     * This is the primary candidate filtering method used during recognition.
     *
     * @param startChar  First character (lowercase ASCII)
     * @param endChar    Last character (lowercase ASCII)
     * @return Vector of matching entries. Empty if none found.
     */
    std::vector<const DictionaryEntry*> getEntriesWithStartEnd(char startChar,
                                                                char endChar) const;

    /**
     * @brief Look up a specific word.
     *
     * @param word  Word to look up (case-insensitive).
     * @return Pointer to the entry, or nullptr if not found.
     */
    const DictionaryEntry* lookup(const std::string& word) const;

    /**
     * @brief Get the last error that occurred.
     * @return ErrorInfo with code and message. Code is NONE if no error.
     */
    ErrorInfo getLastError() const;

private:
    struct Impl;
    Impl* pImpl;
};

} // namespace swipetype
