# Architecture — libswipetype

> **Version:** 0.1.0 &nbsp;|&nbsp; **Updated:** 2026-02

---

## Overview

libswipetype is a swipe/glide typing engine that converts finger gestures into word candidates. The system has three layers:

```
┌─────────────────────────────────────────────────────────────┐
│  Keyboard App (HeliBoard, FlorisBoard, sample-app, etc.)    │
└───────────────────────────┬─────────────────────────────────┘
                            │ SwipeTypeAdapter interface
┌───────────────────────────▼─────────────────────────────────┐
│  swipetype-android  (Java + JNI)                            │
│  SwipeTypeEngine → GestureLibJNI.cpp → native calls         │
└───────────────────────────┬─────────────────────────────────┘
                            │ C ABI (JNI function pointers)
┌───────────────────────────▼─────────────────────────────────┐
│  swipetype-core  (C++17, zero dependencies)                 │
│  GestureEngine → PathProcessor → IdealPathGenerator →       │
│                  Scorer → DictionaryLoader                   │
└─────────────────────────────────────────────────────────────┘
```

---

## Module Map

| Module | Language | Artifact | Purpose |
|--------|----------|----------|---------|
| `swipetype-core/` | C++17 | `libswipetype-core.a` | Pure algorithm library. Zero external dependencies. |
| `swipetype-android/` | Java + JNI | `swipetype-android.aar` | Android AAR wrapping the core via JNI |
| `adapters/heliboard/` | Java | Source files | Reference adapter for the HeliBoard keyboard |
| `sample-app/` | Java | Debug APK | Minimal IME demonstrating the full integration |
| `scripts/` | Python | CLI tools | Dictionary generation (`gen_dict.py`) |
| `test-data/` | JSON, TSV | Test fixtures | Keyboard layouts, gesture scenarios, word lists |

---

## Core Recognition Pipeline

The recognition pipeline runs inside `GestureEngine::recognize()`. All steps execute synchronously on the calling thread.

```
Raw touch points (dp)
        │
        ▼
┌───────────────────┐
│  1. PathProcessor  │  deduplicate → resample(64) → normalize([0,1])
│     normalize()    │
└───────┬───────────┘
        │  GesturePath (64 NormalizedPoints)
        ▼
┌───────────────────┐
│  2. Start/End Key  │  Find nearest key to first & last touch point
│     Detection      │
└───────┬───────────┘
        │  startChar, endChar
        ▼
┌───────────────────┐
│  3. Candidate      │  Filter dictionary by start+end char, then by
│     Filtering      │  estimated word length (key-transition count ±3)
└───────┬───────────┘
        │  filtered DictionaryEntry list
        ▼
┌───────────────────┐
│  4. Ideal Path     │  For each candidate word, generate the "ideal"
│     Generation     │  swipe path through key centers. Cached per word.
│     (IPG)          │
└───────┬───────────┘
        │  GesturePath per candidate
        ▼
┌───────────────────┐
│  5. DTW Scoring    │  Compute DTW distance between gesture and each
│     (Scorer)       │  ideal path using Sakoe-Chiba band (W=6)
└───────┬───────────┘
        │  (word, dtwDistance) pairs
        ▼
┌───────────────────┐
│  6. Confidence     │  Normalize DTW, apply frequency weight (adaptive α),
│     Computation    │  compute confidence = 1 - finalScore
└───────┬───────────┘
        │  GestureCandidate list
        ▼
┌───────────────────┐
│  7. Sort & Prune   │  Sort by confidence descending, truncate to maxCandidates
└───────┬───────────┘
        │
        ▼
  vector<GestureCandidate>
```

### Step 1: Path Normalization (PathProcessor)

**File:** `swipetype-core/src/PathProcessor.cpp`

Three sub-steps:

