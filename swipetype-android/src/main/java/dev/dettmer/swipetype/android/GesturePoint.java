package dev.dettmer.swipetype.android;

/**
 * A single touch point in a gesture path.
 *
 * <p>Coordinates are in the keyboard view's coordinate system using
 * density-independent pixels (dp). Origin (0,0) is the top-left corner
 * of the keyboard view.</p>
 *
 * <p>This is an immutable value type.</p>
 */
public final class GesturePoint {

    /** X coordinate in dp. */
    public final float x;

    /** Y coordinate in dp. */
    public final float y;

    /** Timestamp in milliseconds since the start of this gesture.
     *  The first point should have timestamp 0. */
    public final long timestamp;

    /**
     * Create a new gesture point.
     *
     * @param x         X coordinate in dp
     * @param y         Y coordinate in dp
     * @param timestamp Milliseconds since gesture start
     */
    public GesturePoint(float x, float y, long timestamp) {
        this.x = x;
        this.y = y;
        this.timestamp = timestamp;
    }

    @Override
    public String toString() {
        return "GesturePoint{x=" + x + ", y=" + y + ", t=" + timestamp + "}";
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof GesturePoint)) return false;
        GesturePoint that = (GesturePoint) o;
        return Float.compare(that.x, x) == 0
                && Float.compare(that.y, y) == 0
                && timestamp == that.timestamp;
    }

    @Override
    public int hashCode() {
        int result = Float.floatToIntBits(x);
        result = 31 * result + Float.floatToIntBits(y);
        result = 31 * result + Long.hashCode(timestamp);
        return result;
    }
}
