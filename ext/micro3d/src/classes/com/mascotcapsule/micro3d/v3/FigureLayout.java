package com.mascotcapsule.micro3d.v3;

public class FigureLayout {
    static final int SCALE = 0, PARALLEL = 1, FOV = 2, PERSP_WH = 3;

    AffineTrans[] affineArray;
    AffineTrans affine;
    int scaleX, scaleY;
    int centerX, centerY;
    int parallelWidth, parallelHeight;
    int near, far, angle;
    int perspWidth, perspHeight;
    int projection;

    public FigureLayout() {
        this(null, 512, 512, 0, 0);
    }

    public FigureLayout(AffineTrans trans, int sx, int sy, int cx, int cy) {
        setAffineTrans(trans);
        setScale(sx, sy);
        centerX = cx;
        centerY = cy;
    }

    public final AffineTrans getAffineTrans() { return affine; }
    public final int getCenterX() { return centerX; }
    public final int getCenterY() { return centerY; }
    public final int getScaleX() { return scaleX; }
    public final int getScaleY() { return scaleY; }
    public final int getParallelWidth() { return parallelWidth; }
    public final int getParallelHeight() { return parallelHeight; }

    public final void setAffineTrans(AffineTrans trans) {
        if (trans == null) {
            trans = new AffineTrans();
            trans.setIdentity();
        }
        if (affineArray == null) affineArray = new AffineTrans[] {trans};
        affine = trans;
    }

    public final void setAffineTrans(AffineTrans[] trans) {
        if (trans == null) throw new NullPointerException();
        for (int i = 0; i < trans.length; i++) {
            if (trans[i] == null) throw new NullPointerException();
        }
        affineArray = trans;
    }

    public final void setAffineTransArray(AffineTrans[] trans) {
        setAffineTrans(trans);
    }

    public final void selectAffineTrans(int idx) {
        if (affineArray == null || idx < 0 || idx >= affineArray.length) {
            throw new IllegalArgumentException();
        }
        affine = affineArray[idx];
    }

    public final void setCenter(int cx, int cy) {
        centerX = cx;
        centerY = cy;
    }

    public final void setScale(int sx, int sy) {
        scaleX = sx;
        scaleY = sy;
        projection = SCALE;
    }

    public final void setParallelSize(int w, int h) {
        if (w < 0 || h < 0) throw new IllegalArgumentException();
        parallelWidth = w;
        parallelHeight = h;
        projection = PARALLEL;
    }

    public final void setPerspective(int zNear, int zFar, int angle) {
        if (zNear >= zFar || zNear < 1 || zFar > 32767 || angle < 1 || angle > 2047) {
            throw new IllegalArgumentException();
        }
        near = zNear;
        far = zFar;
        this.angle = angle;
        projection = FOV;
    }

    public final void setPerspective(int zNear, int zFar, int width, int height) {
        if (zNear >= zFar || zNear < 1 || zFar > 32767 || width < 0 || height < 0) {
            throw new IllegalArgumentException();
        }
        near = zNear;
        far = zFar;
        perspWidth = width;
        perspHeight = height;
        projection = PERSP_WH;
    }

    /* packs what the renderer needs into env, see Render */
    void fill(int[] env, int x, int y) {
        env[Render.E_CX] = centerX + x;
        env[Render.E_CY] = centerY + y;
        env[Render.E_PROJ] = projection;
        switch (projection) {
        case SCALE:
            env[Render.E_P0] = scaleX; env[Render.E_P1] = scaleY;
            break;
        case PARALLEL:
            env[Render.E_P0] = parallelWidth; env[Render.E_P1] = parallelHeight;
            break;
        case FOV:
            env[Render.E_P0] = near; env[Render.E_P1] = far; env[Render.E_P2] = angle;
            break;
        default:
            env[Render.E_P0] = near; env[Render.E_P1] = far;
            env[Render.E_P2] = perspWidth; env[Render.E_P3] = perspHeight;
            break;
        }
        affine.get(env, Render.E_MTX);
    }
}
