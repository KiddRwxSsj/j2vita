package javax.microedition.m3g;

import java.util.Hashtable;
import java.util.Vector;
import javax.microedition.lcdui.Graphics;

public class Graphics3D {
    public static final int ANTIALIAS = 2;
    public static final int DITHER = 4;
    public static final int TRUE_COLOR = 8;
    public static final int OVERWRITE = 16;

    private static Graphics3D instance;

    private Graphics target;
    private int vx, vy, vw, vh;
    private Camera camera;
    private final float[] view = new float[16];
    private final Vector lights = new Vector();
    private final Vector lightXf = new Vector();
    private final float[] prm = new float[Native.LEN];
    private final float[] local = new float[16], mv = new float[16];
    private int skinSerial;

    private Graphics3D() {
    }

    public static Graphics3D getInstance() {
        if (instance == null) instance = new Graphics3D();
        return instance;
    }

    public static Hashtable getProperties() {
        Hashtable h = new Hashtable();
        h.put("supportAntialiasing", Boolean.FALSE);
        h.put("supportTrueColor", Boolean.FALSE);
        h.put("supportDithering", Boolean.FALSE);
        h.put("supportMipmapping", Boolean.FALSE);
        h.put("supportPerspectiveCorrection", Boolean.TRUE);
        h.put("supportLocalCameraLighting", Boolean.FALSE);
        h.put("maxLights", new Integer(4));
        h.put("maxViewportWidth", new Integer(1024));
        h.put("maxViewportHeight", new Integer(1024));
        h.put("maxViewportDimension", new Integer(1024));
        h.put("maxTextureDimension", new Integer(1024));
        h.put("maxSpriteCropDimension", new Integer(1024));
        h.put("maxTransformsPerVertex", new Integer(SkinnedMesh.K));
        h.put("numTextureUnits", new Integer(1));
        return h;
    }

    public void bindTarget(Object t) {
        bindTarget(t, true, 0);
    }

    public void bindTarget(Object t, boolean depth, int hints) {
        if (t == null) throw new NullPointerException();
        if (target != null) throw new IllegalStateException();
        if (!(t instanceof Graphics)) throw new IllegalArgumentException();
        Graphics g = (Graphics)t;
        target = g;
        vx = g.getTranslateX() + g.getClipX();
        vy = g.getTranslateY() + g.getClipY();
        vw = g.getClipWidth();
        vh = g.getClipHeight();
        Native.bind(g);
    }

    public Object getTarget() { return target; }
    public boolean isDepthBufferEnabled() { return true; }
    public int getHints() { return 0; }

    public void releaseTarget() {
        target = null;
    }

