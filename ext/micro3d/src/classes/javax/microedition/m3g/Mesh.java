package javax.microedition.m3g;

import java.util.Vector;

public class Mesh extends Node {
    VertexBuffer vertices;
    IndexBuffer[] submeshes;
    Appearance[] appearances;

    public Mesh(VertexBuffer vb, IndexBuffer ib, Appearance ap) {
        this(vb, new IndexBuffer[] {ib}, new Appearance[] {ap});
    }

    public Mesh(VertexBuffer vb, IndexBuffer[] ib, Appearance[] ap) {
        if (vb == null || ib == null) throw new NullPointerException();
        if (ib.length == 0 || (ap != null && ap.length < ib.length)) throw new IllegalArgumentException();
        for (int i = 0; i < ib.length; i++) if (ib[i] == null) throw new NullPointerException();
        vertices = vb;
        submeshes = ib;
        appearances = new Appearance[ib.length];
        if (ap != null) System.arraycopy(ap, 0, appearances, 0, ib.length);
    }

    public void setAppearance(int i, Appearance a) {
        if (i < 0 || i >= appearances.length) throw new IndexOutOfBoundsException();
        appearances[i] = a;
    }

    public Appearance getAppearance(int i) {
        if (i < 0 || i >= appearances.length) throw new IndexOutOfBoundsException();
        return appearances[i];
    }

    public IndexBuffer getIndexBuffer(int i) {
        if (i < 0 || i >= submeshes.length) throw new IndexOutOfBoundsException();
        return submeshes[i];
    }

    public VertexBuffer getVertexBuffer() { return vertices; }
    public int getSubmeshCount() { return submeshes.length; }

    void refs(Vector v) {
        super.refs(v);
        add(v, vertices);
        for (int i = 0; i < submeshes.length; i++) {
            add(v, submeshes[i]);
            add(v, appearances[i]);
        }
    }
}
