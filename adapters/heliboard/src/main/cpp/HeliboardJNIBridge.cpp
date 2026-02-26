#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>

/**
 * @file HeliboardJNIBridge.cpp
 * @brief JNI bridge that exposes the exact method signatures HeliBoard expects.
 *
 * This file is used for the "drop-in .so replacement" integration strategy.
 * It registers native methods with the same class paths and signatures that
 * HeliBoard's BinaryDictionary.java, ProximityInfo.java, and
 * DicTraverseSession.java expect.
 *
 * IMPORTANT: This bridge is for FUTURE use when we want to create a standalone
 * .so that HeliBoard can load without source code changes. For the MVP,
 * integration is done via HeliboardSwipeTypeAdapter.java at the Java level.
 *
 * HeliBoard's JNI methods are registered against these Java classes:
 * - com.android.inputmethod.latin.BinaryDictionary
 * - com.android.inputmethod.keyboard.ProximityInfo
 * - com.android.inputmethod.latin.DicTraverseSession
 *
 * The critical gesture-related method is getSuggestionsNative, which
 * HeliBoard calls with isGesture=true when the user is swiping.
 */

#define LOG_TAG "HeliboardBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================================
// HeliBoard's Expected JNI Signatures
// ============================================================================

/*
 * The following documents HeliBoard's native method signatures.
 * Source: com.android.inputmethod.latin.BinaryDictionary
 *
 * getSuggestionsNative signature:
 *   (JJJ[I[I[I[I[II[I[[I[ZI[I[I[I[I[I[I[F)V
 *
 * Parameters:
 *   jlong dict              — native dictionary handle
 *   jlong proximityInfo     — native ProximityInfo handle
 *   jlong traverseSession   — native DicTraverseSession handle
 *   jintArray xCoordinates  — touch X coords (pixels), or key indices for typing
 *   jintArray yCoordinates  — touch Y coords (pixels)
 *   jintArray times         — timestamps (ms)
 *   jintArray pointerIds    — multi-touch pointer IDs
 *   jintArray inputCodePoints — char codes for typing mode
 *   jint inputSize          — number of input points/chars
 *   jintArray suggestOptions — NativeSuggestOptions array (index 0 = isGesture)
 *   jobjectArray prevWordCodePointArrays — previous words for n-gram
 *   jbooleanArray isBeginningOfSentenceArray — sentence boundary flags
 *   jint prevWordCount      — number of previous words
 *   jintArray outSuggestionCount — output: number of suggestions [1]
 *   jintArray outCodePoints  — output: suggestion chars (flattened)
 *   jintArray outScores      — output: suggestion scores
 *   jintArray outSpaceIndices — output: space indices in multi-word
 *   jintArray outTypes       — output: suggestion types
 *   jintArray outAutoCommitFirstWordConfidence — output: auto-commit confidence [1]
 *   jfloatArray inOutWeightOfLangModelVsSpatialModel — weight param [1]
 */

