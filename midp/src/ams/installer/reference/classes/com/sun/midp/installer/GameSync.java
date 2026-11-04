package com.sun.midp.installer;

import java.io.ByteArrayInputStream;
import java.util.Hashtable;
import java.util.Vector;

import com.sun.midp.io.j2me.storage.File;
import com.sun.midp.io.j2me.storage.RandomAccessStream;
import com.sun.midp.jarutil.JarReader;
import com.sun.midp.midlet.MIDletSuite;
import com.sun.midp.midletsuite.InstallInfo;
import com.sun.midp.midletsuite.MIDletSuiteImpl;
import com.sun.midp.midletsuite.MIDletSuiteStorage;
import com.sun.midp.configurator.Constants;

/**
 * Keeps the installed suites in step with the JARs in the games folder and
 * its subfolders. Zips holding JARs are unpacked in place and removed.
 * New JARs are installed silently, a JAR overwritten with another copy
 * updates its suite. Suites whose JAR was removed are hidden but kept, so
 * their saves come back with the JAR, and a moved JAR takes its suite along.
 */
public final class GameSync {
    public static final String ROOT = "ux0:data/j2vita/games/";

    /* url -> size it had when the install failed, retried once the size changes */
    private static final Hashtable failed = new Hashtable();
    /* folder hash seen by the last sync, and a newer one waiting to settle */
    private static int synced, pending;
    private static boolean haveSynced, havePending;
    /* url -> size at the last sync, a JAR keeping its size skips the manifest check */
    private static Hashtable sizes = new Hashtable();
    private static Hashtable hidden = new Hashtable();
    private static boolean pruned;
    /* ids of suites whose JAR was already gone after the last pass */
    private static Hashtable wasGone = new Hashtable();
    /* suites removed or rewritten since the launcher last asked */
    private static final Vector touched = new Vector();
    /* url -> size of zips with nothing usable inside */
    private static final Hashtable badZips = new Hashtable();
    private static boolean more;
    /* JARs left alone because another copy of the game is installed */
    private static final Hashtable dupes = new Hashtable();
    /* folders under ROOT from the last scan, relative with a trailing slash */
    private static String[] dirs = new String[0];

    /* one sync pass stops after this long so the launcher can show progress */
    private static final long BUDGET = 6000;
    private static final int DEPTH = 4;

    private GameSync() {
    }

    /**
     * Cheap poll for the launcher: true once the folder differs from the
     * last sync and looks the same on two calls in a row, so a JAR still
     * being copied is not installed half written.
     */
    public static boolean changed() {
        int h = hash();
        synchronized (GameSync.class) {
            if (haveSynced && h == synced) {
                havePending = false;
                return false;
            }
            if (!havePending || h != pending) {
                pending = h;
                havePending = true;
                return false;
            }
            return true;
        }
    }

    private static int hash() {
        try {
            return SuiteDownloadInfo.dirHash0(ROOT, DEPTH);
        } catch (Throwable t) {
            return 0;
        }
    }

