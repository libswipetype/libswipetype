package dev.dettmer.swipetype.android;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.InputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Main Android-facing class for gesture recognition.
 *
 * <p>This is the primary entry point for keyboard apps using libswipetype.
 * It manages the native engine lifecycle, handles JNI calls, and delivers results
 * via the {@link SwipeTypeAdapter} interface.</p>
 *
 * <h3>Usage:</h3>
 * <pre>{@code
 * SwipeTypeEngine engine = new SwipeTypeEngine();
 * engine.init(context, myAdapter);
 * engine.loadDictionary("en-US", dictInputStream);
 *
 * // When user swipes:
 * List<GesturePoint> points = collectTouchPoints();
 * engine.processGesture(points);
 * // Results delivered via adapter.onCandidatesReady()
 *
 * // When done:
 * engine.shutdown();
 * }</pre>
 *
 * <p>Thread safety: All public methods are synchronized on the engine instance.
 * {@code processGesture()} can be called from any thread; results are delivered
 * on the caller's thread via the adapter callback.</p>
 */
public class SwipeTypeEngine {

    private static final String TAG = "SwipeTypeEngine";
    private static final String NATIVE_LIB = "glide_jni";
    private static final int DEFAULT_MAX_CANDIDATES = 8;

    private long nativeHandle = 0;
    private SwipeTypeAdapter adapter;
    private boolean initialized = false;
    private Context appContext;

    static {
        System.loadLibrary(NATIVE_LIB);
    }

    // ========================================================================
    // Native method declarations
    // ========================================================================

    private static native long nativeInit(
            float[] keyPositionsX, float[] keyPositionsY,
            float[] keyWidths, float[] keyHeights,
            int[] keyCodePoints, int keyCount,
            float layoutWidth, float layoutHeight,
            String languageTag, String dictPath);

    private static native long nativeInitWithData(
            float[] keyPositionsX, float[] keyPositionsY,
            float[] keyWidths, float[] keyHeights,
            int[] keyCodePoints, int keyCount,
            float layoutWidth, float layoutHeight,
            String languageTag, byte[] dictData);

    private static native int nativeRecognize(
            long handle,
            float[] xCoords, float[] yCoords, long[] timestamps,
            int pointCount, int maxCandidates,
            String[] outWords, float[] outScores, int[] outFlags);

    private static native boolean nativeUpdateLayout(
            long handle,
            float[] keyPositionsX, float[] keyPositionsY,
            float[] keyWidths, float[] keyHeights,
            int[] keyCodePoints, int keyCount,
            float layoutWidth, float layoutHeight);

    private static native void nativeShutdown(long handle);

    private static native boolean nativeIsInitialized(long handle);

    // ========================================================================
    // Public API
    // ========================================================================

    /**
     * Initialize the engine with an adapter.
     *
     * @param context  Android context. Application context is stored internally.
     * @param adapter  Adapter implementation. Must not be null.
     * @throws IllegalArgumentException if adapter is null
     */
    public synchronized void init(Context context, SwipeTypeAdapter adapter) {
        if (adapter == null) {
            throw new IllegalArgumentException("SwipeTypeAdapter must not be null");
        }
        this.appContext = context.getApplicationContext();
        this.adapter = adapter;
        Log.i(TAG, "SwipeTypeEngine initialized (dictionary not yet loaded)");
    }

    /**
     * Load a dictionary from an InputStream.
     *
     * <p>Copies the dictionary to the app's cache directory, then initializes
     * the native engine with the keyboard layout from the adapter.</p>
     *
     * @param languageTag  BCP 47 language tag (e.g., "en-US")
     * @param dictStream   InputStream containing the .glide dictionary file
     * @return true on success
     */
    public synchronized boolean loadDictionary(String languageTag, InputStream dictStream) {
        if (appContext == null || adapter == null) {
            Log.e(TAG, "loadDictionary called before init()");
            return false;
        }

        // Step 1: Copy stream to cache file
        File dictFile = new File(appContext.getCacheDir(), languageTag + ".glide");
        try {
            FileOutputStream fos = new FileOutputStream(dictFile);
            byte[] buffer = new byte[8192];
            int bytesRead;
            while ((bytesRead = dictStream.read(buffer)) != -1) {
                fos.write(buffer, 0, bytesRead);
            }
            fos.close();
        } catch (IOException e) {
            Log.e(TAG, "Failed to write dictionary to cache: " + e.getMessage());
            adapter.onError(SwipeTypeError.DICT_NOT_FOUND);
            return false;
        }

        // Step 2: Get layout from adapter
        KeyboardLayoutDescriptor layout = adapter.getKeyboardLayout();
        if (layout == null || layout.keys == null || layout.keys.isEmpty()) {
            adapter.onError(SwipeTypeError.LAYOUT_INVALID);
            return false;
        }

        // Step 3: Convert layout to native arrays
        int keyCount = layout.keys.size();
        float[] keyX   = new float[keyCount];
        float[] keyY   = new float[keyCount];
        float[] keyW   = new float[keyCount];
        float[] keyH   = new float[keyCount];
        int[]   keyCps = new int[keyCount];

        for (int i = 0; i < keyCount; i++) {
            KeyboardLayoutDescriptor.KeyInfo k = layout.keys.get(i);
            keyX[i]   = k.centerX;
            keyY[i]   = k.centerY;
            keyW[i]   = k.width;
            keyH[i]   = k.height;
            keyCps[i] = k.codePoint;
        }

        // Step 4: Initialize native engine
        nativeHandle = nativeInit(keyX, keyY, keyW, keyH, keyCps, keyCount,
                layout.layoutWidth, layout.layoutHeight,
                languageTag, dictFile.getAbsolutePath());

        if (nativeHandle == 0) {
            Log.e(TAG, "Native engine initialization failed");
            adapter.onError(SwipeTypeError.DICT_CORRUPT);
            return false;
        }

        initialized = true;
        Log.i(TAG, "Dictionary loaded: " + languageTag);
        adapter.onInit(this);
        return true;
    }

