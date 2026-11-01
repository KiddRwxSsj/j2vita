package javax.microedition.m3g;

public class CompositingMode extends Object3D {
    public static final int ALPHA = 64;
    public static final int ALPHA_ADD = 65;
    public static final int MODULATE = 66;
    public static final int MODULATE_X2 = 67;
    public static final int REPLACE = 68;

    int blending = REPLACE;
    float alphaThreshold;
    boolean depthTest = true, depthWrite = true, colorWrite = true, alphaWrite = true;
    float offsetFactor, offsetUnits;

    public CompositingMode() {
    }

    public void setBlending(int m) {
        if (m < ALPHA || m > REPLACE) throw new IllegalArgumentException();
        blending = m;
    }

    public int getBlending() { return blending; }

    public void setAlphaThreshold(float t) {
        if (t < 0 || t > 1) throw new IllegalArgumentException();
        alphaThreshold = t;
    }

    public float getAlphaThreshold() { return alphaThreshold; }
    public void setDepthTestEnable(boolean on) { depthTest = on; }
    public boolean isDepthTestEnabled() { return depthTest; }
    public void setDepthWriteEnable(boolean on) { depthWrite = on; }
    public boolean isDepthWriteEnabled() { return depthWrite; }
    public void setColorWriteEnable(boolean on) { colorWrite = on; }
    public boolean isColorWriteEnabled() { return colorWrite; }
    public void setAlphaWriteEnable(boolean on) { alphaWrite = on; }
    public boolean isAlphaWriteEnabled() { return alphaWrite; }
    public void setDepthOffset(float f, float u) { offsetFactor = f; offsetUnits = u; }
    public float getDepthOffsetFactor() { return offsetFactor; }
    public float getDepthOffsetUnits() { return offsetUnits; }
}
