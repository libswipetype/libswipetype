package dev.swipetype.android;

import java.util.List;

/**
 * Interface that every keyboard adapter must implement.
 *
 * <p>This is the contract between libswipetype and any keyboard app.
 * Each keyboard project (HeliBoard, FlorisBoard, etc.) creates one implementation
 * of this interface that translates between their internal representation and
 * the generic swipetype API.</p>
 *
 * <p>Thread safety: All callbacks are invoked on the thread that called
 * {@link SwipeTypeEngine#processGesture}. Implementations must be prepared for
 * calls from any thread.</p>
 *
 * <p>Lifecycle: The adapter is passed to {@link SwipeTypeEngine#init} and retained
 * for the lifetime of the engine. The adapter must outlive the engine.</p>
 */
public interface SwipeTypeAdapter {

    /**
     * Called when the engine finishes initialization successfully.
     *
     * <p>The adapter should store the engine reference if it needs to call
     * engine methods later (e.g., to reload dictionaries).</p>
     *
     * @param engine The initialized SwipeTypeEngine instance.
     */
    void onInit(SwipeTypeEngine engine);

    /**
     * Called by the engine to obtain the current keyboard layout.
     *
     * <p>The adapter must translate the keyboard app's internal layout
     * representation into a {@link KeyboardLayoutDescriptor}. This method
     * may be called multiple times (e.g., when the user switches languages
     * or rotates the screen).</p>
     *
     * @return Current keyboard layout. Must not be null. Must contain at least
     *         one character key.
     */
    KeyboardLayoutDescriptor getKeyboardLayout();

    /**
     * Called when gesture recognition produces word candidates.
     *
     * <p>The adapter should forward these candidates to the keyboard app's
     * suggestion bar / candidate view.</p>
     *
     * @param candidates Ranked list of candidates, best first. Never null,
     *                   may be empty if recognition failed.
     */
    void onCandidatesReady(List<SwipeTypeCandidate> candidates);

    /**
     * Called when an error occurs during recognition or initialization.
     *
     * <p>The adapter should log the error and optionally show a user-facing
     * message (e.g., "Dictionary not found").</p>
     *
     * @param error The error that occurred. Never null.
     */
    void onError(SwipeTypeError error);
}