1. **Deduplicate** — Remove consecutive points closer than `MIN_POINT_DISTANCE_DP` (2.0 dp). Always keeps first and last point.
2. **Resample** — Equidistant resampling to exactly `RESAMPLE_COUNT` (64) points along the path arc. Uses the $1 Unistroke algorithm (Wobbrock et al., 2007).
3. **Bounding-box normalize** — Scale coordinates to [0.0, 1.0] preserving aspect ratio. Normalizes time to [0.0, 1.0].

Output: `GesturePath` with 64 `NormalizedPoint`s plus metadata (arc length, start/end key indices, aspect ratio).

### Step 2: Start/End Key Detection

Uses `KeyboardLayout::findNearestKey()` on the first and last raw touch points (not the resampled points). Maps to lowercase ASCII characters for dictionary lookup.

### Step 3: Candidate Filtering

Three-tier filter cascade:

1. `getEntriesWithStartEnd(startChar, endChar)` — words matching both start and end character
2. `getEntriesStartingWith(startChar)` — fallback if tier 1 yields nothing
3. `getAllEntries()` — last resort brute-force

After tier selection, a **word-length filter** eliminates candidates whose character count differs from the estimated word length by more than `LENGTH_FILTER_TOLERANCE` (±3.0). The estimate uses **key-transition counting**: walk the raw gesture path, snap each point to its nearest key, count distinct key transitions.

### Step 4: Ideal Path Generation (IdealPathGenerator)

**File:** `swipetype-core/src/IdealPathGenerator.cpp`

For each dictionary word, generates the "perfect" swipe path by connecting key centers with straight lines, then resampling to 64 points. Duplicate consecutive keys (e.g., "l" in "hello") are collapsed to a single key center.

Results are **cached** per word (invalidated when the layout changes via `setLayout()`).

### Step 5: DTW Scoring (Scorer)

**File:** `swipetype-core/src/Scorer.cpp`

Computes Dynamic Time Warping (DTW) distance between the gesture path and each ideal path. Uses:

- **Sakoe-Chiba band** with width `W = ceil(0.10 × 64) = 6` to constrain the warping window
- **Two-row rolling array** for O(N × W) time and O(N) space
- **Euclidean distance** between NormalizedPoint(x, y) pairs as the local cost function
- Final DTW divided by path length (N=64) for per-point normalization

### Step 6: Confidence Computation

```
maxDTW = max(maxCandidateDTW, MAX_DTW_FLOOR)    // floor only for single-candidate; multi uses raw maxDTW
normalizedDTW = min(1.0, dtwDistance / maxDTW)
normalizedFreq = frequency / maxFrequency

// Adaptive alpha: proportional scaling based on DTW range
effectiveAlpha = α × max(0.1, rawRange / 0.5)

finalScore = (1 - effectiveAlpha) × normalizedDTW + effectiveAlpha × (1 - normalizedFreq)
confidence = 1.0 - clamp(finalScore, 0, 1)
```

### Step 7: Sort & Prune

Sort by confidence descending. Truncate to `maxCandidates` (default 8, max 20).

---

## Android Integration Layer

### JNI Bridge

**File:** `swipetype-android/src/main/cpp/GestureLibJNI.cpp`

Translates Java arrays into C++ types and vice versa. The JNI layer is thin — it only marshals data and forwards to `GestureEngine`.

| JNI Function | Calls |
|-------------|-------|
| `nativeInit()` | `GestureEngine::init()` |
| `nativeInitWithData()` | `GestureEngine::initWithData()` |
| `nativeRecognize()` | `GestureEngine::recognize()` |
| `nativeUpdateLayout()` | `GestureEngine::updateLayout()` |
| `nativeShutdown()` | `GestureEngine::shutdown()` |

The native library is named `glide_jni` and loaded via `System.loadLibrary("glide_jni")`.

### SwipeTypeEngine (Java)

**File:** `swipetype-android/src/main/java/dev/dettmer/swipetype/android/SwipeTypeEngine.java`

