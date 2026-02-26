#include "swipetype/Scorer.h"
#include "swipetype/SwipeTypeTypes.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cfloat>

namespace swipetype {

struct Scorer::Impl {
    ScoringConfig config;

    /**
     * Euclidean distance between two NormalizedPoints (x,y only).
     */
    static float pointDistance(const NormalizedPoint& a, const NormalizedPoint& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

Scorer::Scorer() : pImpl(new Impl()) {}
Scorer::~Scorer() { delete pImpl; }

Scorer::Scorer(Scorer&& other) noexcept : pImpl(other.pImpl) {
    other.pImpl = nullptr;
}

Scorer& Scorer::operator=(Scorer&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

void Scorer::configure(const ScoringConfig& config) {
    if (pImpl) pImpl->config = config;
}

float Scorer::computeDTWDistance(const GesturePath& gesture,
                                  const GesturePath& idealPath) const {
    const int N = RESAMPLE_COUNT;

    if (static_cast<int>(gesture.points.size()) != N ||
        static_cast<int>(idealPath.points.size()) != N) {
        return FLT_MAX;
    }

    // Sakoe-Chiba band width
    int W = static_cast<int>(std::ceil(pImpl->config.dtwBandwidthRatio * static_cast<float>(N)));
    if (W < 1) W = 1;

    // Use two-row rolling DTW
    std::vector<float> dtw_prev(N, FLT_MAX);
    std::vector<float> dtw_curr(N, FLT_MAX);

    // Initialize first row
    dtw_prev[0] = Impl::pointDistance(gesture.points[0], idealPath.points[0]);
    for (int j = 1; j <= std::min(W, N - 1); ++j) {
        if (dtw_prev[j - 1] < FLT_MAX) {
            dtw_prev[j] = dtw_prev[j - 1] +
                Impl::pointDistance(gesture.points[0], idealPath.points[j]);
        }
    }

    // Fill row by row
    for (int i = 1; i < N; ++i) {
        std::fill(dtw_curr.begin(), dtw_curr.end(), FLT_MAX);

        int jMin = std::max(0, i - W);
        int jMax = std::min(N - 1, i + W);

        for (int j = jMin; j <= jMax; ++j) {
            float cost = Impl::pointDistance(gesture.points[i], idealPath.points[j]);

            float best = FLT_MAX;
            if (dtw_prev[j] < FLT_MAX)
                best = std::min(best, dtw_prev[j]);
            if (j > 0 && dtw_curr[j - 1] < FLT_MAX)
                best = std::min(best, dtw_curr[j - 1]);
            if (j > 0 && dtw_prev[j - 1] < FLT_MAX)
                best = std::min(best, dtw_prev[j - 1]);

            dtw_curr[j] = (best < FLT_MAX) ? (cost + best) : FLT_MAX;
        }

        std::swap(dtw_prev, dtw_curr);
    }

    float raw = dtw_prev[N - 1];
    if (raw >= FLT_MAX) return FLT_MAX;

    // Normalize by path length
    return raw / static_cast<float>(N);
}

float Scorer::computeConfidence(float dtwDistance, float maxDTWDistance,
                                 uint32_t frequency, uint32_t maxFrequency) const {
    float normalizedDTW = 1.0f;
    if (maxDTWDistance > 0.0f && dtwDistance < FLT_MAX) {
        normalizedDTW = std::min(1.0f, dtwDistance / maxDTWDistance);
    }

    float normalizedFreq = 0.0f;
    if (maxFrequency > 0) {
        normalizedFreq = std::min(1.0f, static_cast<float>(frequency) /
                                        static_cast<float>(maxFrequency));
    }

    float alpha = pImpl->config.frequencyWeight;
    float finalScore = (1.0f - alpha) * normalizedDTW + alpha * (1.0f - normalizedFreq);
    float confidence = 1.0f - std::max(0.0f, std::min(1.0f, finalScore));
    return confidence;
}

} // namespace swipetype
