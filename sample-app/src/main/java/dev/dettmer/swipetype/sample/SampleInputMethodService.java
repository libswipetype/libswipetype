package dev.dettmer.swipetype.sample;

import android.inputmethodservice.InputMethodService;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import dev.dettmer.swipetype.android.GesturePoint;
import dev.dettmer.swipetype.android.KeyboardLayoutDescriptor;
import dev.dettmer.swipetype.android.SwipeTypeAdapter;
import dev.dettmer.swipetype.android.SwipeTypeCandidate;
import dev.dettmer.swipetype.android.SwipeTypeEngine;
import dev.dettmer.swipetype.android.SwipeTypeError;

import java.io.InputStream;
import java.util.List;

/**
 * A minimal Input Method Service that demonstrates gesture recognition with SwipeType.
 *
 * <p>Architecture:
 * <pre>
 *   ┌─────────────────────────────────────────────────────┐
 *   │  SampleInputMethodService (InputMethodService)       │
 *   │                                                      │
 *   │   ┌──────────────────────┐                           │
 *   │   │  SampleKeyboardView  │──(swipe gesture)──────┐  │
 *   │   │  (input view)        │──(key tap)────────────┐│  │
 *   │   └──────────────────────┘                       ││  │
 *   │                                                  ││  │
 *   │   ┌──────────────────────┐    ┌──────────────┐  ││  │
 *   │   │  SwipeTypeEngine     │◄───┤ gesturePoints│◄─┘│  │
 *   │   │  (native C++)        │    │              │   │  │
 *   │   │  → candidates        │────►commitText()  │   │  │
 *   │   └──────────────────────┘    └──────────────┘   │  │
 *   │                                                   │  │
 *   │                                    commitChar()◄──┘  │
 *   └─────────────────────────────────────────────────────┘
 * </pre>
 *
 * <p>Dictionary loading: The IME looks for {@code raw/en_us_sample} in the APK's
 * raw resources. Include {@code en-us-sample.glide} as
 * {@code sample-app/src/main/res/raw/en_us_sample.glide} to enable gesture recognition.
 * (Generate it with {@code scripts/gen_dict.py}.)
 */
