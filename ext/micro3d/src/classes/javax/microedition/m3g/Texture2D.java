package javax.microedition.m3g;

import java.util.Vector;

public class Texture2D extends Transformable {
    public static final int FILTER_BASE_LEVEL = 208;
    public static final int FILTER_LINEAR = 209;
    public static final int FILTER_NEAREST = 210;
    public static final int FUNC_ADD = 224;
    public static final int FUNC_BLEND = 225;
    public static final int FUNC_DECAL = 226;
    public static final int FUNC_MODULATE = 227;
    public static final int FUNC_REPLACE = 228;
    public static final int WRAP_CLAMP = 240;
    public static final int WRAP_REPEAT = 241;

    Image2D image;
    int blending = FUNC_MODULATE, wrapS = WRAP_REPEAT, wrapT = WRAP_REPEAT;
    int levelFilter = FILTER_BASE_LEVEL, imageFilter = FILTER_NEAREST, blendColor;

    public Texture2D(Image2D img) {
        setImage(img);
    }

    public void setImage(Image2D img) {
        if (img == null) throw new NullPointerException();
        image = img;
    }

    public Image2D getImage() { return image; }

    public void setFiltering(int level, int img) {
        levelFilter = level;
        imageFilter = img;
    }

    public int getLevelFilter() { return levelFilter; }
    public int getImageFilter() { return imageFilter; }

    public void setWrapping(int s, int t) {
        if (s < WRAP_CLAMP || s > WRAP_REPEAT || t < WRAP_CLAMP || t > WRAP_REPEAT) {
            throw new IllegalArgumentException();
        }
        wrapS = s;
        wrapT = t;
    }

    public int getWrappingS() { return wrapS; }
    public int getWrappingT() { return wrapT; }

    public void setBlending(int f) {
        if (f < FUNC_ADD || f > FUNC_REPLACE) throw new IllegalArgumentException();
        blending = f;
    }

    public int getBlending() { return blending; }
    public void setBlendColor(int c) { blendColor = c & 0xffffff; }
    public int getBlendColor() { return blendColor; }

    void refs(Vector v) {
        super.refs(v);
        add(v, image);
    }
}
