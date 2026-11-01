package javax.microedition.m3g;

public class VertexArray extends Object3D {
    final int count, comps, size;
    final short[] data;

    public VertexArray(int numVertices, int numComponents, int componentSize) {
        if (numVertices < 1 || numVertices > 65535 || numComponents < 2 || numComponents > 4
                || componentSize < 1 || componentSize > 2) {
            throw new IllegalArgumentException();
        }
        count = numVertices;
        comps = numComponents;
        size = componentSize;
        data = new short[numVertices * numComponents];
    }

    public int getVertexCount() { return count; }
    public int getComponentCount() { return comps; }
    public int getComponentType() { return size; }

    public void set(int first, int n, short[] v) {
        if (v == null) throw new NullPointerException();
        if (size != 2) throw new IllegalStateException();
        if (first < 0 || n < 0 || first + n > count || v.length < n * comps) throw new IllegalArgumentException();
        System.arraycopy(v, 0, data, first * comps, n * comps);
    }

    public void set(int first, int n, byte[] v) {
        if (v == null) throw new NullPointerException();
        if (size != 1) throw new IllegalStateException();
        if (first < 0 || n < 0 || first + n > count || v.length < n * comps) throw new IllegalArgumentException();
        for (int i = 0; i < n * comps; i++) data[first * comps + i] = v[i];
    }

    public void get(int first, int n, short[] v) {
        if (size != 2) throw new IllegalStateException();
        System.arraycopy(data, first * comps, v, 0, n * comps);
    }

    public void get(int first, int n, byte[] v) {
        if (size != 1) throw new IllegalStateException();
        for (int i = 0; i < n * comps; i++) v[i] = (byte)data[first * comps + i];
    }
}