    /** Returns true when the suite list changed. */
    public static synchronized boolean sync() {
        long t0 = System.currentTimeMillis();
        more = false;
        if (!cacheLoaded) loadCache();
        Vector jars = list();
        if (jars == null) return false;
        long end = System.currentTimeMillis() + BUDGET;
        if (unzip(jars, end)) jars = list();
        if (jars == null) return false;
        synced = hash();
        haveSynced = true;
        havePending = false;
        jars = only(jars, false);
        Hashtable present = new Hashtable();
        for (int i = 0; i < jars.size(); i++) {
            Jar j = (Jar)jars.elementAt(i);
            present.put(j.info.url, j);
        }

        MIDletSuiteStorage store = MIDletSuiteStorage.getMIDletSuiteStorage();
        Hashtable installed = new Hashtable();
        Hashtable stale = new Hashtable();
        /* MIDlet-Name|MIDlet-Vendor -> url of suites whose JAR is still there */
        Hashtable live = new Hashtable();
        /* suites whose JAR is gone, as {id, name|vendor, JAR size, old url} */
        Vector gone = new Vector();
        int[] ids = store.getListOfSuites();
        for (int i = 0; ids != null && i < ids.length; i++) {
            String[] si = suiteInfo(store, ids[i]);
            String url = si[0];
            if (url == null) continue;
            installed.put(url, url);
            if (!url.startsWith(ROOT)) continue;
            if (!present.containsKey(url)) {
                gone.addElement(new Object[] {new Integer(ids[i]), si[1], new Integer(jarSize(ids[i])), url});
                continue;
            }
            live.put(si[1], url);
            Jar j = (Jar)present.get(url);
            if ((changedSize(j) && !sameGame(store, ids[i], url)) || otherBuild(ids[i], j)) {
                /* the file was overwritten with another copy */
                stale.put(url, new Integer(ids[i]));
            }
        }
        boolean hiddenGone = gone.size() > 0;

        boolean changed = false, relinked = false;
        if (hiddenGone) {
            /*
             * A moved JAR only needs its suite pointed at the new place. JARs
             * new since the last pass go first, so a copy that was already
             * here can't take the suite.
             */
            Vector order = new Vector();
            int fresh = 0;
            for (int i = 0; i < jars.size(); i++) {
                Jar j = (Jar)jars.elementAt(i);
                if (installed.containsKey(j.info.url)) continue;
                if (sizes.containsKey(j.info.url)) order.addElement(j);
                else order.insertElementAt(j, fresh++);
            }
            for (int i = 0; i < order.size() && gone.size() > 0; i++) {
                if (relinked && System.currentTimeMillis() > end) {
                    more = true;
                    break;
                }
                Jar j = (Jar)order.elementAt(i);
                String url = j.info.url, key;
                /* same file name and size is the same file, no need to open it */
                int g = findMoved(gone, url.substring(url.lastIndexOf('/') + 1), j.size);
                if (g >= 0) {
                    key = (String)((Object[])gone.elementAt(g))[1];
                } else {
                    key = manifestKey(url);
                    g = findGone(gone, key, j.size);
                }
                if (g < 0 || !relink((Object[])gone.elementAt(g), url, key)) continue;
                gone.removeElementAt(g);
                installed.put(j.info.url, j.info.url);
                live.put(key, j.info.url);
                changed = relinked = true;
            }
            if (relinked) SuiteDownloadInfo.saveSuites0();
        }

        int work = 0;
        /* moves left for the next pass must not be taken for new games */
        for (int i = more ? jars.size() : 0; i < jars.size(); i++) {
            Jar j = (Jar)jars.elementAt(i);
            SuiteDownloadInfo d = j.info;
            boolean update = stale.containsKey(d.url);
            if (installed.containsKey(d.url) && !update) continue;
            Integer lost = (Integer)failed.get(d.url);
            if (lost != null && lost.intValue() == j.size) continue;
            if (work++ > 0 && System.currentTimeMillis() > end) {
                more = true;
                break;
            }
            Object[] heir = null;
            if (!update) {
                /* the silent installer would quietly take over the other copy's suite */
                String key = manifestKey(d.url);
                String twin = (String)live.get(key);
                if (twin != null && !twin.equals(d.url)) {
                    if (!dupes.containsKey(d.url)) System.out.println("GameSync: " + d.url + " is the same game as " + twin);
                    dupes.put(d.url, twin);
                    continue;
                }
                /* another build of a game whose JAR is gone gets its suite and saves */
                int g = findGone(gone, key, -1);
                if (g >= 0) {
                    heir = (Object[])gone.elementAt(g);
                    if (sizes.containsKey(d.url) && !wasGone.containsKey(heir[0])) {
                        /* the game's own JAR may still be on its way */
                        more = true;
                        continue;
                    }
                    if (!relink(heir, d.url, key)) continue;
                    gone.removeElementAt(g);
                    SuiteDownloadInfo.saveSuites0();
                    update = true;
                }
            }
            System.out.println("GameSync: " + (update ? "updating " : "installing ") + d.url);
            try {
                try {
                    install(d, update);
                    if (update && heir == null) touched.addElement(stale.get(d.url));
                } catch (InvalidJadException e) {
                    int r = e.getReason();
                    /* a JAR swapped for another copy of the same game updates it */
                    if (!hiddenGone || (r != InvalidJadException.ALREADY_INSTALLED
                            && r != InvalidJadException.OLD_VERSION
                            && r != InvalidJadException.NEW_VERSION)) throw e;
                    /* but never take over a copy that is still in the folder */
                    String twin = (String)live.get(manifestKey(d.url));
                    if (twin != null && !twin.equals(d.url)) throw new Exception("same game as " + twin);
                    install(d, true);
                }
                failed.remove(d.url);
                changed = true;
            } catch (Throwable t) {
                System.out.println("GameSync: failed " + d.url + " " + t);
                failed.put(d.url, new Integer(j.size));
                if (heir != null && relink(heir, (String)heir[3], (String)heir[1])) SuiteDownloadInfo.saveSuites0();
            }
        }

        Hashtable still = new Hashtable();
        for (int i = 0; i < gone.size(); i++) {
            Object id = ((Object[])gone.elementAt(i))[0];
            still.put(id, id);
        }
        wasGone = still;

        Hashtable now = new Hashtable();
        for (int i = 0; i < jars.size(); i++) {
            Jar j = (Jar)jars.elementAt(i);
            now.put(j.info.url, new Integer(j.size));
        }
        if (!same(now, sizes)) cacheDirty = true;
        sizes = now;

        if (prune(store, present)) changed = true;
        if (cacheDirty) saveCache();
        System.out.println("GameSync: pass " + (System.currentTimeMillis() - t0) + " ms");
        return changed;
    }

