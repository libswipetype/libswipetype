#include "swipetype/GestureEngine.h"
#include "swipetype/PathProcessor.h"
#include "swipetype/IdealPathGenerator.h"
#include "swipetype/Scorer.h"
#include "swipetype/DictionaryLoader.h"
#include "swipetype/SwipeTypeTypes.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>

#ifdef __ANDROID__
#include <android/log.h>
#define ST_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "SwipeTypeCore", __VA_ARGS__)
#else
#define ST_LOGD(...) do { std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while(0)
#endif

namespace swipetype {

struct GestureEngine::Impl {
    PathProcessor pathProcessor;
    IdealPathGenerator idealPathGen;
    Scorer scorer;
    DictionaryLoader dictLoader;
    KeyboardLayout layout;
    ScoringConfig config;
    ErrorCallback errorCallback;
    ErrorInfo lastError;
    bool initialized = false;

    void reportError(ErrorCode code, const std::string& msg) {
        lastError = {code, msg};
        if (errorCallback) {
            errorCallback(lastError);
        }
    }

    /**
     * Estimate word length by counting distinct key transitions along the raw path.
     *
     * Walks the raw gesture points, snapping each to the nearest key center.
     * When the nearest key changes, that counts as one key transition.
     * The estimated word length is the number of distinct keys visited.
     *
     * This replaces the previous arc-length heuristic which overestimated
     * zigzag words (e.g. "hello" estimated as 17+ chars instead of 5).
     */
    float estimateWordLengthByKeyTransitions(const RawGesturePath& rawPath) const {
        if (rawPath.points.size() < 2) return 1.0f;

        int32_t prevKey = -1;
        int transitions = 0;
        for (const auto& pt : rawPath.points) {
            int32_t key = layout.findNearestKey(pt.x, pt.y);
            if (key >= 0 && key != prevKey) {
                transitions++;
                prevKey = key;
            }
        }
        return std::max(1.0f, static_cast<float>(transitions));
    }
};

GestureEngine::GestureEngine() : pImpl(new Impl()) {}
GestureEngine::~GestureEngine() { delete pImpl; }

GestureEngine::GestureEngine(GestureEngine&& other) noexcept
    : pImpl(other.pImpl) { other.pImpl = nullptr; }

GestureEngine& GestureEngine::operator=(GestureEngine&& other) noexcept {
    if (this != &other) {
        delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

bool GestureEngine::init(const KeyboardLayout& layout, const std::string& dictPath) {
    if (!pImpl) return false;

    if (!layout.isValid()) {
        pImpl->reportError(ErrorCode::LAYOUT_INVALID, "KeyboardLayout is invalid");
        return false;
    }

    if (!pImpl->dictLoader.load(dictPath)) {
        auto err = pImpl->dictLoader.getLastError();
        pImpl->reportError(err.code, err.message);
        return false;
    }

    pImpl->layout = layout;
    pImpl->idealPathGen.setLayout(layout);
    pImpl->scorer.configure(pImpl->config);
    pImpl->initialized = true;
    return true;
}

bool GestureEngine::initWithData(const KeyboardLayout& layout,
                                  const uint8_t* dictData, size_t dictSize) {
    if (!pImpl) return false;

    if (!layout.isValid()) {
        pImpl->reportError(ErrorCode::LAYOUT_INVALID, "KeyboardLayout is invalid");
        return false;
    }

    if (!pImpl->dictLoader.loadFromMemory(dictData, dictSize)) {
        auto err = pImpl->dictLoader.getLastError();
        pImpl->reportError(err.code, err.message);
        return false;
    }

    pImpl->layout = layout;
    pImpl->idealPathGen.setLayout(layout);
    pImpl->scorer.configure(pImpl->config);
    pImpl->initialized = true;
    return true;
}

std::vector<GestureCandidate> GestureEngine::recognize(const RawGesturePath& rawPath,
                                                        int maxCandidates) {
    std::vector<GestureCandidate> results;
    if (!pImpl) return results;

    // Step 0: Validation
    if (!pImpl->initialized) {
        pImpl->reportError(ErrorCode::ENGINE_NOT_INITIALIZED, "Engine not initialized");
        return results;
    }
    maxCandidates = std::max(1, std::min(maxCandidates, MAX_MAX_CANDIDATES));
    if (rawPath.isEmpty()) {
        pImpl->reportError(ErrorCode::PATH_TOO_SHORT, "Gesture path too short");
        return results;
    }

    // Step 1: Path Normalization
    GesturePath normalizedPath = pImpl->pathProcessor.normalize(rawPath, pImpl->layout);
    if (!normalizedPath.isValid()) return results;

    // Step 2: Determine start/end key characters
    char startChar = 0, endChar = 0;
    bool hasStartEnd = false;

    if (normalizedPath.startKeyIndex >= 0 &&
        normalizedPath.startKeyIndex < static_cast<int>(pImpl->layout.keys.size()) &&
        normalizedPath.endKeyIndex >= 0 &&
        normalizedPath.endKeyIndex < static_cast<int>(pImpl->layout.keys.size())) {

        int32_t cpS = pImpl->layout.keys[static_cast<size_t>(normalizedPath.startKeyIndex)].codePoint;
        int32_t cpE = pImpl->layout.keys[static_cast<size_t>(normalizedPath.endKeyIndex)].codePoint;

        if (cpS >= 'a' && cpS <= 'z') startChar = static_cast<char>(cpS);
        else if (cpS >= 'A' && cpS <= 'Z') startChar = static_cast<char>(std::tolower(cpS));

        if (cpE >= 'a' && cpE <= 'z') endChar = static_cast<char>(cpE);
        else if (cpE >= 'A' && cpE <= 'Z') endChar = static_cast<char>(std::tolower(cpE));

        hasStartEnd = (startChar != 0 && endChar != 0);
    }

    ST_LOGD("PIPELINE: startKey='%c' endKey='%c' hasStartEnd=%d  rawPts=%zu",
            startChar ? startChar : '?', endChar ? endChar : '?',
            (int)hasStartEnd, rawPath.points.size());

    // Step 3: Candidate Filtering
    std::vector<const DictionaryEntry*> dictEntries;
    if (hasStartEnd) {
        dictEntries = pImpl->dictLoader.getEntriesWithStartEnd(startChar, endChar);
    }
    if (dictEntries.empty() && startChar != 0) {
        dictEntries = pImpl->dictLoader.getEntriesStartingWith(startChar);
    }
    if (dictEntries.empty()) {
        // Last resort: score a sample of all entries
        const auto& all = pImpl->dictLoader.getAllEntries();
        for (const auto& e : all) {
            dictEntries.push_back(&e);
        }
    }

    // Apply word-length filter (key-transition count, not arc length)
    float estimatedLen = pImpl->estimateWordLengthByKeyTransitions(rawPath);
    float tol = pImpl->config.lengthFilterTolerance;

    std::vector<const DictionaryEntry*> filtered;
    filtered.reserve(dictEntries.size());
    for (const auto* entry : dictEntries) {
        float wordLen = static_cast<float>(entry->word.size());
        if (std::abs(wordLen - estimatedLen) <= tol) {
            filtered.push_back(entry);
        }
    }
    ST_LOGD("PIPELINE: estWordLen=%.1f  dictEntries=%zu  afterLenFilter=%zu  tol=%.1f",
            estimatedLen, dictEntries.size(), filtered.size(), tol);

    // If filter removed everything, fall back to unfiltered
    if (filtered.empty()) {
        ST_LOGD("PIPELINE: length filter removed ALL — falling back to unfiltered (%zu)",
                dictEntries.size());
        filtered = dictEntries;
    }

    // Step 4: Scoring
    struct ScoredEntry {
        const DictionaryEntry* entry;
        float dtwDistance;
    };
    std::vector<ScoredEntry> scored;
    scored.reserve(filtered.size());

    for (const auto* entry : filtered) {
        GesturePath ideal = pImpl->idealPathGen.getIdealPath(entry->word);
        if (!ideal.isValid()) continue;

        float dtw = pImpl->scorer.computeDTWDistance(normalizedPath, ideal);
        scored.push_back({entry, dtw});
    }

    if (scored.empty()) return results;

    // Step 5: Max DTW normalization.
    // For RANKING multiple candidates: use the actual max candidate DTW so
    // shape differences are properly reflected. A small safety floor prevents
    // division by zero but never compresses real differences.
    // For SINGLE candidate confidence: use the larger maxDTWFloor so the
    // candidate gets a meaningful absolute confidence value.
    float rawMaxDTW = 0.0f;
    float minCandDTW = FLT_MAX;
    for (const auto& s : scored) {
        if (s.dtwDistance < FLT_MAX) {
            if (s.dtwDistance > rawMaxDTW) rawMaxDTW = s.dtwDistance;
            if (s.dtwDistance < minCandDTW) minCandDTW = s.dtwDistance;
        }
    }
    float maxDTW;
    if (scored.size() <= 1) {
        maxDTW = std::max(rawMaxDTW, pImpl->config.maxDTWFloor);
    } else {
        maxDTW = std::max(rawMaxDTW, 0.01f);
    }

    // Step 5b: Adaptive frequency weight.
    // Uses the RAW DTW range (before any floor) to detect when candidates
    // have similar shape scores. When the spread is small, frequency weight
    // is scaled down proportionally so shape dominates the ranking.
    float rawRange = (minCandDTW < FLT_MAX) ? (rawMaxDTW - minCandDTW) : 0.0f;
    float effectiveAlpha = pImpl->config.frequencyWeight;
    if (scored.size() > 1 && rawRange < 0.5f) {
        effectiveAlpha *= std::max(0.1f, rawRange / 0.5f);
    }

    ST_LOGD("PIPELINE: scored=%zu  minDTW=%.4f  rawMaxDTW=%.4f  maxDTW=%.4f  rawRange=%.4f  alpha=%.3f(eff=%.3f)",
            scored.size(), minCandDTW, rawMaxDTW, maxDTW, rawRange,
            pImpl->config.frequencyWeight, effectiveAlpha);

    // Step 6: Compute confidence scores (inlined with adaptive alpha)
    uint32_t maxFreq = pImpl->dictLoader.getMaxFrequency();
    results.reserve(scored.size());

    for (const auto& s : scored) {
        float normalizedDTW = 1.0f;
        if (maxDTW > 0.0f && s.dtwDistance < FLT_MAX) {
            normalizedDTW = std::min(1.0f, s.dtwDistance / maxDTW);
        }

        float normalizedFreq = 0.0f;
        if (maxFreq > 0) {
            normalizedFreq = std::min(1.0f,
                static_cast<float>(s.entry->frequency) / static_cast<float>(maxFreq));
        }

        float finalScore = (1.0f - effectiveAlpha) * normalizedDTW
                         + effectiveAlpha * (1.0f - normalizedFreq);
        float confidence = 1.0f - std::max(0.0f, std::min(1.0f, finalScore));

        GestureCandidate candidate;
        candidate.word = s.entry->word;
        candidate.confidence = confidence;
        candidate.sourceFlags = SOURCE_MAIN_DICT;
        candidate.dtwScore = s.dtwDistance;
        candidate.frequencyScore = (maxFreq > 0)
            ? static_cast<float>(s.entry->frequency) / static_cast<float>(maxFreq)
            : 0.0f;
        results.push_back(std::move(candidate));
    }

    // Step 7: Sort and prune
    std::sort(results.begin(), results.end(),
        [](const GestureCandidate& a, const GestureCandidate& b) {
            return a.confidence > b.confidence;
        });

    if (static_cast<int>(results.size()) > maxCandidates) {
        results.resize(static_cast<size_t>(maxCandidates));
    }

    return results;
}

void GestureEngine::shutdown() {
    if (pImpl) {
        pImpl->dictLoader.unload();
        pImpl->idealPathGen.clearCache();
        pImpl->initialized = false;
    }
}

bool GestureEngine::isInitialized() const {
    return pImpl && pImpl->initialized;
}

bool GestureEngine::updateLayout(const KeyboardLayout& layout) {
    if (!pImpl || !pImpl->initialized) return false;
    if (!layout.isValid()) {
        pImpl->reportError(ErrorCode::LAYOUT_INVALID, "KeyboardLayout is invalid");
        return false;
    }
    pImpl->layout = layout;
    pImpl->idealPathGen.setLayout(layout); // clears cache
    return true;
}

void GestureEngine::configure(const ScoringConfig& config) {
    if (pImpl) {
        pImpl->config = config;
        pImpl->scorer.configure(config);
    }
}

void GestureEngine::setErrorCallback(ErrorCallback callback) {
    if (pImpl) pImpl->errorCallback = std::move(callback);
}

ErrorInfo GestureEngine::getLastError() const {
    return pImpl ? pImpl->lastError : ErrorInfo();
}

} // namespace swipetype