    /**
     * Process a gesture (swipe) and deliver word candidates to the adapter.
     *
     * <p>Results are delivered synchronously via
     * {@link SwipeTypeAdapter#onCandidatesReady} on the calling thread.</p>
     *
     * @param points  Ordered list of touch points. Must contain >= 2 points.
     */
    public synchronized void processGesture(List<GesturePoint> points) {
        if (!initialized || nativeHandle == 0) {
            if (adapter != null) adapter.onError(SwipeTypeError.ENGINE_NOT_INITIALIZED);
            return;
        }
        if (points == null || points.size() < 2) {
            if (adapter != null) adapter.onError(SwipeTypeError.PATH_TOO_SHORT);
            return;
        }

        // Convert list to arrays
        int n = points.size();
        float[] xCoords    = new float[n];
        float[] yCoords    = new float[n];
        long[]  timestamps = new long[n];
        for (int i = 0; i < n; i++) {
            GesturePoint p = points.get(i);
            xCoords[i]    = p.x;
            yCoords[i]    = p.y;
            timestamps[i] = p.timestamp;
        }

        // Allocate output arrays
        String[] outWords  = new String[DEFAULT_MAX_CANDIDATES];
        float[]  outScores = new float[DEFAULT_MAX_CANDIDATES];
        int[]    outFlags  = new int[DEFAULT_MAX_CANDIDATES];

        int count = nativeRecognize(nativeHandle,
                xCoords, yCoords, timestamps, n, DEFAULT_MAX_CANDIDATES,
                outWords, outScores, outFlags);

        if (count < 0) {
            if (adapter != null) adapter.onError(SwipeTypeError.JNI_ERROR);
            return;
        }

        // Build candidate list
        List<SwipeTypeCandidate> candidates = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            if (outWords[i] != null) {
                candidates.add(new SwipeTypeCandidate(outWords[i], outScores[i], outFlags[i]));
            }
        }

        if (adapter != null) {
            adapter.onCandidatesReady(candidates);
        }
    }

    /**
     * Notify the engine that the keyboard layout has changed.
     *
     * <p>Call this when the user switches languages, rotates the device, or
     * the keyboard layout changes for any reason.</p>
     */
    public synchronized void notifyLayoutChanged() {
        if (!initialized || nativeHandle == 0 || adapter == null) return;

        KeyboardLayoutDescriptor layout = adapter.getKeyboardLayout();
        if (layout == null || layout.keys == null || layout.keys.isEmpty()) {
            adapter.onError(SwipeTypeError.LAYOUT_INVALID);
            return;
        }

        int keyCount = layout.keys.size();
        float[] keyX   = new float[keyCount];
        float[] keyY   = new float[keyCount];
        float[] keyW   = new float[keyCount];
        float[] keyH   = new float[keyCount];
        int[]   keyCps = new int[keyCount];

        for (int i = 0; i < keyCount; i++) {
            KeyboardLayoutDescriptor.KeyInfo k = layout.keys.get(i);
            keyX[i]   = k.centerX;
            keyY[i]   = k.centerY;
            keyW[i]   = k.width;
            keyH[i]   = k.height;
            keyCps[i] = k.codePoint;
        }

        boolean ok = nativeUpdateLayout(nativeHandle,
                keyX, keyY, keyW, keyH, keyCps, keyCount,
                layout.layoutWidth, layout.layoutHeight);

        Log.i(TAG, "Layout updated: " + (ok ? "success" : "failed"));
    }

    /**
     * Shut down the engine and free all native resources.
     *
     * <p>Safe to call multiple times.</p>
     */
    public synchronized void shutdown() {
        if (nativeHandle != 0) {
            nativeShutdown(nativeHandle);
            nativeHandle = 0;
        }
        initialized = false;
        Log.i(TAG, "SwipeTypeEngine shut down");
    }

    /**
     * @return true if the engine is initialized and ready for gesture processing
     */
    public synchronized boolean isInitialized() {
        return initialized && nativeHandle != 0;
    }
}