    /**
     * Drops suites that lost to a newer install (two suites on one JAR keep
     * the newest). A suite without its JAR stays hidden for its saves and
     * comes back when the JAR shows up again, moved or not.
     */
    private static boolean prune(MIDletSuiteStorage store, Hashtable present) {
        int[] ids = store.getListOfSuites();
        if (ids == null) return false;
        String[] urls = new String[ids.length];
        Hashtable newest = new Hashtable();
        for (int i = 0; i < ids.length; i++) {
            urls[i] = suiteInfo(store, ids[i])[0];
            if (urls[i] == null || !present.containsKey(urls[i])) continue;
            Integer prev = (Integer)newest.get(urls[i]);
            if (prev == null || prev.intValue() < ids[i]) newest.put(urls[i], new Integer(ids[i]));
        }

        Hashtable hide = new Hashtable();
        boolean removed = false;
        for (int i = 0; i < ids.length; i++) {
            String url = urls[i];
            if (url == null || !url.startsWith(ROOT)) continue;
            if (!present.containsKey(url)) {
                hide.put(new Integer(ids[i]), url);
                continue;
            }
            if (((Integer)newest.get(url)).intValue() == ids[i]) continue;
            try {
                long t = System.currentTimeMillis();
                store.remove(ids[i]);
                System.out.println("GameSync: removed old suite " + ids[i] + " " + url + " in "
                        + (System.currentTimeMillis() - t) + " ms");
                infos.remove(new Integer(ids[i]));
                cacheDirty = true;
                touched.addElement(new Integer(ids[i]));
                removed = true;
            } catch (Throwable t) {
                hide.put(new Integer(ids[i]), url);
            }
        }
        hidden = hide;
        pruned = true;
        return removed;
    }

    /**
     * Points a suite whose JAR is gone at url instead of installing it again,
     * which keeps its id and saves and takes a few ms.
     */
    private static boolean relink(Object[] g, String url, String key) {
        Integer id = (Integer)g[0];
        if (!SuiteDownloadInfo.setJarUrl0(id.intValue(), url)) {
            System.out.println("GameSync: could not move suite " + id + " to " + url);
            return false;
        }
        infos.put(id, new String[] {url, key});
        cacheDirty = true;
        touched.addElement(id);
        System.out.println("GameSync: moved suite " + id + " to " + url);
        return true;
    }

    /* index in gone of a suite of this game, with a JAR of this size unless size < 0 */
    private static int findGone(Vector gone, String key, int size) {
        for (int i = 0; i < gone.size(); i++) {
            Object[] g = (Object[])gone.elementAt(i);
            if (!key.equals(g[1])) continue;
            if (size < 0 || ((Integer)g[2]).intValue() == size) return i;
        }
        return -1;
    }

