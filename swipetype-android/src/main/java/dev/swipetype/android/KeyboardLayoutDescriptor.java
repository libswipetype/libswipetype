package dev.swipetype.android;

import java.util.List;

/**
 * Describes the complete keyboard layout for gesture recognition.
 *
 * <p>Populated by the {@link SwipeTypeAdapter} implementation from the keyboard
 * app's internal layout representation. All coordinates are in
 * density-independent pixels (dp).</p>
 */
public final class KeyboardLayoutDescriptor {

    /** BCP 47 language tag (e.g., "en-US", "de-DE"). */
    public final String languageTag;

    /** All keys on the keyboard. */
    public final List<KeyInfo> keys;

    /** Total keyboard width in dp. */
    public final float layoutWidth;

    /** Total keyboard height in dp. */
    public final float layoutHeight;

    /**
     * Create a new layout descriptor.
     *
     * @param languageTag  BCP 47 language tag
     * @param keys         List of all keys
     * @param layoutWidth  Total keyboard width in dp
     * @param layoutHeight Total keyboard height in dp
     */
    public KeyboardLayoutDescriptor(String languageTag, List<KeyInfo> keys,
                                     float layoutWidth, float layoutHeight) {
        this.languageTag = languageTag;
        this.keys = keys;
        this.layoutWidth = layoutWidth;
        this.layoutHeight = layoutHeight;
    }

    /**
     * Describes a single key on the keyboard.
     */
    public static final class KeyInfo {

        /** Display label (e.g., "a", "shift"). */
        public final String label;

        /** Unicode code point. -1 for non-character keys. */
        public final int codePoint;

        /** Key center X coordinate in dp. */
        public final float centerX;

        /** Key center Y coordinate in dp. */
        public final float centerY;

        /** Key width in dp. */
        public final float width;

        /** Key height in dp. */
        public final float height;

        /**
         * Create a new key info.
         *
         * @param label     Display label
         * @param codePoint Unicode code point (-1 for non-character keys)
         * @param centerX   Center X in dp
         * @param centerY   Center Y in dp
         * @param width     Width in dp
         * @param height    Height in dp
         */
        public KeyInfo(String label, int codePoint,
                       float centerX, float centerY,
                       float width, float height) {
            this.label = label;
            this.codePoint = codePoint;
            this.centerX = centerX;
            this.centerY = centerY;
            this.width = width;
            this.height = height;
        }

        /** @return true if this key represents a character. */
        public boolean isCharacterKey() {
            return codePoint >= 0;
        }
    }
}
