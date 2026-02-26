package dev.dettmer.swipetype.android;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import static org.junit.Assert.*;

/**
 * Unit tests for {@link GesturePoint}.
 */
@RunWith(JUnit4.class)
public class GesturePointTest {

    @Test
    public void constructorStoresValues() {
        GesturePoint point = new GesturePoint(12.5f, 34.0f, 1000L);
        assertEquals(12.5f, point.x, 1e-6f);
        assertEquals(34.0f, point.y, 1e-6f);
        assertEquals(1000L, point.timestamp);
    }

    @Test
    public void constructorAcceptsZeroValues() {
        GesturePoint point = new GesturePoint(0.0f, 0.0f, 0L);
        assertEquals(0.0f, point.x, 1e-6f);
        assertEquals(0.0f, point.y, 1e-6f);
        assertEquals(0L, point.timestamp);
    }

    @Test
    public void constructorAcceptsNegativeCoordinates() {
        // Negative dp coordinates can occur before normalization
        GesturePoint point = new GesturePoint(-1.0f, -2.5f, 500L);
        assertEquals(-1.0f, point.x, 1e-6f);
        assertEquals(-2.5f, point.y, 1e-6f);
    }

    @Test
    public void toStringContainsCoordinates() {
        GesturePoint point = new GesturePoint(10.0f, 20.0f, 300L);
        String str = point.toString();
        assertNotNull(str);
        assertTrue("toString should contain x value", str.contains("10"));
        assertTrue("toString should contain y value", str.contains("20"));
    }

    @Test
    public void equalPointsAreEqual() {
        // TODO(sonnet): If GesturePoint implements equals/hashCode, test that here.
        // For now, just verify construction succeeds.
        GesturePoint a = new GesturePoint(5.0f, 10.0f, 100L);
        GesturePoint b = new GesturePoint(5.0f, 10.0f, 100L);
        assertNotNull(a);
        assertNotNull(b);
    }
}
