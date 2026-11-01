package javax.microedition.m3g;

import javax.microedition.lcdui.Graphics;

/* shares the software rasterizer with micro3d, see micro3d_kni.c */
final class Native {
    static final int MV = 0, CX = 12, CY = 13, KX = 14, KY = 15, NEAR = 16, FAR = 17, PERSP = 18;
    static final int CULL = 19, BLEND = 20, AREF = 21, ZTEST = 22, ZWRITE = 23;
    static final int PSCALE = 24, PBIAS = 25, TSCALE = 28, TBIAS = 29, TEXMOD = 31, REPS = 32, REPT = 33;
    static final int LIT = 34, EMIS = 35, AMB = 36, DIFF = 37, NLIGHTS = 38, LIGHTS = 39;
    static final int COLOR = 51, VP = 52, TCOMPS = 56, SKIN = 57, LEN = 58;

    private Native() {
    }

    static native int inflate(byte[] src, int off, int len, byte[] dst);
    static native void bind(Graphics g);
    static native void clear(Graphics g, int[] args, int[] tex);
    static native void bones(float[] local, int[] parent, int[] boneNode, float[] rest, float[] out);
    static native void draw(Graphics g, float[] prm, short[] pos, short[] nrm, short[] tc,
                            int[] tris, int[] tex, float[] bones, int[] influences);
}
