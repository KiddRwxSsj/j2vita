package com.mascotcapsule.micro3d.v3;

import java.io.IOException;
import java.io.InputStream;
import javax.microedition.lcdui.Graphics;

/* native side lives in micro3d_kni.c; parsed data is kept in int[] blobs so the GC owns it */
final class Render {
    static final int E_CX = 0, E_CY = 1, E_PROJ = 2, E_P0 = 3, E_P1 = 4, E_P2 = 5, E_P3 = 6;
    static final int E_MTX = 7;
    static final int E_ATTR = 19, E_PATTERN = 20;
    static final int E_AMB = 21, E_DIR = 22, E_LX = 23, E_LY = 24, E_LZ = 25;
    static final int E_TOON = 26, E_TOON_HI = 27, E_TOON_LO = 28;
    static final int E_LEN = 29;

    /* blob header slots shared with the native code */
    static final int F_PATTERN = 1, F_NUM_PATTERNS = 2;
    static final int A_NUM_ACTIONS = 1;

    private Render() {
    }

    static byte[] resource(String name) throws IOException {
        if (name == null) throw new NullPointerException();
        if (!name.startsWith("/")) name = "/" + name;
        InputStream in = Render.class.getResourceAsStream(name);
        if (in == null) throw new IOException(name);
        try {
            byte[] buf = new byte[Math.max(in.available(), 1024)];
            int n = 0, r;
            while ((r = in.read(buf, n, buf.length - n)) > 0) {
                n += r;
                if (n == buf.length) {
                    byte[] b = new byte[n * 2];
                    System.arraycopy(buf, 0, b, 0, n);
                    buf = b;
                }
            }
            if (n == buf.length) return buf;
            byte[] out = new byte[n];
            System.arraycopy(buf, 0, out, 0, n);
            return out;
        } finally {
            in.close();
        }
    }

    static native int figureSize(byte[] data, int off, int len);
    static native void figureLoad(byte[] data, int off, int len, int[] blob);
    static native int actionSize(byte[] data, int off, int len);
    static native void actionLoad(byte[] data, int off, int len, int[] blob);
    static native int actionFrames(int[] act, int idx);
    static native void posture(int[] fig, int[] act, int action, int frame);
    static native int textureSize(byte[] data, int off, int len);
    static native void textureLoad(byte[] data, int off, int len, int[] blob);

    static native void bind(Graphics g);
    static native void figure(Graphics g, int[] fig, Object[] textures, int[] env);
    static native void primitives(Graphics g, int[] tex, int[] env, int command, int count,
                                  int[] vertices, int[] normals, int[] texCoords, int[] colors);
}
