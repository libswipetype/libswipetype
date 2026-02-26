#include "swipetype/IdealPathGenerator.h"
#include "swipetype/SwipeTypeTypes.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace swipetype {

struct IdealPathGenerator::Impl {
    KeyboardLayout layout;
    bool layoutSet = false;
    std::unordered_map<std::string, GesturePath> cache;

    static float euclidean(float x1, float y1, float x2, float y2) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    }

    // Resample helper: same algorithm as PathProcessor
    static std::vector<GesturePoint> resamplePoints(
            const std::vector<GesturePoint>& points, int count) {
        if (points.size() < 2 || count < 2) return points;

        float totalLen = 0.0f;
        for (size_t i = 1; i < points.size(); ++i) {
            totalLen += euclidean(points[i-1].x, points[i-1].y,
                                  points[i].x,   points[i].y);
        }
        if (totalLen < 1e-6f) {
            return std::vector<GesturePoint>(count, points[0]);
        }

        float interval = totalLen / static_cast<float>(count - 1);
        std::vector<GesturePoint> result;
        result.reserve(count);
        result.push_back(points[0]);

        float D = 0.0f;
        std::vector<GesturePoint> pts = points;
        size_t i = 1;

        while (i < pts.size() && static_cast<int>(result.size()) < count - 1) {
            float dx = pts[i].x - pts[i-1].x;
            float dy = pts[i].y - pts[i-1].y;
            float d  = std::sqrt(dx * dx + dy * dy);

            if (D + d >= interval) {
                float t = (interval - D) / d;
                GesturePoint np;
                np.x = pts[i-1].x + t * dx;
                np.y = pts[i-1].y + t * dy;
                np.timestamp = pts[i-1].timestamp +
                    static_cast<int64_t>(t * static_cast<float>(pts[i].timestamp - pts[i-1].timestamp));
                result.push_back(np);
                pts.insert(pts.begin() + static_cast<int>(i), np);
                D = 0.0f;
                ++i;
            } else {
                D += d;
                ++i;
            }
        }
        while (static_cast<int>(result.size()) < count) {
            result.push_back(pts.back());
        }
        result.resize(count);
        return result;
    }

    // Normalize to [0,1] bounding box
    static GesturePath normalizeBB(const std::vector<GesturePoint>& points, float arcLen) {
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

        if (width < 0.001f && height < 0.001f) {
            result.points.assign(points.size(), NormalizedPoint(0.5f, 0.5f, 0.5f));
            result.aspectRatio = 1.0f;
            result.totalArcLength = arcLen;
            return result;
        }

        float scale = std::max(width, height);
        result.aspectRatio = (height > 0.001f) ? (width / height) : 1.0f;
        result.totalArcLength = arcLen;

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

    /**
     * Generate the ideal path for a word by connecting key centers.
     */
    GesturePath generate(const std::string& word) const {
        if (!layoutSet) return GesturePath();

        std::vector<GesturePoint> keyPoints;
        int32_t prevKeyIdx = -1;
        int charIdx = 0;

        for (char ch : word) {
            int cp = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(ch)));
            int32_t keyIdx = layout.findKeyByCodePoint(cp);
            if (keyIdx < 0) continue;

            // Skip duplicate consecutive key (repeated letters in swipe typing)
            if (keyIdx == prevKeyIdx) continue;

            const KeyDescriptor& key = layout.keys[static_cast<size_t>(keyIdx)];
            GesturePoint pt;
            pt.x = key.centerX;
            pt.y = key.centerY;
            // Synthetic timestamp: 100ms per character
            pt.timestamp = static_cast<int64_t>(charIdx) * 100LL;
            keyPoints.push_back(pt);
            prevKeyIdx = keyIdx;
            ++charIdx;
        }

        if (keyPoints.size() < 2) return GesturePath();

        // Compute arc length through key centers
        float arcLen = 0.0f;
        for (size_t i = 1; i < keyPoints.size(); ++i) {
            arcLen += euclidean(keyPoints[i-1].x, keyPoints[i-1].y,
                                keyPoints[i].x,   keyPoints[i].y);
        }

        // Resample and normalize
        auto resampled = resamplePoints(keyPoints, RESAMPLE_COUNT);
        auto path = normalizeBB(resampled, arcLen);

        // Set start/end key indices
        if (!keyPoints.empty()) {
            // Find the first/last key indices again
            int cp0 = -1, cpN = -1;
            for (char ch : word) {
                int cp = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(ch)));
                if (layout.findKeyByCodePoint(cp) >= 0) { cp0 = cp; break; }
            }
            for (int wi = static_cast<int>(word.size()) - 1; wi >= 0; --wi) {
                int cp = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(word[static_cast<size_t>(wi)])));
                if (layout.findKeyByCodePoint(cp) >= 0) { cpN = cp; break; }
            }
            path.startKeyIndex = (cp0 >= 0) ? layout.findKeyByCodePoint(cp0) : -1;
            path.endKeyIndex   = (cpN >= 0) ? layout.findKeyByCodePoint(cpN) : -1;
        }

        return path;
    }
};

IdealPathGenerator::IdealPathGenerator() : pImpl(new Impl()) {}
IdealPathGenerator::~IdealPathGenerator() { delete pImpl; }

IdealPathGenerator::IdealPathGenerator(IdealPathGenerator&& other) noexcept
    : pImpl(other.pImpl) { other.pImpl = nullptr; }

IdealPathGenerator& IdealPathGenerator::operator=(IdealPathGenerator&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

void IdealPathGenerator::setLayout(const KeyboardLayout& layout) {
    if (pImpl) {
        pImpl->layout = layout;
        pImpl->layoutSet = true;
        pImpl->cache.clear();
    }
}

GesturePath IdealPathGenerator::getIdealPath(const std::string& word) {
    if (!pImpl || !pImpl->layoutSet) return GesturePath();

    // Lowercase the word for cache key
    std::string key;
    key.reserve(word.size());
    for (char ch : word) {
        key.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch))));
    }

    auto it = pImpl->cache.find(key);
    if (it != pImpl->cache.end()) {
        return it->second;
    }

    GesturePath path = pImpl->generate(key);
    pImpl->cache[key] = path;
    return path;
}

void IdealPathGenerator::pregenerate(const std::vector<std::string>& words) {
    for (const auto& word : words) {
        getIdealPath(word);
    }
}

void IdealPathGenerator::clearCache() {
    if (pImpl) pImpl->cache.clear();
}

size_t IdealPathGenerator::cacheSize() const {
    return pImpl ? pImpl->cache.size() : 0;
}

} // namespace swipetype
