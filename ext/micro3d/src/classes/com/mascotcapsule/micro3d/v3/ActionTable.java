package com.mascotcapsule.micro3d.v3;

import java.io.IOException;

public class ActionTable {
    int[] blob;

    public ActionTable(byte[] b) {
        if (b == null) throw new NullPointerException();
        load(b);
    }

    public ActionTable(String name) throws IOException {
        load(Render.resource(name));
    }

    private void load(byte[] b) {
        int n = Render.actionSize(b, 0, b.length);
        if (n <= 0) throw new IllegalArgumentException("bad action table");
        blob = new int[n];
        Render.actionLoad(b, 0, b.length, blob);
    }

    public final void dispose() {
    }

    public final int getNumActions() {
        return blob[Render.A_NUM_ACTIONS];
    }

    public final int getNumAction() {
        return getNumActions();
    }

    public final int getNumFrames(int idx) {
        if (idx < 0 || idx >= getNumActions()) throw new IllegalArgumentException();
        return Render.actionFrames(blob, idx);
    }

    public final int getNumFrame(int idx) {
        return getNumFrames(idx);
    }
}
