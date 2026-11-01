package javax.microedition.m3g;

import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;
import javax.microedition.lcdui.Image;

public class Loader {
    private static final byte[] ID = {(byte)0xAB, 0x4A, 0x53, 0x52, 0x31, 0x38, 0x34, (byte)0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

    private byte[] d;
    private int p;
    private final Vector objs = new Vector();
    private String base;

    private Loader() {
    }

    public static Object3D[] load(String name) throws IOException {
        if (name == null) throw new NullPointerException();
        Loader l = new Loader();
        int slash = name.lastIndexOf('/');
        l.base = slash >= 0 ? name.substring(0, slash + 1) : "";
        return l.run(read(name));
    }

    public static Object3D[] load(byte[] data, int offset) throws IOException {
        if (data == null) throw new NullPointerException();
        byte[] b = new byte[data.length - offset];
        System.arraycopy(data, offset, b, 0, b.length);
        Loader l = new Loader();
        l.base = "";
        return l.run(b);
    }

    static byte[] read(String name) throws IOException {
        String n = name.startsWith("/") ? name : "/" + name;
        InputStream in = Loader.class.getResourceAsStream(n);
        if (in == null) throw new IOException(name);
        try {
            byte[] buf = new byte[Math.max(in.available(), 4096)];
            int len = 0, r;
            while ((r = in.read(buf, len, buf.length - len)) > 0) {
                len += r;
                if (len == buf.length) {
                    byte[] nb = new byte[len * 2];
                    System.arraycopy(buf, 0, nb, 0, len);
                    buf = nb;
                }
            }
            byte[] out = new byte[len];
            System.arraycopy(buf, 0, out, 0, len);
            return out;
        } finally {
            in.close();
        }
    }

    private Object3D[] run(byte[] data) throws IOException {
        if (data.length > 8 && (data[0] & 255) == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
            Image img = Image.createImage(data, 0, data.length);
            return new Object3D[] {new Image2D(Image2D.RGBA, img)};
        }
        for (int i = 0; i < ID.length; i++)
            if (data.length < 12 || data[i] != ID[i]) throw new IOException("not M3G");
        int q = 12;
        while (q + 9 <= data.length) {
            int comp = data[q] & 255;
            int total = le(data, q + 1), raw = le(data, q + 5);
            if (total < 13 || q + total > data.length) throw new IOException("bad section");
            byte[] body;
            if (comp == 0) {
                body = new byte[total - 13];
                System.arraycopy(data, q + 9, body, 0, body.length);
            } else {
                body = new byte[raw];
                if (Native.inflate(data, q + 9, total - 13, body) != raw) throw new IOException("inflate");
            }
            d = body;
            p = 0;
            while (p + 5 <= d.length) {
                int type = d[p] & 255, len = le(d, p + 1);
                int end = p + 5 + len;
                p += 5;
                objs.addElement(parse(type));
                p = end;
            }
            q += total;
        }
        Vector roots = new Vector();
        boolean[] used = new boolean[objs.size()];
        for (int i = 0; i < objs.size(); i++) {
            Object o = objs.elementAt(i);
            if (o instanceof Object3D) {
                Vector v = new Vector();
                ((Object3D)o).refs(v);
                for (int k = 0; k < v.size(); k++) {
                    int idx = objs.indexOf(v.elementAt(k));
                    if (idx >= 0) used[idx] = true;
                }
            }
        }
        for (int i = 0; i < objs.size(); i++) {
            Object o = objs.elementAt(i);
            if (o instanceof Object3D && !used[i]) roots.addElement(o);
        }
        Object3D[] out = new Object3D[roots.size()];
        roots.copyInto(out);
        return out;
    }

    private static int le(byte[] b, int o) {
        return (b[o] & 255) | (b[o + 1] & 255) << 8 | (b[o + 2] & 255) << 16 | (b[o + 3] & 255) << 24;
    }

    private int u8() { return d[p++] & 255; }
    private int u16() { int v = (d[p] & 255) | (d[p + 1] & 255) << 8; p += 2; return v; }
    private int i32() { int v = le(d, p); p += 4; return v; }
    private boolean bool() { return d[p++] != 0; }
    private float f32() { return bitsToFloat(i32()); }

    /* Float.intBitsToFloat comes back broken on this VM, so decode by hand */
    private static double[] pow2;

    static float bitsToFloat(int bits) {
        if (pow2 == null) {
            double[] t = new double[278];
            double v = 1;
            for (int i = 149; i < 278; i++) { t[i] = v; v *= 2; }
            v = 1;
            for (int i = 149; i >= 0; i--) { t[i] = v; v /= 2; }
            pow2 = t;
        }
        int e = (bits >> 23) & 255, m = bits & 0x7fffff;
        double r;
        if (e == 255) r = m == 0 ? Double.POSITIVE_INFINITY : Double.NaN;
        else if (e == 0) r = m * pow2[0];
        else r = (8388608 + m) * pow2[e - 1];
        return (float)(bits < 0 ? -r : r);
    }
    private int rgb() { int v = u8() << 16; v |= u8() << 8; return v | u8(); }
    private int rgba() { int c = rgb(); return u8() << 24 | c; }

    private String str() {
        int s = p;
        while (d[p] != 0) p++;
        String r;
        try {
            r = new String(d, s, p - s, "UTF-8");
        } catch (Exception e) {
            r = new String(d, s, p - s);
        }
        p++;
        return r;
    }

    private Object ref() {
        int i = i32();
        return i <= 0 || i > objs.size() ? null : objs.elementAt(i - 1);
    }

    private float[] floats(int n) {
        float[] f = new float[n];
        for (int i = 0; i < n; i++) f[i] = f32();
        return f;
    }

    private void object3D(Object3D o) {
        o.userID = i32();
        int n = i32();
        for (int i = 0; i < n; i++) {
            Object t = ref();
            if (t instanceof AnimationTrack) o.addAnimationTrack((AnimationTrack)t);
        }
        n = i32();
        for (int i = 0; i < n; i++) {
            i32();
            p += i32();
        }
    }

    private void transformable(Transformable t) {
        object3D(t);
        if (bool()) {
            t.setTranslation(f32(), f32(), f32());
            t.setScale(f32(), f32(), f32());
            float a = f32();
            t.setOrientation(a, f32(), f32(), f32());
        }
        if (bool()) {
            Transform m = new Transform();
            m.set(floats(16));
            t.setTransform(m);
        }
    }

    private void node(Node n) {
        transformable(n);
        n.rendering = bool();
        n.picking = bool();
        n.alpha = u8() / 255f;
        n.scope = i32();
        if (bool()) {
            u8(); u8();
            ref(); ref();
        }
    }

    private void group(Group g) {
        node(g);
        int n = i32();
        for (int i = 0; i < n; i++) {
            Object c = ref();
            if (c instanceof Node && ((Node)c).parent == null) g.addChild((Node)c);
        }
    }

    private Object parse(int type) throws IOException {
        switch (type) {
        case 0:
            return null;
        case 255: {
            String uri = str();
            try {
                Object3D[] r = run2(uri);
                return r.length > 0 ? r[0] : null;
            } catch (Exception e) {
                return null;
            }
        }
        case 1: {
            AnimationController c = new AnimationController();
            object3D(c);
            c.speed = f32();
            c.weight = f32();
            c.start = i32();
            c.end = i32();
            c.refSeq = f32();
            c.refWorld = i32();
            return c;
        }
        case 2: {
            Object3D tmp = new Group();
            int save = p;
            object3D(tmp);
            KeyframeSequence ks = (KeyframeSequence)ref();
            AnimationController c = (AnimationController)ref();
            int prop = i32();
            AnimationTrack t = new AnimationTrack(ks, prop);
            t.controller = c;
            int after = p;
            p = save;
            object3D(t);
            p = after;
            return t;
        }
        case 3: {
            Appearance a = new Appearance();
            object3D(a);
            a.layer = (byte)u8();
            a.compositing = (CompositingMode)ref();
            a.fog = (Fog)ref();
            a.polygon = (PolygonMode)ref();
            a.material = (Material)ref();
            int n = i32();
            for (int i = 0; i < n; i++) {
                Object t = ref();
                if (i < 2) a.textures[i] = (Texture2D)t;
            }
            return a;
        }
        case 4: {
            Background b = new Background();
            object3D(b);
            b.color = rgba();
            b.image = (Image2D)ref();
            b.modeX = u8();
            b.modeY = u8();
            b.cropX = i32(); b.cropY = i32(); b.cropW = i32(); b.cropH = i32();
            b.depthClear = bool();
            b.colorClear = bool();
            return b;
        }
        case 5: {
            Camera c = new Camera();
            node(c);
            c.type = u8();
            if (c.type == Camera.GENERIC) c.matrix.set(floats(16));
            else { c.fovy = f32(); c.aspect = f32(); c.near = f32(); c.far = f32(); }
            return c;
        }
        case 6: {
            CompositingMode c = new CompositingMode();
            object3D(c);
            c.depthTest = bool();
            c.depthWrite = bool();
            c.colorWrite = bool();
            c.alphaWrite = bool();
            c.blending = u8();
            c.alphaThreshold = u8() / 255f;
            c.offsetFactor = f32();
            c.offsetUnits = f32();
            return c;
        }
        case 7: {
            Fog f = new Fog();
            object3D(f);
            f.color = rgb();
            f.mode = u8();
            if (f.mode == Fog.EXPONENTIAL) f.density = f32();
            else { f.near = f32(); f.far = f32(); }
            return f;
        }
        case 8: {
            PolygonMode m = new PolygonMode();
            object3D(m);
            m.culling = u8();
            m.shading = u8();
            m.winding = u8();
            m.twoSided = bool();
            m.localCamera = bool();
            m.perspective = bool();
            return m;
        }
        case 9: {
            Group g = new Group();
            group(g);
            return g;
        }
        case 22: {
            World w = new World();
            group(w);
            w.camera = (Camera)ref();
            w.background = (Background)ref();
            return w;
        }
        case 10: {
            int save = p;
            Object3D tmp = new Group();
            object3D(tmp);
            int fmt = u8();
            boolean mutable = bool();
            int w = i32(), h = i32();
            Image2D img;
            if (mutable) {
                img = new Image2D(fmt, w, h);
            } else {
                byte[] pal = bytes();
                byte[] px = bytes();
                img = new Image2D(fmt, w, h, px, pal.length > 0 ? pal : null);
            }
            int after = p;
            p = save;
            object3D(img);
            p = after;
            return img;
        }
        case 11: {
            int save = p;
            Object3D tmp = new Group();
            object3D(tmp);
            int enc = u8();
            int[] idx = null;
            int first = 0;
            if (enc == 0) first = i32();
            else if (enc == 1) first = u8();
            else if (enc == 2) first = u16();
            else {
                idx = new int[i32()];
                for (int i = 0; i < idx.length; i++)
                    idx[i] = enc == 128 ? i32() : enc == 129 ? u8() : u16();
            }
            int[] lens = new int[i32()];
            for (int i = 0; i < lens.length; i++) lens[i] = i32();
            TriangleStripArray t = idx == null ? new TriangleStripArray(first, lens) : new TriangleStripArray(idx, lens);
            int after = p;
            p = save;
            object3D(t);
            p = after;
            return t;
        }
        case 12: {
            Light l = new Light();
            node(l);
            l.c0 = f32(); l.c1 = f32(); l.c2 = f32();
            l.color = rgb();
            l.mode = u8();
            l.intensity = f32();
            l.spotAngle = f32();
            l.spotExp = f32();
            return l;
        }
        case 13: {
            Material m = new Material();
            object3D(m);
            m.ambient = rgb();
            m.diffuse = rgba();
            m.emissive = rgb();
            m.specular = rgb();
            m.shininess = f32();
            m.tracking = bool();
            return m;
        }
        case 14:
        case 16: {
            Group dummy = new Group();
            int save = p;
            node(dummy);
            VertexBuffer vb = (VertexBuffer)ref();
            int n = i32();
            IndexBuffer[] ib = new IndexBuffer[n];
            Appearance[] ap = new Appearance[n];
            for (int i = 0; i < n; i++) {
                ib[i] = (IndexBuffer)ref();
                ap[i] = (Appearance)ref();
            }
            Mesh m;
            if (type == 16) {
                Group skel = (Group)ref();
                SkinnedMesh sm = new SkinnedMesh(vb, ib, ap, skel);
                int refs = i32();
                for (int i = 0; i < refs; i++) {
                    Node bone = (Node)ref();
                    int first = i32(), count = i32(), weight = i32();
                    if (bone != null && weight > 0 && count > 0) sm.addTransform(bone, weight, first, count);
                }
                m = sm;
            } else {
                m = new Mesh(vb, ib, ap);
            }
            int after = p;
            p = save;
            node(m);
            p = after;
            return m;
        }
        case 18: {
            Group dummy = new Group();
            int save = p;
            node(dummy);
            Image2D img = (Image2D)ref();
            Appearance ap = (Appearance)ref();
            boolean scaled = bool();
            Sprite3D s = new Sprite3D(scaled, img, ap);
            s.cropX = i32(); s.cropY = i32(); s.cropW = i32(); s.cropH = i32();
            int after = p;
            p = save;
            node(s);
            p = after;
            return s;
        }
        case 17: {
            Group dummy = new Group();
            int save = p;
            transformable(dummy);
            Image2D img = (Image2D)ref();
            int blendColor = rgb();
            int blending = u8(), ws = u8(), wt = u8(), lf = u8(), imf = u8();
            Texture2D t = img != null ? new Texture2D(img) : null;
            int after = p;
            if (t != null) {
                p = save;
                transformable(t);
                t.blendColor = blendColor;
                t.blending = blending;
                t.wrapS = ws;
                t.wrapT = wt;
                t.levelFilter = lf;
                t.imageFilter = imf;
            }
            p = after;
            return t;
        }
        case 19: {
            int save = p;
            Object3D tmp = new Group();
            object3D(tmp);
            int interp = u8(), rep = u8(), enc = u8();
            int dur = i32(), vf = i32(), vl = i32(), cc = i32(), kc = i32();
            KeyframeSequence ks = new KeyframeSequence(kc, cc, interp);
            float[] bias = new float[cc], scale = new float[cc], v = new float[cc];
            if (enc != 0) {
                for (int i = 0; i < cc; i++) bias[i] = f32();
                for (int i = 0; i < cc; i++) scale[i] = f32();
            }
            for (int k = 0; k < kc; k++) {
                int t = i32();
                for (int i = 0; i < cc; i++) {
                    if (enc == 0) v[i] = f32();
                    else if (enc == 1) v[i] = bias[i] + scale[i] * u8() / 255f;
                    else v[i] = bias[i] + scale[i] * u16() / 65535f;
                }
                ks.setKeyframe(k, t, v);
            }
            ks.repeat = rep;
            if (dur > 0) ks.duration = dur;
            if (vf >= 0 && vl < kc && vf <= vl) { ks.first = vf; ks.last = vl; }
            int after = p;
            p = save;
            object3D(ks);
            p = after;
            return ks;
        }
        case 20: {
            int save = p;
            Object3D tmp = new Group();
            object3D(tmp);
            int size = u8(), cc = u8(), enc = u8(), n = u16();
            VertexArray va = new VertexArray(n, cc, size);
            short[] prev = new short[cc];
            for (int i = 0; i < n; i++)
                for (int c = 0; c < cc; c++) {
                    int v = size == 1 ? (byte)d[p++] : (short)u16();
                    if (enc == 1) {
                        v += prev[c];
                        v = size == 1 ? (byte)v : (short)v;
                        prev[c] = (short)v;
                    }
                    va.data[i * cc + c] = (short)v;
                }
            int after = p;
            p = save;
            object3D(va);
            p = after;
            return va;
        }
        case 21: {
            VertexBuffer vb = new VertexBuffer();
            object3D(vb);
            vb.defaultColor = rgba();
            VertexArray pos = (VertexArray)ref();
            float[] bias = floats(3);
            float scale = f32();
            vb.setPositions(pos, scale, bias);
            vb.normals = (VertexArray)ref();
            vb.colors = (VertexArray)ref();
            int n = i32();
            for (int i = 0; i < n; i++) {
                VertexArray tc = (VertexArray)ref();
                float[] b = floats(3);
                float s = f32();
                if (i < 2) vb.setTexCoords(i, tc, s, b);
            }
            return vb;
        }
        default:
            return null;
        }
    }

    private byte[] bytes() {
        int n = i32();
        byte[] b = new byte[n];
        System.arraycopy(d, p, b, 0, n);
        p += n;
        return b;
    }

    private Object3D[] run2(String uri) throws IOException {
        Loader l = new Loader();
        l.base = base;
        return l.run(read(uri.startsWith("/") ? uri : base + uri));
    }
}
