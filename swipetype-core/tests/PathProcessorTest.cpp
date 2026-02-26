#include <gtest/gtest.h>
#include <swipetype/PathProcessor.h>
#include <swipetype/GesturePoint.h>
#include <swipetype/GesturePath.h>
#include "TestHelpers.h"
#include <vector>
#include <cmath>

using namespace swipetype;
using namespace swipetype::test;

class PathProcessorTest : public ::testing::Test {
protected:
    PathProcessor processor;

    // Build a horizontal straight-line RawGesturePath from x=x0 to x=x1 at y=y0
    static RawGesturePath makeLine(float x0, float x1, float y0, int nPoints = 30) {
        RawGesturePath raw;
        for (int i = 0; i < nPoints; ++i) {
            float t = static_cast<float>(i) / (nPoints - 1);
            raw.points.push_back(GesturePoint(x0 + (x1 - x0) * t, y0,
                                              static_cast<int64_t>(i * 10)));
        }
        return raw;
    }
};

// ----- Deduplication (observable through normalize) -----

TEST_F(PathProcessorTest, DeduplicateRemovesDuplicateConsecutivePoints) {
    KeyboardLayout layout = makeQwertyLayout();
    RawGesturePath raw;
    // 15 identical points at (50, 50), then 15 identical at (250, 130)
    for (int i = 0; i < 15; ++i)
        raw.points.push_back(GesturePoint(50.0f, 50.0f, static_cast<int64_t>(i * 10)));
    for (int i = 0; i < 15; ++i)
        raw.points.push_back(GesturePoint(250.0f, 130.0f, static_cast<int64_t>(150 + i * 10)));

    // Two distinct endpoints — normalization should succeed despite many duplicates
    GesturePath result = processor.normalize(raw, layout);
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(static_cast<int>(result.points.size()), RESAMPLE_COUNT);
}

TEST_F(PathProcessorTest, DeduplicatePreservesPointsAboveThreshold) {
    KeyboardLayout layout = makeQwertyLayout();
    // Points 10 dp apart — well above MIN_POINT_DISTANCE_DP (2 dp)
    RawGesturePath raw;
    for (int i = 0; i <= 20; ++i)
        raw.points.push_back(GesturePoint(static_cast<float>(i * 10), 80.0f,
                                          static_cast<int64_t>(i * 10)));
    GesturePath result = processor.normalize(raw, layout);
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(static_cast<int>(result.points.size()), RESAMPLE_COUNT);
}

TEST_F(PathProcessorTest, DeduplicateHandlesEmptyInput) {
    KeyboardLayout layout = makeQwertyLayout();
    RawGesturePath raw; // no points
    GesturePath result = processor.normalize(raw, layout);
    EXPECT_FALSE(result.isValid());
    EXPECT_TRUE(result.points.empty());
}

TEST_F(PathProcessorTest, DeduplicateHandlesSinglePoint) {
    KeyboardLayout layout = makeQwertyLayout();
    RawGesturePath raw;
    raw.points.push_back(GesturePoint(50.0f, 80.0f, 0));
    GesturePath result = processor.normalize(raw, layout);
    // 1 point < MIN_GESTURE_POINTS(2) — should return invalid/empty
    EXPECT_FALSE(result.isValid());
}

// ----- Resampling -----

TEST_F(PathProcessorTest, ResampleProducesExactly64Points) {
    KeyboardLayout layout = makeQwertyLayout();
    RawGesturePath raw = makeLine(16.0f, 304.0f, 80.0f, 30);
    GesturePath result = processor.normalize(raw, layout);
    ASSERT_TRUE(result.isValid());
    EXPECT_EQ(static_cast<int>(result.points.size()), RESAMPLE_COUNT);
}

TEST_F(PathProcessorTest, ResamplePreservesStartAndEndPoints) {
    KeyboardLayout layout = makeQwertyLayout();
    // Horizontal leftward-to-rightward path — first x should be < last x after normalization
    RawGesturePath raw = makeLine(16.0f, 304.0f, 80.0f, 30);
    GesturePath result = processor.normalize(raw, layout);
    ASSERT_TRUE(result.isValid());
    EXPECT_LT(result.points.front().x, result.points.back().x);
}

