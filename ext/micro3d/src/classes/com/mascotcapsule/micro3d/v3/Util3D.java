package com.mascotcapsule.micro3d.v3;

public class Util3D {
    private static int[] sinTable;

    public Util3D() {
    }

    /** integer square root of p taken as unsigned */
    public static int sqrt(int p) {
        long v = p & 0xffffffffL;
        long r = 0, bit = 1L << 32;
        while (bit > v) bit >>= 2;
        while (bit != 0) {
            if (v >= r + bit) {
                v -= r + bit;
                r = (r >> 1) + bit;
            } else {
                r >>= 1;
            }
            bit >>= 2;
        }
        // round to nearest like the handsets do
        if (v > r) r++;
        return (int)r;
    }

    /** 4096 units per turn, result scaled by 4096 */
    public static int sin(int a) {
        int[] t = sinTable;
        if (t == null) {
            t = new int[4096];
            for (int i = 0; i < 4096; i++) {
                double s = Math.sin(i * Math.PI / 2048.0) * 4096.0;
                t[i] = (int)(s < 0 ? s - 0.5 : s + 0.5);
            }
            sinTable = t;
        }
        return t[a & 4095];
    }

    public static int cos(int a) {
        return sin(a + 1024);
    }
}
