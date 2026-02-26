#include "swipetype/PathProcessor.h"
#include "swipetype/SwipeTypeTypes.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>

namespace swipetype {

// ============================================================================
// Impl
// ============================================================================

struct PathProcessor::Impl {
    float minPointDistance = MIN_POINT_DISTANCE_DP;
    int resampleCount = RESAMPLE_COUNT;

    /**
     * Remove consecutive points that are closer than minPointDistance.
     * Always keeps the first and last points.
     */
    std::vector<GesturePoint> deduplicate(const std::vector<GesturePoint>& points) const {
        if (points.size() <= 2) return points;

        std::vector<GesturePoint> result;
        result.reserve(points.size());
        result.push_back(points[0]);

        for (size_t i = 1; i < points.size() - 1; ++i) {
            const GesturePoint& last = result.back();
            const GesturePoint& cur  = points[i];
            float dx = cur.x - last.x;
            float dy = cur.y - last.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= minPointDistance) {
                result.push_back(cur);
            }
        }

        // Always include the last point
        result.push_back(points.back());
        return result;
    }

    /**
     * Compute total arc length of a sequence of points.
     */
    float computeArcLength(const std::vector<GesturePoint>& points) const {
        float totalLength = 0.0f;
        for (size_t i = 1; i < points.size(); ++i) {
            float dx = points[i].x - points[i - 1].x;
            float dy = points[i].y - points[i - 1].y;
            totalLength += std::sqrt(dx * dx + dy * dy);
        }
        return totalLength;
    }

    /**
     * Resample to exactly resampleCount equidistant points along the path.
     * Based on $1 Unistroke Recognizer algorithm (Wobbrock et al., 2007).
     */
    std::vector<GesturePoint> resample(const std::vector<GesturePoint>& points) const {
        if (points.size() < 2) return points;

        float totalLen = computeArcLength(points);
        if (totalLen < 1e-6f) {
            // Degenerate path: return duplicated first point
            std::vector<GesturePoint> filled(resampleCount, points[0]);
            return filled;
        }

        float interval = totalLen / static_cast<float>(resampleCount - 1);
        std::vector<GesturePoint> result;
        result.reserve(resampleCount);
        result.push_back(points[0]);

        float D = 0.0f;
        size_t i = 1;
        // Copy to allow modification during traversal
        std::vector<GesturePoint> pts = points;

        while (i < pts.size() && static_cast<int>(result.size()) < resampleCount - 1) {
            float dx = pts[i].x - pts[i - 1].x;
            float dy = pts[i].y - pts[i - 1].y;
            float d = std::sqrt(dx * dx + dy * dy);

            if (D + d >= interval) {
                float t = (interval - D) / d;
                GesturePoint newPt;
                newPt.x = pts[i - 1].x + t * dx;
                newPt.y = pts[i - 1].y + t * dy;
                // Linear interpolation of timestamp
                newPt.timestamp = pts[i - 1].timestamp +
                    static_cast<int64_t>(t * static_cast<float>(pts[i].timestamp - pts[i - 1].timestamp));
                result.push_back(newPt);

                // Insert new point back and re-process current segment
                pts.insert(pts.begin() + static_cast<int>(i), newPt);
                D = 0.0f;
                ++i; // skip to segment starting at newPt
            } else {
                D += d;
                ++i;
            }
        }

        // Fill remaining (floating-point drift)
        while (static_cast<int>(result.size()) < resampleCount) {
            result.push_back(pts.back());
        }
        // Truncate if over
        result.resize(resampleCount);
        return result;
    }

    /**
     * Normalize coordinates to [0,1] bounding box preserving aspect ratio.
     */
    GesturePath normalizeBoundingBox(const std::vector<GesturePoint>& points,
                                     float totalArcLength) const {
        GesturePath result;

        if (points.empty()) return result;

        float minX = points[0].x, maxX = points[0].x;
        float minY = points[0].y, maxY = points[0].y;
        for (const auto& p : points) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }

        float width  = maxX - minX;
        float height = maxY - minY;

        // Degenerate: near-point path
        if (width < 0.001f && height < 0.001f) {
            result.points.resize(points.size(), NormalizedPoint(0.5f, 0.5f, 0.5f));
            result.aspectRatio = 1.0f;
            result.totalArcLength = totalArcLength;
            return result;
        }

        float scale = std::max(width, height);
        result.aspectRatio = (height > 0.001f) ? (width / height) : 1.0f;
        result.totalArcLength = totalArcLength;

        int64_t firstTs = points.front().timestamp;
        int64_t lastTs  = points.back().timestamp;
        float tsRange = static_cast<float>(lastTs - firstTs);

        result.points.reserve(points.size());
        for (const auto& p : points) {
            float nx = (p.x - minX) / scale;
            float ny = (p.y - minY) / scale;
            float nt = (tsRange > 0.0f)
                ? static_cast<float>(p.timestamp - firstTs) / tsRange
                : 0.5f;
            result.points.emplace_back(nx, ny, nt);
        }

        return result;
    }
};

// ============================================================================
// Public API
// ============================================================================

PathProcessor::PathProcessor() : pImpl(new Impl()) {}

PathProcessor::~PathProcessor() { delete pImpl; }

PathProcessor::PathProcessor(PathProcessor&& other) noexcept : pImpl(other.pImpl) {
    other.pImpl = nullptr;
}

PathProcessor& PathProcessor::operator=(PathProcessor&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

GesturePath PathProcessor::normalize(const RawGesturePath& raw,
                                      const KeyboardLayout& layout) const {
    if (raw.isEmpty()) return GesturePath();

    auto deduped = pImpl->deduplicate(raw.points);
    if (deduped.size() < 2) return GesturePath();

    float arcLen = pImpl->computeArcLength(deduped);
    auto resampled = pImpl->resample(deduped);
    auto path = pImpl->normalizeBoundingBox(resampled, arcLen);

    // Determine start/end keys from original (not resampled) endpoints
    path.startKeyIndex = layout.findNearestKey(raw.points.front().x, raw.points.front().y);
    path.endKeyIndex   = layout.findNearestKey(raw.points.back().x,  raw.points.back().y);

    return path;
}

void PathProcessor::setMinPointDistance(float distanceDp) {
    if (pImpl) pImpl->minPointDistance = distanceDp;
}

void PathProcessor::setResampleCount(int count) {
    if (pImpl && count >= 2) pImpl->resampleCount = count;
}

} // namespace swipetype
