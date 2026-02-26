# libswipetype

An open-source, keyboard-agnostic glide (swipe) typing engine.

**libswipetype** provides accurate gesture-based word recognition for soft keyboards on Android. The core algorithm is written in portable C++17 with no external dependencies. An Android JNI wrapper and a reference HeliBoard adapter are included.

> **Status:** Pre-release — actively under development.

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Keyboard App (HeliBoard, custom, …)            │
├─────────────────────────────────────────────────┤
│  Adapter Layer (Java)                           │
│  adapters/heliboard/  ←──  SwipeTypeAdapter iface  │
├─────────────────────────────────────────────────┤
│  swipetype-android (JNI bridge)                 │
│  SwipeTypeEngine.java  ↔  GestureLibJNI.cpp     │
├─────────────────────────────────────────────────┤
│  swipetype-core (pure C++17)                    │
│  PathProcessor → IdealPathGen → Scorer          │
│  DictionaryLoader      GestureEngine            │
└─────────────────────────────────────────────────┘
```

### Layers

| Layer | Language | Purpose |
|-------|----------|---------|
| **swipetype-core** | C++17 | Gesture recognition algorithms, dictionary loading |
| **swipetype-android** | Java + JNI | Android library wrapping swipetype-core |
| **adapters/heliboard** | Java + JNI | HeliBoard-specific integration adapter |
| **sample-app** | Java | Minimal test keyboard app |

## Quick Start

### Prerequisites

- CMake 3.18+
- C++17 compiler (GCC 9+ or Clang 10+)
- Android Studio Hedgehog+ (for Android modules)
- Android NDK r25+ (installed via SDK Manager)

### Build & Test — Core Library (Desktop)

```bash
cd swipetype-core
mkdir build && cd build
cmake .. -DSWIPETYPE_BUILD_TESTS=ON
cmake --build . -j$(nproc)
ctest --output-on-failure
```

### Build — Android Library

```bash
./gradlew :swipetype-android:assembleRelease
```

The AAR is produced at `swipetype-android/build/outputs/aar/`.

### Build — HeliBoard Adapter

```bash
./gradlew :adapters:heliboard:assembleRelease
```

### Generate a Dictionary

```bash
python3 scripts/gen_dict.py test-data/en-us-sample.tsv test-data/en-us-sample.glide --lang en-US
```

## Integration

### Using the Android Library

```java
import dev.dettmer.swipetype.android.*;

// 1. Create engine & adapter
SwipeTypeEngine engine = new SwipeTypeEngine();
MyAdapter adapter = new MyAdapter();  // implements SwipeTypeAdapter

// 2. Initialize and load dictionary
engine.init(context, adapter);
InputStream dictStream = context.getResources().openRawResource(R.raw.en_us_sample);
engine.loadDictionary("en-US", dictStream);

// 3. Recognize gestures (results delivered via adapter callback)
List<GesturePoint> points = collectTouchPoints();
engine.processGesture(points);

// In your adapter's onCandidatesReady():
@Override
public void onCandidatesReady(List<SwipeTypeCandidate> candidates) {
    String word = candidates.get(0).word;           // public field
    float confidence = candidates.get(0).confidence; // public field
}
```

### Writing a Custom Adapter

Implement `SwipeTypeAdapter`:

```java
public class MyKeyboardAdapter implements SwipeTypeAdapter {
    @Override
    public void onInit(SwipeTypeEngine engine) {
        // Engine is ready — load dictionary, etc.
    }

    @Override
    public KeyboardLayoutDescriptor getKeyboardLayout() {
        // Return your keyboard's key positions (in dp)
        return new KeyboardLayoutDescriptor("en-US", keys, width, height);
    }

    @Override
    public void onCandidatesReady(List<SwipeTypeCandidate> candidates) {
        // Display candidates to the user
        String topWord = candidates.get(0).word;
    }

    @Override
    public void onError(SwipeTypeError error) {
        Log.e("Adapter", "Error: " + error.message);
    }
}
```

See [docs/HOW_TO_WRITE_AN_ADAPTER.md](docs/HOW_TO_WRITE_AN_ADAPTER.md) for a full guide.

## Repository Structure

```
libswipetype/
├── swipetype-core/             # Pure C++17 engine
│   ├── include/swipetype/      # Public headers
│   ├── src/                    # Implementation
│   └── tests/                  # Google Test suite
├── swipetype-android/          # Android JNI wrapper
│   ├── src/main/java/          # Java API
│   └── src/main/cpp/           # JNI bridge
├── adapters/heliboard/         # HeliBoard reference adapter
├── sample-app/                 # Minimal test app
├── scripts/                    # gen_dict.py, run_tests.sh
├── test-data/                  # Sample dictionaries & gesture scenarios
└── docs/                       # Documentation
```

## Algorithm Overview

1. **Preprocess** — Deduplicate → resample to 64 evenly-spaced points → normalize to [0, 1]
2. **Filter** — Select dictionary words matching the gesture's start key, end key, and estimated length
3. **Generate ideal paths** — For each candidate word, generate the "ideal" gesture path through key centers
4. **Score** — Compare gesture path against ideal paths using Dynamic Time Warping (DTW) with Sakoe-Chiba band constraint
5. **Rank** — Combine DTW distance with word frequency: `finalScore = (1 − α) · shapeScore + α · freqScore`

## Performance Targets

| Metric | Budget |
|--------|--------|
| Recognition latency | < 50ms on Snapdragon 665 |
| Memory (50k words) | < 30 MB |
| Init time | < 200ms |
| Minimum Android API | 21 (Lollipop) |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development workflow, branch naming, and code review process.

## License

Licensed under the [Apache License 2.0](LICENSE).

---

<p align="center">Made with ❤️ by <a href="https://github.com/inventory69">inventory69</a> & <a href="https://github.com/Hyphonical">Hyphonical</a></p>