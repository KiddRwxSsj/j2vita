package javax.microedition.m3g;

/* parsed and kept, not drawn */
public class Fog extends Object3D {
    public static final int EXPONENTIAL = 80;
    public static final int LINEAR = 81;

    int mode = LINEAR, color;
    float density = 1, near, far = 1;

    public Fog() {
    }

    public void setMode(int m) { mode = m; }
    public int getMode() { return mode; }
    public void setLinear(float n, float f) { near = n; far = f; }
    public float getNearDistance() { return near; }
    public float getFarDistance() { return far; }
    public void setDensity(float d) { density = d; }
    public float getDensity() { return density; }
    public void setColor(int c) { color = c & 0xffffff; }
    public int getColor() { return color; }
}
