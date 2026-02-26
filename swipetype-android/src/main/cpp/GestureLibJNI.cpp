#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>

#include "swipetype/GestureEngine.h"
#include "swipetype/GesturePath.h"
#include "swipetype/GestureCandidate.h"
#include "swipetype/KeyboardLayout.h"
#include "swipetype/SwipeTypeTypes.h"

#define LOG_TAG "SwipeTypeJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/**
 * @file GestureLibJNI.cpp
 * @brief JNI bridge between SwipeTypeEngine.java and swipetype-core C++ library.
 *
 * This file:
 * - Converts Java arrays to C++ data structures
 * - Manages GestureEngine lifetime via opaque handles (jlong pointers)
 * - Converts C++ results back to Java arrays/strings
 * - Handles all JNI exceptions to prevent native crashes from reaching Java
 *
 * Threading: All methods assume external synchronization (provided by
 * SwipeTypeEngine.java's synchronized blocks).
 */

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Build a KeyboardLayout from JNI arrays.
 */
static swipetype::KeyboardLayout buildLayout(
        JNIEnv* env,
        jfloatArray keyPositionsX, jfloatArray keyPositionsY,
        jfloatArray keyWidths, jfloatArray keyHeights,
        jintArray keyCodePoints, jint keyCount,
        jfloat layoutWidth, jfloat layoutHeight,
        jstring languageTag) {

    swipetype::KeyboardLayout layout;

    // Language tag
    if (languageTag != nullptr) {
        const char* langStr = env->GetStringUTFChars(languageTag, nullptr);
        if (langStr) {
            layout.languageTag = std::string(langStr);
            env->ReleaseStringUTFChars(languageTag, langStr);
        }
    }

    layout.layoutWidth  = layoutWidth;
    layout.layoutHeight = layoutHeight;

    if (keyCount <= 0) return layout;

    jfloat* xArr  = env->GetFloatArrayElements(keyPositionsX, nullptr);
    jfloat* yArr  = env->GetFloatArrayElements(keyPositionsY, nullptr);
    jfloat* wArr  = env->GetFloatArrayElements(keyWidths, nullptr);
    jfloat* hArr  = env->GetFloatArrayElements(keyHeights, nullptr);
    jint*   cpArr = env->GetIntArrayElements(keyCodePoints, nullptr);

    if (xArr && yArr && wArr && hArr && cpArr) {
        layout.keys.reserve(keyCount);
        for (jint i = 0; i < keyCount; ++i) {
            swipetype::KeyDescriptor key;
            key.centerX    = xArr[i];
            key.centerY    = yArr[i];
            key.width      = wArr[i];
            key.height     = hArr[i];
            key.codePoint  = cpArr[i];
            key.label      = (cpArr[i] > 0 && cpArr[i] < 128)
                ? std::string(1, static_cast<char>(cpArr[i]))
                : std::string();
            layout.keys.push_back(key);
        }
    }

    if (xArr)  env->ReleaseFloatArrayElements(keyPositionsX, xArr, JNI_ABORT);
    if (yArr)  env->ReleaseFloatArrayElements(keyPositionsY, yArr, JNI_ABORT);
    if (wArr)  env->ReleaseFloatArrayElements(keyWidths,     wArr, JNI_ABORT);
    if (hArr)  env->ReleaseFloatArrayElements(keyHeights,    hArr, JNI_ABORT);
    if (cpArr) env->ReleaseIntArrayElements(keyCodePoints,  cpArr, JNI_ABORT);

    return layout;
}

// ============================================================================
// JNI Method Implementations
// ============================================================================

extern "C" {

/**
 * Initialize the native engine with layout and dictionary file path.
 *
 * @return Native handle (cast GestureEngine* to jlong), or 0 on failure.
 */
JNIEXPORT jlong JNICALL
Java_dev_dettmer_swipetype_android_SwipeTypeEngine_nativeInit(
        JNIEnv* env, jclass /*clazz*/,
        jfloatArray keyPositionsX, jfloatArray keyPositionsY,
        jfloatArray keyWidths, jfloatArray keyHeights,
        jintArray keyCodePoints, jint keyCount,
        jfloat layoutWidth, jfloat layoutHeight,
        jstring languageTag, jstring dictPath) {

    try {
        swipetype::KeyboardLayout layout = buildLayout(
            env, keyPositionsX, keyPositionsY, keyWidths, keyHeights,
            keyCodePoints, keyCount, layoutWidth, layoutHeight, languageTag);

        const char* pathStr = env->GetStringUTFChars(dictPath, nullptr);
        std::string dictPathStr(pathStr ? pathStr : "");
        if (pathStr) env->ReleaseStringUTFChars(dictPath, pathStr);

        auto* engine = new swipetype::GestureEngine();
        if (!engine->init(layout, dictPathStr)) {
            LOGE("Failed to initialize engine: %s",
                 engine->getLastError().message.c_str());
            delete engine;
            return 0;
        }

        LOGI("Engine initialized with dictionary: %s", dictPathStr.c_str());
        return reinterpret_cast<jlong>(engine);
    } catch (...) {
        LOGE("Exception in nativeInit");
        return 0;
    }
}

/**
 * Initialize with dictionary data from memory (byte array).
 */
JNIEXPORT jlong JNICALL
Java_dev_dettmer_swipetype_android_SwipeTypeEngine_nativeInitWithData(
        JNIEnv* env, jclass /*clazz*/,
        jfloatArray keyPositionsX, jfloatArray keyPositionsY,
        jfloatArray keyWidths, jfloatArray keyHeights,
        jintArray keyCodePoints, jint keyCount,
        jfloat layoutWidth, jfloat layoutHeight,
        jstring languageTag, jbyteArray dictData) {

    try {
        swipetype::KeyboardLayout layout = buildLayout(
            env, keyPositionsX, keyPositionsY, keyWidths, keyHeights,
            keyCodePoints, keyCount, layoutWidth, layoutHeight, languageTag);

        jsize dataSize = env->GetArrayLength(dictData);
        jbyte* dataPtr = env->GetByteArrayElements(dictData, nullptr);

        auto* engine = new swipetype::GestureEngine();
        bool ok = engine->initWithData(layout,
            reinterpret_cast<const uint8_t*>(dataPtr),
            static_cast<size_t>(dataSize));

        env->ReleaseByteArrayElements(dictData, dataPtr, JNI_ABORT);

        if (!ok) {
            LOGE("Failed to initialize engine from memory: %s",
                 engine->getLastError().message.c_str());
            delete engine;
            return 0;
        }

        LOGI("Engine initialized from memory (%d bytes)", (int)dataSize);
        return reinterpret_cast<jlong>(engine);
    } catch (...) {
        LOGE("Exception in nativeInitWithData");
        return 0;
    }
}

/**
 * Recognize a gesture path and write results to output arrays.
 *
 * @return Number of candidates written, or -1 on error.
 */
JNIEXPORT jint JNICALL
Java_dev_dettmer_swipetype_android_SwipeTypeEngine_nativeRecognize(
        JNIEnv* env, jclass /*clazz*/,
        jlong handle,
        jfloatArray xCoords, jfloatArray yCoords, jlongArray timestamps,
        jint pointCount, jint maxCandidates,
        jobjectArray outWords, jfloatArray outScores, jintArray outFlags) {

    try {
        auto* engine = reinterpret_cast<swipetype::GestureEngine*>(handle);
        if (engine == nullptr) return -1;

        // Build raw path from JNI arrays
        jfloat* xArr = env->GetFloatArrayElements(xCoords, nullptr);
        jfloat* yArr = env->GetFloatArrayElements(yCoords, nullptr);
        jlong*  tArr = env->GetLongArrayElements(timestamps, nullptr);

        swipetype::RawGesturePath raw;
        raw.points.reserve(pointCount);
        if (xArr && yArr && tArr) {
            for (jint i = 0; i < pointCount; ++i) {
                raw.points.emplace_back(xArr[i], yArr[i], static_cast<int64_t>(tArr[i]));
            }
        }

        if (xArr) env->ReleaseFloatArrayElements(xCoords,    xArr, JNI_ABORT);
        if (yArr) env->ReleaseFloatArrayElements(yCoords,    yArr, JNI_ABORT);
        if (tArr) env->ReleaseLongArrayElements(timestamps,  tArr, JNI_ABORT);

        // Recognize
        auto candidates = engine->recognize(raw, maxCandidates);
        int count = static_cast<int>(
            std::min(candidates.size(), static_cast<size_t>(maxCandidates)));

        // Debug: log all candidates with scores
        LOGD("recognize: %d pts -> %d candidates", (int)pointCount, count);
        for (int i = 0; i < count; ++i) {
            LOGD("  #%d %-12s  conf=%.4f  dtw=%.4f  freq=%.4f",
                 i + 1,
                 candidates[i].word.c_str(),
                 candidates[i].confidence,
                 candidates[i].dtwScore,
                 candidates[i].frequencyScore);
        }

        // Write results back to Java arrays
        jfloat* scoreArr = env->GetFloatArrayElements(outScores, nullptr);
        jint*   flagArr  = env->GetIntArrayElements(outFlags, nullptr);

        for (int i = 0; i < count; ++i) {
            jstring word = env->NewStringUTF(candidates[i].word.c_str());
            env->SetObjectArrayElement(outWords, i, word);
            env->DeleteLocalRef(word);

            if (scoreArr) scoreArr[i] = candidates[i].confidence;
            if (flagArr)  flagArr[i]  = static_cast<jint>(candidates[i].sourceFlags);
        }

        if (scoreArr) env->ReleaseFloatArrayElements(outScores, scoreArr, 0);
        if (flagArr)  env->ReleaseIntArrayElements(outFlags,   flagArr,  0);

        return count;
    } catch (...) {
        LOGE("Exception in nativeRecognize");
        return -1;
    }
}

/**
 * Update keyboard layout without reloading dictionary.
 */
JNIEXPORT jboolean JNICALL
Java_dev_dettmer_swipetype_android_SwipeTypeEngine_nativeUpdateLayout(
        JNIEnv* env, jclass /*clazz*/,
        jlong handle,
        jfloatArray keyPositionsX, jfloatArray keyPositionsY,
        jfloatArray keyWidths, jfloatArray keyHeights,
        jintArray keyCodePoints, jint keyCount,
        jfloat layoutWidth, jfloat layoutHeight) {

    try {
        auto* engine = reinterpret_cast<swipetype::GestureEngine*>(handle);
        if (engine == nullptr) return JNI_FALSE;

        swipetype::KeyboardLayout layout = buildLayout(
            env, keyPositionsX, keyPositionsY, keyWidths, keyHeights,
            keyCodePoints, keyCount, layoutWidth, layoutHeight, nullptr);

        return engine->updateLayout(layout) ? JNI_TRUE : JNI_FALSE;
    } catch (...) {
        LOGE("Exception in nativeUpdateLayout");
        return JNI_FALSE;
    }
}

/**
 * Shut down the engine and free resources.
 */
JNIEXPORT void JNICALL
Java_dev_dettmer_swipetype_android_SwipeTypeEngine_nativeShutdown(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle) {
    auto* engine = reinterpret_cast<swipetype::GestureEngine*>(handle);
    if (engine != nullptr) {
        engine->shutdown();
        delete engine;
        LOGI("Native engine shut down");
    }
}

/**
 * Check if engine is initialized.
 */
JNIEXPORT jboolean JNICALL
Java_dev_dettmer_swipetype_android_SwipeTypeEngine_nativeIsInitialized(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle) {
    auto* engine = reinterpret_cast<swipetype::GestureEngine*>(handle);
    if (engine == nullptr) return JNI_FALSE;
    return engine->isInitialized() ? JNI_TRUE : JNI_FALSE;
}

// ============================================================================
// JNI Lifecycle
// ============================================================================

JNIEXPORT jint JNI_OnLoad(JavaVM* /*vm*/, void* /*reserved*/) {
    LOGI("libswipetype JNI loaded");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* /*vm*/, void* /*reserved*/) {
    LOGI("libswipetype JNI unloaded");
}

} // extern "C"
