package javax.microedition.m3g;

import java.util.Vector;

public abstract class Object3D {
    int userID;
    Object userObject;
    Vector tracks;

    /* bumped whenever references change, invalidates the flat lists below */
    static int version;
    private int flatVersion = -1;
    private Object3D[] flat;
    private Object3D[] animated;
    private float[] sum, s;

    public final int getUserID() { return userID; }
    public final void setUserID(int id) { userID = id; }
    public final Object getUserObject() { return userObject; }
    public final void setUserObject(Object o) { userObject = o; }

    public final void addAnimationTrack(AnimationTrack t) {
        if (t == null) throw new NullPointerException();
        if (tracks == null) tracks = new Vector();
        if (!tracks.contains(t)) tracks.addElement(t);
        version++;
    }

    public final void removeAnimationTrack(AnimationTrack t) {
        if (tracks != null) tracks.removeElement(t);
        version++;
    }

    public final AnimationTrack getAnimationTrack(int i) {
        if (tracks == null || i < 0 || i >= tracks.size()) throw new IndexOutOfBoundsException();
        return (AnimationTrack)tracks.elementAt(i);
    }

    public final int getAnimationTrackCount() { return tracks == null ? 0 : tracks.size(); }

    /* everything this object points at, never its parent */
    void refs(Vector v) {
        if (tracks != null)
            for (int i = 0; i < tracks.size(); i++) v.addElement(tracks.elementAt(i));
    }

    static void add(Vector v, Object o) {
        if (o != null) v.addElement(o);
    }

    public int getReferences(Object3D[] out) {
        Vector v = new Vector();
        refs(v);
        if (out != null) {
            if (out.length < v.size()) throw new IllegalArgumentException();
            v.copyInto(out);
        }
        return v.size();
    }

    /* everything reachable from here, each object once */
    private void flatten() {
        if (flatVersion == version && flat != null) return;
        Vector all = new Vector(), anim = new Vector(), v = new Vector();
        java.util.Hashtable seen = new java.util.Hashtable();
        all.addElement(this);
        seen.put(this, this);
        for (int i = 0; i < all.size(); i++) {
            Object3D o = (Object3D)all.elementAt(i);
            if (o.tracks != null && o.tracks.size() > 0) anim.addElement(o);
            v.removeAllElements();
            o.refs(v);
            for (int k = 0; k < v.size(); k++) {
                Object r = v.elementAt(k);
                if (!seen.containsKey(r)) {
                    seen.put(r, r);
                    all.addElement(r);
                }
            }
        }
        flat = new Object3D[all.size()];
        all.copyInto(flat);
        animated = new Object3D[anim.size()];
        anim.copyInto(animated);
        flatVersion = version;
    }

    public Object3D find(int id) {
        if (userID == id) return this;
        flatten();
        for (int i = 0; i < flat.length; i++)
            if (flat[i].userID == id) return flat[i];
        return null;
    }

    public final Object3D duplicate() {
        return this;
    }

    public final int animate(int time) {
        animateTree(time);
        return 0;
    }

    void animateTree(int time) {
        flatten();
        for (int i = 0; i < animated.length; i++) animated[i].applyTracks(time);
    }

    private void applyTracks(int time) {
        int n = tracks.size();
        if (sum == null) {
            sum = new float[4];
            s = new float[4];
        }
        float[] sum = this.sum, s = this.s;
        for (int i = 0; i < n; i++) {
            AnimationTrack ti = (AnimationTrack)tracks.elementAt(i);
            int prop = ti.property;
            boolean seenBefore = false;
            for (int k = 0; k < i; k++)
                if (((AnimationTrack)tracks.elementAt(k)).property == prop) { seenBefore = true; break; }
            if (seenBefore) continue;
            float total = 0;
            int comps = 0;
            for (int j = 0; j < 4; j++) sum[j] = 0;
            for (int k = i; k < n; k++) {
                AnimationTrack t = (AnimationTrack)tracks.elementAt(k);
                if (t.property != prop) continue;
                AnimationController c = t.controller;
                if (c == null || !c.active(time) || c.weight <= 0) continue;
                KeyframeSequence ks = t.sequence;
                comps = ks.sample(c.position(time), s);
                if (prop == AnimationTrack.ORIENTATION && total > 0
                        && s[0] * sum[0] + s[1] * sum[1] + s[2] * sum[2] + s[3] * sum[3] < 0) {
                    for (int j = 0; j < 4; j++) s[j] = -s[j];
                }
                for (int j = 0; j < comps; j++) sum[j] += s[j] * c.weight;
                total += c.weight;
            }
            if (total > 0) {
                for (int j = 0; j < comps; j++) sum[j] /= total;
                setProperty(prop, sum, comps);
            }
        }
    }

    void setProperty(int prop, float[] v, int n) {
    }
}
