package dev.dettmer.swipetype.sample;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import dev.dettmer.swipetype.android.GesturePoint;
import dev.dettmer.swipetype.android.KeyboardLayoutDescriptor;

import java.util.ArrayList;
import java.util.List;

/**
 * A simple QWERTY keyboard view that:
 * <ul>
 *   <li>Renders 3 rows of letter keys and a space bar</li>
 *   <li>Tracks finger movement to build a gesture path</li>
 *   <li>Dispatches tap and swipe gestures to a {@link KeyboardListener}</li>
 * </ul>
 *
 * Key coordinates are in density-independent pixels (dp). The view converts them
 * to screen pixels for rendering and to dp for the SwipeType engine.
 */
public class SampleKeyboardView extends View {

    // -----------------------------------------------------------------------
    // Constants
    // -----------------------------------------------------------------------

    /** Rows of keys in order (top to bottom). */
    private static final String[] KEY_ROWS = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};

    /** Minimum swipe distance (dp) to classify a gesture as a swipe rather than a tap. */
    private static final float SWIPE_THRESHOLD_DP = 30.0f;

    // -----------------------------------------------------------------------
    // Layout state (populated in onSizeChanged, in dp units)
    // -----------------------------------------------------------------------

    private KeyboardLayoutDescriptor layoutDescriptor;
    private float[] keyCX;   // key center X in dp
    private float[] keyCY;   // key center Y in dp
    private float[] keyW;    // key width in dp
    private float[] keyH;    // key height in dp
    private int[]   keyCodes;
    private String[] keyLabels;
    private int keyCount;

    /** Space bar bounds in dp */
    private float spaceCX, spaceCY, spaceW, spaceH;

    // -----------------------------------------------------------------------
    // Gesture tracking state
    // -----------------------------------------------------------------------

    /** Gesture path accumulated during ACTION_MOVE (dp coordinates). */
    private final List<GesturePoint> activePath = new ArrayList<>();
    private long gestureStartMs;
    private boolean isSwiping;
    private boolean touchStartOnSpace;  // true when touch began in the space bar zone
    private float touchStartX, touchStartY;

    // -----------------------------------------------------------------------
    // Rendering
    // -----------------------------------------------------------------------

    private final Paint keyBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint keyTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint trailPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint keyPressedPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private int pressedKeyIndex = -1;

    // -----------------------------------------------------------------------
    // Callback
    // -----------------------------------------------------------------------

    public interface KeyboardListener {
        /** Called when the user taps a single key. */
        void onKeyTap(int codePoint);
        /** Called when the user completes a swipe gesture. */
        void onSwipeGesture(List<GesturePoint> points);
    }

    private KeyboardListener listener;

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    public SampleKeyboardView(Context context) {
        super(context);
        init();
    }

    public SampleKeyboardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public SampleKeyboardView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        keyBgPaint.setColor(Color.WHITE);
        keyBgPaint.setStyle(Paint.Style.FILL);

        keyPressedPaint.setColor(Color.parseColor("#B0C4DE")); // light steel blue
        keyPressedPaint.setStyle(Paint.Style.FILL);

        keyTextPaint.setColor(Color.BLACK);
        keyTextPaint.setTextAlign(Paint.Align.CENTER);

        trailPaint.setColor(Color.parseColor("#4A90D9"));
        trailPaint.setStyle(Paint.Style.STROKE);
        trailPaint.setStrokeCap(Paint.Cap.ROUND);
        trailPaint.setStrokeJoin(Paint.Join.ROUND);
    }

    public void setKeyboardListener(KeyboardListener l) {
        this.listener = l;
    }

    // -----------------------------------------------------------------------
    // Layout computation
    // -----------------------------------------------------------------------

    /** Keyboard is always exactly 260 dp tall — prevents it filling the whole screen. */
    private static final float KEYBOARD_HEIGHT_DP = 260.0f;

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int w = MeasureSpec.getSize(widthMeasureSpec);
        int h = (int) dpToPx(KEYBOARD_HEIGHT_DP);
        setMeasuredDimension(w, h);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldW, int oldH) {
        super.onSizeChanged(w, h, oldW, oldH);
        buildLayout(pxToDp(w), pxToDp(h));
    }

    /**
     * Compute key positions in dp and populate {@link #layoutDescriptor}.
     *
     * Layout uses even distribution: 75% of height for 3 letter rows, 25% for
     * space bar. All rows use a uniform key width (based on the 10-key top row)
     * and each row is centred horizontally to produce standard QWERTY stagger.
     *
     * Both rendering and the engine receive the same dp coordinates, so gesture
     * recognition is always consistent regardless of device screen density.
     */
    private void buildLayout(float widthDp, float heightDp) {
        final float letterAreaH = heightDp * 0.75f;
        final float rowH = letterAreaH / 3.0f;
        final float kw = widthDp / 10.0f;  // uniform key width (10 keys per top row)

        int totalKeys = 0;
        for (String row : KEY_ROWS) totalKeys += row.length();

        keyCX     = new float[totalKeys];
        keyCY     = new float[totalKeys];
        keyW      = new float[totalKeys];
        keyH      = new float[totalKeys];
        keyCodes  = new int[totalKeys];
        keyLabels = new String[totalKeys];
        keyCount  = 0;

        for (int r = 0; r < KEY_ROWS.length; r++) {
            String row = KEY_ROWS[r];
            float rowY   = rowH / 2.0f + r * rowH;
            // Centre each row horizontally
            float offsetX = (widthDp - row.length() * kw) / 2.0f + kw / 2.0f;
            for (int c = 0; c < row.length(); c++) {
                char ch = row.charAt(c);
                keyCX[keyCount]     = offsetX + c * kw;
                keyCY[keyCount]     = rowY;
                keyW[keyCount]      = kw;
                keyH[keyCount]      = rowH;
                keyCodes[keyCount]  = (int) ch;
                keyLabels[keyCount] = String.valueOf(ch);
                keyCount++;
            }
        }

        // Space bar centred in the bottom 25% area
        final float spaceAreaY = letterAreaH;
        spaceW  = widthDp * 0.5f;
        spaceH  = (heightDp - letterAreaH) * 0.65f;
        spaceCX = widthDp / 2.0f;
        spaceCY = spaceAreaY + (heightDp - letterAreaH) / 2.0f;

        // Build KeyboardLayoutDescriptor for the engine
        List<KeyboardLayoutDescriptor.KeyInfo> infos = new ArrayList<>();
        for (int i = 0; i < keyCount; i++) {
            infos.add(new KeyboardLayoutDescriptor.KeyInfo(
                    keyLabels[i], keyCodes[i],
                    keyCX[i], keyCY[i], keyW[i], keyH[i]));
        }
        layoutDescriptor = new KeyboardLayoutDescriptor(
                "en-US", infos, widthDp, heightDp);
    }

    /** Returns the current layout descriptor (may be null before first layout pass). */
    public KeyboardLayoutDescriptor getLayoutDescriptor() {
        return layoutDescriptor;
    }

    // -----------------------------------------------------------------------
    // Drawing
    // -----------------------------------------------------------------------

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.drawColor(Color.parseColor("#D3D3D3")); // light gray background

        if (keyCount == 0) return;

        float radius = dpToPx(4);
        keyTextPaint.setTextSize(dpToPx(14));
        keyTextPaint.setColor(Color.BLACK); // always black regardless of system theme
        trailPaint.setStrokeWidth(dpToPx(3));

        for (int i = 0; i < keyCount; i++) {
            float cx = dpToPx(keyCX[i]);
            float cy = dpToPx(keyCY[i]);
            float hw = dpToPx(keyW[i]) / 2.0f - dpToPx(1);
            float hh = dpToPx(keyH[i]) / 2.0f - dpToPx(1);

            RectF rect = new RectF(cx - hw, cy - hh, cx + hw, cy + hh);
            canvas.drawRoundRect(rect, radius, radius,
                    i == pressedKeyIndex ? keyPressedPaint : keyBgPaint);
            canvas.drawText(keyLabels[i].toUpperCase(), cx, cy + dpToPx(5), keyTextPaint);
        }

        // Space bar
        RectF spaceRect = new RectF(
                dpToPx(spaceCX - spaceW / 2.0f), dpToPx(spaceCY - spaceH / 2.0f),
                dpToPx(spaceCX + spaceW / 2.0f), dpToPx(spaceCY + spaceH / 2.0f));
        canvas.drawRoundRect(spaceRect, radius, radius, keyBgPaint);
        canvas.drawText("space", dpToPx(spaceCX), dpToPx(spaceCY) + dpToPx(5), keyTextPaint);

        // Draw gesture trail
        if (activePath.size() >= 2) {
            for (int i = 1; i < activePath.size(); i++) {
                canvas.drawLine(
                        dpToPx(activePath.get(i - 1).x),
                        dpToPx(activePath.get(i - 1).y),
                        dpToPx(activePath.get(i).x),
                        dpToPx(activePath.get(i).y),
                        trailPaint);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Touch handling
    // -----------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        float xDp = pxToDp(event.getX());
        float yDp = pxToDp(event.getY());

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                activePath.clear();
                isSwiping = false;
                touchStartX = xDp;
                touchStartY = yDp;
                touchStartOnSpace = isInSpaceZone(yDp);
                gestureStartMs = event.getEventTime();
                if (!touchStartOnSpace) {
                    activePath.add(new GesturePoint(xDp, yDp, 0));
                }
                pressedKeyIndex = touchStartOnSpace ? -1 : findNearestKey(xDp, yDp);
                invalidate();
                break;

            case MotionEvent.ACTION_MOVE:
                if (!touchStartOnSpace) {
                    long elapsed = event.getEventTime() - gestureStartMs;
                    activePath.add(new GesturePoint(xDp, yDp, elapsed));
                    float dx = xDp - touchStartX;
                    float dy = yDp - touchStartY;
                    if (!isSwiping && Math.sqrt(dx * dx + dy * dy) > SWIPE_THRESHOLD_DP) {
                        isSwiping = true;
                        pressedKeyIndex = -1;
                    }
                }
                invalidate();
                break;

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                pressedKeyIndex = -1;
                if (touchStartOnSpace) {
                    // Any touch that started in the space zone is always a space
                    if (listener != null) listener.onKeyTap(' ');
                } else if (isSwiping && activePath.size() >= 2 && listener != null) {
                    listener.onSwipeGesture(new ArrayList<>(activePath));
                } else if (!isSwiping && listener != null) {
                    int ki = findNearestKey(xDp, yDp);
                    if (ki >= 0) listener.onKeyTap(keyCodes[ki]);
                }
                activePath.clear();
                invalidate();
                break;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Hit testing helpers
    // -----------------------------------------------------------------------

    private int findNearestKey(float xDp, float yDp) {
        float best = Float.MAX_VALUE;
        int bestIdx = -1;
        for (int i = 0; i < keyCount; i++) {
            float dx = xDp - keyCX[i];
            float dy = yDp - keyCY[i];
            float dist = dx * dx + dy * dy;
            if (dist < best) { best = dist; bestIdx = i; }
        }
        return bestIdx;
    }

    private boolean isOnSpaceBar(float xDp, float yDp) {
        return Math.abs(xDp - spaceCX) <= spaceW / 2.0f
                && Math.abs(yDp - spaceCY) <= spaceH / 2.0f;
    }

    /**
     * Returns true if the y-coordinate is below the letter area (bottom 25%).
     * The entire bottom zone is treated as the space bar for touch purposes.
     */
    private boolean isInSpaceZone(float yDp) {
        float letterAreaH = KEYBOARD_HEIGHT_DP * 0.75f;
        return yDp > letterAreaH;
    }

    // -----------------------------------------------------------------------
    // Unit conversion helpers
    // -----------------------------------------------------------------------

    private float dpToPx(float dp) {
        return dp * getResources().getDisplayMetrics().density;
    }

    private float pxToDp(float px) {
        return px / getResources().getDisplayMetrics().density;
    }
}
