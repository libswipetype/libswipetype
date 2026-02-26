# How to Write an Adapter — libswipetype

> A step-by-step guide to integrating libswipetype into any Android keyboard app.

---

## What is an Adapter?

An adapter is the glue between a keyboard app and libswipetype. It implements the `SwipeTypeAdapter` interface, translating between the keyboard app's internal types and the generic swipetype API.

```
┌────────────────────┐        ┌──────────────────────┐
│  Your Keyboard App │◄──────►│  YourSwipeTypeAdapter │
│  (e.g., FlorisBoard)│        │  implements            │
│  Key positions      │        │  SwipeTypeAdapter      │
│  Touch events       │        │                        │
│  Suggestion bar     │        │  ┌──────────────────┐  │
└────────────────────┘        │  │ SwipeTypeEngine   │  │
                               │  │ (libswipetype)    │  │
                               │  └──────────────────┘  │
                               └──────────────────────┘
```

---

## Prerequisites

- Your keyboard app can provide key positions (center, width, height) in dp
- You have access to touch events (x, y, timestamp) during a swipe gesture
- You have a place to display word candidates (suggestion bar, popup, etc.)
- The `swipetype-android` AAR is available as a dependency

### Adding the dependency

In your keyboard module's `build.gradle`:

```groovy
dependencies {
    implementation project(':swipetype-android')
    // Or, when published:
    // implementation 'dev.dettmer.swipetype:swipetype-android:0.1.0'
}
```

---

## Step-by-Step Guide

### Step 1: Implement `SwipeTypeAdapter`

Create a class that implements all four methods:

```java
package com.example.mykeyboard;

import dev.dettmer.swipetype.android.*;
import java.util.List;

public class MySwipeTypeAdapter implements SwipeTypeAdapter {

    private final MyKeyboardService service;
    private SwipeTypeEngine engine;

    public MySwipeTypeAdapter(MyKeyboardService service) {
        this.service = service;
    }

    // ── Called after loadDictionary() succeeds ───────────────────────
    @Override
    public void onInit(SwipeTypeEngine engine) {
        this.engine = engine;
        // Store the engine reference for later use (e.g., shutdown)
        // Do NOT call loadDictionary() from here — it's already loaded.
    }

    // ── Called by the engine to get key positions ────────────────────
    @Override
    public KeyboardLayoutDescriptor getKeyboardLayout() {
        // Translate your keyboard's internal layout to KeyboardLayoutDescriptor
        List<KeyboardLayoutDescriptor.KeyInfo> keys = new ArrayList<>();

        for (MyKey key : service.getCurrentKeys()) {
            // IMPORTANT: All coordinates must be in dp, not pixels!
            keys.add(new KeyboardLayoutDescriptor.KeyInfo(
                key.getLabel(),           // "a", "b", etc.
                key.getCodePoint(),       // Unicode code point (97 for 'a')
                key.getCenterXDp(),       // center X in dp
                key.getCenterYDp(),       // center Y in dp
                key.getWidthDp(),         // key width in dp
                key.getHeightDp()         // key height in dp
            ));
        }

        return new KeyboardLayoutDescriptor(
            "en-US",                      // BCP 47 language tag
            keys,
            service.getKeyboardWidthDp(), // total keyboard width
            service.getKeyboardHeightDp() // total keyboard height
        );
    }

    // ── Called with recognition results ──────────────────────────────
    @Override
    public void onCandidatesReady(List<SwipeTypeCandidate> candidates) {
        // Forward to your keyboard's suggestion bar
        List<String> words = new ArrayList<>();
        for (SwipeTypeCandidate c : candidates) {
            words.add(c.word);  // public field, not a getter
        }
        service.showSuggestions(words);
    }

    // ── Called on errors ─────────────────────────────────────────────
    @Override
    public void onError(SwipeTypeError error) {
        Log.e("MyAdapter", "SwipeType error [" + error.code + "]: " + error.message);
        // Optionally show a user-facing message for critical errors
    }
}
```

### Step 2: Initialize the Engine

In your keyboard service's `onCreate()`:

```java
public class MyKeyboardService extends InputMethodService {

    private SwipeTypeEngine engine;
    private MySwipeTypeAdapter adapter;

    @Override
    public void onCreate() {
        super.onCreate();

        adapter = new MySwipeTypeAdapter(this);
        engine = new SwipeTypeEngine();
        engine.init(this, adapter);

        // Load dictionary — onInit() callback fires on success
        loadDictionary();
    }

    private void loadDictionary() {
        // Option A: From APK raw resources
        int resId = getResources().getIdentifier("en_us", "raw", getPackageName());
        if (resId != 0) {
            try (InputStream is = getResources().openRawResource(resId)) {
                engine.loadDictionary("en-US", is);
            } catch (Exception e) {
                Log.e("MyIME", "Dict load failed: " + e);
            }
        }

        // Option B: From a file on disk
        // try (InputStream is = new FileInputStream("/path/to/dict.glide")) {
        //     engine.loadDictionary("en-US", is);
        // }
    }

    @Override
    public void onDestroy() {
        if (engine != null) engine.shutdown();
        super.onDestroy();
    }
}
```

### Step 3: Feed Touch Events

Collect `ACTION_DOWN` → `ACTION_MOVE` → `ACTION_UP` touch events during a swipe gesture and convert them to `GesturePoint` objects:

```java
// In your keyboard view's onTouchEvent():
private List<GesturePoint> activePath = new ArrayList<>();
private long gestureStartMs;

@Override
public boolean onTouchEvent(MotionEvent event) {
    // Convert pixels to dp!
    float xDp = event.getX() / getResources().getDisplayMetrics().density;
    float yDp = event.getY() / getResources().getDisplayMetrics().density;

    switch (event.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
            activePath.clear();
            gestureStartMs = event.getEventTime();
            activePath.add(new GesturePoint(xDp, yDp, 0));
            break;

        case MotionEvent.ACTION_MOVE:
            long elapsed = event.getEventTime() - gestureStartMs;
            activePath.add(new GesturePoint(xDp, yDp, elapsed));
            break;

        case MotionEvent.ACTION_UP:
            if (activePath.size() >= 2) {
                // This triggers recognition → onCandidatesReady()
                engine.notifyLayoutChanged(); // ensure layout is fresh
                engine.processGesture(new ArrayList<>(activePath));
            }
            activePath.clear();
            break;
    }
    return true;
}
```

### Step 4: Handle Layout Changes

Call `engine.notifyLayoutChanged()` when:

- The user rotates the device
- The user switches languages
- The keyboard layout changes size
- Before the first `processGesture()` call (if layout may have changed since init)

```java
@Override
public void onStartInputView(EditorInfo info, boolean restarting) {
    super.onStartInputView(info, restarting);
    if (engine != null && engine.isInitialized()) {
        engine.notifyLayoutChanged();
    }
}
```

---

## Coordinate Conversion Checklist

The **#1 cause of wrong results** is coordinate mismatch. Use this checklist:

- [ ] Key positions are in **dp** (not pixels)
- [ ] Touch event coordinates are converted from pixels to **dp**
- [ ] Key `centerX`/`centerY` are relative to the **keyboard view's top-left** (not the screen)
- [ ] `layoutWidth`/`layoutHeight` match the keyboard view's actual **dp dimensions**
- [ ] Both the layout and touch events use the **same coordinate space**

Quick dp conversion:
```java
float density = context.getResources().getDisplayMetrics().density;
float dp = pixels / density;
float pixels = dp * density;
```

---

## Reference: HeliBoard Adapter

The HeliBoard adapter in `adapters/heliboard/` is a working reference implementation.

Key points:
- Translates HeliBoard's `ProximityInfo` and `Key[]` objects into `KeyboardLayoutDescriptor`
- Translates HeliBoard's `InputPointers` into `List<GesturePoint>`
- Delivers candidates to HeliBoard's `SuggestedWords` interface

File: `adapters/heliboard/src/main/java/dev/dettmer/swipetype/adapters/heliboard/HeliboardSwipeTypeAdapter.java`

---

## Generating a Dictionary

The engine requires a binary `.glide` dictionary file. Generate one from a TSV word list:

```bash
python3 scripts/gen_dict.py \
    --input words.tsv \
    --output mydict.glide \
    --language en-US
```

TSV format (no header):
```
the     1000000
and     800000
hello   50000
world   40000
```

Tab-separated: `word<TAB>frequency`. Higher frequency = more likely to be suggested when DTW scores are similar.

Place the `.glide` file in your APK's `res/raw/` directory with underscores instead of hyphens (e.g., `en_us.glide` → accessed as `R.raw.en_us`).

---

## Common Pitfalls

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| Coordinates in pixels instead of dp | Empty results or wrong candidates | Divide by `displayMetrics.density` |
| Missing `loadDictionary()` call | `onCandidatesReady()` never fires | Call `loadDictionary()` in `onCreate()` |
| Calling `loadDictionary()` from `onInit()` | Infinite loop / stack overflow | `onInit` is a callback — don't reload from it |
| Layout not updated before gesture | First gesture returns wrong results | Call `notifyLayoutChanged()` before `processGesture()` |
| Using `getWord()` instead of `.word` | Compile error | `SwipeTypeCandidate` uses public fields, not getters |
| Wrong native lib name | `UnsatisfiedLinkError` | The lib is `glide_jni`, ensure it's in `jniLibs/` |
| Dictionary too small | Poor accuracy | Use ≥ 10,000 words for production quality |

---

## Minimal Working Example

A complete, self-contained adapter in ~60 lines:

```java
public class MinimalAdapter implements SwipeTypeAdapter {
    private final InputMethodService ime;

    public MinimalAdapter(InputMethodService ime) { this.ime = ime; }

    @Override
    public void onInit(SwipeTypeEngine engine) {
        Log.i("Minimal", "Engine ready");
    }

    @Override
    public KeyboardLayoutDescriptor getKeyboardLayout() {
        // Hardcoded QWERTY layout (360×260 dp)
        float w = 360f, h = 260f, kw = 36f, kh = 65f;
        String[] rows = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
        List<KeyboardLayoutDescriptor.KeyInfo> keys = new ArrayList<>();
        float rowY = kh / 2f;
        for (String row : rows) {
            float offsetX = (w - row.length() * kw) / 2f + kw / 2f;
            for (int c = 0; c < row.length(); c++) {
                char ch = row.charAt(c);
                keys.add(new KeyboardLayoutDescriptor.KeyInfo(
                    String.valueOf(ch), (int) ch,
                    offsetX + c * kw, rowY, kw, kh));
            }
            rowY += kh;
        }
        return new KeyboardLayoutDescriptor("en-US", keys, w, h);
    }

    @Override
    public void onCandidatesReady(List<SwipeTypeCandidate> candidates) {
        if (!candidates.isEmpty()) {
            InputConnection ic = ime.getCurrentInputConnection();
            if (ic != null) ic.commitText(candidates.get(0).word + " ", 1);
        }
    }

    @Override
    public void onError(SwipeTypeError error) {
        Log.e("Minimal", error.message);
    }
}
```

---

## Next Steps

- Read [API.md](API.md) for the full API reference
- Read [ARCHITECTURE.md](ARCHITECTURE.md) to understand the recognition pipeline
- Study the sample app for a complete working integration
- Check the [CHANGELOG.md](../CHANGELOG.md) for the latest changes