public class SampleInputMethodService extends InputMethodService
        implements SwipeTypeAdapter, SampleKeyboardView.KeyboardListener {

    private static final String TAG = "SampleIME";

    private SwipeTypeEngine engine;
    private SampleKeyboardView keyboardView;
    private boolean engineReady = false;

    // -----------------------------------------------------------------------
    // InputMethodService lifecycle
    // -----------------------------------------------------------------------

    @Override
    public void onCreate() {
        super.onCreate();
        engine = new SwipeTypeEngine();
        engine.init(this, this);
        // engine.init() only stores context+adapter — it does NOT call onInit().
        // onInit() is fired by the engine at the END of loadDictionary() as a
        // success callback. So we must call loadDictionary() here ourselves.
        loadDict();
    }

    private void loadDict() {
        int resId = getResources().getIdentifier("en_us_sample", "raw", getPackageName());
        if (resId == 0) {
            Log.w(TAG, "Dictionary resource raw/en_us_sample not found");
            return;
        }
        try (InputStream is = getResources().openRawResource(resId)) {
            boolean ok = engine.loadDictionary("en-US", is);
            Log.i(TAG, "loadDictionary returned: " + ok);
        } catch (Exception e) {
            Log.e(TAG, "loadDict failed: " + e.getMessage());
        }
    }

    @Override
    public void onDestroy() {
        if (engine != null) {
            engine.shutdown();
        }
        super.onDestroy();
    }

    @Override
    public View onCreateInputView() {
        keyboardView = new SampleKeyboardView(this);
        keyboardView.setKeyboardListener(this);

        // Force a transparent window background so the keyboard view's own
        // canvas background (light gray #D3D3D3) is the only visible color.
        // Without this, the system theme's windowBackground bleeds through.
        Window w = getWindow().getWindow();
        if (w != null) {
            w.setBackgroundDrawableResource(android.R.color.transparent);
        }

        return keyboardView;
    }

    @Override
    public void onStartInputView(EditorInfo info, boolean restarting) {
        super.onStartInputView(info, restarting);
        // Let the engine re-query layout via getKeyboardLayout()
        if (engineReady) {
            engine.notifyLayoutChanged();
        }
    }

    // -----------------------------------------------------------------------
    // SwipeTypeAdapter implementation
    // -----------------------------------------------------------------------

    /**
     * Called by SwipeTypeEngine at the END of loadDictionary() on success.
     * Pure callback -- do NOT call loadDictionary() from here.
     */
    @Override
    public void onInit(SwipeTypeEngine swipeTypeEngine) {
        engineReady = true;
        Log.i(TAG, "onInit callback -- engine ready");
        if (keyboardView != null && keyboardView.getLayoutDescriptor() != null) {
            engine.notifyLayoutChanged();
        }
    }

    @Override
    public KeyboardLayoutDescriptor getKeyboardLayout() {
        if (keyboardView != null) {
            KeyboardLayoutDescriptor desc = keyboardView.getLayoutDescriptor();
            if (desc != null) return desc;
        }
        return buildFallbackLayout();
    }

    @Override
    public void onCandidatesReady(List<SwipeTypeCandidate> candidates) {
        if (candidates == null || candidates.isEmpty()) {
            Log.w(TAG, "onCandidatesReady: no candidates returned");
            return;
        }

        InputConnection ic = getCurrentInputConnection();
        if (ic == null) return;

        // Log ALL candidates for accuracy analysis
        StringBuilder sb = new StringBuilder();
        sb.append("Candidates (").append(candidates.size()).append("):");
        for (int i = 0; i < candidates.size(); i++) {
            SwipeTypeCandidate c = candidates.get(i);
            sb.append(String.format(java.util.Locale.US,
                    "\n  #%d %-12s conf=%.4f", i + 1, c.word, c.confidence));
        }
        Log.d(TAG, sb.toString());

        SwipeTypeCandidate top = candidates.get(0);
        Log.i(TAG, "COMMIT: \"" + top.word + "\" (confidence=" + top.confidence + ")");
        ic.commitText(top.word + " ", 1);
    }

    @Override
    public void onError(SwipeTypeError error) {
        Log.e(TAG, "SwipeType error [" + error.code + "]: " + error.message);
    }

    // -----------------------------------------------------------------------
    // SampleKeyboardView.KeyboardListener implementation
    // -----------------------------------------------------------------------

    @Override
    public void onKeyTap(int codePoint) {
        InputConnection ic = getCurrentInputConnection();
        if (ic != null) ic.commitText(String.valueOf((char) codePoint), 1);
    }

    @Override
    public void onSwipeGesture(List<GesturePoint> points) {
        if (!engineReady) {
            Log.w(TAG, "Engine not ready — dictionary missing or load failed");
            return;
        }
        // Diagnostic: log layout size + gesture bounding box to verify coordinate alignment
        KeyboardLayoutDescriptor desc = getKeyboardLayout();
        float minX = Float.MAX_VALUE, maxX = -Float.MAX_VALUE;
        float minY = Float.MAX_VALUE, maxY = -Float.MAX_VALUE;
        for (GesturePoint p : points) {
            if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;
        }
        Log.d(TAG, String.format(java.util.Locale.US,
                "Layout %.0fx%.0f dp | gesture x=[%.0f..%.0f] y=[%.0f..%.0f] %d pts",
                desc != null ? desc.layoutWidth : -1,
                desc != null ? desc.layoutHeight : -1,
                minX, maxX, minY, maxY, points.size()));
        engine.notifyLayoutChanged();
        engine.processGesture(points);
    }

    // -----------------------------------------------------------------------
    // Fallback layout (used before the keyboard view has been laid out)
    // -----------------------------------------------------------------------

    private static KeyboardLayoutDescriptor buildFallbackLayout() {
        final float W = 360.0f, H = 260.0f;
        final float rowH = (H * 0.75f) / 3.0f;
        final float kw = W / 10.0f;
        final String[] rows = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
        List<KeyboardLayoutDescriptor.KeyInfo> keys = new java.util.ArrayList<>();
        float rowY = rowH / 2.0f;
        for (String row : rows) {
            float offsetX = (W - row.length() * kw) / 2.0f + kw / 2.0f;
            for (int c = 0; c < row.length(); c++) {
                char ch = row.charAt(c);
                keys.add(new KeyboardLayoutDescriptor.KeyInfo(
                        String.valueOf(ch), (int) ch,
                        offsetX + c * kw, rowY, kw, rowH));
            }
            rowY += rowH;
        }
        return new KeyboardLayoutDescriptor("en-US", keys, W, H);
    }
}
