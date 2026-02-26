#include <gtest/gtest.h>
#include <swipetype/DictionaryLoader.h>
#include <swipetype/SwipeTypeTypes.h>
#include "TestHelpers.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unistd.h>

using namespace swipetype;

class DictionaryLoaderTest : public ::testing::Test {
protected:
    DictionaryLoader loader;
    std::vector<std::string> tempFiles;

    void TearDown() override {
        for (const auto& path : tempFiles) {
            std::remove(path.c_str());
        }
    }

    // Write little-endian uint16 to buf at offset
    static void writeU16LE(std::vector<uint8_t>& buf, size_t offset, uint16_t val) {
        buf[offset]     = static_cast<uint8_t>(val & 0xFF);
        buf[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    }
    static void writeU32LE(std::vector<uint8_t>& buf, size_t offset, uint32_t val) {
        buf[offset]     = static_cast<uint8_t>(val & 0xFF);
        buf[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
        buf[offset + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
        buf[offset + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    }

    /// Create a minimal valid .glide file in memory and return as byte vector.
    /// Format:
    ///   32-byte header: magic(4) version(2) flags(2) entryCount(4) langLen(2) langTag(N) pad
    ///   Each entry:     wordLen(1) word(N) frequency(4) flags(1)
    std::vector<uint8_t> makeMinimalDict(
        const std::string& lang,
        const std::vector<std::pair<std::string, uint32_t>>& words)
    {
        // Build entries first, then header
        std::vector<uint8_t> entries;
        for (const auto& [word, freq] : words) {
            uint8_t wlen = static_cast<uint8_t>(word.size());
            entries.push_back(wlen);
            for (char c : word) entries.push_back(static_cast<uint8_t>(c));
            // frequency: 4 bytes LE
            entries.push_back(static_cast<uint8_t>(freq         & 0xFF));
            entries.push_back(static_cast<uint8_t>((freq >>  8) & 0xFF));
            entries.push_back(static_cast<uint8_t>((freq >> 16) & 0xFF));
            entries.push_back(static_cast<uint8_t>((freq >> 24) & 0xFF));
            // flags: 1 byte
            entries.push_back(0x00);
        }

        // Header: exactly 32 bytes
        std::vector<uint8_t> buf(DICT_HEADER_SIZE, 0);
        writeU32LE(buf, 0, DICT_MAGIC);
        writeU16LE(buf, 4, DICT_VERSION);
        writeU16LE(buf, 6, 0);  // flags
        writeU32LE(buf, 8, static_cast<uint32_t>(words.size()));
        uint16_t langLen = static_cast<uint16_t>(std::min(lang.size(), size_t(18)));
        writeU16LE(buf, 12, langLen);
        for (size_t i = 0; i < langLen; ++i) buf[14 + i] = static_cast<uint8_t>(lang[i]);

        // Append entries
        buf.insert(buf.end(), entries.begin(), entries.end());
        return buf;
    }

    /// Write byte vector to a temporary file and return its path.
    std::string writeToTempFile(const std::vector<uint8_t>& data) {
        char tmp[] = "/tmp/swipetype_test_XXXXXX";
        int fd = mkstemp(tmp);
        if (fd < 0) return "";

        std::string path(tmp);
        tempFiles.push_back(path);

        // Write via fstream; fd is still open, so use fdopen or just write to path
        if (!data.empty()) {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));
        }
        close(fd);
        return path;
    }
};

TEST_F(DictionaryLoaderTest, LoadValidDictionary) {
    auto data = makeMinimalDict("en-US", {{"hello", 100}, {"world", 200}, {"test", 50}});
    std::string path = writeToTempFile(data);
    ASSERT_FALSE(path.empty());

    bool ok = loader.load(path);
    EXPECT_TRUE(ok) << loader.getLastError().message;
    EXPECT_TRUE(loader.isLoaded());
    EXPECT_EQ(loader.getEntryCount(), 3u);
}

TEST_F(DictionaryLoaderTest, LoadFromMemory) {
    auto data = makeMinimalDict("en-US", {{"foo", 1000}, {"bar", 2000}});
    bool ok = loader.loadFromMemory(data.data(), data.size());
    EXPECT_TRUE(ok) << loader.getLastError().message;
    EXPECT_EQ(loader.getEntryCount(), 2u);
}

TEST_F(DictionaryLoaderTest, RejectInvalidMagic) {
    auto data = makeMinimalDict("en-US", {{"hello", 100}});
    // Corrupt magic bytes
    data[0] = 0xDE; data[1] = 0xAD; data[2] = 0xBE; data[3] = 0xEF;
    bool ok = loader.loadFromMemory(data.data(), data.size());
    EXPECT_FALSE(ok);
    EXPECT_NE(loader.getLastError().code, ErrorCode::NONE);
}

TEST_F(DictionaryLoaderTest, RejectUnsupportedVersion) {
    auto data = makeMinimalDict("en-US", {{"hello", 100}});
    // Set version to 99
    data[4] = 99; data[5] = 0;
    bool ok = loader.loadFromMemory(data.data(), data.size());
    EXPECT_FALSE(ok);
    EXPECT_EQ(loader.getLastError().code, ErrorCode::DICT_VERSION_MISMATCH);
}

TEST_F(DictionaryLoaderTest, RejectTruncatedFile) {
    auto data = makeMinimalDict("en-US", {{"hello", 100}, {"world", 200}});
    // Truncate halfway through entries
    data.resize(DICT_HEADER_SIZE + 3);
    bool ok = loader.loadFromMemory(data.data(), data.size());
    EXPECT_FALSE(ok);
}

TEST_F(DictionaryLoaderTest, LookupByPrefix) {
    auto data = makeMinimalDict("en-US",
        {{"hello", 100}, {"help", 80}, {"hero", 60}, {"world", 200}});
    ASSERT_TRUE(loader.loadFromMemory(data.data(), data.size()));

    // getEntriesStartingWith('h') returns hello, help, hero (all start with 'h')
    auto hEntries = loader.getEntriesStartingWith('h');
    EXPECT_EQ(hEntries.size(), 3u);

    // Verify "hello" and "help" are present, "world" is not
    bool hasHello = false, hasHelp = false, hasWorld = false;
    for (const auto* e : hEntries) {
        if (e->word == "hello") hasHello = true;
        if (e->word == "help")  hasHelp  = true;
        if (e->word == "world") hasWorld = true;
    }
    EXPECT_TRUE(hasHello);
    EXPECT_TRUE(hasHelp);
    EXPECT_FALSE(hasWorld);
}

TEST_F(DictionaryLoaderTest, LookupByStartAndEndKey) {
    auto data = makeMinimalDict("en-US",
        {{"hello", 100}, {"help", 80}, {"world", 200}, {"happy", 150}});
    ASSERT_TRUE(loader.loadFromMemory(data.data(), data.size()));

    // getEntriesWithStartEnd('h', 'o') — starts with 'h', ends with 'o' → only "hello"
    auto matches = loader.getEntriesWithStartEnd('h', 'o');
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0]->word, "hello");
}

TEST_F(DictionaryLoaderTest, EmptyFileFails) {
    std::string path = writeToTempFile({});
    ASSERT_FALSE(path.empty());
    bool ok = loader.load(path);
    EXPECT_FALSE(ok);
    EXPECT_NE(loader.getLastError().code, ErrorCode::NONE);
}
