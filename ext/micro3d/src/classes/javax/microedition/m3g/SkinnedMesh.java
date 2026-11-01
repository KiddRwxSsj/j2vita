package javax.microedition.m3g;

import java.util.Vector;

public class SkinnedMesh extends Mesh {
    static final int K = 4;

    Group skeleton;
    final Vector bones = new Vector();
    /* per bone: first vertex, count, weight, inverse rest matrix */
    final Vector boneInfo = new Vector();
    /* per vertex K influences: bone << 16 | weight 0..65535, built lazily */
    int[] influences;
    float[] boneMats;

    public SkinnedMesh(VertexBuffer vb, IndexBuffer ib, Appearance ap, Group skel) {
        super(vb, ib, ap);
        setSkeleton(skel);
    }

    public SkinnedMesh(VertexBuffer vb, IndexBuffer[] ib, Appearance[] ap, Group skel) {
        super(vb, ib, ap);
        setSkeleton(skel);
    }

    private void setSkeleton(Group skel) {
        if (skel == null) throw new NullPointerException();
        if (skel.parent != null) throw new IllegalArgumentException();
        skeleton = skel;
        skel.parent = this;
    }

    public Group getSkeleton() { return skeleton; }

    public void addTransform(Node bone, int weight, int first, int count) {
        if (bone == null) throw new NullPointerException();
        if (weight <= 0 || count <= 0 || first < 0) throw new IllegalArgumentException();
        float[] m = new float[16];
        restOf(bone, m);
        bones.addElement(bone);
        boneInfo.addElement(new Object[] {new int[] {first, count, weight}, m});
        influences = null;
    }

    public void getBoneTransform(Node bone, Transform t) {
        int i = bones.indexOf(bone);
        if (i < 0) throw new IllegalArgumentException();
        Transform r = new Transform();
        r.set((float[])((Object[])boneInfo.elementAt(i))[1]);
        r.invert();
        t.set(r);
    }

    public int getBoneVertices(Node bone, int[] idx, float[] w) {
        return 0;
    }

    /* bone to mesh at the current pose, inverted */
    private void restOf(Node bone, float[] out) {
        toMesh(bone, out);
        Transform t = new Transform();
        t.set(out);
        try {
            t.invert();
        } catch (ArithmeticException e) {
            t.setIdentity();
        }
        System.arraycopy(t.m, 0, out, 0, 16);
    }

    private void toMesh(Node n, float[] out) {
        float[] l = new float[16];
        Transform.quatToMatrix(out, 0, 0, 0, 1);
        for (; n != null && n != this; n = n.parent) {
            n.composite(l);
            Transform.mul(out, l, out);
        }
    }

    private void buildInfluences() {
        int nv = vertices.getVertexCount();
        int[] inf = new int[nv * K];
        int[] wsum = new int[nv];
        int[][] raw = new int[nv * K][];
        for (int b = 0; b < bones.size(); b++) {
            int[] info = (int[])((Object[])boneInfo.elementAt(b))[0];
            for (int v = info[0]; v < info[0] + info[1] && v < nv; v++) {
                for (int k = 0; k < K; k++) {
                    if (raw[v * K + k] == null) {
                        raw[v * K + k] = new int[] {b, info[2]};
                        wsum[v] += info[2];
                        break;
                    }
                }
            }
        }
        for (int v = 0; v < nv; v++)
            for (int k = 0; k < K; k++) {
                int[] r = raw[v * K + k];
                if (r == null || wsum[v] == 0) { inf[v * K + k] = -1; continue; }
                int w = (int)((long)r[1] * 65535 / wsum[v]);
                inf[v * K + k] = r[0] << 16 | w;
            }
        influences = inf;
        boneMats = new float[bones.size() * 12];
    }

    private final float[] scratch = new float[16];
    /* skeleton flattened parents first, for the native bone pass */
    private Node[] nodes;
    private int[] parentIdx, boneNode;
    private float[] local, rest12;
    static final int LS = 23;

    private void flattenSkeleton() {
        Vector list = new Vector(), par = new Vector();
        list.addElement(skeleton);
        par.addElement(new Integer(-1));
        for (int i = 0; i < list.size(); i++) {
            Node n = (Node)list.elementAt(i);
            if (n instanceof Group) {
                Vector c = ((Group)n).children;
                for (int k = 0; k < c.size(); k++) {
                    list.addElement(c.elementAt(k));
                    par.addElement(new Integer(i));
                }
            }
        }
        int nn = list.size(), nb = bones.size();
        nodes = new Node[nn];
        list.copyInto(nodes);
        parentIdx = new int[nn];
        for (int i = 0; i < nn; i++) parentIdx[i] = ((Integer)par.elementAt(i)).intValue();
        boneNode = new int[nb];
        rest12 = new float[nb * 12];
        for (int b = 0; b < nb; b++) {
            boneNode[b] = list.indexOf(bones.elementAt(b));
            System.arraycopy((float[])((Object[])boneInfo.elementAt(b))[1], 0, rest12, b * 12, 12);
        }
        local = new float[nn * LS];
        builtVersion = version;
    }

    private int builtVersion = -1;

    /* every node under the skeleton gets its transform to the mesh in Node.world */
    private void walk(Node n, float[] parent) {
        n.composite(scratch);
        Transform.mul(n.world, parent, scratch);
        if (n instanceof Group) {
            Vector c = ((Group)n).children;
            for (int i = 0; i < c.size(); i++) walk((Node)c.elementAt(i), n.world);
        }
    }

    private static final float[] IDENT = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    /* fills boneMats with current bone to mesh times inverse rest */
    void updateBones() {
        if (influences == null) buildInfluences();
        if (nodes == null || builtVersion != version) flattenSkeleton();
        float[] l = local;
        for (int i = 0, o = 0; i < nodes.length; i++, o += LS) {
            Node n = nodes[i];
            l[o] = n.tx; l[o + 1] = n.ty; l[o + 2] = n.tz;
            l[o + 3] = n.qx; l[o + 4] = n.qy; l[o + 5] = n.qz; l[o + 6] = n.qw;
            l[o + 7] = n.sx; l[o + 8] = n.sy; l[o + 9] = n.sz;
            if (n.general != null) {
                l[o + 10] = 1;
                System.arraycopy(n.general.m, 0, l, o + 11, 12);
            } else {
                l[o + 10] = 0;
            }
        }
        Native.bones(l, parentIdx, boneNode, rest12, boneMats);
    }

    void refs(Vector v) {
        super.refs(v);
        add(v, skeleton);
    }
}
