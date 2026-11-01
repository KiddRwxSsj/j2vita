package com.mascotcapsule.micro3d.v3;

/* 3x4 matrix, rotation part scaled by 4096 */
public class AffineTrans {
    public int m00, m01, m02, m03;
    public int m10, m11, m12, m13;
    public int m20, m21, m22, m23;

    public AffineTrans() {
    }

    public AffineTrans(AffineTrans a) {
        set(a);
    }

    public AffineTrans(int[][] a) {
        set(a);
    }

    public AffineTrans(int[] a) {
        set(a, 0);
    }

    public AffineTrans(int[] a, int offset) {
        set(a, offset);
    }

    public AffineTrans(int m00, int m01, int m02, int m03,
                       int m10, int m11, int m12, int m13,
                       int m20, int m21, int m22, int m23) {
        set(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23);
    }

    public final void set(int m00, int m01, int m02, int m03,
                          int m10, int m11, int m12, int m13,
                          int m20, int m21, int m22, int m23) {
        this.m00 = m00; this.m01 = m01; this.m02 = m02; this.m03 = m03;
        this.m10 = m10; this.m11 = m11; this.m12 = m12; this.m13 = m13;
        this.m20 = m20; this.m21 = m21; this.m22 = m22; this.m23 = m23;
    }

    public final void set(AffineTrans a) {
        if (a == null) throw new NullPointerException();
        set(a.m00, a.m01, a.m02, a.m03, a.m10, a.m11, a.m12, a.m13,
            a.m20, a.m21, a.m22, a.m23);
    }

    public final void set(int[][] a) {
        if (a == null) throw new NullPointerException();
        if (a.length < 3 || a[0].length < 4 || a[1].length < 4 || a[2].length < 4) {
            throw new IllegalArgumentException();
        }
        set(a[0][0], a[0][1], a[0][2], a[0][3], a[1][0], a[1][1], a[1][2], a[1][3],
            a[2][0], a[2][1], a[2][2], a[2][3]);
    }

    public final void set(int[] a) {
        set(a, 0);
    }

    public final void set(int[] a, int o) {
        if (a == null) throw new NullPointerException();
        if (o < 0 || a.length - o < 12) throw new IllegalArgumentException();
        set(a[o], a[o + 1], a[o + 2], a[o + 3], a[o + 4], a[o + 5], a[o + 6], a[o + 7],
            a[o + 8], a[o + 9], a[o + 10], a[o + 11]);
    }

    public final void get(int[] a) {
        get(a, 0);
    }

    public final void get(int[] a, int o) {
        if (a == null) throw new NullPointerException();
        if (o < 0 || a.length - o < 12) throw new IllegalArgumentException();
        a[o] = m00; a[o + 1] = m01; a[o + 2] = m02; a[o + 3] = m03;
        a[o + 4] = m10; a[o + 5] = m11; a[o + 6] = m12; a[o + 7] = m13;
        a[o + 8] = m20; a[o + 9] = m21; a[o + 10] = m22; a[o + 11] = m23;
    }

    public final void setIdentity() {
        set(4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096, 0);
    }

    public final void rotationX(int r) {
        int c = Util3D.cos(r), s = Util3D.sin(r);
        m00 = 4096; m01 = 0; m02 = 0;
        m10 = 0; m11 = c; m12 = -s;
        m20 = 0; m21 = s; m22 = c;
    }

    public final void rotationY(int r) {
        int c = Util3D.cos(r), s = Util3D.sin(r);
        m00 = c; m01 = 0; m02 = s;
        m10 = 0; m11 = 4096; m12 = 0;
        m20 = -s; m21 = 0; m22 = c;
    }

    public final void rotationZ(int r) {
        int c = Util3D.cos(r), s = Util3D.sin(r);
        m00 = c; m01 = -s; m02 = 0;
        m10 = s; m11 = c; m12 = 0;
        m20 = 0; m21 = 0; m22 = 4096;
    }

    public final void setRotationX(int r) { rotationX(r); }
    public final void setRotationY(int r) { rotationY(r); }
    public final void setRotationZ(int r) { rotationZ(r); }

