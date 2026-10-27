/*
 * PSPKVM Vita short-sound Player.
 *
 * Supported:
 *   audio/x-wav / audio/wav / audio/wave
 *   audio/amr / audio/x-amr (AMR-NB)
 */
package javax.microedition.media;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;
import javax.microedition.media.control.VolumeControl;

final class VitaSfxPlayer implements Player, VolumeControl {
    private static final int KIND_WAV = 1;
    private static final int KIND_AMR = 2;
    private static final Vector livePlayers = new Vector();

    private int nativeHandle;
    private int state = UNREALIZED;
    private int loopCount = 1;
    private int level = 100;
    private boolean muted;
    private final String contentType;
    private final Vector listeners = new Vector();

    VitaSfxPlayer(InputStream stream, String type)
            throws IOException, MediaException {
        if (stream == null) throw new IllegalArgumentException();

        int kind = kindForType(type);
        if (kind == 0) {
            throw new MediaException("Unsupported Vita SFX type: " + type);
        }

        contentType = kind == KIND_AMR ? "audio/amr" : "audio/x-wav";

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] temp = new byte[2048];

        for (;;) {
            int n = stream.read(temp, 0, temp.length);
            if (n < 0) break;
            if (n > 0) out.write(temp, 0, n);
        }

        byte[] data = out.toByteArray();
        if (data.length == 0) {
            throw new MediaException("Empty audio stream");
        }

        nativeHandle = nCreate(data, kind);
        if (nativeHandle == 0) {
            throw new MediaException(
                    "Vita SFX backend could not decode " + contentType);
        }