TEST_F(PathProcessorTest, ResampleEvenlySpreadsPoints) {
    KeyboardLayout layout = makeQwertyLayout();
    // Dense straight line — resampled output should have approximately equal spacing
    RawGesturePath raw = makeLine(16.0f, 304.0f, 80.0f, 100);
    GesturePath result = processor.normalize(raw, layout);
    ASSERT_TRUE(result.isValid());
    ASSERT_GE(result.points.size(), 2u);

    float firstDist = -1.0f;
    for (size_t i = 1; i < result.points.size(); ++i) {
        float dx = result.points[i].x - result.points[i - 1].x;
        float dy = result.points[i].y - result.points[i - 1].y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (firstDist < 0.0f) {
            firstDist = dist;
        } else {
            EXPECT_NEAR(dist, firstDist, firstDist * 0.1f)
                << "Uneven spacing at index " << i;
        }
    }
}

TEST_F(PathProcessorTest, ResampleHandlesCurvedPath) {
    KeyboardLayout layout = makeQwertyLayout();
    RawGesturePath raw;
    // Semi-circular arc within the keyboard area
    for (int i = 0; i <= 60; ++i) {
        float angle = static_cast<float>(i) / 60.0f * 3.14159265f;
        float x = 160.0f + 120.0f * std::cos(angle);
        float y = 50.0f + 40.0f * std::sin(angle);
        raw.points.push_back(GesturePoint(x, y, static_cast<int64_t>(i * 10)));
    }
    GesturePath result = processor.normalize(raw, layout);
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(static_cast<int>(result.points.size()), RESAMPLE_COUNT);
}

// ----- Normalization -----

TEST_F(PathProcessorTest, NormalizeScalesToUnitSquare) {
    KeyboardLayout layout = makeQwertyLayout();
    RawGesturePath raw = makeLine(16.0f, 304.0f, 50.0f, 40);
    GesturePath result = processor.normalize(raw, layout);
    ASSERT_TRUE(result.isValid());
    for (const auto& p : result.points) {
        EXPECT_GE(p.x, -0.01f) << "x below 0";
        EXPECT_LE(p.x,  1.01f) << "x above 1";
        EXPECT_GE(p.y, -0.01f) << "y below 0";
        EXPECT_LE(p.y,  1.01f) << "y above 1";
    }
}

TEST_F(PathProcessorTest, NormalizePreservesRelativePositions) {
    KeyboardLayout layout = makeQwertyLayout();
    RawGesturePath raw;
    // Monotonically increasing x — first point must have smaller normalized x than last
    raw.points.push_back(GesturePoint(50.0f,  80.0f, 0));
    raw.points.push_back(GesturePoint(160.0f, 80.0f, 100));
    raw.points.push_back(GesturePoint(270.0f, 80.0f, 200));
    GesturePath result = processor.normalize(raw, layout);
    ASSERT_TRUE(result.isValid());
    EXPECT_LT(result.points.front().x, result.points.back().x);
}

// ----- Full pipeline -----

TEST_F(PathProcessorTest, ProcessFullPipeline) {
    KeyboardLayout layout = makeQwertyLayout();
    std::vector<GesturePoint> rawPts = makePathForWord(layout, "hello");
    ASSERT_GE(rawPts.size(), 2u) << "makePathForWord returned < 2 points for 'hello'";

    RawGesturePath raw;
    raw.points = rawPts;
    GesturePath result = processor.normalize(raw, layout);

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(static_cast<int>(result.points.size()), RESAMPLE_COUNT);
    for (const auto& p : result.points) {
        EXPECT_GE(p.x, -0.01f);
        EXPECT_LE(p.x,  1.01f);
        EXPECT_GE(p.y, -0.01f);
        EXPECT_LE(p.y,  1.01f);
    }
}