    /* url -> size of a JAR already updated because its suite was installed from another size */
    private static final Hashtable resized = new Hashtable();

    private static boolean otherBuild(int id, Jar j) {
        int n = jarSize(id);
        if (n < 0 || n == j.size || j.size < 0) return false;
        Integer size = new Integer(j.size);
        if (size.equals(resized.get(j.info.url))) return false;
        resized.put(j.info.url, size);
        return true;
    }

    private static int findMoved(Vector gone, String file, int size) {
        for (int i = 0; i < gone.size(); i++) {
            Object[] g = (Object[])gone.elementAt(i);
            String old = (String)g[3];
            if (((Integer)g[2]).intValue() == size && old.substring(old.lastIndexOf('/') + 1).equals(file)) return i;
        }
        return -1;
    }

    private static int jarSize(int id) {
        String sj = SuiteDownloadInfo.suiteJar0(id);
        try {
            return Integer.parseInt(sj.substring(sj.lastIndexOf('\t') + 1));
        } catch (Throwable t) {
            return -1;
        }
    }

    private static String manifestKey(String url) {
        try {
            byte[] raw = JarReader.readJarEntry(url, MIDletSuite.JAR_MANIFEST);
            ManifestProperties m = new ManifestProperties();
            m.load(new ByteArrayInputStream(raw));
            String n = m.getProperty("MIDlet-Name"), v = m.getProperty("MIDlet-Vendor");
            return (n == null ? null : n.trim()) + "|" + (v == null ? null : v.trim());
        } catch (Throwable t) {
            return "";
        }
    }

    private static boolean changedSize(Jar j) {
        Integer old = (Integer)sizes.get(j.info.url);
        return old == null || old.intValue() != j.size || j.size < 0;
    }

    /** False when the JAR's manifest no longer matches the installed suite. */
    private static boolean sameGame(MIDletSuiteStorage store, int id, String url) {
        ManifestProperties m = new ManifestProperties();
        try {
            byte[] raw = JarReader.readJarEntry(url, MIDletSuite.JAR_MANIFEST);
            if (raw == null) return true;
            m.load(new ByteArrayInputStream(raw));
        } catch (Throwable t) {
            return true;
        }
        MIDletSuiteImpl suite = null;
        try {
            suite = store.getMIDletSuite(id, false);
            if (suite == null) return true;
            for (int i = 0; i < KEYS.length; i++) {
                String a = m.getProperty(KEYS[i]);
                String b = suite.getProperty(KEYS[i]);
                if (a != null && !a.trim().equals(b == null ? "" : b.trim())) return false;
            }
            return true;
        } catch (Throwable t) {
            return true;
        } finally {
            if (suite != null) suite.close();
        }
    }

    private static final String[] KEYS = {
        "MIDlet-Name", "MIDlet-Vendor", "MIDlet-Version", "MIDlet-1"
    };

    /* suite id -> {jar url, name|vendor}; an update can move a suite to another JAR */
    private static final Hashtable infos = new Hashtable();

    private static String[] suiteInfo(MIDletSuiteStorage store, int id) {
        if (!cacheLoaded) loadCache();
        Integer key = new Integer(id);
        String[] r = (String[])infos.get(key);
        if (r != null) return r;
        r = new String[2];
        MIDletSuiteImpl suite = null;
        try {
            suite = store.getMIDletSuite(id, false);
            if (suite != null) {
                InstallInfo ii = suite.getInstallInfo();
                r[0] = ii == null ? null : ii.getJarUrl();
                String n = suite.getProperty("MIDlet-Name"), v = suite.getProperty("MIDlet-Vendor");
                r[1] = (n == null ? null : n.trim()) + "|" + (v == null ? null : v.trim());
            }
        } catch (Throwable t) {
        } finally {
            if (suite != null) suite.close();
        }
        if (r[0] == null) {
            /* a suite whose JAR moved does not open (MIDP calls it corrupted), the suite list still knows it */
            String sj = SuiteDownloadInfo.suiteJar0(id);
            int tab = sj == null ? -1 : sj.indexOf('\t');
            if (tab > 0) {
                String nv = sj.substring(tab + 1, sj.lastIndexOf('\t'));
                int bar = nv.indexOf('|');
                r[0] = sj.substring(0, tab);
                r[1] = bar < 0 ? nv : nv.substring(0, bar).trim() + "|" + nv.substring(bar + 1).trim();
            }
        }
        if (r[0] != null) {
            infos.put(key, r);
            cacheDirty = true;
        }
        return r;
    }