    public void setViewport(int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) throw new IllegalArgumentException();
        if (target != null) {
            x += target.getTranslateX();
            y += target.getTranslateY();
        }
        vx = x; vy = y; vw = w; vh = h;
    }

    public int getViewportX() { return vx; }
    public int getViewportY() { return vy; }
    public int getViewportWidth() { return vw; }
    public int getViewportHeight() { return vh; }
    public void setDepthRange(float n, float f) { }
    public float getDepthRangeNear() { return 0; }
    public float getDepthRangeFar() { return 1; }

    public void setCamera(Camera c, Transform t) {
        camera = c;
        Transform v = t == null ? new Transform() : new Transform(t);
        try {
            v.invert();
        } catch (ArithmeticException e) {
            v.setIdentity();
        }
        System.arraycopy(v.m, 0, view, 0, 16);
    }

    public Camera getCamera(Transform t) {
        if (t != null) {
            Transform v = new Transform();
            v.set(view);
            v.invert();
            t.set(v);
        }
        return camera;
    }

    public int addLight(Light l, Transform t) {
        if (l == null) throw new NullPointerException();
        lights.addElement(l);
        lightXf.addElement(t == null ? new Transform() : new Transform(t));
        return lights.size() - 1;
    }

    public void setLight(int i, Light l, Transform t) {
        if (i < 0 || i >= lights.size()) throw new IndexOutOfBoundsException();
        lights.setElementAt(l, i);
        lightXf.setElementAt(t == null ? new Transform() : new Transform(t), i);
    }

    public void resetLights() {
        lights.removeAllElements();
        lightXf.removeAllElements();
    }

    public int getLightCount() { return lights.size(); }

    public Light getLight(int i, Transform t) {
        if (t != null) t.set((Transform)lightXf.elementAt(i));
        return (Light)lights.elementAt(i);
    }

    public void clear(Background b) {
        if (target == null) throw new IllegalStateException();
        int[] a = new int[13];
        a[0] = vx; a[1] = vy; a[2] = vw; a[3] = vh;
        int[] tex = null;
        if (b == null) {
            a[4] = 0; a[5] = 1; a[6] = 1;
        } else {
            a[4] = b.color;
            a[5] = b.colorClear ? 1 : 0;
            a[6] = b.depthClear ? 1 : 0;
            a[7] = b.modeX == Background.REPEAT ? 1 : 0;
            a[8] = b.modeY == Background.REPEAT ? 1 : 0;
            a[9] = b.cropX; a[10] = b.cropY; a[11] = b.cropW; a[12] = b.cropH;
            if (b.image != null) tex = b.image.tex;
        }
        Native.clear(target, a, tex);
    }

    public void render(World w) {
        if (target == null) throw new IllegalStateException();
        if (w == null) throw new NullPointerException();
        Camera c = w.camera;
        if (c == null) throw new IllegalStateException();
        Transform ct = new Transform();
        c.getTransformTo(w, ct);
        setCamera(c, ct);
        clear(w.background);
        float[] id = new float[16];
        Transform.quatToMatrix(id, 0, 0, 0, 1);
        for (int i = 0; i < w.children.size(); i++) {
            Node n = (Node)w.children.elementAt(i);
            float[] m = new float[16];
            n.composite(m);
            draw(n, m);
        }
    }

    public void render(Node n, Transform t) {
        if (target == null || camera == null) throw new IllegalStateException();
        if (n == null) throw new NullPointerException();
        float[] m = new float[16];
        if (t == null) Transform.quatToMatrix(m, 0, 0, 0, 1);
        else System.arraycopy(t.m, 0, m, 0, 16);
        draw(n, m);
    }

    public void render(VertexBuffer vb, IndexBuffer ib, Appearance ap, Transform t) {
        render(vb, ib, ap, t, -1);
    }

    public void render(VertexBuffer vb, IndexBuffer ib, Appearance ap, Transform t, int scope) {
        if (target == null || camera == null) throw new IllegalStateException();
        if (vb == null || ib == null || ap == null) throw new NullPointerException();
        float[] m = new float[16];
        if (t == null) Transform.quatToMatrix(m, 0, 0, 0, 1);
        else System.arraycopy(t.m, 0, m, 0, 16);
        submesh(vb, ib, ap, m, null);
    }

    private void draw(Node n, float[] m) {
        if (!n.rendering) return;
        if (n instanceof Group) {
            Group g = (Group)n;
            for (int i = 0; i < g.children.size(); i++) {
                Node c = (Node)g.children.elementAt(i);
                c.composite(local);
                Transform.mul(c.world, m, local);
                draw(c, c.world);
            }
        } else if (n instanceof Mesh) {
            Mesh mesh = (Mesh)n;
            SkinnedMesh sm = null;
            if (n instanceof SkinnedMesh) {
                sm = (SkinnedMesh)n;
                if (sm.bones.size() > 0) sm.updateBones();
                else sm = null;
            }
            /* submeshes of one draw share the skinned vertices */
            skinSerial = sm != null ? (skinSerial + 1) & 0xffffff : 0;
            for (int i = 0; i < mesh.submeshes.length; i++)
                if (mesh.appearances[i] != null)
                    submesh(mesh.vertices, mesh.submeshes[i], mesh.appearances[i], m, sm);
        }
    }

    private void submesh(VertexBuffer vb, IndexBuffer ib, Appearance ap, float[] model, SkinnedMesh sm) {
        if (vb.positions == null || ib.tris.length == 0) return;
        float[] p = prm, mv = this.mv;
        Transform.mul(mv, view, model);
        for (int i = 0; i < 12; i++) p[Native.MV + i] = i >= 4 ? -mv[i] : mv[i];

        float cx = vx + vw * 0.5f, cy = vy + vh * 0.5f;
        p[Native.CX] = cx;
        p[Native.CY] = cy;
        p[Native.VP] = vx; p[Native.VP + 1] = vy; p[Native.VP + 2] = vx + vw; p[Native.VP + 3] = vy + vh;
        Camera c = camera;
        if (c.type == Camera.PERSPECTIVE) {
            float f = (float)(1 / Math.tan(c.fovy * Math.PI / 360));
            p[Native.KX] = vw * 0.5f * f / c.aspect;
            p[Native.KY] = vh * 0.5f * f;
            p[Native.PERSP] = 1;
        } else if (c.type == Camera.PARALLEL) {
            p[Native.KX] = vw / (c.fovy * c.aspect);
            p[Native.KY] = vh / c.fovy;
            p[Native.PERSP] = 0;
        } else {
            p[Native.KX] = vw * 0.5f * c.matrix.m[0];
            p[Native.KY] = vh * 0.5f * c.matrix.m[5];
            p[Native.PERSP] = c.matrix.m[15] == 0 ? 1 : 0;
        }
        p[Native.NEAR] = c.type == Camera.GENERIC ? 0.1f : c.near;
        p[Native.FAR] = c.type == Camera.GENERIC ? 1000 : c.far;

        PolygonMode pm = ap.polygon;
        int cull = pm == null ? PolygonMode.CULL_BACK : pm.culling;
        boolean cw = pm != null && pm.winding == PolygonMode.WINDING_CW;
        if (cull == PolygonMode.CULL_NONE) p[Native.CULL] = 0;
        else p[Native.CULL] = (cull == PolygonMode.CULL_BACK) != cw ? 2 : 1;

        CompositingMode cm = ap.compositing;
        int bl = cm == null ? CompositingMode.REPLACE : cm.blending;
        p[Native.BLEND] = bl == CompositingMode.ALPHA ? 4 : bl == CompositingMode.ALPHA_ADD ? 5
                : bl == CompositingMode.MODULATE ? 6 : bl == CompositingMode.MODULATE_X2 ? 7 : 0;
        p[Native.AREF] = cm == null ? 0 : cm.alphaThreshold * 256;
        p[Native.ZTEST] = cm == null || cm.depthTest ? 1 : 0;
        p[Native.ZWRITE] = cm == null || cm.depthWrite ? 1 : 0;

        p[Native.PSCALE] = vb.posScale;
        p[Native.PBIAS] = vb.posBias[0]; p[Native.PBIAS + 1] = vb.posBias[1]; p[Native.PBIAS + 2] = vb.posBias[2];

        Texture2D tx = ap.textures[0];
        VertexArray tc = vb.texCoords[0];
        int[] tex = null;
        if (tx != null && tc != null && tx.image != null) {
            tex = tx.image.tex;
            p[Native.TSCALE] = vb.tcScale[0];
            p[Native.TBIAS] = vb.tcBias[0][0];
            p[Native.TBIAS + 1] = vb.tcBias[0][1];
            p[Native.TCOMPS] = tc.comps;
            p[Native.TEXMOD] = tx.blending == Texture2D.FUNC_REPLACE ? 0 : 1;
            p[Native.REPS] = tx.wrapS == Texture2D.WRAP_REPEAT ? 1 : 0;
            p[Native.REPT] = tx.wrapT == Texture2D.WRAP_REPEAT ? 1 : 0;
        }
        p[Native.COLOR] = vb.defaultColor & 0xffffff;
        p[Native.SKIN] = sm != null ? skinSerial : 0;

        Material mat = ap.material;
        int nl = 0;
        if (mat != null && vb.normals != null) {
            float amb = 0;
            for (int i = 0; i < lights.size(); i++) {
                Light l = (Light)lights.elementAt(i);
                if (!l.rendering) continue;
                if (l.mode == Light.AMBIENT) {
                    amb += l.lum();
                } else if (nl < 4) {
                    float[] lm = ((Transform)lightXf.elementAt(i)).m;
                    float dx = lm[2], dy = lm[6], dz = lm[10];
                    float ex = view[0] * dx + view[1] * dy + view[2] * dz;
                    float ey = view[4] * dx + view[5] * dy + view[6] * dz;
                    float ez = view[8] * dx + view[9] * dy + view[10] * dz;
                    float len = (float)Math.sqrt(ex * ex + ey * ey + ez * ez);
                    if (len == 0) continue;
                    float s = l.lum() / len;
                    p[Native.LIGHTS + nl * 3] = ex * s;
                    p[Native.LIGHTS + nl * 3 + 1] = -ey * s;
                    p[Native.LIGHTS + nl * 3 + 2] = -ez * s;
                    nl++;
                }
            }
            p[Native.LIT] = 1;
            p[Native.EMIS] = Material.lum(mat.emissive);
            p[Native.AMB] = Material.lum(mat.ambient) * amb;
            p[Native.DIFF] = Material.lum(mat.diffuse);
        } else {
            p[Native.LIT] = 0;
        }
        p[Native.NLIGHTS] = nl;

        Native.draw(target, p, vb.positions.data, vb.normals != null ? vb.normals.data : null,
                    tex != null ? tc.data : null, ib.tris, tex,
                    sm != null ? sm.boneMats : null, sm != null ? sm.influences : null);
    }
}
