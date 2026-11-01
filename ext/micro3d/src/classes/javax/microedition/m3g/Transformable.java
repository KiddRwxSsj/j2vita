package javax.microedition.m3g;

public abstract class Transformable extends Object3D {
    float tx, ty, tz;
    float qx, qy, qz, qw = 1;
    float sx = 1, sy = 1, sz = 1;
    Transform general;

    public void setTranslation(float x, float y, float z) { tx = x; ty = y; tz = z; }
    public void translate(float x, float y, float z) { tx += x; ty += y; tz += z; }

    public void getTranslation(float[] v) {
        if (v.length < 3) throw new IllegalArgumentException();
        v[0] = tx; v[1] = ty; v[2] = tz;
    }

    public void setOrientation(float angle, float ax, float ay, float az) {
        float[] q = new float[4];
        Transform.axisAngle(q, angle, ax, ay, az);
        qx = q[0]; qy = q[1]; qz = q[2]; qw = q[3];
    }

    private void mulQuat(float[] q, boolean post) {
        float ax = qx, ay = qy, az = qz, aw = qw, bx = q[0], by = q[1], bz = q[2], bw = q[3];
        if (!post) {
            float t;
            t = ax; ax = bx; bx = t; t = ay; ay = by; by = t;
            t = az; az = bz; bz = t; t = aw; aw = bw; bw = t;
        }
        qx = aw * bx + ax * bw + ay * bz - az * by;
        qy = aw * by - ax * bz + ay * bw + az * bx;
        qz = aw * bz + ax * by - ay * bx + az * bw;
        qw = aw * bw - ax * bx - ay * by - az * bz;
    }

    public void postRotate(float angle, float ax, float ay, float az) {
        float[] q = new float[4];
        Transform.axisAngle(q, angle, ax, ay, az);
        mulQuat(q, true);
    }

    public void preRotate(float angle, float ax, float ay, float az) {
        float[] q = new float[4];
        Transform.axisAngle(q, angle, ax, ay, az);
        mulQuat(q, false);
    }

    public void getOrientation(float[] v) {
        if (v.length < 4) throw new IllegalArgumentException();
        float s = (float)Math.sqrt(qx * qx + qy * qy + qz * qz);
        if (s == 0) {
            v[0] = 0; v[1] = 0; v[2] = 0; v[3] = 0;
            return;
        }
        // CLDC has no acos, so go through atan2 built from sqrt
        float w = qw < -1 ? -1 : qw > 1 ? 1 : qw;
        v[0] = (float)(2 * atan(s, w) * 180 / Math.PI);
        v[1] = qx / s; v[2] = qy / s; v[3] = qz / s;
    }

    private static double atan(double y, double x) {
        // y >= 0 here
        double a = Math.abs(x) < 1e-6 ? Math.PI / 2 : 0;
        if (a == 0) {
            double t = y / Math.abs(x), r = 0;
            double u = t > 1 ? 1 / t : t;
            r = u / (1 + 0.28 * u * u);
            if (t > 1) r = Math.PI / 2 - r;
            a = x < 0 ? Math.PI - r : r;
        }
        return a;
    }

    public void setScale(float x, float y, float z) { sx = x; sy = y; sz = z; }
    public void scale(float x, float y, float z) { sx *= x; sy *= y; sz *= z; }

    public void getScale(float[] v) {
        if (v.length < 3) throw new IllegalArgumentException();
        v[0] = sx; v[1] = sy; v[2] = sz;
    }

    public void setTransform(Transform t) {
        general = t == null ? null : new Transform(t);
    }

    public void getTransform(Transform t) {
        if (general == null) t.setIdentity();
        else t.set(general);
    }

    /* T * R * S * M */
    void composite(float[] out) {
        Transform.quatToMatrix(out, qx, qy, qz, qw);
        for (int i = 0; i < 3; i++) {
            out[i * 4] *= sx;
            out[i * 4 + 1] *= sy;
            out[i * 4 + 2] *= sz;
        }
        out[3] = tx; out[7] = ty; out[11] = tz;
        if (general != null) Transform.mul(out, out, general.m);
    }

    public void getCompositeTransform(Transform t) {
        composite(t.m);
    }

    void setProperty(int prop, float[] v, int n) {
        switch (prop) {
        case AnimationTrack.TRANSLATION:
            if (n >= 3) { tx = v[0]; ty = v[1]; tz = v[2]; }
            break;
        case AnimationTrack.ORIENTATION:
            if (n >= 4) {
                float l = (float)Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
                if (l > 0) { qx = v[0] / l; qy = v[1] / l; qz = v[2] / l; qw = v[3] / l; }
            }
            break;
        case AnimationTrack.SCALE:
            if (n >= 3) { sx = v[0]; sy = v[1]; sz = v[2]; }
            else if (n == 1) { sx = sy = sz = v[0]; }
            break;
        }
    }
}
