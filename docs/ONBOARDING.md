# Developer Onboarding — libswipetype

> Get from zero to a working build + passing tests in under 15 minutes.

---

## Prerequisites

| Tool | Version | Check |
|------|---------|-------|
| **Android Studio** or IntelliJ IDEA | 2023.1+ | `studio --version` |
| **JDK** | 17+ | `java -version` |
| **Android SDK** | API 34 (compileSdk) | SDK Manager |
| **Android NDK** | 25.2.9519653 | SDK Manager → SDK Tools → NDK |
| **CMake** | 3.18+ | `cmake --version` |
| **Python** | 3.8+ | `python3 --version` (for dictionary generation) |
| **Git** | 2.x | `git --version` |

Optional:

| Tool | Purpose |
|------|---------|
| **clang-format** | Code formatting (`.clang-format` in repo root) |
| **adb** | On-device testing |

---

## Quick Start

### 1. Clone the repository

```bash
git clone https://github.com/libswipetype/libswipetype.git
cd libswipetype
```

### 2. Build & test the C++ core (standalone)

The C++ core has zero external dependencies (Google Test is fetched automatically).

```bash
cd swipetype-core
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DSWIPETYPE_BUILD_TESTS=ON
cmake --build . -j$(nproc)
ctest --output-on-failure
```

You should see **48 tests passed, 0 failed**.

### 3. Build the full Android project

Open the project root in Android Studio, or build from the command line:

```bash
cd /path/to/libswipetype
./gradlew build
```

This builds:
- `swipetype-core` (via CMake, triggered by Gradle)
- `swipetype-android` AAR
- `adapters/heliboard` module
- `sample-app` debug APK

### 4. Run the sample app

```bash
# Install on a connected device or emulator
./gradlew :sample-app:installDebug

# Or use Android Studio: Run → sample-app
```

Then on the device:
1. Open "SwipeType Demo" app
2. Tap "Enable IME" → enable "SwipeType Sample IME" in Settings
3. Return to the app
4. Tap the text field → switch to the SwipeType keyboard via the globe icon
5. Draw a swipe gesture across the keys

### 5. Generate a dictionary (optional)

The sample app already includes `en_us_sample.glide` in `res/raw/`. To regenerate or create a new dictionary:

```bash
python3 scripts/gen_dict.py \
    --input test-data/en-us-full.tsv \
    --output sample-app/src/main/res/raw/en_us_sample.glide \
    --language en-US
```

TSV format: `word<TAB>frequency` per line. No header row.

---

## Project Structure at a Glance

```
libswipetype/
├── swipetype-core/        ← C++ library (the brain)
├── swipetype-android/     ← Android AAR (JNI bridge)
├── adapters/heliboard/    ← Reference adapter for HeliBoard
├── sample-app/            ← Minimal IME demo
├── scripts/               ← Dictionary generation
├── test-data/             ← Test fixtures (layouts, gestures, word lists)
├── docs/                  ← Documentation (you are here)
├── build.gradle           ← Root Gradle build
└── settings.gradle        ← Module declarations
```

For a detailed breakdown, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Key Concepts

### Coordinate System

Everything uses **density-independent pixels (dp)**, relative to the top-left corner of the keyboard view. The keyboard app provides key positions in dp; the user's touch events must also be in dp. This ensures the algorithm is screen-density-independent.

### Adapter Pattern

The library doesn't depend on any specific keyboard app. Instead, each keyboard provides an **adapter** implementing `SwipeTypeAdapter`:

```
┌──────────────┐     SwipeTypeAdapter     ┌──────────────┐
│  HeliBoard   │──────────────────────────│  SwipeType   │
│  FlorisBoard │  getKeyboardLayout()     │  Engine      │
│  Your App    │  onCandidatesReady()     │              │
└──────────────┘  onError()               └──────────────┘
```

### Recognition Pipeline (TL;DR)

1. Raw touch points → normalize to 64 equidistant points in [0,1] box
2. Find start/end keys → filter dictionary candidates
3. For each candidate: generate ideal path through key centers → compute DTW distance
4. Rank by combined shape + frequency score → return top N

---

## Running Tests

### C++ Tests (Google Test)

```bash
cd swipetype-core/build
ctest --output-on-failure
```

Or run individual test suites:

```bash
./swipetype-core-tests --gtest_filter="GestureEngineTest.*"
./swipetype-core-tests --gtest_filter="PathProcessorTest.*"
./swipetype-core-tests --gtest_filter="ScorerTest.*"
```

### Android Tests

```bash
# Unit tests (JVM)
./gradlew :swipetype-android:test

# Connected tests (requires device/emulator)
./gradlew :swipetype-android:connectedAndroidTest
```

> **Note:** Some Java unit tests are currently disabled (skipped via `Assume.assumeTrue(false)`) pending JNI native library loading on the host JVM. The C++ tests provide full pipeline coverage.

---

## Common Development Workflows

### Modifying the recognition algorithm

1. Edit files in `swipetype-core/src/`
2. Rebuild & test: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
3. The Gradle build will automatically rebuild the native library for Android

### Adding a new word to the test dictionary

1. Add `word<TAB>frequency` to `test-data/en-us-full.tsv`
2. Regenerate: `python3 scripts/gen_dict.py --input test-data/en-us-full.tsv --output sample-app/src/main/res/raw/en_us_sample.glide`
3. Rebuild the sample app

### Tuning scoring parameters

Edit `ScoringConfig` defaults in `swipetype-core/include/swipetype/SwipeTypeTypes.h`, or configure at runtime:

```cpp
ScoringConfig config;
config.frequencyWeight = 0.20f;    // reduce frequency influence
config.lengthFilterTolerance = 4.0f; // widen length filter
engine.configure(config);
```

### Writing a new adapter

See [HOW_TO_WRITE_AN_ADAPTER.md](HOW_TO_WRITE_AN_ADAPTER.md).

---

## Debugging Tips

### Logcat tags

| Tag | Source | What it shows |
|-----|--------|---------------|
| `SampleIME` | `SampleInputMethodService` | Layout size, gesture bounding box, top candidate |
| `SwipeTypeEngine` | `SwipeTypeEngine.java` | Init, dict load, JNI errors |
| `GestureLibJNI` | `GestureLibJNI.cpp` | Native-side errors, candidate counts |

### View gesture coordinates

The sample IME logs the gesture bounding box on every swipe:

```
D SampleIME: Layout 360x260 dp | gesture x=[48..288] y=[26..134] 47 pts
```

This helps verify coordinate alignment between the keyboard view and the engine.

### Examine DTW scores

Enable verbose logging in `GestureEngine.cpp` (define `SWIPETYPE_DEBUG`) to see per-candidate DTW scores, normalized values, and final confidence.

---

## Gotchas

1. **Native library name is `glide_jni`**, not `swipetype`. If you see `UnsatisfiedLinkError`, check that the `.so` files are in the correct `jniLibs/` directory.

2. **Dictionary must be loaded before `processGesture()`**. The `onInit()` callback fires only after `loadDictionary()` succeeds — do NOT call `loadDictionary()` from inside `onInit()` (infinite loop).

3. **Coordinates must be in dp**, not pixels. If candidates are wrong or empty, check that touch coordinates and layout key positions use the same unit.

4. **The C++ library is NOT thread-safe**. The Java `SwipeTypeEngine` handles synchronization, but if you call the C++ API directly, you must synchronize externally.

5. **Duplicate package `dev.swipetype`** exists in the codebase alongside `dev.dettmer.swipetype`. Both work identically — the duplicates will be consolidated in Phase 12.
