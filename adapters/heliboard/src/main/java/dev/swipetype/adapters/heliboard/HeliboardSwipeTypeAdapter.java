package dev.swipetype.adapters.heliboard;

import android.content.Context;
import android.util.Log;

import dev.swipetype.android.SwipeTypeAdapter;
import dev.swipetype.android.SwipeTypeCandidate;
import dev.swipetype.android.SwipeTypeEngine;
import dev.swipetype.android.SwipeTypeError;
import dev.swipetype.android.GesturePoint;
import dev.swipetype.android.KeyboardLayoutDescriptor;
import dev.swipetype.android.KeyboardLayoutDescriptor.KeyInfo;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * HeliBoard-specific adapter for the libswipetype.
 *
 * <p>This adapter translates between HeliBoard's internal keyboard representation
 * and the generic {@link SwipeTypeAdapter} interface. It serves as the
 * <strong>reference implementation</strong> for how any keyboard project
 * integrates the libswipetype.</p>
 *
 * <h3>Integration into HeliBoard:</h3>
 * <ol>
 *   <li>Add swipetype-android and this adapter as Gradle dependencies</li>
 *   <li>Create an instance of this adapter in LatinIME</li>
 *   <li>Call {@link #initialize(Context, InputStream)} with the dictionary</li>
 *   <li>When gesture input is detected, call {@link #onGestureInput(int[], int[], int[], int)}</li>
 *   <li>Receive candidates via the registered callback</li>
 * </ol>
 *
 * <h3>HeliBoard-Specific Knowledge:</h3>
 * <ul>
 *   <li>HeliBoard uses ProximityInfo to describe keyboard layout (key coords, dimensions)</li>
 *   <li>Touch coordinates arrive as int arrays (pixels), which we convert to float (dp)</li>
 *   <li>HeliBoard's NativeSuggestOptions.isGesture() flag determines gesture vs. typing mode</li>
 *   <li>Suggestion results are expected as SuggestedWordInfo objects in HeliBoard's format</li>
 * </ul>
 */
public class HeliboardSwipeTypeAdapter implements SwipeTypeAdapter {

    private static final String TAG = "HeliboardSwipeTypeAdapter";

    private SwipeTypeEngine engine;
    private Context context;

    // HeliBoard's keyboard state — updated via setKeyboardLayout()
    private int displayWidth;
    private int displayHeight;
    private float displayDensity = 1.0f;
    private List<HeliboardKeyInfo> currentKeys = new ArrayList<>();
    private String currentLanguageTag = "en-US";

    // Callback for delivering results to HeliBoard
    private CandidateCallback candidateCallback;

    /**
     * Callback interface for delivering candidates back to HeliBoard.
     *
     * <p>HeliBoard should implement this to receive gesture recognition results
     * and convert them to its internal SuggestedWordInfo format.</p>
     */
    public interface CandidateCallback {
        /**
         * Called when gesture recognition produces candidates.
         *
         * @param words       Array of recognized words, best first
         * @param scores      Array of confidence scores (0-1000000 in HeliBoard's scale)
         * @param count       Number of valid entries in the arrays
         */
        void onCandidates(String[] words, int[] scores, int count);

        /**
         * Called when an error occurs.
         *
         * @param errorMessage Human-readable error description
         */
        void onError(String errorMessage);
    }

    /**
     * Create a new HeliBoard adapter.
     *
     * @param callback Callback for delivering results to HeliBoard
     */
    public HeliboardSwipeTypeAdapter(CandidateCallback callback) {
        this.candidateCallback = callback;
        this.engine = new SwipeTypeEngine();
    }

    /**
     * Initialize the adapter with a dictionary.
     *
     * <p>Call this from HeliBoard's initialization code (e.g., LatinIME.onCreate
     * or when the dictionary changes).</p>
     *
     * @param context     Android context
     * @param dictStream  InputStream for the .glide dictionary file
     * @return true on success
     */
    public boolean initialize(Context context, InputStream dictStream) {
        this.context = context.getApplicationContext();
        this.displayDensity = context.getResources().getDisplayMetrics().density;

        engine.init(context, this);
        return engine.loadDictionary(currentLanguageTag, dictStream);
    }

    /**
     * Set the keyboard layout from HeliBoard's ProximityInfo data.
     *
     * <p>Call this whenever the keyboard layout changes (language switch,
     * rotation, etc.).</p>
     *
     * <p><strong>HeliBoard call site:</strong> This maps to the data passed to
     * {@code ProximityInfo.setProximityInfoNative()}, specifically:<br>
     * - keyXCoordinates, keyYCoordinates: top-left corner of each key (pixels)<br>
     * - keyWidths, keyHeights: key dimensions (pixels)<br>
     * - keyCharCodes: Unicode code points for each key<br>
     * - displayWidth, displayHeight: keyboard dimensions (pixels)</p>
     *
     * @param displayWidth       Keyboard width in pixels
     * @param displayHeight      Keyboard height in pixels
     * @param keyCount           Number of keys
     * @param keyXCoordinates    Key top-left X coordinates (pixels)
     * @param keyYCoordinates    Key top-left Y coordinates (pixels)
     * @param keyWidths          Key widths (pixels)
     * @param keyHeights         Key heights (pixels)
     * @param keyCharCodes       Unicode code points per key
     * @param languageTag        BCP 47 language tag
     */
    public void setKeyboardLayout(
            int displayWidth, int displayHeight,
            int keyCount,
            int[] keyXCoordinates, int[] keyYCoordinates,
            int[] keyWidths, int[] keyHeights,
            int[] keyCharCodes,
            String languageTag) {

        this.displayWidth = displayWidth;
        this.displayHeight = displayHeight;
        this.currentLanguageTag = languageTag;

        currentKeys.clear();
        for (int i = 0; i < keyCount; i++) {
            float centerXDp = (keyXCoordinates[i] + keyWidths[i] / 2.0f) / displayDensity;
            float centerYDp = (keyYCoordinates[i] + keyHeights[i] / 2.0f) / displayDensity;
            float widthDp = keyWidths[i] / displayDensity;
            float heightDp = keyHeights[i] / displayDensity;
            currentKeys.add(new HeliboardKeyInfo(
                    keyCharCodes[i], centerXDp, centerYDp, widthDp, heightDp));
        }

        if (engine != null && engine.isInitialized()) {
            engine.notifyLayoutChanged();
        }

        Log.i(TAG, "Layout updated: " + keyCount + " keys, " + languageTag);
    }

    /**
     * Process gesture input from HeliBoard.
     *
     * <p><strong>HeliBoard call site:</strong> This maps to the data from
     * {@code InputPointers} which HeliBoard passes to {@code getSuggestionsNative()}
     * when {@code NativeSuggestOptions.isGesture() == true}.</p>
     *
     * <p>HeliBoard provides touch coordinates as int arrays in pixel coordinates.
     * This method converts them to dp and forwards to the engine.</p>
     *
     * @param xCoordinates  X touch coordinates (pixels)
     * @param yCoordinates  Y touch coordinates (pixels)
     * @param times         Timestamps (ms since first event)
     * @param pointCount    Number of valid points in the arrays
     */
    public void onGestureInput(int[] xCoordinates, int[] yCoordinates,
                                int[] times, int pointCount) {
        List<GesturePoint> points = new ArrayList<>(pointCount);
        for (int i = 0; i < pointCount; i++) {
            float xDp = xCoordinates[i] / displayDensity;
            float yDp = yCoordinates[i] / displayDensity;
            long timestamp = times[i]; // already in ms
            points.add(new GesturePoint(xDp, yDp, timestamp));
        }
        engine.processGesture(points);
        // Results come back via onCandidatesReady() callback
    }

    // ========================================================================
    // SwipeTypeAdapter interface implementation
    // ========================================================================

    @Override
    public void onInit(SwipeTypeEngine engine) {
        Log.i(TAG, "Glide engine initialized for HeliBoard");
    }

    @Override
    public KeyboardLayoutDescriptor getKeyboardLayout() {
        List<KeyInfo> keys = new ArrayList<>(currentKeys.size());
        for (HeliboardKeyInfo key : currentKeys) {
            String label = new String(Character.toChars(key.charCode));
            keys.add(new KeyInfo(
                    label, key.charCode,
                    key.centerX, key.centerY,
                    key.width, key.height));
        }
        float widthDp = displayWidth / displayDensity;
        float heightDp = displayHeight / displayDensity;
        return new KeyboardLayoutDescriptor(currentLanguageTag, keys, widthDp, heightDp);
    }

    @Override
    public void onCandidatesReady(List<SwipeTypeCandidate> candidates) {
        if (candidateCallback == null) return;

        int count = candidates.size();
        String[] words = new String[count];
        int[] scores = new int[count];

        for (int i = 0; i < count; i++) {
            words[i] = candidates.get(i).word;
            scores[i] = (int) (candidates.get(i).confidence * 1_000_000);
        }

        Log.d(TAG, "Delivering " + count + " candidates to HeliBoard");
        candidateCallback.onCandidates(words, scores, count);
    }

    @Override
    public void onError(SwipeTypeError error) {
        Log.e(TAG, "Glide error: " + error.message);
        if (candidateCallback != null) {
            candidateCallback.onError(error.message);
        }
    }

    /**
     * Shut down the adapter and engine.
     */
    public void shutdown() {
        if (engine != null) {
            engine.shutdown();
        }
    }

    // ========================================================================
    // Internal data class
    // ========================================================================

    /** Internal representation of a HeliBoard key (in dp coordinates). */
    private static class HeliboardKeyInfo {
        final int charCode;
        final float centerX;
        final float centerY;
        final float width;
        final float height;

        HeliboardKeyInfo(int charCode, float centerX, float centerY,
                         float width, float height) {
            this.charCode = charCode;
            this.centerX = centerX;
            this.centerY = centerY;
            this.width = width;
            this.height = height;
        }
    }
}
