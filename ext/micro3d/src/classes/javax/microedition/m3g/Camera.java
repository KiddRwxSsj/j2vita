package javax.microedition.m3g;

public class Camera extends Node {
    public static final int GENERIC = 48;
    public static final int PARALLEL = 49;
    public static final int PERSPECTIVE = 50;

    int type = GENERIC;
    float fovy, aspect = 1, near = 0.1f, far = 1;
    final Transform matrix = new Transform();

    public Camera() {
    }

    public void setParallel(float h, float a, float n, float f) {
        if (h <= 0 || a <= 0) throw new IllegalArgumentException();
        type = PARALLEL; fovy = h; aspect = a; near = n; far = f;
    }

    public void setPerspective(float fov, float a, float n, float f) {
        if (fov <= 0 || fov >= 180 || a <= 0 || n <= 0 || f <= 0) throw new IllegalArgumentException();
        type = PERSPECTIVE; fovy = fov; aspect = a; near = n; far = f;
    }

    public void setGeneric(Transform t) {
        type = GENERIC;
        matrix.set(t);
    }

    public int getProjection(float[] p) {
        if (p != null && type != GENERIC) {
            p[0] = fovy; p[1] = aspect; p[2] = near; p[3] = far;
        }
        return type;
    }

    public int getProjection(Transform t) {
        if (t != null) t.set(matrix);
        return type;
    }

    void setProperty(int prop, float[] v, int n) {
        if (prop == AnimationTrack.FIELD_OF_VIEW) fovy = v[0];
        else if (prop == AnimationTrack.NEAR_DISTANCE) near = v[0];
        else if (prop == AnimationTrack.FAR_DISTANCE) far = v[0];
        else super.setProperty(prop, v, n);
    }
}
