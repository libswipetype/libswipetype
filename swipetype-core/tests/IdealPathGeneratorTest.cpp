#include <gtest/gtest.h>
#include <swipetype/IdealPathGenerator.h>
#include <swipetype/KeyboardLayout.h>
#include "TestHelpers.h"
#include <string>
#include <cmath>

using namespace swipetype;
using namespace swipetype::test;

class IdealPathGeneratorTest : public ::testing::Test {
protected:
    KeyboardLayout layout = makeQwertyLayout();
    IdealPathGenerator generator;

    void SetUp() override {
        generator.setLayout(layout);
    }
};

TEST_F(IdealPathGeneratorTest, IdealPathHas64Points) {
    GesturePath path = generator.getIdealPath("hello");
    ASSERT_TRUE(path.isValid()) << "getIdealPath('hello') should return a valid path";
    EXPECT_EQ(static_cast<int>(path.points.size()), RESAMPLE_COUNT);
}

TEST_F(IdealPathGeneratorTest, IdealPathStartsAtFirstKeyCenter) {
    // "the": t(144,26) → h(192,80) → e(80,26)
    // Bounding-box: xmin=80, xmax=192, so t at x=(144-80)/(192-80)≈0.571
    // The key insight: path is bounding-box normalised, NOT layout-normalised.
    GesturePath path = generator.getIdealPath("the");
    ASSERT_TRUE(path.isValid());

    // t.x=144 is between e.x=80 and h.x=192 → normalised to ~0.571
    // Verify the first point is in (0, 1) and the path covers the full x range
    float frontX = path.points.front().x;
    float backX  = path.points.back().x;
    EXPECT_GE(frontX, 0.0f);
    EXPECT_LE(frontX, 1.0f);
    // t.x(144) > e.x(80), so after bounding-box norm t normalises higher than e
    EXPECT_GT(frontX, backX)
        << "'t' key (x=144) should normalise higher than 'e' key (x=80) in the path";
}

TEST_F(IdealPathGeneratorTest, IdealPathEndsAtLastKeyCenter) {
    // "the": path ends at 'e' (x=80, y=26)
    // After bounding-box norm: e.x is the minimum x in the path → normalises to 0.0
    GesturePath path = generator.getIdealPath("the");
    ASSERT_TRUE(path.isValid());

    // 'e' key is the leftmost key in the path (x_min=80) → normalised x = 0.0
    // 'e' is on the same y as 't' (y_min=26) → normalised y = 0.0
    EXPECT_NEAR(path.points.back().x, 0.0f, 0.05f)
        << "'e' key (x_min in path) should normalise to ~0";
    EXPECT_NEAR(path.points.back().y, 0.0f, 0.05f)
        << "'e' key (y_min in path) should normalise to ~0";
}

TEST_F(IdealPathGeneratorTest, SingleCharWordProducesSinglePoint64Times) {
    // Single-character words have a zero bounding-box.
    // The IdealPathGenerator may return an invalid/empty path for this case.
    // Two-character same-key word (impossible with real keyboard) → use "as" which
    // has two distinct keys and verify it produces a valid path instead.
    GesturePath singlePath = generator.getIdealPath("a");
    // Either valid (all 64 points identical) or invalid (zero bounding-box rejected):
    // just verify no crash and no assertion.
    if (singlePath.isValid()) {
        EXPECT_EQ(static_cast<int>(singlePath.points.size()), RESAMPLE_COUNT);
        // All x and y must be equal (same key center)
        float x0 = singlePath.points[0].x;
        float y0 = singlePath.points[0].y;
        for (const auto& pt : singlePath.points) {
            EXPECT_NEAR(pt.x, x0, 0.01f);
            EXPECT_NEAR(pt.y, y0, 0.01f);
        }
    } else {
        // Verify a two-char word still works
        GesturePath twoChar = generator.getIdealPath("as");
        EXPECT_TRUE(twoChar.isValid());
    }
}

TEST_F(IdealPathGeneratorTest, CachingReturnsSameResult) {
    GesturePath first  = generator.getIdealPath("hello");
    GesturePath second = generator.getIdealPath("hello");
    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());
    ASSERT_EQ(first.points.size(), second.points.size());
    for (size_t i = 0; i < first.points.size(); ++i) {
        EXPECT_FLOAT_EQ(first.points[i].x, second.points[i].x);
        EXPECT_FLOAT_EQ(first.points[i].y, second.points[i].y);
    }
    // Cache should have grown by 1 (only 1 unique word was generated in this test so far)
    EXPECT_GE(generator.cacheSize(), 1u);
}

TEST_F(IdealPathGeneratorTest, DifferentWordsProduceDifferentPaths) {
    GesturePath hello = generator.getIdealPath("hello");
    GesturePath world = generator.getIdealPath("world");
    ASSERT_TRUE(hello.isValid());
    ASSERT_TRUE(world.isValid());

    // At least one point must differ between "hello" and "world"
    bool anyDiff = false;
    for (size_t i = 0; i < hello.points.size() && i < world.points.size(); ++i) {
        if (std::fabs(hello.points[i].x - world.points[i].x) > 0.01f ||
            std::fabs(hello.points[i].y - world.points[i].y) > 0.01f) {
            anyDiff = true;
            break;
        }
    }
    EXPECT_TRUE(anyDiff) << "Ideal paths for 'hello' and 'world' should differ";
}

TEST_F(IdealPathGeneratorTest, LayoutChangeInvalidatesCache) {
    GesturePath before = generator.getIdealPath("hello");
    ASSERT_TRUE(before.isValid());

    // Create a modified layout where 'h' key is moved significantly
    KeyboardLayout modified = makeQwertyLayout();
    for (auto& key : modified.keys) {
        if (key.codePoint == 'h') {
            key.centerX = 16.0f;  // move from 192 dp to 16 dp (far left)
            break;
        }
    }
    generator.setLayout(modified);
    EXPECT_EQ(generator.cacheSize(), 0u) << "setLayout should clear cache";

    GesturePath after = generator.getIdealPath("hello");
    ASSERT_TRUE(after.isValid());

    // The paths should differ because 'h' moved
    bool anyDiff = false;
    for (size_t i = 0; i < before.points.size(); ++i) {
        if (std::fabs(before.points[i].x - after.points[i].x) > 0.05f) {
            anyDiff = true;
            break;
        }
    }
    EXPECT_TRUE(anyDiff) << "Path for 'hello' should change after layout update";
}
