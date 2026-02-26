# API Reference — libswipetype

> **Version:** 0.1.0 &nbsp;|&nbsp; **Language:** C++17 (core) &amp; Java (Android wrapper) &nbsp;|&nbsp; **Updated:** 2026-02

---

## Table of Contents

1. [C++ Core API](#c-core-api)
   - [GestureEngine](#gestureengine)
   - [RawGesturePath / GesturePath](#rawgesturepath--gesturepath)
   - [GestureCandidate](#gesturecandidate)
   - [KeyboardLayout / KeyDescriptor](#keyboardlayout--keydescriptor)
   - [ScoringConfig](#scoringconfig)
   - [DictionaryLoader](#dictionaryloader)
   - [Error Handling](#error-handling)
   - [Constants](#constants)
2. [Android (Java) API](#android-java-api)
   - [SwipeTypeEngine](#swipetypeengine)
   - [SwipeTypeAdapter](#swipetypeadapter)
   - [GesturePoint (Java)](#gesturepoint-java)
   - [KeyboardLayoutDescriptor](#keyboardlayoutdescriptor)
   - [SwipeTypeCandidate (Java)](#swipetypecandidate-java)
   - [SwipeTypeError](#swipetypeerror)

---

## C++ Core API

All C++ types live in the `swipetype` namespace.  
Header root: `swipetype-core/include/swipetype/`

### GestureEngine

**Header:** `GestureEngine.h`

The main entry point for gesture recognition. Orchestrates the full pipeline:
normalize → filter → score → rank → return.

```cpp
class GestureEngine {
public:
    GestureEngine();
    ~GestureEngine();

    // Move-only (non-copyable)
    GestureEngine(GestureEngine&&) noexcept;
    GestureEngine& operator=(GestureEngine&&) noexcept;

    bool init(const KeyboardLayout& layout, const std::string& dictPath);
    bool initWithData(const KeyboardLayout& layout,
                      const uint8_t* dictData, size_t dictSize);
    std::vector<GestureCandidate> recognize(const RawGesturePath& rawPath,
                                             int maxCandidates = 8);
    void shutdown();
    bool isInitialized() const;
    bool updateLayout(const KeyboardLayout& layout);
    void configure(const ScoringConfig& config);
    void setErrorCallback(ErrorCallback callback);
    ErrorInfo getLastError() const;
};
```

#### `init(layout, dictPath) → bool`

Initialize the engine with a keyboard layout and a path to a binary `.glide` dictionary file.

| Parameter | Type | Description |
|-----------|------|-------------|
| `layout` | `const KeyboardLayout&` | Keyboard layout with key positions in dp |
| `dictPath` | `const std::string&` | Absolute file path to the `.glide` dictionary |

**Returns:** `true` on success. On failure, call `getLastError()` for details.

**Post-condition:** `isInitialized() == true` on success.

#### `initWithData(layout, dictData, dictSize) → bool`

Initialize the engine with a keyboard layout and an in-memory dictionary.

| Parameter | Type | Description |
|-----------|------|-------------|
| `layout` | `const KeyboardLayout&` | Keyboard layout |
| `dictData` | `const uint8_t*` | Pointer to raw dictionary bytes |
| `dictSize` | `size_t` | Size of dictionary data in bytes |

**Returns:** `true` on success.

#### `recognize(rawPath, maxCandidates) → vector<GestureCandidate>`

Run the recognition pipeline on a raw gesture path.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `rawPath` | `const RawGesturePath&` | — | Raw touch points (≥ 2 points) |
| `maxCandidates` | `int` | `8` | Max results. Clamped to [1, 20] |

**Returns:** Candidates sorted by confidence descending (best first). Empty if engine not initialized, path too short, or no matches found.

**Pipeline steps:**
1. Deduplicate → resample to 64 points → bounding-box normalize
2. Determine start/end key characters from first/last touch point
3. Filter dictionary by start+end character, then by estimated word length
4. Generate ideal path for each candidate word and compute DTW distance
5. Normalize DTW scores, apply adaptive frequency weighting
6. Sort by confidence and return top N

#### `shutdown()`

Release all resources (dictionary, caches). `isInitialized()` returns `false` after this call.

#### `updateLayout(layout) → bool`

Hot-swap the keyboard layout (e.g., after device rotation or language switch).
Invalidates the ideal path cache. Does not reload the dictionary.

#### `configure(config)`

Override scoring parameters. See [ScoringConfig](#scoringconfig).

#### `setErrorCallback(callback)`

Register a callback for error notifications. Called synchronously from the thread that encounters the error.

```cpp
engine.setErrorCallback([](const ErrorInfo& err) {
    std::cerr << "Error " << static_cast<int>(err.code) << ": " << err.message << "\n";
});
```

---

### RawGesturePath / GesturePath

**Header:** `GesturePath.h`

```cpp
// Raw input from the keyboard view
struct RawGesturePath {
    std::vector<GesturePoint> points;   // dp coordinates, ordered by time
    bool isEmpty() const;               // true if < 2 points
    size_t size() const;
};

// After normalization (64 points in [0,1] bounding box)
struct GesturePath {
    std::vector<NormalizedPoint> points; // exactly RESAMPLE_COUNT (64)
    float aspectRatio;                   // originalWidth / originalHeight
    float totalArcLength;                // in dp, before normalization
    int32_t startKeyIndex;               // index into KeyboardLayout::keys
    int32_t endKeyIndex;                 // index into KeyboardLayout::keys
    bool isValid() const;                // true if points.size() == 64
};
```

---

### GestureCandidate

**Header:** `GestureCandidate.h`

```cpp
struct GestureCandidate {
    std::string word;          // UTF-8 word
    float confidence;          // [0.0, 1.0] — 1.0 = best
    uint32_t sourceFlags;      // bitmask: SOURCE_MAIN_DICT, SOURCE_USER_DICT, etc.
    float dtwScore;            // raw DTW distance (lower = better) — for debugging
    float frequencyScore;      // normalized frequency [0.0, 1.0] — for debugging
};
```

**Source flags:**

| Constant | Value | Description |
|----------|-------|-------------|
| `SOURCE_MAIN_DICT` | `0x01` | From the primary dictionary |
| `SOURCE_USER_DICT` | `0x02` | From the user dictionary (future) |
| `SOURCE_COMPLETION` | `0x04` | Prefix completion (future) |

---

### KeyboardLayout / KeyDescriptor

**Header:** `KeyboardLayout.h`

```cpp
struct KeyDescriptor {
    std::string label;    // e.g. "a", "shift" — display/debug only
    int32_t codePoint;    // Unicode code point. -1 for non-character keys
    float centerX;        // key center X in dp
    float centerY;        // key center Y in dp
    float width;          // key width in dp
    float height;         // key height in dp
    bool isCharacterKey() const;
};

struct KeyboardLayout {
    std::string languageTag;             // BCP 47 ("en-US")
    std::vector<KeyDescriptor> keys;     // all keys
    float layoutWidth;                   // total keyboard width in dp
    float layoutHeight;                  // total keyboard height in dp

    int32_t findNearestKey(float x, float y) const;       // nearest char key index
    int32_t findKeyByCodePoint(int32_t codePoint) const;   // by code point (case-insensitive)
    bool isValid() const;                                   // ≥ 1 character key
};
```

**Coordinate system:** Origin is the top-left corner of the keyboard. All values are in density-independent pixels (dp). The same dp coordinates must be used for both the layout and the gesture touch points.

---

### ScoringConfig

**Header:** `SwipeTypeTypes.h`

```cpp
struct ScoringConfig {
    int resampleCount = 64;           // points after resampling
    float minPointDistance = 2.0f;     // dedup threshold (dp)
    float dtwBandwidthRatio = 0.10f;  // Sakoe-Chiba band = ceil(0.10 * 64) = 6
    float frequencyWeight = 0.30f;    // α: weight of frequency in final score
    int maxCandidatesEvaluated = 20;  // hard cap on evaluated candidates
    float lengthFilterTolerance = 3.0f; // ± tolerance for word-length filter
    float maxDTWFloor = 3.0f;         // absolute floor for DTW normalization
};
```

Pass a modified config to `GestureEngine::configure()` to tune scoring behavior.

---

### DictionaryLoader

**Header:** `DictionaryLoader.h`

```cpp
struct DictionaryEntry {
    std::string word;        // UTF-8 word
    uint32_t frequency;      // higher = more common
    uint8_t flags;           // DICT_FLAG_PROPER_NOUN, DICT_FLAG_PROFANITY
};

class DictionaryLoader {
public:
    bool load(const std::string& filePath);
    bool loadFromMemory(const uint8_t* data, size_t size);
    void unload();

    const std::vector<DictionaryEntry>& getAllEntries() const;
    std::vector<const DictionaryEntry*> getEntriesStartingWith(char c) const;
    std::vector<const DictionaryEntry*> getEntriesWithStartEnd(char start, char end) const;
    uint32_t getMaxFrequency() const;
    ErrorInfo getLastError() const;
};
```

The loader reads the binary `.glide` format (see `scripts/gen_dict.py`).  
**Thread safety:** Read-only operations are safe after loading. Load/unload are not thread-safe.

---

### Error Handling

```cpp
enum class ErrorCode : int {
    NONE = 0,
    DICT_NOT_FOUND = 1,
    DICT_CORRUPT = 2,
    DICT_VERSION_MISMATCH = 3,
    LAYOUT_INVALID = 4,
    PATH_TOO_SHORT = 5,
    ENGINE_NOT_INITIALIZED = 6,
    OUT_OF_MEMORY = 7
};

struct ErrorInfo {
    ErrorCode code;
    std::string message;
};

using ErrorCallback = std::function<void(const ErrorInfo& error)>;
```

Errors are reported via:
1. Return values (`false` from `init()`, empty vector from `recognize()`)
2. `getLastError()` — last error info
3. `setErrorCallback()` — synchronous callback on error

---

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `RESAMPLE_COUNT` | `64` | Points after resampling |
| `MIN_POINT_DISTANCE_DP` | `2.0f` | Dedup threshold (dp) |
| `MIN_GESTURE_POINTS` | `2` | Minimum points for a valid gesture |
| `MAX_GESTURE_POINTS` | `10000` | Hard cap on raw input points |
| `DTW_BANDWIDTH` | `6` | Sakoe-Chiba band width |
| `FREQUENCY_WEIGHT` | `0.30f` | Default α for frequency weighting |
| `LENGTH_FILTER_TOLERANCE` | `3.0f` | Word-length filter tolerance (±) |
| `MAX_DTW_FLOOR` | `3.0f` | Absolute DTW normalization floor |
| `DEFAULT_MAX_CANDIDATES` | `8` | Default max candidates |
| `MAX_MAX_CANDIDATES` | `20` | Hard cap on max candidates |
| `DICT_MAGIC` | `0x474C4944` | `.glide` file magic ("GLID") |
| `DICT_VERSION` | `1` | Current dict format version |
| `DICT_HEADER_SIZE` | `32` | Fixed header size in bytes |
| `MAX_WORD_LENGTH` | `64` | Max word length (UTF-8 bytes) |

---

## Android (Java) API

Package: `dev.dettmer.swipetype.android`

### SwipeTypeEngine

The main Android entry point. Wraps the C++ core via JNI.

```java
public class SwipeTypeEngine {
    // Lifecycle
    void init(Context context, SwipeTypeAdapter adapter);
    boolean loadDictionary(String languageTag, InputStream dictStream);
    void shutdown();
    boolean isInitialized();

    // Recognition
    void processGesture(List<GesturePoint> points);

    // Layout
    void notifyLayoutChanged();
}
```

#### `init(context, adapter)`

Store the app context and adapter. Does NOT load the dictionary or initialize native code. Call `loadDictionary()` after this.

#### `loadDictionary(languageTag, dictStream) → boolean`

Copy the dictionary to the cache directory, query the adapter for the current layout, and initialize the native engine. Calls `adapter.onInit(this)` on success.

#### `processGesture(points)`

Run recognition on the given touch points. Results are delivered synchronously via `adapter.onCandidatesReady()` on the calling thread.

| Parameter | Type | Description |
|-----------|------|-------------|
| `points` | `List<GesturePoint>` | ≥ 2 touch points with dp coordinates |

#### `notifyLayoutChanged()`

Re-query the adapter for the current layout and update the native engine. Call after device rotation, language switch, or layout resize.

#### `shutdown()`

Release all native resources. Safe to call multiple times.

**Thread safety:** All public methods are `synchronized`. `processGesture()` can be called from any thread.

---

### SwipeTypeAdapter

The contract between libswipetype and any keyboard app. Every keyboard integration must implement this interface.

```java
public interface SwipeTypeAdapter {
    void onInit(SwipeTypeEngine engine);
    KeyboardLayoutDescriptor getKeyboardLayout();
    void onCandidatesReady(List<SwipeTypeCandidate> candidates);
    void onError(SwipeTypeError error);
}
```

| Method | When called | What to do |
|--------|-------------|------------|
| `onInit` | After `loadDictionary()` succeeds | Store engine reference if needed |
| `getKeyboardLayout` | During init and `notifyLayoutChanged()` | Return current key positions in dp |
| `onCandidatesReady` | After `processGesture()` | Show candidates in suggestion bar |
| `onError` | On any error | Log and optionally show user message |

---

### GesturePoint (Java)

```java
public class GesturePoint {
    public final float x;          // dp
    public final float y;          // dp
    public final long timestamp;   // ms since gesture start

    public GesturePoint(float x, float y, long timestamp);
}
```

---

### KeyboardLayoutDescriptor

```java
public class KeyboardLayoutDescriptor {
    public final String languageTag;
    public final List<KeyInfo> keys;
    public final float layoutWidth;    // dp
    public final float layoutHeight;   // dp

    public static class KeyInfo {
        public final String label;
        public final int codePoint;    // Unicode. -1 for non-char keys
        public final float centerX;    // dp
        public final float centerY;    // dp
        public final float width;      // dp
        public final float height;     // dp
    }
}
```

---

### SwipeTypeCandidate (Java)

```java
public final class SwipeTypeCandidate {
    public final String word;       // recognized word
    public final float confidence;  // [0.0, 1.0]
    public final int sourceFlags;   // bitmask

    public static final int SOURCE_MAIN_DICT = 0x01;
    public static final int SOURCE_USER_DICT = 0x02;
    public static final int SOURCE_COMPLETION = 0x04;

    public boolean isFromMainDict();
}
```

---

### SwipeTypeError

```java
public class SwipeTypeError {
    public final int code;
    public final String message;

    // Predefined error constants:
    public static final SwipeTypeError DICT_NOT_FOUND;
    public static final SwipeTypeError DICT_CORRUPT;
    public static final SwipeTypeError LAYOUT_INVALID;
    public static final SwipeTypeError PATH_TOO_SHORT;
    public static final SwipeTypeError ENGINE_NOT_INITIALIZED;
    public static final SwipeTypeError JNI_ERROR;
}
```
