package javax.microedition.m3g;

import javax.microedition.lcdui.Image;

public class Image2D extends Object3D {
    public static final int ALPHA = 96;
    public static final int LUMINANCE = 97;
    public static final int LUMINANCE_ALPHA = 98;
    public static final int RGB = 99;
    public static final int RGBA = 100;

    int format, width, height;
    boolean mutable;
    /* w, h, then ARGB pixels: what the native side samples */
    int[] tex;

    public Image2D(int format, Object image) {
        check(format);
        if (image == null) throw new NullPointerException();
        if (!(image instanceof Image)) throw new IllegalArgumentException();
        Image img = (Image)image;
        alloc(format, img.getWidth(), img.getHeight());
        int[] row = new int[width];
        for (int y = 0; y < height; y++) {
            img.getRGB(row, 0, width, 0, y, width, 1);
            for (int x = 0; x < width; x++) {
                int c = row[x];
                tex[2 + y * width + x] = convert(c >>> 24, (c >> 16) & 255, (c >> 8) & 255, c & 255);
            }
        }
    }

    public Image2D(int format, int w, int h) {
        check(format);
        if (w <= 0 || h <= 0) throw new IllegalArgumentException();
        alloc(format, w, h);
        mutable = true;
        for (int i = 0; i < w * h; i++) tex[2 + i] = 0xffffffff;
    }

    public Image2D(int format, int w, int h, byte[] pixels) {
        this(format, w, h, pixels, null);
    }

    public Image2D(int format, int w, int h, byte[] pixels, byte[] palette) {
        check(format);
        if (pixels == null) throw new NullPointerException();
        if (w <= 0 || h <= 0) throw new IllegalArgumentException();
        alloc(format, w, h);
        int bpp = bpp(format);
        for (int i = 0; i < w * h; i++) {
            int o = palette != null ? (pixels[i] & 255) * bpp : i * bpp;
            byte[] src = palette != null ? palette : pixels;
            tex[2 + i] = o + bpp <= src.length ? fromBytes(src, o) : 0;
        }
    }

    private static void check(int f) {
        if (f < ALPHA || f > RGBA) throw new IllegalArgumentException();
    }

    private void alloc(int f, int w, int h) {
        format = f;
        width = w;
        height = h;
        tex = new int[2 + w * h];
        tex[0] = w;
        tex[1] = h;
    }

    static int bpp(int f) {
        return f == RGBA ? 4 : f == RGB ? 3 : f == LUMINANCE_ALPHA ? 2 : 1;
    }

    private int fromBytes(byte[] b, int o) {
        switch (format) {
        case ALPHA: return convert(b[o] & 255, 255, 255, 255);
        case LUMINANCE: { int l = b[o] & 255; return convert(255, l, l, l); }
        case LUMINANCE_ALPHA: { int l = b[o] & 255; return convert(b[o + 1] & 255, l, l, l); }
        case RGB: return convert(255, b[o] & 255, b[o + 1] & 255, b[o + 2] & 255);
        default: return convert(b[o + 3] & 255, b[o] & 255, b[o + 1] & 255, b[o + 2] & 255);
        }
    }

    private int convert(int a, int r, int g, int b) {
        switch (format) {
        case ALPHA: return a << 24 | 0xffffff;
        case LUMINANCE: { int l = (r + g + b) / 3; return 0xff000000 | l << 16 | l << 8 | l; }
        case LUMINANCE_ALPHA: { int l = (r + g + b) / 3; return a << 24 | l << 16 | l << 8 | l; }
        case RGB: return 0xff000000 | r << 16 | g << 8 | b;
        default: return a << 24 | r << 16 | g << 8 | b;
        }
    }

    public void set(int x, int y, int w, int h, byte[] pixels) {
        if (!mutable) throw new IllegalStateException();
        if (pixels == null) throw new NullPointerException();
        int bpp = bpp(format);
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                int o = (j * w + i) * bpp;
                if (x + i < width && y + j < height && o + bpp <= pixels.length)
                    tex[2 + (y + j) * width + x + i] = fromBytes(pixels, o);
            }
    }

    public int getFormat() { return format; }
    public int getWidth() { return width; }
    public int getHeight() { return height; }
    public boolean isMutable() { return mutable; }
}