        synchronized (livePlayers) {
            livePlayers.addElement(this);
        }
    }

    static boolean isSfxType(String type) {
        return kindForType(type) != 0;
    }

    private static int kindForType(String type) {
        if (type == null) return 0;

        if (type.equalsIgnoreCase("audio/x-wav") ||
                type.equalsIgnoreCase("audio/wav") ||
                type.equalsIgnoreCase("audio/wave")) {
            return KIND_WAV;
        }

        if (type.equalsIgnoreCase("audio/amr") ||
                type.equalsIgnoreCase("audio/x-amr")) {
            return KIND_AMR;
        }

        return 0;
    }

    public synchronized void realize() throws MediaException {
        checkOpen();
        if (state < REALIZED) state = REALIZED;
    }

    public synchronized void prefetch() throws MediaException {
        checkOpen();
        if (state < REALIZED) realize();
        if (state < PREFETCHED) state = PREFETCHED;
    }

    public synchronized void start() throws MediaException {
        checkOpen();
        if (state < PREFETCHED) prefetch();

        if (state == STARTED && nIsPlaying(nativeHandle)) {
            return;
        }

        if (!nStart(nativeHandle, loopCount)) {
            throw new MediaException("Vita SFX start failed");
        }

        state = STARTED;
        fire(PlayerListener.STARTED, new Long(getMediaTime()));
    }

    public synchronized void stop() throws MediaException {
        checkOpen();
        if (state == STARTED) {
            nStop(nativeHandle);
            state = PREFETCHED;
            fire(PlayerListener.STOPPED, new Long(getMediaTime()));
        }
    }

    public synchronized void deallocate() {
        if (state == CLOSED) return;
        if (state == STARTED) nStop(nativeHandle);
        if (state >= PREFETCHED) state = REALIZED;
    }

    public synchronized void close() {
        if (state == CLOSED) return;

        if (nativeHandle != 0) {
            nStop(nativeHandle);
            nClose(nativeHandle);
            nativeHandle = 0;
        }

        state = CLOSED;

        synchronized (livePlayers) {
            livePlayers.removeElement(this);
        }

        fire(PlayerListener.CLOSED, null);
        listeners.removeAllElements();
    }

    private synchronized void forceCloseSilently() {
        if (state == CLOSED) return;

        if (nativeHandle != 0) {
            try { nStop(nativeHandle); } catch (Throwable ignored) {}
            try { nClose(nativeHandle); } catch (Throwable ignored) {}
            nativeHandle = 0;
        }

        state = CLOSED;
        listeners.removeAllElements();
    }

    static void vitaForceShutdownAll() {
        VitaSfxPlayer[] snapshot;

        synchronized (livePlayers) {
            snapshot = new VitaSfxPlayer[livePlayers.size()];
            livePlayers.copyInto(snapshot);
            livePlayers.removeAllElements();
        }

        for (int i = 0; i < snapshot.length; i++) {
            try {
                if (snapshot[i] != null) snapshot[i].forceCloseSilently();
            } catch (Throwable ignored) {
            }
        }
    }

    static void playTone(int note, int duration, int volume)
            throws MediaException {
        if (!nPlayTone(note, duration, volume)) {
            throw new MediaException("Vita tone playback failed");
        }
    }

    public synchronized long setMediaTime(long now) throws MediaException {
        checkOpen();
        if (now < 0) now = 0;
        return nSetMediaTime(nativeHandle, now);
    }

    public synchronized long getMediaTime() {
        checkOpen();
        return nGetMediaTime(nativeHandle);
    }

    public synchronized long getDuration() {
        checkOpen();
        return nGetDuration(nativeHandle);
    }

    public synchronized String getContentType() {
        checkOpen();
        return contentType;
    }

    public synchronized void setLoopCount(int count) {
        checkOpen();
        if (state == STARTED) {
            throw new IllegalStateException(
                    "cannot change loop count while started");
        }
        if (count == 0) {
            throw new IllegalArgumentException("loop count cannot be zero");
        }
        loopCount = count;
    }

    public synchronized int getState() {
        if (state == STARTED &&
                nativeHandle != 0 &&
                !nIsPlaying(nativeHandle)) {
            state = PREFETCHED;
        }
        return state;
    }

    public synchronized void addPlayerListener(PlayerListener listener) {
        checkOpen();
        if (listener != null && !listeners.contains(listener)) {
            listeners.addElement(listener);
        }
    }

    public synchronized void removePlayerListener(PlayerListener listener) {
        checkOpen();
        listeners.removeElement(listener);
    }

    public synchronized Control[] getControls() {
        checkOpen();
        return new Control[] { this };
    }

    public synchronized Control getControl(String controlType) {
        checkOpen();
        if (controlType == null) throw new IllegalArgumentException();

        if (controlType.equals("VolumeControl") ||
                controlType.equals(
                    "javax.microedition.media.control.VolumeControl")) {
            return this;
        }

        return null;
    }

    public synchronized void setMute(boolean value) {
        checkOpen();
        if (muted != value) {
            muted = value;
            nSetVolume(nativeHandle, muted ? 0 : level);
            fire(PlayerListener.VOLUME_CHANGED, this);
        }
    }

    public synchronized boolean isMuted() {
        checkOpen();
        return muted;
    }

    public synchronized int setLevel(int value) {
        checkOpen();
        if (value < 0) value = 0;
        if (value > 100) value = 100;

        level = value;
        nSetVolume(nativeHandle, muted ? 0 : level);
        fire(PlayerListener.VOLUME_CHANGED, this);
        return level;
    }

    public synchronized int getLevel() {
        checkOpen();
        return level;
    }

    private void checkOpen() {
        if (state == CLOSED) {
            throw new IllegalStateException("player is closed");
        }
    }

    private void fire(String event, Object data) {
        for (int i = 0; i < listeners.size(); i++) {
            try {
                ((PlayerListener)listeners.elementAt(i))
                        .playerUpdate(this, event, data);
            } catch (Throwable ignored) {
            }
        }
    }

    private static native int nCreate(byte[] data, int kind);
    private static native boolean nStart(int handle, int loopCount);
    private static native boolean nStop(int handle);
    private static native boolean nIsPlaying(int handle);
    private static native void nClose(int handle);
    private static native int nSetVolume(int handle, int level);
    private static native long nGetMediaTime(int handle);
    private static native long nGetDuration(int handle);
    private static native long nSetMediaTime(int handle, long value);
    private static native boolean nPlayTone(
            int note, int duration, int volume);
}
