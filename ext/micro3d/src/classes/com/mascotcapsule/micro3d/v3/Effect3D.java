package com.mascotcapsule.micro3d.v3;

public class Effect3D {
    public static final int NORMAL_SHADING = 0;
    public static final int TOON_SHADING = 1;

    Light light;
    Texture sphere;
    int shading;
    int toonThreshold;
    int toonHigh;
    int toonLow;
    boolean transparency = true;

    public Effect3D() {
    }

    public Effect3D(Light light, int shading, boolean isEnableTrans, Texture tex) {
        setShadingType(shading);
        setSphereTexture(tex);
        this.light = light;
        transparency = isEnableTrans;
    }

    public final Light getLight() { return light; }
    public final void setLight(Light light) { this.light = light; }
    public final int getShadingType() { return shading; }
    public final int getShading() { return shading; }

    public final void setShadingType(int s) {
        if (s != NORMAL_SHADING && s != TOON_SHADING) throw new IllegalArgumentException();
        shading = s;
    }

    public final void setShading(int s) { setShadingType(s); }
    public final int getToonThreshold() { return toonThreshold; }
    public final int getToonHigh() { return toonHigh; }
    public final int getToonLow() { return toonLow; }
    public final int getThreshold() { return toonThreshold; }
    public final int getThresholdHigh() { return toonHigh; }
    public final int getThresholdLow() { return toonLow; }

    public final void setToonParams(int threshold, int high, int low) {
        if (((threshold | high | low) & ~0xff) != 0) throw new IllegalArgumentException();
        toonThreshold = threshold;
        toonHigh = high;
        toonLow = low;
    }

    public final void setThreshold(int threshold, int high, int low) {
        setToonParams(threshold, high, low);
    }

    public final boolean isTransparency() { return transparency; }
    public final boolean isSemiTransparentEnabled() { return transparency; }
    public final void setTransparency(boolean on) { transparency = on; }
    public final void setSemiTransparentEnabled(boolean on) { transparency = on; }
    public final Texture getSphereTexture() { return sphere; }
    public final Texture getSphereMap() { return sphere; }

    public final void setSphereTexture(Texture tex) {
        if (tex != null && tex.forModel) throw new IllegalArgumentException();
        sphere = tex;
    }

    public final void setSphereMap(Texture tex) { setSphereTexture(tex); }
}
