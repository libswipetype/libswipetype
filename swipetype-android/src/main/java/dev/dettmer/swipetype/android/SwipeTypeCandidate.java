package dev.dettmer.swipetype.android;

/**
 * A word candidate produced by gesture recognition.
 *
 * <p>Candidates are ranked by confidence (highest first). This is an
 * immutable value type.</p>
 */
public final class SwipeTypeCandidate {

    /** Source flag: word from main dictionary. */
    public static final int SOURCE_MAIN_DICT = 0x01;

    /** Source flag: word from user dictionary. */
    public static final int SOURCE_USER_DICT = 0x02;

    /** Source flag: prefix completion. */
    public static final int SOURCE_COMPLETION = 0x04;

    /** The recognized word (UTF-8). */
    public final String word;

    /** Confidence score in [0.0, 1.0]. 1.0 = highest confidence. */
    public final float confidence;

    /** Source flags bitmask (SOURCE_MAIN_DICT, SOURCE_USER_DICT, etc.). */
    public final int sourceFlags;

    /**
     * Create a new candidate.
     *
     * @param word        The recognized word
     * @param confidence  Confidence score in [0.0, 1.0]
     * @param sourceFlags Source flags bitmask
     */
    public SwipeTypeCandidate(String word, float confidence, int sourceFlags) {
        this.word = word;
        this.confidence = confidence;
        this.sourceFlags = sourceFlags;
    }

    /** @return true if this candidate came from the main dictionary. */
    public boolean isFromMainDict() {
        return (sourceFlags & SOURCE_MAIN_DICT) != 0;
    }

    @Override
    public String toString() {
        return "SwipeTypeCandidate{word='" + word + "', confidence=" + confidence + "}";
    }
}
