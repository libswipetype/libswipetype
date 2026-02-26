#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== libswipetype Test Runner ==="
echo ""

# ---- Core C++ Tests ----
echo ">>> Building and running swipetype-core tests..."
cd "$ROOT_DIR/swipetype-core"

cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DGLIDE_BUILD_TESTS=ON \
    2>&1 | tail -5

cmake --build build --parallel "$(nproc)" 2>&1 | tail -5

echo ">>> Running C++ unit tests..."
cd build
ctest --output-on-failure --verbose
cd "$ROOT_DIR"

echo ""
echo ">>> Core tests complete."
echo ""

# ---- Android Tests (if Gradle available) ----
if command -v ./gradlew &> /dev/null || [ -f "$ROOT_DIR/gradlew" ]; then
    echo ">>> Running Android unit tests..."
    cd "$ROOT_DIR"
    chmod +x gradlew
    ./gradlew test --stacktrace 2>&1 | tail -20
    echo ">>> Android tests complete."
else
    echo ">>> Skipping Android tests (no gradlew found)."
fi

echo ""
echo "=== All tests finished ==="
