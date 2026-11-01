package javax.microedition.m3g;

import java.util.Vector;

public class VertexBuffer extends Object3D {
    VertexArray positions, normals, colors;
    final VertexArray[] texCoords = new VertexArray[2];
    float posScale = 1;
    final float[] posBias = new float[3];
    final float[] tcScale = {1, 1};
    final float[][] tcBias = new float[2][3];
    int defaultColor = 0xffffffff;

    public VertexBuffer() {
    }

    public int getVertexCount() { return positions != null ? positions.count : 0; }

    public void setPositions(VertexArray a, float scale, float[] bias) {
        if (a != null && a.comps != 3) throw new IllegalArgumentException();
        positions = a;
        posScale = scale;
        for (int i = 0; i < 3; i++) posBias[i] = bias != null && bias.length > i ? bias[i] : 0;
    }

    public VertexArray getPositions(float[] sb) {
        if (sb != null) { sb[0] = posScale; sb[1] = posBias[0]; sb[2] = posBias[1]; sb[3] = posBias[2]; }
        return positions;
    }

    public void setTexCoords(int i, VertexArray a, float scale, float[] bias) {
        if (i < 0 || i >= 2) throw new IndexOutOfBoundsException();
        texCoords[i] = a;
        tcScale[i] = scale;
        for (int k = 0; k < 3; k++) tcBias[i][k] = bias != null && bias.length > k ? bias[k] : 0;
    }

    public VertexArray getTexCoords(int i, float[] sb) {
        if (sb != null) { sb[0] = tcScale[i]; sb[1] = tcBias[i][0]; sb[2] = tcBias[i][1]; }
        return texCoords[i];
    }

    public void setNormals(VertexArray a) {
        if (a != null && a.comps != 3) throw new IllegalArgumentException();
        normals = a;
    }

    public VertexArray getNormals() { return normals; }
    public void setColors(VertexArray a) { colors = a; }
    public VertexArray getColors() { return colors; }
    public void setDefaultColor(int argb) { defaultColor = argb; }
    public int getDefaultColor() { return defaultColor; }

    void refs(Vector v) {
        super.refs(v);
        add(v, positions);
        add(v, normals);
        add(v, colors);
        add(v, texCoords[0]);
        add(v, texCoords[1]);
    }

    void setProperty(int prop, float[] v, int n) {
        if (prop == AnimationTrack.COLOR && n >= 3) defaultColor = (defaultColor & 0xff000000) | Appearance.rgb(v);
    }
}
