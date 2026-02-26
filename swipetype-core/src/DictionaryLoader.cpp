#include "swipetype/DictionaryLoader.h"
#include "swipetype/SwipeTypeTypes.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cctype>

namespace swipetype {

struct DictionaryLoader::Impl {
    std::vector<DictionaryEntry> entries;
    DictionaryHeader header;
    uint32_t maxFrequency = 0;
    ErrorInfo lastError;
    bool loaded = false;

    void setError(ErrorCode code, const std::string& msg) {
        lastError.code = code;
        lastError.message = msg;
    }

    void clearError() {
        lastError.code = ErrorCode::NONE;
        lastError.message.clear();
    }

    // Read uint16_t little-endian from buffer at offset
    static uint16_t readU16LE(const uint8_t* buf, size_t offset) {
        return static_cast<uint16_t>(buf[offset]) |
               (static_cast<uint16_t>(buf[offset + 1]) << 8);
    }

    // Read uint32_t little-endian from buffer at offset
    static uint32_t readU32LE(const uint8_t* buf, size_t offset) {
        return static_cast<uint32_t>(buf[offset]) |
               (static_cast<uint32_t>(buf[offset + 1]) << 8) |
               (static_cast<uint32_t>(buf[offset + 2]) << 16) |
               (static_cast<uint32_t>(buf[offset + 3]) << 24);
    }

    bool parseFromBuffer(const uint8_t* data, size_t size) {
        clearError();
        entries.clear();
        header = DictionaryHeader();
        maxFrequency = 0;

        if (size < DICT_HEADER_SIZE) {
            setError(ErrorCode::DICT_CORRUPT, "File too small for header");
            return false;
        }

        // Parse header fields
        header.magic   = readU32LE(data, 0);
        header.version = readU16LE(data, 4);
        header.flags   = readU16LE(data, 6);
        header.entryCount = readU32LE(data, 8);

        uint16_t langLen = readU16LE(data, 12);
        // languageTag fits within the 32-byte header (max 18 bytes after offset 14)
        if (langLen > 0 && static_cast<uint32_t>(14) + langLen <= DICT_HEADER_SIZE) {
            header.languageTag = std::string(
                reinterpret_cast<const char*>(data + 14), langLen);
        }

        if (header.magic != DICT_MAGIC) {
            setError(ErrorCode::DICT_CORRUPT, "Invalid magic bytes");
            return false;
        }
        if (header.version != DICT_VERSION) {
            setError(ErrorCode::DICT_VERSION_MISMATCH,
                     "Unsupported dictionary version: " + std::to_string(header.version));
            return false;
        }

        // Parse entries
        size_t pos = DICT_HEADER_SIZE;
        entries.reserve(header.entryCount);

        for (uint32_t i = 0; i < header.entryCount; ++i) {
            if (pos + 1 > size) {
                setError(ErrorCode::DICT_CORRUPT, "Unexpected end of data at entry " + std::to_string(i));
                return false;
            }

            uint8_t wordLen = data[pos++];
            if (wordLen > MAX_WORD_LENGTH) {
                setError(ErrorCode::DICT_CORRUPT, "Word length exceeds maximum");
                return false;
            }
            if (pos + wordLen + 4 + 1 > size) {
                setError(ErrorCode::DICT_CORRUPT, "Truncated entry at index " + std::to_string(i));
                return false;
            }

            DictionaryEntry entry;
            entry.word = std::string(reinterpret_cast<const char*>(data + pos), wordLen);
            pos += wordLen;
            entry.frequency = readU32LE(data, pos);
            pos += 4;
            entry.flags = data[pos++];

            if (entry.frequency > maxFrequency) {
                maxFrequency = entry.frequency;
            }
            entries.push_back(std::move(entry));
        }

        loaded = true;
        return true;
    }
};

DictionaryLoader::DictionaryLoader() : pImpl(new Impl()) {}
DictionaryLoader::~DictionaryLoader() { delete pImpl; }

DictionaryLoader::DictionaryLoader(DictionaryLoader&& other) noexcept
    : pImpl(other.pImpl) { other.pImpl = nullptr; }

DictionaryLoader& DictionaryLoader::operator=(DictionaryLoader&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

bool DictionaryLoader::load(const std::string& filePath) {
    if (!pImpl) return false;
    unload();

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        pImpl->setError(ErrorCode::DICT_NOT_FOUND,
                        "Cannot open file: " + filePath);
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        pImpl->setError(ErrorCode::DICT_CORRUPT, "Failed to read file: " + filePath);
        return false;
    }

    return pImpl->parseFromBuffer(buffer.data(), buffer.size());
}

bool DictionaryLoader::loadFromMemory(const uint8_t* data, size_t size) {
    if (!pImpl) return false;
    unload();
    return pImpl->parseFromBuffer(data, size);
}

void DictionaryLoader::unload() {
    if (pImpl) {
        pImpl->entries.clear();
        pImpl->header = DictionaryHeader();
        pImpl->maxFrequency = 0;
        pImpl->loaded = false;
        pImpl->clearError();
    }
}

bool DictionaryLoader::isLoaded() const {
    return pImpl && pImpl->loaded;
}

DictionaryHeader DictionaryLoader::getHeader() const {
    return pImpl ? pImpl->header : DictionaryHeader();
}

uint32_t DictionaryLoader::getEntryCount() const {
    return pImpl ? static_cast<uint32_t>(pImpl->entries.size()) : 0;
}

uint32_t DictionaryLoader::getMaxFrequency() const {
    return pImpl ? pImpl->maxFrequency : 0;
}

const std::vector<DictionaryEntry>& DictionaryLoader::getAllEntries() const {
    static const std::vector<DictionaryEntry> empty;
    return (pImpl && pImpl->loaded) ? pImpl->entries : empty;
}

std::vector<const DictionaryEntry*> DictionaryLoader::getEntriesStartingWith(char startChar) const {
    std::vector<const DictionaryEntry*> result;
    if (!pImpl || !pImpl->loaded) return result;

    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(startChar)));
    for (const auto& entry : pImpl->entries) {
        if (!entry.word.empty() &&
            std::tolower(static_cast<unsigned char>(entry.word[0])) ==
                static_cast<unsigned char>(lc)) {
            result.push_back(&entry);
        }
    }
    return result;
}

