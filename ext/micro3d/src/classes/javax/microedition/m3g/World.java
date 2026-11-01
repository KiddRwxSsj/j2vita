package javax.microedition.m3g;

import java.util.Vector;

public class World extends Group {
    Camera camera;
    Background background;

    public World() {
    }

    public void setActiveCamera(Camera c) {
        if (c == null) throw new NullPointerException();
        camera = c;
    }

    public Camera getActiveCamera() { return camera; }
    public void setBackground(Background b) { background = b; }
    public Background getBackground() { return background; }

    void refs(Vector v) {
        super.refs(v);
        if (camera != null && !children.contains(camera)) v.addElement(camera);
        add(v, background);
    }
}