    /*
     * suiteInfo and the JAR sizes survive restarts in a file next to the
     * suite database, so the first sync after boot does not reopen every
     * suite and every JAR. Lines are id, url, name|vendor and size, tab separated.
     */
    private static boolean cacheLoaded, cacheDirty;

    private static synchronized void loadCache() {
        if (cacheLoaded) return;
        cacheLoaded = true;
        byte[] b = readStore("gamesync.cache");
        if (b == null) return;
        try {
            String text = com.pspkvm.system.J2Vita.utf8(b, 0, b.length);
            for (int p = 0; p < text.length();) {
                int e = text.indexOf('\n', p);
                if (e < 0) e = text.length();
                String[] f = split(text.substring(p, e));
                p = e + 1;
                if (f == null) continue;
                Integer id = Integer.valueOf(f[0]);
                infos.put(id, new String[] {f[1], f[2]});
                sizes.put(f[1], Integer.valueOf(f[3]));
            }
        } catch (Throwable t) {
        }
    }

    /** A file next to the suite database, or null. */
    public static byte[] readStore(String name) {
        RandomAccessStream s = new RandomAccessStream();
        try {
            s.connect(File.getStorageRoot(Constants.INTERNAL_STORAGE_ID) + name,
                    javax.microedition.io.Connector.READ);
            byte[] b = new byte[s.getSizeOf()];
            int n = 0;
            while (n < b.length) {
                int r = s.readBytes(b, n, b.length - n);
                if (r <= 0) return null;
                n += r;
            }
            return b;
        } catch (Throwable t) {
            return null;
        } finally {
            try { s.disconnect(); } catch (Throwable t) { }
        }
    }

    public static void writeStore(String name, byte[] data) {
        RandomAccessStream s = new RandomAccessStream();
        try {
            s.connect(File.getStorageRoot(Constants.INTERNAL_STORAGE_ID) + name,
                    RandomAccessStream.READ_WRITE_TRUNCATE);
            s.writeBytes(data, 0, data.length);
            s.commitWrite();
        } catch (Throwable t) {
        } finally {
            try { s.disconnect(); } catch (Throwable t) { }
        }
    }

    /** Size of the suite's JAR as of the last sync, -1 when unknown. */
    public static synchronized int jarSizeOf(int suiteId) {
        if (!cacheLoaded) loadCache();
        String jar = jarOf(suiteId);
        Integer n = jar == null ? null : (Integer)sizes.get(jar);
        return n == null ? -1 : n.intValue();
    }

    private static String[] split(String line) {
        String[] f = new String[4];
        int p = 0;
        for (int i = 0; i < 4; i++) {
            int e = i < 3 ? line.indexOf('\t', p) : line.length();
            if (e < 0) return null;
            f[i] = line.substring(p, e);
            p = e + 1;
        }
        return f;
    }

    private static void saveCache() {
        cacheDirty = false;
        StringBuffer b = new StringBuffer();
        for (java.util.Enumeration en = infos.keys(); en.hasMoreElements();) {
            Integer id = (Integer)en.nextElement();
            String[] r = (String[])infos.get(id);
            Integer size = (Integer)sizes.get(r[0]);
            if (size == null) continue;
            b.append(id).append('\t').append(r[0]).append('\t').append(r[1]).append('\t')
                    .append(size).append('\n');
        }
        try {
            writeStore("gamesync.cache", b.toString().getBytes("UTF-8"));
        } catch (Throwable t) {
        }
    }

