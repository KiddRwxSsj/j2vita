/*
 * Minimal PS Vita MIDI Player for the MIDP 2 media compatibility layer.
 *
 * This intentionally implements only the subset used by Java ME games such
 * as Bounce Tales: createPlayer(InputStream,"audio/midi"), prefetch, volume,
 * loop count, start/stop/deallocate/close and basic media-time queries.
 */
package javax.microedition.media;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;
import javax.microedition.media.control.VolumeControl;

final class VitaMidiPlayer implements Player, VolumeControl {
    private int nativeHandle;
    private int state = UNREALIZED;
    private int loopCount = 1;
    private int level = 100;
    private boolean muted = false;
    private final String contentType;
    private final Vector listeners = new Vector();
    private int playbackGeneration = 0;
    private boolean endEventSent = false;
    private static final Vector livePlayers = new Vector();

    VitaMidiPlayer(InputStream stream, String type) throws IOException, MediaException {
        if (stream == null) {
            throw new IllegalArgumentException();
        }
        if (!isMidiType(type)) {
            throw new MediaException("Unsupported Vita media type: " + type);
        }

        contentType = "audio/midi";

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] temp = new byte[1024];
        for (;;) {
            int n = stream.read(temp, 0, temp.length);
            if (n < 0) {
                break;
            }
            if (n > 0) {
                out.write(temp, 0, n);
            }
        }

        byte[] data = out.toByteArray();
        if (data.length == 0) {
            throw new MediaException("Empty MIDI stream");
        }

        nativeHandle = nCreate(data);
        if (nativeHandle == 0) {
            throw new MediaException("Vita MIDI backend could not load stream");
        }

