package com.mascotcapsule.micro3d.v3;

import java.io.IOException;

public class Figure {
    int[] blob;
    Texture[] textures;
    int textureIndex = -1;

    public Figure(byte[] b) {
        if (b == null) throw new NullPointerException();
        load(b);
    }

    public Figure(String name) throws IOException {
        load(Render.resource(name));
    }

    private void load(byte[] b) {
        int n = Render.figureSize(b, 0, b.length);
        if (n <= 0) throw new IllegalArgumentException("bad figure");
        blob = new int[n];
        Render.figureLoad(b, 0, b.length, blob);
    }

    public final void dispose() {
    }

    public final int getNumPattern() {
        return blob[Render.F_NUM_PATTERNS];
    }

    public final void setPattern(int p) {
        blob[Render.F_PATTERN] = p;
    }

    public final void setPosture(ActionTable act, int action, int frame) {
        if (act == null) throw new NullPointerException();
        if (action < 0 || action >= act.getNumActions()) throw new IllegalArgumentException();
        Render.posture(blob, act.blob, action, frame);
    }

    public final int getNumTextures() {
        return textures == null ? 0 : textures.length;
    }

    public final Texture getTexture() {
        return textureIndex < 0 ? null : textures[textureIndex];
    }

    public final void selectTexture(int idx) {
        if (idx < 0 || idx >= getNumTextures()) throw new IllegalArgumentException();
        textureIndex = idx;
    }

    public final void setTexture(Texture tex) {
        if (tex == null) throw new NullPointerException();
        if (!tex.forModel) throw new IllegalArgumentException();
        textures = new Texture[] {tex};
        textureIndex = 0;
    }

    public final void setTexture(Texture[] t) {
        if (t == null) throw new NullPointerException();
        if (t.length == 0) throw new IllegalArgumentException();
        for (int i = 0; i < t.length; i++) {
            if (t[i] == null) throw new NullPointerException();
            if (!t[i].forModel) throw new IllegalArgumentException();
        }
        textures = t;
        textureIndex = -1;
    }
}