    private static int fx(int a, int b) {
        return a * b + 2048 >> 12;
    }

    /** rotation by r around the unit axis v */
    public final void setRotation(Vector3D v, int r) {
        if (v == null) throw new NullPointerException();
        int c = Util3D.cos(r), s = Util3D.sin(r), t = 4096 - c;
        int x = v.x, y = v.y, z = v.z;
        int xs = fx(x, s), ys = fx(y, s), zs = fx(z, s);
        int xy = fx(fx(x, y), t), yz = fx(fx(y, z), t), zx = fx(fx(z, x), t);
        m00 = c + fx(fx(x, x), t); m01 = xy - zs; m02 = zx + ys;
        m10 = xy + zs; m11 = c + fx(fx(y, y), t); m12 = yz - xs;
        m20 = zx - ys; m21 = yz + xs; m22 = c + fx(fx(z, z), t);
    }

    public final void rotationV(Vector3D v, int r) {
        setRotation(v, r);
    }

    /** this = a * b */
    public final void mul(AffineTrans a, AffineTrans b) {
        if (a == null || b == null) throw new NullPointerException();
        int n00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20 + 2048 >> 12;
        int n01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21 + 2048 >> 12;
        int n02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22 + 2048 >> 12;
        int n03 = (a.m00 * b.m03 + a.m01 * b.m13 + a.m02 * b.m23 + 2048 >> 12) + a.m03;
        int n10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20 + 2048 >> 12;
        int n11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21 + 2048 >> 12;
        int n12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22 + 2048 >> 12;
        int n13 = (a.m10 * b.m03 + a.m11 * b.m13 + a.m12 * b.m23 + 2048 >> 12) + a.m13;
        int n20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20 + 2048 >> 12;
        int n21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21 + 2048 >> 12;
        int n22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22 + 2048 >> 12;
        int n23 = (a.m20 * b.m03 + a.m21 * b.m13 + a.m22 * b.m23 + 2048 >> 12) + a.m23;
        set(n00, n01, n02, n03, n10, n11, n12, n13, n20, n21, n22, n23);
    }

    public final void mul(AffineTrans a) {
        mul(this, a);
    }

    public final void multiply(AffineTrans a) {
        mul(this, a);
    }

    public final void multiply(AffineTrans a, AffineTrans b) {
        mul(a, b);
    }

    public final Vector3D transform(Vector3D v) {
        if (v == null) throw new NullPointerException();
        return new Vector3D(
            (v.x * m00 + v.y * m01 + v.z * m02 + 2048 >> 12) + m03,
            (v.x * m10 + v.y * m11 + v.z * m12 + 2048 >> 12) + m13,
            (v.x * m20 + v.y * m21 + v.z * m22 + 2048 >> 12) + m23);
    }

    public final Vector3D transPoint(Vector3D v) {
        return transform(v);
    }

    /** camera at pos looking along look, rows are the camera axes */
    public final void lookAt(Vector3D pos, Vector3D look, Vector3D up) {
        if (pos == null || look == null || up == null) throw new NullPointerException();
        Vector3D zx = new Vector3D(look);
        zx.unit();
        Vector3D xx = Vector3D.outerProduct(look, up);
        xx.unit();
        Vector3D yx = Vector3D.outerProduct(look, xx);
        yx.unit();
        int px = -pos.x, py = -pos.y, pz = -pos.z;
        m00 = xx.x; m01 = xx.y; m02 = xx.z;
        m03 = px * xx.x + py * xx.y + pz * xx.z + 2048 >> 12;
        m10 = yx.x; m11 = yx.y; m12 = yx.z;
        m13 = px * yx.x + py * yx.y + pz * yx.z + 2048 >> 12;
        m20 = zx.x; m21 = zx.y; m22 = zx.z;
        m23 = px * zx.x + py * zx.y + pz * zx.z + 2048 >> 12;
    }

    public final void setViewTrans(Vector3D pos, Vector3D look, Vector3D up) {
        lookAt(pos, look, up);
    }
}
