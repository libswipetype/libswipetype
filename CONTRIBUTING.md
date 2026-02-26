# Contributing to libswipetype

Thank you for your interest in contributing to the open-source libswipetype!

## Branch Naming Convention

All branches must follow this naming pattern:

| Prefix | Usage | Example |
|--------|-------|---------|
| `feature/` | New functionality | `feature/dtw-scoring` |
| `fix/` | Bug fixes | `fix/path-normalization-crash` |
| `adapter/` | Adapter work | `adapter/heliboard-jni-bridge` |
| `docs/` | Documentation only | `docs/onboarding-guide` |
| `test/` | Test additions/fixes | `test/scorer-edge-cases` |
| `refactor/` | Code restructuring | `refactor/engine-pimpl` |

Branch names use lowercase with hyphens. No underscores, no camelCase.

## Commit Message Format

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

**Types:**
- `feat` — New feature
- `fix` — Bug fix
- `docs` — Documentation changes
- `test` — Adding or fixing tests
- `build` — Build system changes (CMake, Gradle, CI)
- `refactor` — Code restructuring without behavior change
- `perf` — Performance improvement
- `chore` — Maintenance tasks

**Scopes:** `core`, `android`, `heliboard`, `sample`, `ci`, `docs`

**Examples:**
```
feat(core): implement DTW scoring with Sakoe-Chiba band
fix(android): prevent JNI crash on null gesture path
docs(heliboard): add integration guide for HeliBoard v2.x
test(core): add edge case tests for single-point paths
build(ci): add arm64-v8a to CI build matrix
```

## Module Ownership

| Module | Primary Owner | Review Required For |
|--------|---------------|---------------------|
| `swipetype-core/include/swipetype/` | Developer A | **All changes** (joint review) |
| `swipetype-core/src/` | Developer A | Internal — single review OK |
| `swipetype-core/tests/` | Developer A | Internal — single review OK |
| `swipetype-android/` (API interfaces) | Developer B | **Interface changes** (joint review) |
| `swipetype-android/` (internals) | Developer B | Internal — single review OK |
| `adapters/heliboard/` | Developer B | Single review OK |
| `sample-app/` | Developer B | Single review OK |
| `.github/workflows/` | Both | **All changes** (joint review) |

### Stable API Rule

Files in `swipetype-core/include/swipetype/` and the `SwipeTypeAdapter.java` / `SwipeTypeEngine.java` interfaces are **stable API surfaces**. Any changes to these files require:

1. A PR with the `api-change` label
2. Approval from both developers
3. A rationale comment explaining why the change is necessary
4. Updated documentation in `docs/API.md`

## Pull Request Process

1. Create a branch following the naming convention
2. Make your changes with conventional commit messages
3. Ensure all tests pass locally (`scripts/run_tests.sh`)
4. Push and open a PR using the template
5. Request review from the appropriate owner(s)
6. Address review feedback
7. Squash-merge when approved

## Development Setup

See [docs/ONBOARDING.md](docs/ONBOARDING.md) for complete setup instructions.

## Clean-Room Notice

This project observes HeliBoard's JNI call signatures to ensure compatibility but does NOT copy any HeliBoard implementation code. All algorithms and implementations are original work. When documenting HeliBoard interface compatibility:

- Reference only public method signatures (names, parameter types, return types)
- Never copy implementation logic from HeliBoard or AOSP LatinIME
- Document the source of any interface information as "observed from public API"

## License

By contributing, you agree that your contributions will be licensed under the Apache License 2.0.
