package javax.microedition.m3g;

public abstract class Node extends Transformable {
    public static final int NONE = 144;
    public static final int ORIGIN = 145;
    public static final int X_AXIS = 146;
    public static final int Y_AXIS = 147;
    public static final int Z_AXIS = 148;

    Node parent;
    boolean rendering = true, picking = true;
    float alpha = 1;
    int scope = -1;
    /* scratch for the renderer */
    final float[] world = new float[16];

    public Node getParent() { return parent; }
    public void setRenderingEnable(boolean on) { rendering = on; }
    public boolean isRenderingEnabled() { return rendering; }
    public void setPickingEnable(boolean on) { picking = on; }
    public boolean isPickingEnabled() { return picking; }
    public void setAlphaFactor(float a) {
        if (a < 0 || a > 1) throw new IllegalArgumentException();
        alpha = a;
    }
    public float getAlphaFactor() { return alpha; }
    public void setScope(int s) { scope = s; }
    public int getScope() { return scope; }
    public void setAlignment(Node z, int zt, Node y, int yt) { }
    public void align(Node ref) { }

    public boolean getTransformTo(Node target, Transform out) {
        float[] a = new float[16], b = new float[16], tmp = new float[16];
        toRoot(this, a);
        toRoot(target, b);
        Transform t = new Transform();
        t.set(b);
        t.invert();
        Transform.mul(tmp, t.m, a);
        out.set(tmp);
        return true;
    }

    static void toRoot(Node n, float[] out) {
        float[] l = new float[16];
        Transform.quatToMatrix(out, 0, 0, 0, 1);
        for (; n != null; n = n.parent) {
            n.composite(l);
            Transform.mul(out, l, out);
        }
    }

    void setProperty(int prop, float[] v, int n) {
        if (prop == AnimationTrack.ALPHA) alpha = v[0] < 0 ? 0 : v[0] > 1 ? 1 : v[0];
        else if (prop == AnimationTrack.VISIBILITY) rendering = v[0] >= 0.5f;
        else super.setProperty(prop, v, n);
    }
}
