package com.mascotcapsule.micro3d.v3;

public class Vector3D {
    public int x;
    public int y;
    public int z;

    public Vector3D() {
    }

    public Vector3D(int x, int y, int z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public Vector3D(Vector3D v) {
        set(v);
    }

    public final int getX() { return x; }
    public final int getY() { return y; }
    public final int getZ() { return z; }
    public final void setX(int x) { this.x = x; }
    public final void setY(int y) { this.y = y; }
    public final void setZ(int z) { this.z = z; }

    public final void set(int x, int y, int z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public final void set(Vector3D v) {
        if (v == null) throw new NullPointerException();
        x = v.x;
        y = v.y;
        z = v.z;
    }

    public final int innerProduct(Vector3D v) {
        if (v == null) throw new NullPointerException();
        return x * v.x + y * v.y + z * v.z;
    }

    public static int innerProduct(Vector3D a, Vector3D b) {
        if (a == null) throw new NullPointerException();
        return a.innerProduct(b);
    }

    public final void outerProduct(Vector3D v) {
        if (v == null) throw new NullPointerException();
        int nx = y * v.z - z * v.y;
        int ny = z * v.x - x * v.z;
        int nz = x * v.y - y * v.x;
        x = nx;
        y = ny;
        z = nz;
    }

    public static Vector3D outerProduct(Vector3D a, Vector3D b) {
        if (a == null || b == null) throw new NullPointerException();
        Vector3D r = new Vector3D(a);
        r.outerProduct(b);
        return r;
    }

    /** scales to length 4096, (0,0,4096) for a zero vector */
    public final void unit() {
        int ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, az = z < 0 ? -z : z;
        int m = ax | ay | az;
        if (m == 0) {
            x = 0;
            y = 0;
            z = 4096;
            return;
        }
        // bring the biggest component near 2^14 so the squares fit in an int
        int tx = x, ty = y, tz = z;
        while (m >= 1 << 15) {
            tx >>= 1; ty >>= 1; tz >>= 1;
            m >>= 1;
        }
        while (m < 1 << 14) {
            tx <<= 1; ty <<= 1; tz <<= 1;
            m <<= 1;
        }
        int len = Util3D.sqrt(tx * tx + ty * ty + tz * tz);
        x = (tx << 12) / len;
        y = (ty << 12) / len;
        z = (tz << 12) / len;
    }
}