Manages the lifecycle:
1. `init(context, adapter)` — stores context and adapter reference
2. `loadDictionary(tag, stream)` — copies stream to cache, queries adapter for layout, calls `nativeInit()`
3. `processGesture(points)` — converts `List<GesturePoint>` to arrays, calls `nativeRecognize()`, wraps results in `SwipeTypeCandidate` list, delivers via `adapter.onCandidatesReady()`
4. `notifyLayoutChanged()` — re-queries layout and calls `nativeUpdateLayout()`
5. `shutdown()` — calls `nativeShutdown()`

All public methods are `synchronized`.

---

## Dictionary Format

Binary `.glide` format produced by `scripts/gen_dict.py` from a TSV word list.

### File Layout

```
┌──────────────────────────────┐
│  Header (32 bytes, fixed)    │
├──────────────────────────────┤
│  Entry 0                     │
│  Entry 1                     │
│  ...                         │
│  Entry N-1                   │
└──────────────────────────────┘
```

### Header (32 bytes)

| Offset | Size | Field | Value |
|--------|------|-------|-------|
| 0 | 4 | magic | `0x474C4944` ("GLID", little-endian) |
| 4 | 2 | version | `1` |
| 6 | 2 | flags | `0` (reserved) |
| 8 | 4 | entryCount | number of words |
| 12 | 2 | langLen | length of language tag |
| 14 | N | langTag | UTF-8 language tag (e.g., "en") |
| 14+N | padding | — | zeros to byte 32 |

### Entry Format

| Size | Field | Description |
|------|-------|-------------|
| 1 | wordLen | Length of word in bytes |
| wordLen | word | UTF-8 word string |
| 4 | frequency | Little-endian uint32 |
| 1 | flags | `DICT_FLAG_PROPER_NOUN` (0x01), `DICT_FLAG_PROFANITY` (0x02) |

---

## Directory Structure

```
libswipetype/
├── swipetype-core/                  # C++17 core library
│   ├── CMakeLists.txt               # Build config (standalone or via Gradle)
│   ├── include/swipetype/           # Public headers
│   │   ├── GestureEngine.h          # Main API
│   │   ├── GesturePath.h            # Path data structures
│   │   ├── GesturePoint.h           # Point types (raw + normalized)
│   │   ├── GestureCandidate.h       # Recognition result
│   │   ├── KeyboardLayout.h         # Layout descriptor
│   │   ├── PathProcessor.h          # Path normalization
│   │   ├── IdealPathGenerator.h     # Reference path generation
│   │   ├── Scorer.h                 # DTW scoring
│   │   ├── DictionaryLoader.h       # Dictionary I/O
│   │   └── SwipeTypeTypes.h         # Shared types / constants
│   ├── src/                         # Implementation files
│   │   ├── GestureEngine.cpp
│   │   ├── PathProcessor.cpp
│   │   ├── IdealPathGenerator.cpp
│   │   ├── Scorer.cpp
│   │   ├── DictionaryLoader.cpp
│   │   └── AdjacencyMap.cpp
│   └── tests/                       # Google Test suite
│       ├── CMakeLists.txt
│       ├── TestHelpers.h
│       ├── PathProcessorTest.cpp
│       ├── ScorerTest.cpp
│       ├── DictionaryLoaderTest.cpp
│       ├── GestureEngineTest.cpp
│       └── IdealPathGeneratorTest.cpp
├── swipetype-android/               # Android AAR module
│   ├── build.gradle                 # Gradle + CMake NDK build
│   └── src/main/
│       ├── cpp/GestureLibJNI.cpp    # JNI bridge
│       └── java/dev/dettmer/swipetype/android/
│           ├── SwipeTypeEngine.java
│           ├── SwipeTypeAdapter.java
│           ├── GesturePoint.java
│           ├── KeyboardLayoutDescriptor.java
│           ├── SwipeTypeCandidate.java
│           └── SwipeTypeError.java
├── adapters/
│   └── heliboard/                   # Reference adapter for HeliBoard
│       └── src/main/java/dev/dettmer/swipetype/adapters/heliboard/
│           └── HeliboardSwipeTypeAdapter.java
├── sample-app/                      # Minimal sample IME
│   ├── build.gradle
│   └── src/main/java/dev/dettmer/swipetype/sample/
│       ├── MainActivity.java
│       ├── SampleInputMethodService.java
│       └── SampleKeyboardView.java
├── scripts/
│   └── gen_dict.py                  # TSV → .glide dictionary generator
├── test-data/
│   ├── en-us-full.tsv               # 302-word English word list
│   ├── gesture-scenarios.json       # Test gesture definitions
│   └── qwerty-standard.json         # Standard QWERTY layout definition
├── docs/                            # Documentation (this directory)
│   ├── API.md
│   ├── ARCHITECTURE.md (this file)
│   ├── ONBOARDING.md
│   └── HOW_TO_WRITE_AN_ADAPTER.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── LICENSE (Apache 2.0)
└── README.md
```

