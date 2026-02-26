package dev.dettmer.swipetype.android;

/**
 * Error codes from the swipetype engine.
 *
 * <p>Delivered to the adapter via {@link SwipeTypeAdapter#onError(SwipeTypeError)}.</p>
 */
public enum SwipeTypeError {

    /** Dictionary file not found at the specified path. */
    DICT_NOT_FOUND(1, "Dictionary file not found"),

    /** Dictionary file is corrupt or has an invalid format. */
    DICT_CORRUPT(2, "Dictionary file is corrupt or invalid format"),

    /** Dictionary format version is not supported by this library version. */
    DICT_VERSION_MISMATCH(3, "Dictionary format version not supported"),

    /** Keyboard layout is invalid (no keys, zero dimensions, etc.). */
    LAYOUT_INVALID(4, "Keyboard layout is invalid or empty"),

    /** Gesture path has too few points for recognition. */
    PATH_TOO_SHORT(5, "Gesture path has too few points"),

    /** Engine is not initialized — call init() and loadDictionary() first. */
    ENGINE_NOT_INITIALIZED(6, "Engine not initialized"),

    /** Out of memory during processing. */
    OUT_OF_MEMORY(7, "Out of memory during processing"),

    /** Internal JNI bridge error. */
    JNI_ERROR(100, "JNI bridge error"),

    /** Unknown error. */
    UNKNOWN(999, "Unknown error");

    /** Numeric error code. */
    public final int code;

    /** Human-readable error description. */
    public final String message;

    SwipeTypeError(int code, String message) {
        this.code = code;
        this.message = message;
    }

    /**
     * Look up a SwipeTypeError by its numeric code.
     *
     * @param code Error code from native layer
     * @return Matching SwipeTypeError, or UNKNOWN if code not recognized
     */
    public static SwipeTypeError fromCode(int code) {
        for (SwipeTypeError e : values()) {
            if (e.code == code) return e;
        }
        return UNKNOWN;
    }
}
