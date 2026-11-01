package javax.microedition.m3g;

import java.util.Vector;

public class Background extends Object3D {
    public static final int BORDER = 32;
    public static final int REPEAT = 33;

    int color;
    Image2D image;
    int modeX = BORDER, modeY = BORDER;
    int cropX, cropY, cropW, cropH;
    boolean depthClear = true, colorClear = true;

    public Background() {
    }

    public void setColor(int argb) { color = argb; }
    public int getColor() { return color; }

    public void setImage(Image2D img) {
        if (img != null && img.format != Image2D.RGB && img.format != Image2D.RGBA) {
            throw new IllegalArgumentException();
        }
        image = img;
        if (img != null) {
            cropX = 0; cropY = 0; cropW = img.width; cropH = img.height;
        }
    }

    public Image2D getImage() { return image; }

    public void setImageMode(int x, int y) {
        modeX = x;
        modeY = y;
    }

    public int getImageModeX() { return modeX; }
    public int getImageModeY() { return modeY; }

    public void setCrop(int x, int y, int w, int h) {
        if (w < 0 || h < 0) throw new IllegalArgumentException();
        cropX = x; cropY = y; cropW = w; cropH = h;
    }

    public int getCropX() { return cropX; }
    public int getCropY() { return cropY; }
    public int getCropWidth() { return cropW; }
    public int getCropHeight() { return cropH; }
    public void setDepthClearEnable(boolean on) { depthClear = on; }
    public boolean isDepthClearEnabled() { return depthClear; }
    public void setColorClearEnable(boolean on) { colorClear = on; }
    public boolean isColorClearEnabled() { return colorClear; }

    void refs(Vector v) {
        super.refs(v);
        add(v, image);
    }

    void setProperty(int prop, float[] v, int n) {
        if (prop == AnimationTrack.COLOR && n >= 3) {
            color = (color & 0xff000000) | Appearance.rgb(v);
        } else if (prop == AnimationTrack.CROP && n >= 2) {
            cropX = (int)v[0];
            cropY = (int)v[1];
            if (n >= 4) { cropW = (int)v[2]; cropH = (int)v[3]; }
        }
    }
}
