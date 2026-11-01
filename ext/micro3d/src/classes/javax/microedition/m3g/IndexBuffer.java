package javax.microedition.m3g;

public abstract class IndexBuffer extends Object3D {
    /* plain triangle list, same winding for all */
    int[] tris = new int[0];

    public int getIndexCount() { return tris.length; }

    public void getIndices(int[] out) {
        if (out == null) throw new NullPointerException();
        if (out.length < tris.length) throw new IllegalArgumentException();
        System.arraycopy(tris, 0, out, 0, tris.length);
    }
}
