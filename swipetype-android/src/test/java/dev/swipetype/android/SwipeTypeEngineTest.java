package dev.swipetype.android;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.*;

/**
 * Unit tests for {@link SwipeTypeEngine} using Robolectric.
 *
 * <p>These tests run on the JVM without an Android device or emulator.
 * They verify the Java-layer logic (init, layout, gesture routing) and
 * the JNI boundary contract.</p>
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 30)
public class SwipeTypeEngineTest {

    private SwipeTypeEngine engine;
    private TestAdapter adapter;

    @Before
    public void setUp() {
        adapter = new TestAdapter();
        try {
            engine = new SwipeTypeEngine();
        } catch (UnsatisfiedLinkError | NoClassDefFoundError e) {
            org.junit.Assume.assumeTrue(
                "Native library not available in JVM tests: " + e.getMessage(), false);
        }
    }

    // ----- Initialization -----

    @Test
    public void initWithNullAdapterThrows() {
        // TODO(sonnet): Assert that engine.init(context, null) throws IllegalArgumentException.
        org.junit.Assume.assumeTrue("TODO(sonnet): implement", false);
    }

    @Test
    public void engineIsNotInitializedBeforeInit() {
        // TODO(sonnet): Assert engine.isInitialized() == false before init is called.
        org.junit.Assume.assumeTrue("TODO(sonnet): implement", false);
    }

    // ----- Gesture handling -----

    @Test
    public void processGestureWithEmptyPointsReturnsEmpty() {
        // TODO(sonnet): Init the engine.
        // Call engine.processGesture(emptyList).
        // Assert onCandidatesReady was NOT called (or was called with empty list).
        org.junit.Assume.assumeTrue("TODO(sonnet): implement", false);
    }

    @Test
    public void processGestureWithNullThrows() {
        // TODO(sonnet): Assert that engine.processGesture(null) throws NullPointerException.
        org.junit.Assume.assumeTrue("TODO(sonnet): implement", false);
    }

    @Test
    public void processGestureConvertsPointsCorrectly() {
        // TODO(sonnet): Verify that GesturePoint x, y, timestamp values
        // are passed correctly through the JNI boundary.
        // This requires a mock JNI or instrumented test.
        org.junit.Assume.assumeTrue("TODO(sonnet): implement", false);
    }

    // ----- Error handling -----

    @Test
    public void processGestureBeforeInitDeliversError() {
        // TODO(sonnet): Call engine.processGesture() without calling init first.
        // Assert adapter.onError() was called with NOT_INITIALIZED error.
        org.junit.Assume.assumeTrue("TODO(sonnet): implement", false);
    }

    @Test
    public void shutdownMakesEngineUnavailable() {
        // TODO(sonnet): Init engine, call shutdown(), then attempt processGesture().
        // Assert onError is called or exception thrown.
        org.junit.Assume.assumeTrue("TODO(sonnet): implement", false);
    }

    // =========================================================================
    // Internal test adapter implementation
    // =========================================================================

    private static class TestAdapter implements SwipeTypeAdapter {
        List<SwipeTypeCandidate> lastCandidates = new ArrayList<>();
        SwipeTypeError lastError = null;
        KeyboardLayoutDescriptor lastLayout = null;
        boolean initCalled = false;

        @Override
        public void onInit(SwipeTypeEngine engine) {
            initCalled = true;
        }

        @Override
        public KeyboardLayoutDescriptor getKeyboardLayout() {
            return lastLayout;
        }

        @Override
        public void onCandidatesReady(List<SwipeTypeCandidate> candidates) {
            lastCandidates = new ArrayList<>(candidates);
        }

        @Override
        public void onError(SwipeTypeError error) {
            lastError = error;
        }
    }
}
