package javax.microedition.m3g;

public class Light extends Node {
    public static final int AMBIENT = 128;
    public static final int DIRECTIONAL = 129;
    public static final int OMNI = 130;
    public static final int SPOT = 131;

    int mode = DIRECTIONAL;
    int color = 0xffffff;
    float intensity = 1;
    float c0 = 1, c1, c2, spotAngle = 45, spotExp;

    public Light() {
    }

    public void setMode(int m) {
        if (m < AMBIENT || m > SPOT) throw new IllegalArgumentException();
        mode = m;
    }

    public int getMode() { return mode; }
    public void setColor(int c) { color = c & 0xffffff; }
    public int getColor() { return color; }
    public void setIntensity(float i) { intensity = i; }
    public float getIntensity() { return intensity; }
    public void setAttenuation(float a, float b, float c) { c0 = a; c1 = b; c2 = c; }
    public float getConstantAttenuation() { return c0; }
    public float getLinearAttenuation() { return c1; }
    public float getQuadraticAttenuation() { return c2; }
    public void setSpotAngle(float a) { spotAngle = a; }
    public float getSpotAngle() { return spotAngle; }
    public void setSpotExponent(float e) { spotExp = e; }
    public float getSpotExponent() { return spotExp; }

    float lum() {
        return (((color >> 16) & 255) + ((color >> 8) & 255) + (color & 255)) / 765f * intensity;
    }

    void setProperty(int prop, float[] v, int n) {
        if (prop == AnimationTrack.INTENSITY) intensity = v[0];
        else if (prop == AnimationTrack.COLOR && n >= 3) color = Appearance.rgb(v);
        else super.setProperty(prop, v, n);
    }
}