        registerLivePlayer();
    }

    static boolean isMidiType(String type) {
        if (type == null) {
            return false;
        }
        return type.equalsIgnoreCase("audio/midi") ||
               type.equalsIgnoreCase("audio/mid") ||
               type.equalsIgnoreCase("audio/x-midi") ||
               type.equalsIgnoreCase("audio/sp-midi");
    }

    public synchronized void realize() throws MediaException {
        checkOpen();
        if (state < REALIZED) {
            state = REALIZED;
        }
    }

    public synchronized void prefetch() throws MediaException {
        checkOpen();
        if (state < REALIZED) {
            realize();
        }
        if (state < PREFETCHED) {
            state = PREFETCHED;
        }
    }

    public synchronized void start() throws MediaException {
        checkOpen();

        if (state == STARTED) {
            checkNaturalEndLocked();

            if (state == STARTED && nIsPlaying(nativeHandle)) {
                return;
            }

            if (state == STARTED) {
                state = PREFETCHED;
            }
        }

        if (state < PREFETCHED) {
            prefetch();
        }

        endEventSent = false;
        playbackGeneration++;

        if (!nStart(nativeHandle, loopCount)) {
            throw new MediaException("Vita MIDI start failed");
        }

        state = STARTED;

        fire(PlayerListener.STARTED, new Long(getMediaTime()));
        startEndWatcher();
    }

    public synchronized void stop() throws MediaException {
        checkOpen();

        if (state == STARTED) {
            playbackGeneration++;
            endEventSent = true;

            if (!nStop(nativeHandle)) {
                throw new MediaException("Vita MIDI stop failed");
            }

            state = PREFETCHED;
            fire(PlayerListener.STOPPED, new Long(getMediaTime()));
        }
    }

    public synchronized void deallocate() {
        if (state == CLOSED) {
            return;
        }

        playbackGeneration++;
        endEventSent = true;

        if (state == STARTED) {
            nStop(nativeHandle);
        }

        if (state >= PREFETCHED) {
            state = REALIZED;
        }
    }

    public synchronized void close() {
        playbackGeneration++;
        endEventSent = true;
        if (state == CLOSED) {
            return;
        }
        if (nativeHandle != 0) {
            nClose(nativeHandle);
            nativeHandle = 0;
        }
        state = CLOSED;
        unregisterLivePlayer();
        fire(PlayerListener.CLOSED, null);
        listeners.removeAllElements();
    }

    public synchronized long setMediaTime(long now) throws MediaException {
        checkOpen();
        if (now < 0) {
            now = 0;
        }
        long result = nSetMediaTime(nativeHandle, now);
        return result < 0 ? getMediaTime() : result;
    }

    public synchronized long getMediaTime() {
        if (state == CLOSED || nativeHandle == 0) {
            return 0;
        }
        long value = nGetMediaTime(nativeHandle);
        return value < 0 ? 0 : value;
    }

    public synchronized int getState() {
        if (state == STARTED) {
            checkNaturalEndLocked();
        }

        return state;
    }

    public synchronized long getDuration() {
        if (state == CLOSED || nativeHandle == 0) {
            return TIME_UNKNOWN;
        }
        long value = nGetDuration(nativeHandle);
        return value < 0 ? TIME_UNKNOWN : value;
    }

    public String getContentType() {
        return contentType;
    }

    public synchronized void setLoopCount(int count) {
        checkOpen();
        if (count == 0) {
            throw new IllegalArgumentException("loop count cannot be zero");
        }
        if (state == STARTED) {
            throw new IllegalStateException("player already started");
        }
        loopCount = count;
    }

    public synchronized void addPlayerListener(PlayerListener listener) {
        checkOpen();
        if (listener != null && !listeners.contains(listener)) {
            listeners.addElement(listener);
        }
    }

    public synchronized void removePlayerListener(PlayerListener listener) {
        if (listener != null) {
            listeners.removeElement(listener);
        }
    }

    public synchronized Control[] getControls() {
        checkOpen();
        return new Control[] { this };
    }

    public synchronized Control getControl(String controlType) {
        checkOpen();
        if (controlType == null) {
            throw new IllegalArgumentException();
        }
        if (controlType.equals("VolumeControl") ||
            controlType.equals("javax.microedition.media.control.VolumeControl")) {
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
        return level;
    }


    private void registerLivePlayer() {
        synchronized (livePlayers) {
            if (!livePlayers.contains(this)) {
                livePlayers.addElement(this);
            }
        }
    }

    private void unregisterLivePlayer() {
        synchronized (livePlayers) {
            livePlayers.removeElement(this);
        }
    }

    private synchronized void forceCloseSilently() {
        playbackGeneration++;
        endEventSent = true;
        if (state == CLOSED) {
            unregisterLivePlayer();
            return;
        }

        if (nativeHandle != 0) {
            try {
                nStop(nativeHandle);
            } catch (Throwable ignored) {
            }
            try {
                nClose(nativeHandle);
            } catch (Throwable ignored) {
            }
            nativeHandle = 0;
        }

        state = CLOSED;
        unregisterLivePlayer();
        listeners.removeAllElements();
    }

    static void vitaForceShutdownAll() {
        VitaMidiPlayer[] snapshot;

        synchronized (livePlayers) {
            snapshot = new VitaMidiPlayer[livePlayers.size()];
            livePlayers.copyInto(snapshot);
            livePlayers.removeAllElements();
        }

        for (int i = 0; i < snapshot.length; i++) {
            try {
                if (snapshot[i] != null) {
                    snapshot[i].forceCloseSilently();
                }
            } catch (Throwable ignored) {
            }
        }
    }


    private void checkNaturalEndLocked() {
        if (state != STARTED || nativeHandle == 0 || endEventSent) {
            return;
        }

        if (!nIsPlaying(nativeHandle)) {
            state = PREFETCHED;
            endEventSent = true;
            playbackGeneration++;

            long mediaTime = nGetMediaTime(nativeHandle);


            fire(PlayerListener.END_OF_MEDIA, new Long(mediaTime));
        }
    }

    private void startEndWatcher() {
        final int generation = playbackGeneration;

        Thread watcher = new Thread(new Runnable() {
            public void run() {
                for (;;) {
                    try {
                        Thread.sleep(25);
                    } catch (Throwable ignored) {
                    }

                    synchronized (VitaMidiPlayer.this) {
                        if (state != STARTED ||
                                nativeHandle == 0 ||
                                generation != playbackGeneration) {
                            return;
                        }

                        checkNaturalEndLocked();

                        if (state != STARTED ||
                                generation != playbackGeneration) {
                            return;
                        }
                    }
                }
            }
        });

        try {
            watcher.setPriority(Thread.MIN_PRIORITY);
        } catch (Throwable ignored) {
        }

        watcher.start();
    }

    private void checkOpen() {
        if (state == CLOSED) {
            throw new IllegalStateException("player is closed");
        }
    }

    private void fire(String event, Object data) {
        for (int i = 0; i < listeners.size(); i++) {
            try {
                ((PlayerListener)listeners.elementAt(i)).playerUpdate(this, event, data);
            } catch (Throwable ignored) {
            }
        }
    }

    private static native int nCreate(byte[] data);
    private static native boolean nStart(int handle, int loopCount);
    private static native boolean nStop(int handle);
    private static native boolean nIsPlaying(int handle);
    private static native void nClose(int handle);
    private static native int nSetVolume(int handle, int level);
    private static native long nGetMediaTime(int handle);
    private static native long nGetDuration(int handle);
    private static native long nSetMediaTime(int handle, long value);
}
