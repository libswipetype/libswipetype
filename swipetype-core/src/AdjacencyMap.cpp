#include "swipetype/KeyboardLayout.h"
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <cctype>

namespace swipetype {

int32_t KeyboardLayout::findNearestKey(float x, float y) const {
    int32_t bestIndex = -1;
    float bestDist = FLT_MAX;

    for (size_t i = 0; i < keys.size(); ++i) {
        const KeyDescriptor& key = keys[i];
        if (!key.isCharacterKey()) continue;

        float dx = key.centerX - x;
        float dy = key.centerY - y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = static_cast<int32_t>(i);
        }
    }

    return bestIndex;
}

int32_t KeyboardLayout::findKeyByCodePoint(int32_t codePoint) const {
    // Lowercase the search code point if ASCII uppercase
    int32_t searchCp = codePoint;
    if (searchCp >= 'A' && searchCp <= 'Z') {
        searchCp = searchCp - 'A' + 'a';
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        int32_t kcp = keys[i].codePoint;
        if (kcp >= 'A' && kcp <= 'Z') {
            kcp = kcp - 'A' + 'a';
        }
        if (kcp == searchCp) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

bool KeyboardLayout::isValid() const {
    if (keys.empty()) return false;
    if (layoutWidth <= 0.0f || layoutHeight <= 0.0f) return false;

    for (const auto& key : keys) {
        if (key.isCharacterKey()) return true;
    }
    return false;
}

} // namespace swipetype
