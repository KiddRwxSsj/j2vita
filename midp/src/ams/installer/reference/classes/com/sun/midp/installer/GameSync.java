package com.sun.midp.installer;

import java.io.ByteArrayInputStream;
import java.util.Hashtable;
import java.util.Vector;

import com.sun.midp.jarutil.JarReader;
import com.sun.midp.midlet.MIDletSuite;
import com.sun.midp.midletsuite.InstallInfo;
import com.sun.midp.midletsuite.MIDletSuiteImpl;
import com.sun.midp.midletsuite.MIDletSuiteStorage;
import com.sun.midp.configurator.Constants;

/**
 * Keeps the installed suites in step with the JARs in the games folder.
 * New JARs are installed silently, a JAR overwritten with another copy
 * updates its suite. Suites whose JAR was removed are hidden but kept, so
 * their saves come back with the JAR, unless another JAR now holds a game
 * with the same name.
 */
public final class GameSync {
    public static final String ROOT = "ux0:data/j2vita/games/";

    private static final Hashtable failed = new Hashtable();
    private static Hashtable hidden = new Hashtable();
    /* suites removed or rewritten since the launcher last asked */
    private static final Vector touched = new Vector();

    private GameSync() {
    }

    /** Returns true when the suite list changed. */
    public static synchronized boolean sync() {
        Hashtable present = new Hashtable();
        Vector jars = new Vector();
        try {
            Vector list = SuiteDownloadInfo.getDownloadInfoFromDir(ROOT);
            for (int i = 0; i < list.size(); i++) {
                SuiteDownloadInfo d = (SuiteDownloadInfo)list.elementAt(i);
                if (!d.dir && d.url.toLowerCase().endsWith(".jar")) {
                    present.put(d.url, d.label);
                    jars.addElement(d);
                }
            }
        } catch (Throwable t) {
            return false;
        }

        MIDletSuiteStorage store = MIDletSuiteStorage.getMIDletSuiteStorage();
        Hashtable installed = new Hashtable();
        Hashtable stale = new Hashtable();
        boolean hiddenGone = false;
        int[] ids = store.getListOfSuites();
        for (int i = 0; ids != null && i < ids.length; i++) {
            String url = jarUrl(store, ids[i]);
            if (url == null) continue;
            installed.put(url, url);
            if (!url.startsWith(ROOT)) continue;
            if (!present.containsKey(url)) {
                hiddenGone = true;
            } else if (!sameGame(store, ids[i], url)) {
                /* the file was overwritten with another copy */
                stale.put(url, new Integer(ids[i]));
            }
        }

        boolean changed = false;
        for (int i = 0; i < jars.size(); i++) {
            SuiteDownloadInfo d = (SuiteDownloadInfo)jars.elementAt(i);
            boolean update = stale.containsKey(d.url);
            if ((installed.containsKey(d.url) && !update) || failed.containsKey(d.url)) continue;
            System.out.println("GameSync: " + (update ? "updating " : "installing ") + d.url);
            try {
                try {
                    install(d, update);
                    if (update) touched.addElement(stale.get(d.url));
                } catch (InvalidJadException e) {
                    int r = e.getReason();
                    /* a JAR swapped for another copy of the same game updates it */
                    if (!hiddenGone || (r != InvalidJadException.ALREADY_INSTALLED
                            && r != InvalidJadException.OLD_VERSION
                            && r != InvalidJadException.NEW_VERSION)) throw e;
                    install(d, true);
                }
                changed = true;
            } catch (Throwable t) {
                System.out.println("GameSync: failed " + d.url + " " + t);
                failed.put(d.url, d.url);
            }
        }

        if (prune(store, present)) changed = true;
        return changed;
    }

    /**
     * Drops suites that lost to a newer install: two suites on one JAR keep
     * the newest, and a suite whose JAR is gone goes when a live suite has
     * the same name. Anything else without a JAR stays hidden for its saves.
     */
    private static boolean prune(MIDletSuiteStorage store, Hashtable present) {
        int[] ids = store.getListOfSuites();
        if (ids == null) return false;
        String[] urls = new String[ids.length];
        String[] names = new String[ids.length];
        Hashtable liveNames = new Hashtable();
        Hashtable newest = new Hashtable();
        for (int i = 0; i < ids.length; i++) {
            urls[i] = jarUrl(store, ids[i]);
            names[i] = property(store, ids[i], "MIDlet-Name");
            if (urls[i] == null || !urls[i].startsWith(ROOT)) continue;
            if (present.containsKey(urls[i])) {
                if (names[i] != null) liveNames.put(names[i], names[i]);
                Integer prev = (Integer)newest.get(urls[i]);
                if (prev == null || prev.intValue() < ids[i]) newest.put(urls[i], new Integer(ids[i]));
            }
        }

        Hashtable hide = new Hashtable();
        boolean removed = false;
        for (int i = 0; i < ids.length; i++) {
            String url = urls[i];
            if (url == null || !url.startsWith(ROOT)) continue;
            boolean drop;
            if (present.containsKey(url)) {
                drop = ((Integer)newest.get(url)).intValue() != ids[i];
            } else {
                drop = names[i] != null && liveNames.containsKey(names[i]);
                if (!drop) hide.put(new Integer(ids[i]), url);
            }
            if (!drop) continue;
            try {
                System.out.println("GameSync: removing old suite " + ids[i] + " " + url);
                store.remove(ids[i]);
                touched.addElement(new Integer(ids[i]));
                removed = true;
            } catch (Throwable t) {
                hide.put(new Integer(ids[i]), url);
            }
        }
        hidden = hide;
        return removed;
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

    private static String property(MIDletSuiteStorage store, int id, String key) {
        MIDletSuiteImpl suite = null;
        try {
            suite = store.getMIDletSuite(id, false);
            return suite == null ? null : suite.getProperty(key);
        } catch (Throwable t) {
            return null;
        } finally {
            if (suite != null) suite.close();
        }
    }

    private static void install(SuiteDownloadInfo d, boolean force) throws Exception {
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
        return !hidden.containsKey(new Integer(suiteId));
    }

    /** JAR in the games folder the suite came from, or null. */
    public static String jarOf(int suiteId) {
        String url = jarUrl(MIDletSuiteStorage.getMIDletSuiteStorage(), suiteId);
        return url != null && url.startsWith(ROOT) ? url : null;
    }

    private static String jarUrl(MIDletSuiteStorage store, int id) {
        MIDletSuiteImpl suite = null;
        try {
            suite = store.getMIDletSuite(id, false);
            if (suite == null) return null;
            InstallInfo ii = suite.getInstallInfo();
            return ii == null ? null : ii.getJarUrl();
        } catch (Throwable t) {
            return null;
        } finally {
            if (suite != null) suite.close();
        }
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
