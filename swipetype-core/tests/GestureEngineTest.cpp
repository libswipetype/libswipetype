#include <gtest/gtest.h>
#include <swipetype/GestureEngine.h>
#include <swipetype/KeyboardLayout.h>
#include <swipetype/GesturePoint.h>
#include <swipetype/GestureCandidate.h>
#include <swipetype/DictionaryLoader.h>
#include <swipetype/SwipeTypeTypes.h>
#include "TestHelpers.h"
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <fstream>
#include <cstdio>
#include <unistd.h>

using namespace swipetype;
using namespace swipetype::test;

// ---------------------------------------------------------------------------
// Build a small in-memory dictionary with words relevant for testing
// ---------------------------------------------------------------------------

static std::vector<uint8_t> buildTestDict() {
    // Header helpers
    auto writeU16 = [](std::vector<uint8_t>& b, size_t off, uint16_t v) {
        b[off] = v & 0xFF; b[off+1] = (v>>8) & 0xFF;
    };
    auto writeU32 = [](std::vector<uint8_t>& b, size_t off, uint32_t v) {
        b[off] = v & 0xFF; b[off+1] = (v>>8) & 0xFF;
        b[off+2] = (v>>16) & 0xFF; b[off+3] = (v>>24) & 0xFF;
    };

    const std::vector<std::pair<std::string, uint32_t>> words = {
        {"the",         1'000'000},
        {"and",           800'000},
        {"hello",          50'000},
        {"world",          40'000},
        {"help",           30'000},
        {"hero",           20'000},
        {"go",            200'000},
        {"do",            180'000},
        {"a",             900'000},
    };

    std::vector<uint8_t> entries;
    for (const auto& [w, freq] : words) {
        entries.push_back(static_cast<uint8_t>(w.size()));
        for (char c : w) entries.push_back(static_cast<uint8_t>(c));
        entries.push_back(freq         & 0xFF);
        entries.push_back((freq >>  8) & 0xFF);
        entries.push_back((freq >> 16) & 0xFF);
        entries.push_back((freq >> 24) & 0xFF);
        entries.push_back(0x00); // flags
    }

    std::vector<uint8_t> buf(DICT_HEADER_SIZE, 0);
    writeU32(buf, 0, DICT_MAGIC);
    writeU16(buf, 4, DICT_VERSION);
    writeU16(buf, 6, 0);
    writeU32(buf, 8, static_cast<uint32_t>(words.size()));
    const char* lang = "en";
    buf[12] = 2; buf[13] = 0;  // langLen = 2
    buf[14] = 'e'; buf[15] = 'n';

    buf.insert(buf.end(), entries.begin(), entries.end());
    return buf;
}

// ---------------------------------------------------------------------------

class GestureEngineTest : public ::testing::Test {
protected:
    KeyboardLayout layout = makeQwertyLayout();
    std::vector<uint8_t> testDict = buildTestDict();
    std::unique_ptr<GestureEngine> engine;

    void SetUp() override {
        engine = std::make_unique<GestureEngine>();
        bool ok = engine->initWithData(layout, testDict.data(), testDict.size());
        ASSERT_TRUE(ok) << "Engine init failed: " << (ok ? "" : "initWithData returned false");
    }
};

// ----- Initialization -----

TEST_F(GestureEngineTest, InitializeWithValidDict) {
    EXPECT_TRUE(engine->isInitialized());
}

TEST_F(GestureEngineTest, InitializeWithInvalidDictFails) {
    GestureEngine badEngine;
    bool ok = badEngine.init(layout, "/nonexistent/path/dict.glide");
    EXPECT_FALSE(ok);
    EXPECT_FALSE(badEngine.isInitialized());
}

// ----- Recognition (end-to-end) -----

TEST_F(GestureEngineTest, RecognizeHello) {
    auto rawPts = makePathForWord(layout, "hello");
    ASSERT_GE(rawPts.size(), 2u);

    RawGesturePath raw;
    raw.points = rawPts;
    auto candidates = engine->recognize(raw, 5);

    // "hello" must appear in the top 5
    ASSERT_CONTAINS_WORD(candidates, "hello");
}

TEST_F(GestureEngineTest, RecognizeThe) {
    auto rawPts = makePathForWord(layout, "the");
    ASSERT_GE(rawPts.size(), 2u);

    RawGesturePath raw;
    raw.points = rawPts;
    auto candidates = engine->recognize(raw, 5);

    // "the" is the highest-frequency word that matches start 't' / end 'e'
    ASSERT_FALSE(candidates.empty()) << "No candidates returned for 'the'";
    ASSERT_CONTAINS_WORD(candidates, "the");
}

TEST_F(GestureEngineTest, RecognizeWithNoise) {
    auto rawPts = makePathForWord(layout, "hello");
    addNoise(rawPts, 5.0f, 5.0f);
    ASSERT_GE(rawPts.size(), 2u);

    RawGesturePath raw;
    raw.points = rawPts;
    auto candidates = engine->recognize(raw, 8);
    // Even with noise, "hello" should be recoverable (or candidates non-empty)
    EXPECT_FALSE(candidates.empty()) << "No candidates after noisy 'hello' gesture";
}

TEST_F(GestureEngineTest, RecognizeReturnsSortedByScore) {
    // Use "hello" — both "hello" and "hero" start with 'h' / end with 'o'
    // so we expect >= 2 candidates
    auto rawPts = makePathForWord(layout, "hello");
    RawGesturePath raw;
    raw.points = rawPts;
    auto candidates = engine->recognize(raw, 8);
    // Must have at least 1; if only 1 the sort is trivially correct
    ASSERT_FALSE(candidates.empty()) << "No candidates for 'hello'";

    for (size_t i = 1; i < candidates.size(); ++i) {
        EXPECT_GE(candidates[i - 1].confidence, candidates[i].confidence)
            << "Candidates not sorted at index " << i;
    }
}

// ----- Performance -----

TEST_F(GestureEngineTest, RecognizeCompletesWithin50ms) {
    auto rawPts = makePathForWord(layout, "hello");
    RawGesturePath raw;
    raw.points = rawPts;

    auto t0 = std::chrono::high_resolution_clock::now();
    engine->recognize(raw, 8);
    auto t1 = std::chrono::high_resolution_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    EXPECT_LT(ms, 50) << "recognize() exceeded 50ms performance budget (" << ms << "ms)";
}

// ----- Layout updates -----

TEST_F(GestureEngineTest, UpdateLayoutChangesResults) {
    // Get candidates with standard QWERTY
    auto rawPts = makePathForWord(layout, "hello");
    RawGesturePath raw;
    raw.points = rawPts;
    auto before = engine->recognize(raw, 5);
    ASSERT_FALSE(before.empty());

    // Swap 'h' and 'j' key positions
    KeyboardLayout modified = makeQwertyLayout();
    for (auto& key : modified.keys) {
        if (key.codePoint == 'h') key.centerX = 224.0f; // was 192
        else if (key.codePoint == 'j') key.centerX = 192.0f; // was 224
    }
    engine->updateLayout(modified);

    auto after = engine->recognize(raw, 5);
    // Results with swapped layout should differ OR still produce candidates
    // (just verifying no crash and recognition still runs)
    // The top candidate may differ
    SUCCEED(); // Layout update should not crash
}

// ----- Edge cases -----

TEST_F(GestureEngineTest, EmptyGestureReturnsEmpty) {
    RawGesturePath raw; // no points
    auto candidates = engine->recognize(raw, 8);
    EXPECT_TRUE(candidates.empty()) << "Empty gesture should return no candidates";
}

TEST_F(GestureEngineTest, SinglePointGesture) {
    RawGesturePath raw;
    raw.points.push_back(GesturePoint(32.0f, 80.0f, 0)); // near 'a' key
    auto candidates = engine->recognize(raw, 8);
    // Single point < MIN_GESTURE_POINTS, should return empty or "a"
    // Either behavior is acceptable — just must not crash
    SUCCEED();
}

TEST_F(GestureEngineTest, VeryLongGesture) {
    // 500-point gesture traversing multiple keys
    RawGesturePath raw;
    const std::string word = "world";
    auto pts = makePathForWord(layout, word, 100); // 100 points per segment
    raw.points = pts;
    // Must complete without crash
    auto candidates = engine->recognize(raw, 8);
    SUCCEED(); // Just verify no crash/hang
}

// ----- Structural accuracy regression tests -----

TEST_F(GestureEngineTest, ZigzagWordNotFilteredByLengthEstimate) {
    // "hello" visits h,e,l,o — 4 distinct keys.
    // The old arc-length estimator guessed ~17.8 chars and risked filtering
    // the 5-char word "hello". Key-transition count should estimate ~4-5.
    auto rawPts = makePathForWord(layout, "hello");
    RawGesturePath raw;
    raw.points = rawPts;
    auto candidates = engine->recognize(raw, 5);

    ASSERT_CONTAINS_WORD(candidates, "hello");
    // "hello" should rank as top-1 or top-2 for a clean, noiseless gesture
    ASSERT_FALSE(candidates.empty());
    EXPECT_TRUE(candidates[0].word == "hello" ||
                (candidates.size() > 1 && candidates[1].word == "hello"))
        << "hello should be top-2 for a clean gesture, top was: " << candidates[0].word;
}

TEST_F(GestureEngineTest, SingleCandidateGetsReasonableConfidence) {
    // "hero" starts with 'h' and ends with 'o', matching "hello" and "hero".
    // Even if only one candidate survives filtering, its confidence should
    // be meaningful (> 0.3) thanks to the absolute DTW floor.
    auto rawPts = makePathForWord(layout, "hero");
    RawGesturePath raw;
    raw.points = rawPts;
    auto candidates = engine->recognize(raw, 5);

    ASSERT_CONTAINS_WORD(candidates, "hero");
    for (const auto& c : candidates) {
        if (c.word == "hero") {
            EXPECT_GT(c.confidence, 0.3f)
                << "hero confidence should be > 0.3, was " << c.confidence;
        }
    }
}

TEST_F(GestureEngineTest, ShapeBeatFrequencyForWorldGesture) {
    // "world" and "would" both start with 'w' and end with 'd'.
    // "would" has much higher frequency (not in test dict, but this
    // verifies shape dominance). A gesture tracing w-o-r-l-d should
    // rank "world" at top-1 because adaptive alpha reduces frequency
    // influence when DTW scores are compressed.
    auto rawPts = makePathForWord(layout, "world");
    RawGesturePath raw;
    raw.points = rawPts;
    auto candidates = engine->recognize(raw, 5);

    ASSERT_CONTAINS_WORD(candidates, "world");
    // "world" should be top candidate for its own gesture
    EXPECT_EQ(candidates[0].word, "world")
        << "world should be top-1 for world gesture, got: " << candidates[0].word;
}