extern "C" {

/**
 * HeliBoard's getSuggestionsNative — the critical gesture recognition entry point.
 *
 * When suggestOptions[0] (isGesture) is 1:
 *   - xCoordinates/yCoordinates contain touch trail coordinates
 *   - inputSize is the number of touch points
 *   - We intercept and route to our SwipeTypeEngine
 *
 * When suggestOptions[0] is 0:
 *   - This is a typing suggestion request
 *   - We would need to forward to HeliBoard's original implementation
 *   - For the drop-in replacement, we need the original dictionary logic too
 *
 * OUTPUT CONTRACT (what HeliBoard expects):
 *   - outSuggestionCount[0] = number of suggestions
 *   - outCodePoints: each suggestion is MAX_WORD_LENGTH (64) ints, packed sequentially
 *     e.g., suggestion 0 at [0..63], suggestion 1 at [64..127], etc.
 *     Each int is a Unicode code point. Terminated by 0.
 *   - outScores: score per suggestion (higher = better, range 0-2000000000)
 *   - outTypes: suggestion type flags (0 = regular word)
 */
static void heliboard_getSuggestionsNative(
        JNIEnv* env, jclass /*clazz*/,
        jlong /*dict*/, jlong /*proximityInfo*/, jlong /*traverseSession*/,
        jintArray xCoordinates, jintArray yCoordinates,
        jintArray times, jintArray /*pointerIds*/,
        jintArray /*inputCodePoints*/, jint inputSize,
        jintArray suggestOptions,
        jobjectArray /*prevWordCodePointArrays*/,
        jbooleanArray /*isBeginningOfSentenceArray*/, jint /*prevWordCount*/,
        jintArray outSuggestionCount, jintArray /*outCodePoints*/,
        jintArray /*outScores*/, jintArray /*outSpaceIndices*/,
        jintArray /*outTypes*/, jintArray /*outAutoCommitFirstWordConfidence*/,
        jfloatArray /*inOutWeightOfLangModelVsSpatialModel*/) {

    // Check gesture mode flag
    jint* opts = env->GetIntArrayElements(suggestOptions, nullptr);
    bool isGesture = (opts[0] == 1);
    env->ReleaseIntArrayElements(suggestOptions, opts, JNI_ABORT);

    if (!isGesture) {
        // Typing mode — not handled by this bridge. Zero results.
        jint* count = env->GetIntArrayElements(outSuggestionCount, nullptr);
        count[0] = 0;
        env->ReleaseIntArrayElements(outSuggestionCount, count, 0);
        return;
    }

    LOGI("getSuggestionsNative: gesture input, %d points", inputSize);

    // TODO: Route to SwipeTypeEngine for gesture recognition.
    // For now, return 0 results as a safe placeholder.
    // Full implementation requires:
    //   1. A global/static SwipeTypeEngine instance initialized at JNI_OnLoad
    //   2. Reading x/y/t arrays and building GesturePoint vector
    //   3. Calling engine.recognize() and writing results to output arrays
    (void)xCoordinates;
    (void)yCoordinates;
    (void)times;

    jint* count = env->GetIntArrayElements(outSuggestionCount, nullptr);
    count[0] = 0;
    env->ReleaseIntArrayElements(outSuggestionCount, count, 0);
}

/**
 * HeliBoard's setProximityInfoNative.
 *
 * This is called when the keyboard layout changes. We capture the key positions
 * to update our engine's KeyboardLayout.
 *
 * @return Native handle for ProximityInfo (we can store our own data here).
 */
static jlong heliboard_setProximityInfoNative(
        JNIEnv* env, jclass /*clazz*/,
        jint displayWidth, jint displayHeight,
        jint /*gridWidth*/, jint /*gridHeight*/,
        jint /*mostCommonKeyWidth*/, jint /*mostCommonKeyHeight*/,
        jintArray /*proximityCharsArray*/,
        jint keyCount,
        jintArray keyXCoordinates, jintArray keyYCoordinates,
        jintArray keyWidths, jintArray keyHeights,
        jintArray keyCharCodes,
        jfloatArray /*sweetSpotCenterXs*/, jfloatArray /*sweetSpotCenterYs*/,
        jfloatArray /*sweetSpotRadii*/) {

    LOGI("setProximityInfoNative: %d keys, display %dx%d", keyCount, displayWidth, displayHeight);

    // Read key arrays
    jint* xArr   = env->GetIntArrayElements(keyXCoordinates, nullptr);
    jint* yArr   = env->GetIntArrayElements(keyYCoordinates, nullptr);
    jint* wArr   = env->GetIntArrayElements(keyWidths, nullptr);
    jint* hArr   = env->GetIntArrayElements(keyHeights, nullptr);
    jint* cArr   = env->GetIntArrayElements(keyCharCodes, nullptr);

    // TODO: Store keyboard layout data and update SwipeTypeEngine.
    // For now, we log and release.
    (void)displayWidth; (void)displayHeight; (void)keyCount;
    (void)xArr; (void)yArr; (void)wArr; (void)hArr; (void)cArr;

    env->ReleaseIntArrayElements(keyXCoordinates, xArr, JNI_ABORT);
    env->ReleaseIntArrayElements(keyYCoordinates, yArr, JNI_ABORT);
    env->ReleaseIntArrayElements(keyWidths, wArr, JNI_ABORT);
    env->ReleaseIntArrayElements(keyHeights, hArr, JNI_ABORT);
    env->ReleaseIntArrayElements(keyCharCodes, cArr, JNI_ABORT);

    return 0; // placeholder handle
}

/**
 * HeliBoard's releaseProximityInfoNative.
 */
static void heliboard_releaseProximityInfoNative(JNIEnv* /*env*/, jclass /*clazz*/, jlong /*info*/) {
    LOGI("releaseProximityInfoNative");
}

// ============================================================================
// JNI Registration (for drop-in .so replacement)
// ============================================================================

/*
 * Method registration tables.
 * These map Java method names to our C++ implementations.
 *
 * For the factory-based approach (recommended MVP), this registration
 * is NOT needed — HeliBoard's built-in library handles all JNI registration,
 * and we only inject a GestureSuggestPolicy.
 *
 * Uncomment when implementing Strategy A (drop-in .so replacement):
 */

// static const JNINativeMethod sBinaryDictionaryMethods[] = {
//     {"getSuggestionsNative", "(JJJ[I[I[I[I[II[I[[I[ZI[I[I[I[I[I[I[F)V",
//      (void*)heliboard_getSuggestionsNative},
// };
//
// static const JNINativeMethod sProximityInfoMethods[] = {
//     {"setProximityInfoNative", "(IIIIII[II[I[I[I[I[I[F[F[F)J",
//      (void*)heliboard_setProximityInfoNative},
//     {"releaseProximityInfoNative", "(J)V",
//      (void*)heliboard_releaseProximityInfoNative},
// };
//
// JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
//     JNIEnv* env;
//     if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return -1;
//
//     jclass binaryDictClass = env->FindClass("com/android/inputmethod/latin/BinaryDictionary");
//     env->RegisterNatives(binaryDictClass, sBinaryDictionaryMethods,
//                          sizeof(sBinaryDictionaryMethods)/sizeof(sBinaryDictionaryMethods[0]));
//
//     jclass proximityInfoClass = env->FindClass(
//         "com/android/inputmethod/keyboard/ProximityInfo");
//     env->RegisterNatives(proximityInfoClass, sProximityInfoMethods,
//                          sizeof(sProximityInfoMethods)/sizeof(sProximityInfoMethods[0]));
//
//     LOGI("HeliBoard glide bridge loaded");
//     return JNI_VERSION_1_6;
// }

// Stub JNI_OnLoad for library validity (remove when using method registration above)
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    LOGI("HeliboardJNIBridge loaded (MVP stub)");
    return JNI_VERSION_1_6;
}

} // extern "C"
