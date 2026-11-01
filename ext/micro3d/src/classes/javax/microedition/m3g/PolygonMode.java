package javax.microedition.m3g;

public class PolygonMode extends Object3D {
    public static final int CULL_BACK = 160;
    public static final int CULL_FRONT = 161;
    public static final int CULL_NONE = 162;
    public static final int SHADE_FLAT = 164;
    public static final int SHADE_SMOOTH = 165;
    public static final int WINDING_CCW = 168;
    public static final int WINDING_CW = 169;

    int culling = CULL_BACK, shading = SHADE_SMOOTH, winding = WINDING_CCW;
    boolean twoSided, localCamera, perspective = true;

    public PolygonMode() {
    }

    public void setCulling(int m) {
        if (m < CULL_BACK || m > CULL_NONE) throw new IllegalArgumentException();
        culling = m;
    }

    public int getCulling() { return culling; }
    public void setShading(int m) { shading = m; }
    public int getShading() { return shading; }

    public void setWinding(int m) {
        if (m != WINDING_CCW && m != WINDING_CW) throw new IllegalArgumentException();
        winding = m;
    }

    public int getWinding() { return winding; }
    public void setTwoSidedLightingEnable(boolean on) { twoSided = on; }
    public boolean isTwoSidedLightingEnabled() { return twoSided; }
    public void setLocalCameraLightingEnable(boolean on) { localCamera = on; }
    public boolean isLocalCameraLightingEnabled() { return localCamera; }
    public void setPerspectiveCorrectionEnable(boolean on) { perspective = on; }
    public boolean isPerspectiveCorrectionEnabled() { return perspective; }
}
