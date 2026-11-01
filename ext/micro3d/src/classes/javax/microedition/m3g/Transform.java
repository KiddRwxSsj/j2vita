package javax.microedition.m3g;

/* 4x4 row major */
public class Transform {
    final float[] m = new float[16];

    public Transform() { setIdentity(); }
    public Transform(Transform t) { set(t); }

    public void setIdentity() {
        for (int i = 0; i < 16; i++) m[i] = (i % 5) == 0 ? 1 : 0;
    }

    public void set(Transform t) {
        if (t == null) throw new NullPointerException();
        System.arraycopy(t.m, 0, m, 0, 16);
    }

    public void set(float[] a) {
        if (a == null) throw new NullPointerException();
        if (a.length < 16) throw new IllegalArgumentException();
        System.arraycopy(a, 0, m, 0, 16);
    }

    public void get(float[] a) {
        if (a == null) throw new NullPointerException();
        if (a.length < 16) throw new IllegalArgumentException();
        System.arraycopy(m, 0, a, 0, 16);
    }

    private static final float[] tmp = new float[16];

    static synchronized void mul(float[] out, float[] a, float[] b) {
        float[] r = tmp;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
        System.arraycopy(r, 0, out, 0, 16);
    }

    public void postMultiply(Transform t) {
        if (t == null) throw new NullPointerException();
        mul(m, m, t.m);
    }

    public void postScale(float sx, float sy, float sz) {
        for (int i = 0; i < 4; i++) {
            m[i * 4] *= sx;
            m[i * 4 + 1] *= sy;
            m[i * 4 + 2] *= sz;
        }
    }

    public void postTranslate(float tx, float ty, float tz) {
        for (int i = 0; i < 4; i++)
            m[i * 4 + 3] += m[i * 4] * tx + m[i * 4 + 1] * ty + m[i * 4 + 2] * tz;
    }

    static void quatToMatrix(float[] r, float x, float y, float z, float w) {
        float n = x * x + y * y + z * z + w * w;
        float s = n > 0 ? 2 / n : 0;
        r[0] = 1 - s * (y * y + z * z); r[1] = s * (x * y - w * z); r[2] = s * (x * z + w * y); r[3] = 0;
        r[4] = s * (x * y + w * z); r[5] = 1 - s * (x * x + z * z); r[6] = s * (y * z - w * x); r[7] = 0;
        r[8] = s * (x * z - w * y); r[9] = s * (y * z + w * x); r[10] = 1 - s * (x * x + y * y); r[11] = 0;
        r[12] = 0; r[13] = 0; r[14] = 0; r[15] = 1;
    }

    static void axisAngle(float[] q, float deg, float ax, float ay, float az) {
        float l = (float)Math.sqrt(ax * ax + ay * ay + az * az);
        if (deg == 0 || l == 0) {
            q[0] = 0; q[1] = 0; q[2] = 0; q[3] = 1;
            return;
        }
        double h = deg * Math.PI / 360;
        float s = (float)Math.sin(h) / l;
        q[0] = ax * s; q[1] = ay * s; q[2] = az * s; q[3] = (float)Math.cos(h);
    }

    public void postRotate(float angle, float ax, float ay, float az) {
        float[] q = new float[4], r = new float[16];
        axisAngle(q, angle, ax, ay, az);
        quatToMatrix(r, q[0], q[1], q[2], q[3]);
        mul(m, m, r);
    }

    public void postRotateQuat(float qx, float qy, float qz, float qw) {
        float[] r = new float[16];
        quatToMatrix(r, qx, qy, qz, qw);
        mul(m, m, r);
    }

    public void transpose() {
        for (int i = 0; i < 4; i++)
            for (int j = i + 1; j < 4; j++) {
                float t = m[i * 4 + j];
                m[i * 4 + j] = m[j * 4 + i];
                m[j * 4 + i] = t;
            }
    }

    /* general 4x4 inverse by cofactors */
    public void invert() {
        float[] a = m, inv = new float[16];
        inv[0] = a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
        inv[4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
        inv[8] = a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
        inv[12] = -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
        inv[1] = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
        inv[5] = a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
        inv[9] = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
        inv[13] = a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
        inv[2] = a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
        inv[6] = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
        inv[10] = a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
        inv[14] = -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];
        inv[3] = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
        inv[7] = a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
        inv[11] = -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11] - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
        inv[15] = a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10] + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];
        float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        if (det == 0) throw new ArithmeticException();
        det = 1 / det;
        for (int i = 0; i < 16; i++) m[i] = inv[i] * det;
    }

    public void transform(float[] v) {
        if (v == null) throw new NullPointerException();
        if (v.length % 4 != 0) throw new IllegalArgumentException();
        for (int i = 0; i < v.length; i += 4) {
            float x = v[i], y = v[i + 1], z = v[i + 2], w = v[i + 3];
            for (int r = 0; r < 4; r++)
                v[i + r] = m[r * 4] * x + m[r * 4 + 1] * y + m[r * 4 + 2] * z + m[r * 4 + 3] * w;
        }
    }
}