    private static boolean same(Hashtable a, Hashtable b) {
        if (a.size() != b.size()) return false;
        for (java.util.Enumeration en = a.keys(); en.hasMoreElements();) {
            Object k = en.nextElement();
            if (!a.get(k).equals(b.get(k))) return false;
        }
        return true;
    }

    private static void install(SuiteDownloadInfo d, boolean force) throws Exception {
        if (force) {
            /* the suite that gets taken over points at this JAR or at one that is gone */
            File f = new File();
            Vector drop = new Vector();
            for (java.util.Enumeration en = infos.keys(); en.hasMoreElements();) {
                Object id = en.nextElement();
                String url = ((String[])infos.get(id))[0];
                if (url.equals(d.url) || !f.exists(url)) drop.addElement(id);
            }
            for (int i = 0; i < drop.size(); i++) infos.remove(drop.elementAt(i));
            cacheDirty = true;
        }
        new FileInstaller().installJar(d.url, d.label,
                Constants.INTERNAL_STORAGE_ID, force, false, new Silent());
    }

    /** Suites the launcher must drop and reload, cleared on read. */
    public static synchronized int[] takeTouched() {
        int[] r = new int[touched.size()];
        for (int i = 0; i < r.length; i++) r[i] = ((Integer)touched.elementAt(i)).intValue();
        touched.removeAllElements();
        return r;
    }

    public static boolean visible(int suiteId) {
        if (!pruned) {
            /* before the first pass, a JAR seen by the last one is trusted without a stat */
            String jar = jarOf(suiteId);
            return jar == null || sizes.containsKey(jar) || !missing(suiteId);
        }
        return !hidden.containsKey(new Integer(suiteId));
    }

    /** True when the last sync stopped early and another pass should follow. */
    public static synchronized boolean more() {
        return more;
    }

    /** Unpacks the JARs of each zip next to it and drops the zip. True when anything changed. */
    private static boolean unzip(Vector all, long end) {
        boolean done = false;
        for (int i = 0; i < all.size(); i++) {
            Jar z = (Jar)all.elementAt(i);
            if (!z.zip) continue;
            Integer bad = (Integer)badZips.get(z.info.url);
            if (bad != null && bad.intValue() == z.size) continue;
            if (done && System.currentTimeMillis() > end) {
                more = true;
                break;
            }
            String dir = z.info.url.substring(0, z.info.url.lastIndexOf('/') + 1);
            int got = 0;
            boolean ok = true;
            try {
                String names = SuiteDownloadInfo.zipEntries0(z.info.url);
                for (int p = 0; names != null && p < names.length();) {
                    int e = names.indexOf('\n', p);
                    if (e < 0) e = names.length();
                    String name = names.substring(p, e);
                    p = e + 1;
                    if (!name.toLowerCase().endsWith(".jar")) continue;
                    String dest = dir + name.substring(name.lastIndexOf('/') + 1);
                    if (!new File().exists(dest)) write(dest, JarReader.readJarEntry(z.info.url, name));
                    got++;
                }
            } catch (Throwable t) {
                System.out.println("GameSync: unzip failed " + z.info.url + " " + t);
                ok = false;
            }
            if (ok && got > 0) {
                try {
                    new File().delete(z.info.url);
                } catch (Throwable t) {
                }
                done = true;
            } else {
                badZips.put(z.info.url, new Integer(z.size));
                if (got > 0) done = true;
            }
        }
        return done;
    }

    private static void write(String dest, byte[] data) throws Exception {
        if (data == null) throw new Exception("empty entry");
        RandomAccessStream s = new RandomAccessStream();
        s.connect(dest, RandomAccessStream.READ_WRITE_TRUNCATE);
        try {
            s.writeBytes(data, 0, data.length);
            s.commitWrite();
        } finally {
            s.disconnect();
        }
    }

    private static Vector only(Vector all, boolean zips) {
        Vector r = new Vector();
        for (int i = 0; i < all.size(); i++) {
            if (((Jar)all.elementAt(i)).zip == zips) r.addElement(all.elementAt(i));
        }
        return r;
    }

    /** True when the suite came from the games folder and its JAR is gone. */
    public static boolean missing(int suiteId) {
        String jar = jarOf(suiteId);
        try {
            return jar != null && !new File().exists(jar);
        } catch (Throwable t) {
            return false;
        }
    }

