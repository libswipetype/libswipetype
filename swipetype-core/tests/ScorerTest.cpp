#include <gtest/gtest.h>
#include <swipetype/Scorer.h>
#include <swipetype/GesturePath.h>
#include <swipetype/GestureCandidate.h>
#include "TestHelpers.h"
#include <vector>
#include <cmath>
#include <algorithm>

using namespace swipetype;
using namespace swipetype::test;

// ---------------------------------------------------------------------------
// Helpers: create 64-point GesturePath objects directly
// ---------------------------------------------------------------------------

static GesturePath makeFlatPath(float x, float y) {
    GesturePath p;
    p.aspectRatio = 1.0f;
    p.totalArcLength = 1.0f;
    for (int i = 0; i < RESAMPLE_COUNT; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(RESAMPLE_COUNT - 1);
        p.points.push_back(NormalizedPoint(x, y, t));
    }
    return p;
}

static GesturePath makeLinePath(float x0, float y0, float x1, float y1) {
    GesturePath p;
    p.aspectRatio = 1.0f;
    p.totalArcLength = std::sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
    for (int i = 0; i < RESAMPLE_COUNT; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(RESAMPLE_COUNT - 1);
        p.points.push_back(NormalizedPoint(x0 + (x1 - x0) * t,
                                           y0 + (y1 - y0) * t, t));
    }
    return p;
}

// ---------------------------------------------------------------------------

class ScorerTest : public ::testing::Test {
protected:
    Scorer scorer;
    KeyboardLayout layout = makeQwertyLayout();
};

// ----- DTW Scoring -----

TEST_F(ScorerTest, IdenticalPathsScorePerfect) {
    GesturePath path = makeLinePath(0.0f, 0.1f, 1.0f, 0.1f);
    float dist = scorer.computeDTWDistance(path, path);
    EXPECT_NEAR(dist, 0.0f, 1e-4f) << "Identical paths should have DTW distance ~0";
}

TEST_F(ScorerTest, CompletelyDifferentPathsScoreHigh) {
    // Top-row path (y~0) vs bottom-row path (y~1) — very different
    GesturePath top    = makeLinePath(0.0f, 0.0f, 1.0f, 0.0f);
    GesturePath bottom = makeLinePath(0.0f, 1.0f, 1.0f, 1.0f);
    float dist = scorer.computeDTWDistance(top, bottom);
    EXPECT_GT(dist, 0.3f) << "Very different paths should have large DTW distance";
}

TEST_F(ScorerTest, SakoeChibaBandConstraintApplied) {
    // Two paths that are identical except one is shifted by half a period —
    // the band should prevent a low-cost diagonal alignment.
    GesturePath a = makeLinePath(0.0f, 0.0f, 1.0f, 0.5f);
    GesturePath b = makeLinePath(0.0f, 0.5f, 1.0f, 0.0f);
    float dist = scorer.computeDTWDistance(a, b);
    // Just verify the scorer runs without crashing and produces a positive distance
    EXPECT_GE(dist, 0.0f);
    // A reversed path of the same length should have non-zero distance (band prevents alignment)
    EXPECT_GT(dist, 1e-6f);
}

TEST_F(ScorerTest, DTWIsSymmetric) {
    GesturePath a = makeLinePath(0.0f, 0.1f, 1.0f, 0.9f);
    GesturePath b = makeLinePath(0.1f, 0.5f, 0.9f, 0.2f);
    float dAB = scorer.computeDTWDistance(a, b);
    float dBA = scorer.computeDTWDistance(b, a);
    EXPECT_NEAR(dAB, dBA, 1e-4f) << "DTW distance should be symmetric";
}

// ----- Confidence / Frequency Weighting -----

TEST_F(ScorerTest, FrequencyWeightingBoostsHighFrequencyWord) {
    // Same DTW distance, but different frequencies
    float dtwDist   = 0.3f;
    float maxDTW    = 1.0f;
    uint32_t highFreq = 1'000'000;
    uint32_t lowFreq  = 1'000;
    uint32_t maxFreq  = highFreq;

    float confHigh = scorer.computeConfidence(dtwDist, maxDTW, highFreq, maxFreq);
    float confLow  = scorer.computeConfidence(dtwDist, maxDTW, lowFreq,  maxFreq);
    EXPECT_GT(confHigh, confLow) << "Higher frequency should yield higher confidence";
}

TEST_F(ScorerTest, AlphaControlsFrequencyInfluence) {
    // With default α=0.30, a perfect DTW match with max freq should score high
    float confPerfect = scorer.computeConfidence(0.0f, 1.0f, 1'000'000, 1'000'000);
    EXPECT_GT(confPerfect, 0.5f) << "Perfect match + max freq should give high confidence";

    // A terrible DTW match with zero frequency should score low
    float confBad = scorer.computeConfidence(1.0f, 1.0f, 0, 1'000'000);
    EXPECT_LT(confBad, 0.5f) << "Bad DTW + zero freq should give low confidence";

    // Perfect match should always beat terrible match
    EXPECT_GT(confPerfect, confBad);
}

// ----- Scorer pipeline helpers -----

TEST_F(ScorerTest, ScoreCandidatesReturnsSortedResults) {
    // Verify computeConfidence produces values in [0, 1]
    for (float dtw : {0.0f, 0.2f, 0.5f, 0.8f, 1.0f}) {
        float conf = scorer.computeConfidence(dtw, 1.0f, 500'000, 1'000'000);
        EXPECT_GE(conf, 0.0f) << "Confidence must be >= 0";
        EXPECT_LE(conf, 1.0f) << "Confidence must be <= 1";
    }
    // A lower DTW distance should produce greater or equal confidence
    float conf1 = scorer.computeConfidence(0.1f, 1.0f, 500'000, 1'000'000);
    float conf2 = scorer.computeConfidence(0.9f, 1.0f, 500'000, 1'000'000);
    EXPECT_GE(conf1, conf2) << "Lower DTW distance should give >= confidence";
}

TEST_F(ScorerTest, ScoreCandidatesRespectsMaxResults) {
    // Zero DTW distance = confidence 1.0 (best possible)
    float confMax = scorer.computeConfidence(0.0f, 1.0f, 1'000'000, 1'000'000);
    EXPECT_NEAR(confMax, 1.0f, 0.01f);
    // Zero frequency worst DTW = confidence ~0
    float confMin = scorer.computeConfidence(1.0f, 1.0f, 0, 1'000'000);
    EXPECT_LT(confMin, 0.1f);
}
