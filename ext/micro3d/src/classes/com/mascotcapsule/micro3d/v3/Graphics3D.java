package com.mascotcapsule.micro3d.v3;

import javax.microedition.lcdui.Graphics;

/*
 * Drawing goes straight into the bound Graphics. Like the handsets the depth
 * buffer starts empty on bind and after every drawFigure and flush.
 */
public class Graphics3D {
    public static final int COMMAND_LIST_VERSION_1_0 = 0xFE000001;
    public static final int COMMAND_END = 0x80000000;
    public static final int COMMAND_NOP = 0x81000000;
    public static final int COMMAND_FLUSH = 0x82000000;
    public static final int COMMAND_ATTRIBUTE = 0x83000000;
    public static final int COMMAND_CLIP = 0x84000000;
    public static final int COMMAND_CENTER = 0x85000000;
    public static final int COMMAND_TEXTURE_INDEX = 0x86000000;
    public static final int COMMAND_AFFINE_INDEX = 0x87000000;
    public static final int COMMAND_PARALLEL_SCALE = 0x90000000;
    public static final int COMMAND_PARALLEL_SIZE = 0x91000000;
    public static final int COMMAND_PERSPECTIVE_FOV = 0x92000000;
    public static final int COMMAND_PERSPECTIVE_WH = 0x93000000;
    public static final int COMMAND_AMBIENT_LIGHT = 0xA0000000;
    public static final int COMMAND_DIRECTION_LIGHT = 0xA1000000;
    public static final int COMMAND_THRESHOLD = 0xAF000000;

    public static final int PRIMITVE_POINTS = 0x01000000;
    public static final int PRIMITVE_LINES = 0x02000000;
    public static final int PRIMITVE_TRIANGLES = 0x03000000;
    public static final int PRIMITVE_QUADS = 0x04000000;
    public static final int PRIMITVE_POINT_SPRITES = 0x05000000;

    public static final int POINT_SPRITE_LOCAL_SIZE = 0;
    public static final int POINT_SPRITE_PIXEL_SIZE = 1;
    public static final int POINT_SPRITE_PERSPECTIVE = 0;
    public static final int POINT_SPRITE_NO_PERS = 2;

    public static final int ENV_ATTR_LIGHTING = 1;
    public static final int ENV_ATTR_SPHERE_MAP = 2;
    public static final int ENV_ATTR_TOON_SHADING = 4;
    public static final int ENV_ATTR_SEMI_TRANSPARENT = 8;

    public static final int PATTR_LIGHTING = 1;
    public static final int PATTR_SPHERE_MAP = 2;
    public static final int PATTR_COLORKEY = 16;
    public static final int PATTR_BLEND_NORMAL = 0;
    public static final int PATTR_BLEND_HALF = 32;
    public static final int PATTR_BLEND_ADD = 64;
    public static final int PATTR_BLEND_SUB = 96;

    public static final int PDATA_NORMAL_NONE = 0;
    public static final int PDATA_NORMAL_PER_FACE = 512;
    public static final int PDATA_NORMAL_PER_VERTEX = 768;
    public static final int PDATA_COLOR_NONE = 0;
    public static final int PDATA_COLOR_PER_COMMAND = 1024;
    public static final int PDATA_COLOR_PER_FACE = 2048;
    public static final int PDATA_TEXURE_COORD_NONE = 0;
    public static final int PDATA_POINT_SPRITE_PARAMS_PER_CMD = 4096;
    public static final int PDATA_POINT_SPRITE_PARAMS_PER_FACE = 8192;
    public static final int PDATA_POINT_SPRITE_PARAMS_PER_VERTEX = 12288;
    public static final int PDATA_TEXURE_COORD = 12288;

    private Graphics target;
    private final int[] env = new int[Render.E_LEN];

    public Graphics3D() {
    }

    public final synchronized void bind(Graphics g) {
        if (g == null) throw new NullPointerException();
        if (target != null) throw new IllegalStateException("Target already bound");
        target = g;
        Render.bind(g);
    }

    public final synchronized void release(Graphics g) {
        if (g == null) throw new NullPointerException();
        target = null;
    }

    public final void flush() {
        check();
        Render.bind(target);
    }

    public final void dispose() {
        target = null;
    }

    private void check() {
        if (target == null) throw new IllegalStateException("No target is bound");
    }

    private void setup(int x, int y, FigureLayout layout, Effect3D effect) {
        layout.fill(env, x, y);
        int attr = 0;
        Light l = effect.light;
        if (l != null) {
            attr |= ENV_ATTR_LIGHTING;
            env[Render.E_AMB] = l.ambIntensity;
            env[Render.E_DIR] = l.dirIntensity;
            env[Render.E_LX] = l.direction.x;
            env[Render.E_LY] = l.direction.y;
            env[Render.E_LZ] = l.direction.z;
            if (effect.shading == Effect3D.TOON_SHADING) attr |= ENV_ATTR_TOON_SHADING;
        }
        if (effect.transparency) attr |= ENV_ATTR_SEMI_TRANSPARENT;
        env[Render.E_ATTR] = attr;
        env[Render.E_TOON] = effect.toonThreshold;
        env[Render.E_TOON_HI] = effect.toonHigh;
        env[Render.E_TOON_LO] = effect.toonLow;
    }

    private void draw(Figure figure, Texture[] tex, int x, int y, FigureLayout layout, Effect3D effect) {
        check();
        if (figure == null || layout == null || effect == null) throw new NullPointerException();
        setup(x, y, layout, effect);
        env[Render.E_PATTERN] = figure.blob[Render.F_PATTERN];
        Object[] blobs = null;
        if (tex != null) {
            blobs = new Object[tex.length];
            for (int i = 0; i < tex.length; i++) blobs[i] = tex[i] == null ? null : tex[i].blob;
        }
        Render.figure(target, figure.blob, blobs, env);
    }

    public final void renderFigure(Figure figure, int x, int y, FigureLayout layout, Effect3D effect) {
        draw(figure, figure == null ? null : figure.textures, x, y, layout, effect);
    }

    public final void drawFigure(Figure figure, int x, int y, FigureLayout layout, Effect3D effect) {
        Texture t = figure == null ? null : figure.getTexture();
        draw(figure, t != null ? new Texture[] {t} : figure == null ? null : figure.textures,
             x, y, layout, effect);
        Render.bind(target);
    }

    public final void renderPrimitives(Texture texture, int x, int y, FigureLayout layout,
                                       Effect3D effect, int command, int numPrimitives,
                                       int[] vertexCoords, int[] normals, int[] textureCoords,
                                       int[] colors) {
        check();
        if (layout == null || effect == null || vertexCoords == null || normals == null
                || textureCoords == null || colors == null) {
            throw new NullPointerException();
        }
        if (command < 0 || numPrimitives <= 0 || numPrimitives >= 256) {
            throw new IllegalArgumentException();
        }
        setup(x, y, layout, effect);
        Render.primitives(target, texture == null ? null : texture.blob, env, command,
                          numPrimitives, vertexCoords, normals, textureCoords, colors);
    }

    public final void drawCommandList(Texture[] textures, int x, int y, FigureLayout layout,
                                      Effect3D effect, int[] commandList) {
        check();
        if (layout == null || effect == null || commandList == null) throw new NullPointerException();
        System.out.println("micro3d: drawCommandList not supported");
    }

    public final void drawCommandList(Texture texture, int x, int y, FigureLayout layout,
                                      Effect3D effect, int[] commandList) {
        drawCommandList(new Texture[] {texture}, x, y, layout, effect, commandList);
    }
}
