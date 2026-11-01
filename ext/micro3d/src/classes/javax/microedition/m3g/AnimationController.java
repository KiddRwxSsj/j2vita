package javax.microedition.m3g;

public class AnimationController extends Object3D {
    float speed = 1, weight = 1, refSeq;
    int start, end, refWorld;

    public AnimationController() {
    }

    public void setActiveInterval(int s, int e) {
        if (s > e) throw new IllegalArgumentException();
        start = s;
        end = e;
    }

    public int getActiveIntervalStart() { return start; }
    public int getActiveIntervalEnd() { return end; }

    public void setSpeed(float s, int worldTime) {
        refSeq = position(worldTime);
        refWorld = worldTime;
        speed = s;
    }

    public float getSpeed() { return speed; }

    public void setPosition(float seq, int worldTime) {
        refSeq = seq;
        refWorld = worldTime;
    }

    public float getPosition(int worldTime) { return position(worldTime); }
    public int getRefWorldTime() { return refWorld; }

    public void setWeight(float w) {
        if (w < 0) throw new IllegalArgumentException();
        weight = w;
    }

    public float getWeight() { return weight; }

    boolean active(int t) {
        return start == end || (t >= start && t < end);
    }

    float position(int t) {
        return refSeq + speed * (t - refWorld);
    }
}
