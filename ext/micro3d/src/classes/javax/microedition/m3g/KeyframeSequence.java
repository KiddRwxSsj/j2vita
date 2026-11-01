package javax.microedition.m3g;

public class KeyframeSequence extends Object3D {
    public static final int CONSTANT = 192;
    public static final int LOOP = 193;
    public static final int LINEAR = 176;
    public static final int SLERP = 177;
    public static final int SPLINE = 178;
    public static final int SQUAD = 179;
    public static final int STEP = 180;

    final int interpolation, comps, count;
    final int[] times;
    final float[] values;
    int repeat = CONSTANT, duration, first, last;

    public KeyframeSequence(int numKeyframes, int numComponents, int interpolation) {
        if (numKeyframes < 1 || numComponents < 1 || interpolation < LINEAR || interpolation > STEP) {
            throw new IllegalArgumentException();
        }
        count = numKeyframes;
        comps = numComponents;
        this.interpolation = interpolation;
        times = new int[numKeyframes];
        values = new float[numKeyframes * numComponents];
        last = numKeyframes - 1;
    }

    public int getComponentCount() { return comps; }
    public int getKeyframeCount() { return count; }
    public int getInterpolationType() { return interpolation; }

    public void setKeyframe(int i, int time, float[] v) {
        if (v == null) throw new NullPointerException();
        if (i < 0 || i >= count) throw new IndexOutOfBoundsException();
        if (time < 0 || v.length < comps) throw new IllegalArgumentException();
        times[i] = time;
        System.arraycopy(v, 0, values, i * comps, comps);
    }

    public int getKeyframe(int i, float[] v) {
        if (v != null) System.arraycopy(values, i * comps, v, 0, comps);
        return times[i];
    }

    public void setValidRange(int f, int l) {
        if (f < 0 || l >= count || f > l) throw new IndexOutOfBoundsException();
        first = f;
        last = l;
    }

    public int getValidRangeFirst() { return first; }
    public int getValidRangeLast() { return last; }

    public void setDuration(int d) {
        if (d <= 0) throw new IllegalArgumentException();
        duration = d;
    }

    public int getDuration() { return duration; }

    public void setRepeatMode(int m) {
        if (m != CONSTANT && m != LOOP) throw new IllegalArgumentException();
        repeat = m;
    }

    public int getRepeatMode() { return repeat; }

    /* writes up to 4 components at sequence time t, returns how many */
    int sample(float t, float[] out) {
        int n = comps > 4 ? 4 : comps;
        if (repeat == LOOP && duration > 0) {
            t = t % duration;
            if (t < 0) t += duration;
        }
        boolean loop = repeat == LOOP && duration > 0 && first != last;
        int a, b;
        float ta, tb;
        if (t < times[first]) {
            if (!loop) { copy(first, out, n); return n; }
            a = last;
            b = first;
            ta = times[last];
            tb = times[first] + duration;
            t += duration;
        } else {
            // last keyframe at or before t; sequences often pack every action, so bisect
            int lo = first, hi = last;
            while (lo < hi) {
                int mid = (lo + hi + 1) >> 1;
                if (times[mid] <= t) lo = mid;
                else hi = mid - 1;
            }
            a = lo;
            ta = times[a];
            if (a < last) {
                b = a + 1;
                tb = times[b];
            } else {
                if (!loop) { copy(a, out, n); return n; }
                b = first;
                tb = times[first] + duration;
            }
        }
        if (interpolation == STEP || tb <= ta) { copy(a, out, n); return n; }
        float s = (t - ta) / (tb - ta);
        int oa = a * comps, ob = b * comps;
        boolean quat = (interpolation == SLERP || interpolation == SQUAD) && n == 4;
        float sign = 1;
        if (quat) {
            float d = 0;
            for (int i = 0; i < 4; i++) d += values[oa + i] * values[ob + i];
            if (d < 0) sign = -1;
        }
        for (int i = 0; i < n; i++)
            out[i] = values[oa + i] + (values[ob + i] * sign - values[oa + i]) * s;
        return n;
    }

    private void copy(int k, float[] out, int n) {
        System.arraycopy(values, k * comps, out, 0, n);
    }
}
