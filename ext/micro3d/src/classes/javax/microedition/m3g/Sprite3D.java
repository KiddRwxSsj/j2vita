package javax.microedition.m3g;

import java.util.Vector;

/* kept for loading and references, not drawn */
public class Sprite3D extends Node {
    Image2D image;
    Appearance appearance;
    boolean scaled;
    int cropX, cropY, cropW, cropH;

    public Sprite3D(boolean scaled, Image2D img, Appearance ap) {
        this.scaled = scaled;
        appearance = ap;
        setImage(img);
    }

    public boolean isScaled() { return scaled; }
    public void setAppearance(Appearance a) { appearance = a; }
    public Appearance getAppearance() { return appearance; }

    public void setImage(Image2D img) {
        image = img;
        if (img != null) { cropX = 0; cropY = 0; cropW = img.width; cropH = img.height; }
    }

    public Image2D getImage() { return image; }
    public void setCrop(int x, int y, int w, int h) { cropX = x; cropY = y; cropW = w; cropH = h; }
    public int getCropX() { return cropX; }
    public int getCropY() { return cropY; }
    public int getCropWidth() { return cropW; }
    public int getCropHeight() { return cropH; }

    void refs(Vector v) {
        super.refs(v);
        add(v, image);
        add(v, appearance);
    }
}