std::vector<const DictionaryEntry*> DictionaryLoader::getEntriesWithStartEnd(
        char startChar, char endChar) const {
    std::vector<const DictionaryEntry*> result;
    if (!pImpl || !pImpl->loaded) return result;

    char lcS = static_cast<char>(std::tolower(static_cast<unsigned char>(startChar)));
    char lcE = static_cast<char>(std::tolower(static_cast<unsigned char>(endChar)));

    for (const auto& entry : pImpl->entries) {
        if (entry.word.empty()) continue;
        char first = static_cast<char>(std::tolower(
            static_cast<unsigned char>(entry.word.front())));
        char last  = static_cast<char>(std::tolower(
            static_cast<unsigned char>(entry.word.back())));
        if (first == lcS && last == lcE) {
            result.push_back(&entry);
        }
    }
    return result;
}

const DictionaryEntry* DictionaryLoader::lookup(const std::string& word) const {
    if (!pImpl || !pImpl->loaded || word.empty()) return nullptr;

    // Create lowercase query
    std::string lcWord;
    lcWord.reserve(word.size());
    for (char ch : word) {
        lcWord.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch))));
    }

    for (const auto& entry : pImpl->entries) {
        std::string lcEntry;
        lcEntry.reserve(entry.word.size());
        for (char ch : entry.word) {
            lcEntry.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch))));
        }
        if (lcEntry == lcWord) return &entry;
    }
    return nullptr;
}

ErrorInfo DictionaryLoader::getLastError() const {
    return pImpl ? pImpl->lastError : ErrorInfo();
}

} // namespace swipetype
