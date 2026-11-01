package javax.microedition.m3g;

public class TriangleStripArray extends IndexBuffer {
    public TriangleStripArray(int first, int[] lengths) {
        build(null, first, lengths);
    }

    public TriangleStripArray(int[] indices, int[] lengths) {
        if (indices == null) throw new NullPointerException();
        build(indices, 0, lengths);
    }

    private void build(int[] idx, int first, int[] lengths) {
        if (lengths == null) throw new NullPointerException();
        int n = 0, total = 0;
        for (int i = 0; i < lengths.length; i++) {
            if (lengths[i] < 3) throw new IllegalArgumentException();
            n += lengths[i] - 2;
            total += lengths[i];
        }
        if (idx != null && idx.length < total) throw new IllegalArgumentException();
        tris = new int[n * 3];
        int o = 0, p = 0;
        for (int s = 0; s < lengths.length; s++) {
            for (int k = 0; k < lengths[s] - 2; k++) {
                int a = p + k, b = p + k + 1, c = p + k + 2;
                // every other triangle in a strip is flipped back to the strip's winding
                if ((k & 1) != 0) { int t = a; a = b; b = t; }
                tris[o++] = idx != null ? idx[a] : first + a;
                tris[o++] = idx != null ? idx[b] : first + b;
                tris[o++] = idx != null ? idx[c] : first + c;
            }
            p += lengths[s];
        }
    }
}