    /**
     * JARs and zips under the games folder with their sizes, or null when it
     * can't be read. Folders come back from scan as Jar entries with size -2.
     */
    private static Vector list() {
        Vector all = new Vector();
        try {
            scan(ROOT, DEPTH, all);
        } catch (Throwable t) {
            return null;
        }
        Vector jars = new Vector(), found = new Vector();
        for (int i = 0; i < all.size(); i++) {
            Jar j = (Jar)all.elementAt(i);
            if (j.size == -2) found.addElement(j.info.url.substring(ROOT.length()) + "/");
            else jars.addElement(j);
        }
        String[] d = new String[found.size()];
        found.copyInto(d);
        dirs = d;
        return jars;
    }

    /** Folders under the games folder, relative with a trailing slash. */
    public static String[] folders() {
        return dirs;
    }

    public static boolean makeFolder(String rel) {
        return com.pspkvm.system.J2Vita.makeDir(ROOT + strip(rel));
    }

    public static boolean removeFolder(String rel) {
        return com.pspkvm.system.J2Vita.removeDir(ROOT + strip(rel));
    }

    public static boolean renameFolder(String from, String to) {
        return com.pspkvm.system.J2Vita.move(ROOT + strip(from), ROOT + strip(to));
    }

    /** Moves the suite's JAR into another folder; the next sync makes the suite follow it, saves included. */
    public static boolean moveGame(int suiteId, String toRel) {
        String jar = jarOf(suiteId);
        if (jar == null) return false;
        return com.pspkvm.system.J2Vita.move(jar, ROOT + toRel + jar.substring(jar.lastIndexOf('/') + 1));
    }

    private static String strip(String rel) {
        return rel.endsWith("/") ? rel.substring(0, rel.length() - 1) : rel;
    }

    private static void scan(String dir, int depth, Vector out) throws Exception {
        String all = SuiteDownloadInfo.listDir0(dir);
        if (all == null) throw new java.io.IOException(dir);
        for (int p = 0; p < all.length();) {
            int tab = all.indexOf('\t', p), e = all.indexOf('\n', tab);
            int size = Integer.parseInt(all.substring(p, tab));
            String name = all.substring(tab + 1, e);
            p = e + 1;
            SuiteDownloadInfo d = new SuiteDownloadInfo(dir + name, name, size == -2);
            d.http = false;
            if (size != -2) {
                out.addElement(new Jar(d, size, name.toLowerCase().endsWith(".zip")));
            } else if (depth > 1 && !name.startsWith(".")) {
                out.addElement(new Jar(d, -2, false));
                try {
                    scan(d.url + "/", depth - 1, out);
                } catch (Throwable t) {
                }
            }
        }
    }

    private static final class Jar {
        final SuiteDownloadInfo info;
        final int size;
        final boolean zip;

        Jar(SuiteDownloadInfo info, int size, boolean zip) {
            this.info = info;
            this.size = size;
            this.zip = zip;
        }
    }

    /** Folder of the suite's JAR relative to the games folder ("" for the top), or null. */
    public static String folderOf(int suiteId) {
        String url = jarOf(suiteId);
        return url == null ? null : url.substring(ROOT.length(), url.lastIndexOf('/') + 1);
    }

    /** JAR in the games folder the suite came from, or null. */
    public static String jarOf(int suiteId) {
        String url = jarUrl(MIDletSuiteStorage.getMIDletSuiteStorage(), suiteId);
        return url != null && url.startsWith(ROOT) ? url : null;
    }

    private static String jarUrl(MIDletSuiteStorage store, int id) {
        return suiteInfo(store, id)[0];
    }

    private static final class Silent implements InstallListener {
        public boolean warnUser(InstallState state) { return true; }
        public boolean confirmJarDownload(InstallState state) { return true; }
        public void updateStatus(int status, InstallState state) { }
        public boolean keepRMS(InstallState state) { return true; }
        public boolean confirmAuthPath(InstallState state) { return true; }
        public void updateDeviceType(InstallState state) { }
    }
}
