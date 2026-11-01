package javax.microedition.m3g;

import java.util.Vector;

public class Appearance extends Object3D {
    int layer;
    CompositingMode compositing;
    PolygonMode polygon;
    Material material;
    Fog fog;
    final Texture2D[] textures = new Texture2D[2];

    public Appearance() {
    }

    static int rgb(float[] v) {
        int r = (int)(v[0] * 255), g = (int)(v[1] * 255), b = (int)(v[2] * 255);
        r = r < 0 ? 0 : r > 255 ? 255 : r;
        g = g < 0 ? 0 : g > 255 ? 255 : g;
        b = b < 0 ? 0 : b > 255 ? 255 : b;
        return r << 16 | g << 8 | b;
    }

    public void setLayer(int l) {
        if (l < -63 || l > 63) throw new IllegalArgumentException();
        layer = l;
    }

    public int getLayer() { return layer; }
    public void setCompositingMode(CompositingMode c) { compositing = c; }
    public CompositingMode getCompositingMode() { return compositing; }
    public void setPolygonMode(PolygonMode p) { polygon = p; }
    public PolygonMode getPolygonMode() { return polygon; }
    public void setMaterial(Material m) { material = m; }
    public Material getMaterial() { return material; }
    public void setFog(Fog f) { fog = f; }
    public Fog getFog() { return fog; }

    public void setTexture(int i, Texture2D t) {
        if (i < 0 || i >= textures.length) throw new IndexOutOfBoundsException();
        textures[i] = t;
    }

    public Texture2D getTexture(int i) {
        if (i < 0 || i >= textures.length) throw new IndexOutOfBoundsException();
        return textures[i];
    }

    void refs(Vector v) {
        super.refs(v);
        add(v, compositing);
        add(v, polygon);
        add(v, material);
        add(v, fog);
        add(v, textures[0]);
        add(v, textures[1]);
    }
}