---

## Design Decisions

### Why pImpl?

All core classes (`GestureEngine`, `PathProcessor`, `IdealPathGenerator`, `Scorer`, `DictionaryLoader`) use the pointer-to-implementation (pImpl) idiom. This:

1. **Hides implementation details** from public headers — keyboard apps only include headers, never see internal types
2. **Enables binary compatibility** — internal changes don't break ABI
3. **Reduces compile times** — changes to `.cpp` files don't force recompilation of dependents

### Why DTW instead of neural networks?

DTW is deterministic, explainable, and requires zero training data. It runs in under 5ms per candidate on mid-range Android devices. The tradeoff is lower accuracy on edge cases (very short words, similar shapes), which is acceptable for a v0.1 library.

### Why bounding-box normalization?

Normalizing gesture and ideal paths to a [0, 1] bounding box makes the DTW comparison scale-invariant. A gesture on a tablet (high dp) produces the same normalized path as one on a phone (low dp). Aspect ratio is preserved to disambiguate horizontal vs. vertical swipes.

### Why key-transition counting for length estimation?

The original arc-length heuristic divided total gesture arc length by average key spacing. This overestimated drastically for zigzag words (e.g., "hello" estimated at 17+ characters). Key-transition counting walks the raw path, snaps each point to its nearest key, and counts distinct transitions — yielding an estimate that closely matches actual word length regardless of path geometry.

### Why adaptive frequency weight?

A fixed frequency weight (α = 0.30) allows high-frequency words to dominate when DTW scores are compressed (all candidates match similarly well). The adaptive approach scales α proportionally with the DTW range (`effectiveAlpha *= max(0.1, rawRange/0.5)`), so when scores are tightly clustered, frequency influence shrinks smoothly. This ensures shape remains the primary discriminator unless there's a clear shape winner.

---

## Performance Characteristics

| Operation | Time | Notes |
|-----------|------|-------|
| `normalize()` | < 1ms | Resample 64 points |
| `getIdealPath()` (cached) | < 10μs | Hash map lookup |
| `getIdealPath()` (miss) | < 0.5ms | Generate + resample |
| `computeDTWDistance()` | < 2ms | 64×64 with band W=6 |
| `recognize()` (full pipeline) | < 50ms | With 302-word dictionary |

Memory: ~10KB per loaded dictionary word (entry + cached ideal path). 302 words ≈ 3MB.

---

## Thread Safety

| Component | Thread Safety |
|-----------|---------------|
| `GestureEngine` (C++) | NOT thread-safe. External sync required |
| `SwipeTypeEngine` (Java) | All public methods `synchronized` |
| `DictionaryLoader` (after load) | Read-only operations thread-safe |
| `PathProcessor` | Stateless after construction — thread-safe |
| `Scorer` | Stateless after `configure()` — thread-safe |
| `IdealPathGenerator` | NOT thread-safe (mutable cache) |
