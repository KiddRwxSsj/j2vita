package com.mascotcapsule.micro3d.v3;

import java.io.IOException;

public class Texture {
    int[] blob;
    final boolean forModel;

    public Texture(byte[] b, boolean isForModel) {
        if (b == null) throw new NullPointerException();
        forModel = isForModel;
        load(b);
    }

    public Texture(String name, boolean isForModel) throws IOException {
        forModel = isForModel;
        load(Render.resource(name));
    }

    private void load(byte[] b) {
        int n = Render.textureSize(b, 0, b.length);
        if (n <= 0) throw new IllegalArgumentException("bad texture");
        blob = new int[n];
        Render.textureLoad(b, 0, b.length, blob);
    }

    public final void dispose() {
    }
}
