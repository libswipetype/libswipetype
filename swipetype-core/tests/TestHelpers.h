#pragma once
// TestHelpers.h — Shared test utilities and fixtures

#include <swipetype/SwipeTypeTypes.h>
#include <swipetype/KeyboardLayout.h>
#include <swipetype/GesturePath.h>
#include <swipetype/GesturePoint.h>
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <string>

namespace swipetype::test {

// ============================================================
// QWERTY layout for testing (320×160 dp, 26 keys)
// ============================================================
inline swipetype::KeyboardLayout makeQwertyLayout() {
    KeyboardLayout layout;
    layout.languageTag = "en-US";
    layout.layoutWidth  = 320.0f;
    layout.layoutHeight = 160.0f;

    struct KeyDef { char label; int32_t cp; float cx; float cy; float w; float h; };
    const KeyDef keys[] = {
        // Row 1: Q W E R T Y U I O P
        {'q', 113, 16,  26, 32, 52},  {'w', 119, 48,  26, 32, 52},
        {'e', 101, 80,  26, 32, 52},  {'r', 114, 112, 26, 32, 52},
        {'t', 116, 144, 26, 32, 52},  {'y', 121, 176, 26, 32, 52},
        {'u', 117, 208, 26, 32, 52},  {'i', 105, 240, 26, 32, 52},
        {'o', 111, 272, 26, 32, 52},  {'p', 112, 304, 26, 32, 52},
        // Row 2: A S D F G H J K L
        {'a',  97, 32,  80, 32, 52},  {'s', 115, 64,  80, 32, 52},
        {'d', 100, 96,  80, 32, 52},  {'f', 102, 128, 80, 32, 52},
        {'g', 103, 160, 80, 32, 52},  {'h', 104, 192, 80, 32, 52},
        {'j', 106, 224, 80, 32, 52},  {'k', 107, 256, 80, 32, 52},
        {'l', 108, 288, 80, 32, 52},
        // Row 3: Z X C V B N M
        {'z', 122, 64,  134, 32, 52}, {'x', 120, 96,  134, 32, 52},
        {'c',  99, 128, 134, 32, 52}, {'v', 118, 160, 134, 32, 52},
        {'b',  98, 192, 134, 32, 52}, {'n', 110, 224, 134, 32, 52},
        {'m', 109, 256, 134, 32, 52},
    };

    for (const auto& k : keys) {
        layout.keys.push_back(KeyDescriptor(
            std::string(1, k.label), k.cp,
            k.cx, k.cy, k.w, k.h
        ));
    }
    return layout;
}

// ============================================================
// Path generators — create gesture paths for known words
// ============================================================

/// Generate a straight-line path between key centers for a word.
/// Returns a vector of GesturePoint with timestamps at 10ms intervals.
inline std::vector<GesturePoint> makePathForWord(
    const KeyboardLayout& layout, const std::string& word, int pointsPerSegment = 8)
{
    std::vector<GesturePoint> points;
    if (word.empty()) return points;

    // Collect key centers
    std::vector<std::pair<float, float>> centers;
    for (char c : word) {
        int32_t cp = static_cast<int32_t>(c);
        int32_t idx = layout.findKeyByCodePoint(cp);
        if (idx >= 0) {
            centers.push_back({layout.keys[static_cast<size_t>(idx)].centerX,
                               layout.keys[static_cast<size_t>(idx)].centerY});
        }
    }
    if (centers.empty()) return points;

    int64_t ts = 0;
    for (size_t i = 0; i < centers.size() - 1; ++i) {
        float x0 = centers[i].first,  y0 = centers[i].second;
        float x1 = centers[i+1].first, y1 = centers[i+1].second;

        for (int j = 0; j < pointsPerSegment; ++j) {
            float t = static_cast<float>(j) / pointsPerSegment;
            float x = x0 + (x1 - x0) * t;
            float y = y0 + (y1 - y0) * t;
            points.push_back(GesturePoint{x, y, ts});
            ts += 10;
        }
    }
    // Add final point
    points.push_back(GesturePoint{
        centers.back().first, centers.back().second, ts
    });

    return points;
}

/// Add Gaussian noise to a path to simulate imprecise gestures.
inline void addNoise(std::vector<GesturePoint>& points, float stddevX, float stddevY, uint32_t seed = 42) {
    // Simple LCG-based noise for reproducibility
    uint32_t state = seed;
    auto nextFloat = [&]() -> float {
        state = state * 1664525u + 1013904223u;
        return (static_cast<float>(state) / static_cast<float>(0xFFFFFFFF)) * 2.0f - 1.0f;
    };
    for (auto& p : points) {
        p.x += nextFloat() * stddevX;
        p.y += nextFloat() * stddevY;
    }
}

// ============================================================
// Assertion helpers
// ============================================================

/// Assert that the top candidate matches the expected word.
#define ASSERT_TOP_CANDIDATE(candidates, expected) \
    ASSERT_FALSE(candidates.empty()) << "No candidates returned"; \
    EXPECT_EQ(candidates[0].word, expected) \
        << "Expected top candidate '" << expected << "', got '" << candidates[0].word << "'"

/// Assert candidate list contains a specific word.
#define ASSERT_CONTAINS_WORD(candidates, expectedWord) \
    { bool _found = false; \
      for (const auto& _c : candidates) { if (_c.word == expectedWord) { _found = true; break; } } \
      EXPECT_TRUE(_found) << "Expected candidates to contain '" << expectedWord << "'"; }

} // namespace swipetype::test
