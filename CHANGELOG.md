# Changelog

All notable changes to libswipetype are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [0.1.0-dev] — 2026-02-26

Initial release — Phases 1–11 complete.

### Added
- **swipetype-core** — C++17 gesture recognition library
  - `PathProcessor` — deduplicate, resample (64 points), bounding-box normalize
  - `IdealPathGenerator` — reference path generation with caching
  - `Scorer` — DTW distance with Sakoe-Chiba band (W=6)
  - `DictionaryLoader` — binary `.glide` format reader
  - `GestureEngine` — recognition pipeline orchestrator
  - `AdjacencyMap` — key adjacency computation (unused in scoring, for future use)
  - 48 Google Test unit tests (all passing)

- **swipetype-android** — Android AAR module
  - `SwipeTypeEngine` — Java lifecycle manager
  - `SwipeTypeAdapter` — keyboard integration interface
  - `GesturePoint`, `KeyboardLayoutDescriptor`, `SwipeTypeCandidate`, `SwipeTypeError` — data types
  - `GestureLibJNI.cpp` — JNI bridge

- **adapters/heliboard** — reference adapter for HeliBoard keyboard
  - `HeliboardSwipeTypeAdapter` — translates HeliBoard's `ProximityInfo`/`InputPointers` to swipetype API

- **sample-app** — minimal Input Method Service demo
  - `SampleKeyboardView` — custom QWERTY keyboard renderer with gesture trail
  - `SampleInputMethodService` — IME integration using `SwipeTypeAdapter`
  - `MainActivity` — setup guide with IME enable/switch buttons

- **scripts/gen_dict.py** — TSV-to-`.glide` dictionary generator
- **test-data/** — 302-word English dictionary, QWERTY layout JSON, gesture scenarios

- **Documentation (Phase 9)**
  - `docs/API.md` — full C++ and Java API reference
  - `docs/ARCHITECTURE.md` — system architecture, pipeline diagram, design decisions
  - `docs/ONBOARDING.md` — developer onboarding guide (build, test, run)
  - `docs/HOW_TO_WRITE_AN_ADAPTER.md` — step-by-step adapter integration guide
  - `CHANGELOG.md` (this file)

- **Sample App Theme Fix (Phase 10)**
  - `styles.xml` with `AppTheme` and `SwipeTypeImeTheme`
  - Transparent window background prevents system theme bleed-through

- **Structural Accuracy Fixes (Phase 11)** — 3 algorithmic improvements
  - Key-transition word length estimation (replaced arc-length heuristic)
  - Absolute DTW normalization floor for single-candidate sets
  - Adaptive frequency weight: `effectiveAlpha *= max(0.1, rawRange/0.5)`
  - 3 new regression tests

- **CI/CD** — GitHub Actions workflows
  - `ci.yml` — Core C++ tests, Android build, NDK ABI matrix (arm64, armv7, x86_64)
  - `release.yml` — tag-triggered AAR release

### Fixed (Phase 8 — BUG-1 through BUG-6)
- BUG-1: Dictionary loading moved from `onInit()` callback to `onCreate()` (eliminated infinite loop)
- BUG-2: Keyboard layout now uses even key distribution
- BUG-3: Dark theme with explicit colors (hardcoded in `SampleKeyboardView`)
- BUG-4: `maxDTWFloor = 3.0` for single-candidate normalization
- BUG-5: Frequency weight set to `α = 0.30` to balance shape vs. frequency
- BUG-6: Dictionary expanded from 10 to 302 words
