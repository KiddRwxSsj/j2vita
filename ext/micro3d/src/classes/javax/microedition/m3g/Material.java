package javax.microedition.m3g;

public class Material extends Object3D {
    public static final int AMBIENT = 1024;
    public static final int DIFFUSE = 2048;
    public static final int EMISSIVE = 4096;
    public static final int SPECULAR = 8192;

    int ambient = 0x333333, diffuse = 0xffcccccc, emissive, specular;
    float shininess;
    boolean tracking;

    public Material() {
    }

    public void setColor(int target, int argb) {
        if ((target & ~(AMBIENT | DIFFUSE | EMISSIVE | SPECULAR)) != 0 || target == 0) {
            throw new IllegalArgumentException();
        }
        if ((target & AMBIENT) != 0) ambient = argb & 0xffffff;
        if ((target & DIFFUSE) != 0) diffuse = argb;
        if ((target & EMISSIVE) != 0) emissive = argb & 0xffffff;
        if ((target & SPECULAR) != 0) specular = argb & 0xffffff;
    }

    public int getColor(int target) {
        switch (target) {
        case AMBIENT: return ambient;
        case DIFFUSE: return diffuse;
        case EMISSIVE: return emissive;
        case SPECULAR: return specular;
        }
        throw new IllegalArgumentException();
    }

    public void setShininess(float s) { shininess = s; }
    public float getShininess() { return shininess; }
    public void setVertexColorTrackingEnable(boolean on) { tracking = on; }
    public boolean isVertexColorTrackingEnabled() { return tracking; }

    static float lum(int c) {
        return (((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255)) / 765f;
    }

    void setProperty(int prop, float[] v, int n) {
        if (n < 3) return;
        if (prop == AnimationTrack.AMBIENT_COLOR) ambient = Appearance.rgb(v);
        else if (prop == AnimationTrack.DIFFUSE_COLOR) diffuse = (diffuse & 0xff000000) | Appearance.rgb(v);
        else if (prop == AnimationTrack.EMISSIVE_COLOR) emissive = Appearance.rgb(v);
        else if (prop == AnimationTrack.SPECULAR_COLOR) specular = Appearance.rgb(v);
    }
}
