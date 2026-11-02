package com.sun.midp.appmanager;

import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Vector;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

import com.pspkvm.system.J2Vita;
import com.sun.midp.installer.GameSync;
import com.sun.midp.installer.J2Style;
import com.sun.midp.midletsuite.MIDletSuiteInfo;
import com.sun.midp.midletsuite.MIDletSuiteStorage;

/**
 * J2Vita home screen, drawn at 480x272: a grid of icon tiles four rows
 * high that scrolls sideways, folders first, the selected name in a tab
 * at the bottom. Subfolders of the games folder show up as folder tiles.
 * Cross opens, Circle goes up a folder, Triangle (or L / Start) opens
 * Options. The frontend sends Fire for Cross, '#' for Triangle, Soft1 for
 * L/Start and Soft2 for Circle/R.
 */
final class LauncherScreen extends Canvas {
    private static final int TILE = 40, GAP = 9, STEP = TILE + GAP, ROWS = 4, ICON = TILE - 8;
    private static final int NAME_H = 24;

    private static final int GAME = 0, FOLDER = 1, BACK = 2;

    private static final int LIST = 0, OPTIONS = 1, CONFIRM = 2, MOVE = 3;
    private static final int A_OPEN = 0, A_MOVE = 1, A_DELETE = 2, A_NEWDIR = 3, A_RENAME = 4,
            A_RMDIR = 5, A_SETTINGS = 6, A_QUIT = 7;
    private static final String[] A_LABELS = {"Open", "Move to folder", "Delete game", "New folder",
            "Rename folder", "Delete folder", "Settings", "Quit J2Vita"};
    private static final int OPT_W = 200, OPT_ROW = 24, MOVE_W = 260, MOVE_ROWS = 6;

    private static final class Entry {
        final int kind;
        final String name, path;
        final AMSMidletCustomItem item;
        int count;

        Entry(int kind, String name, String path, AMSMidletCustomItem item) {
            this.kind = kind;
            this.name = name;
            this.path = path;
            this.item = item;
        }

        String key() {
            return kind == GAME ? "#" + item.msi.suiteId : kind == FOLDER ? "/" + path : "..";
        }
    }

    private final AppManagerUI owner;
    private final Vector entries = new Vector();
    private final Hashtable icons = new Hashtable();
    private static final Object NO_ICON = new Object();
    private String dir = "";
    private int selected, firstCol, mode = LIST, opt;
    private int[] opts = new int[0];
    private String[] dests = new String[0];
    private int moveSel, moveTop;
    private int pressX, pressY;
    private Timer watch;
    private int ticks;
    /* cursor (grid coordinates) and scroll as shown, in 1/16 px, eased towards the selection */
    private int curX, curY, scroll;
    private long lastFrame;
    private boolean dragging;
    private int pressScroll, dragScroll;
    /* icons decoded per frame, the rest wait for the next frames so fast scrolling stays smooth */
    private int iconBudget;
    private boolean iconsPending;
    /* every game of every folder as of the last rebuild, walked by the icon preloader */
    private volatile Vector allItems = new Vector();
    private Thread preloader;
    private boolean preloadAgain, cacheRead;
    /* tile sized icons from the last boot, kept with the size of the JAR they came from */
    private static final String ICON_CACHE = "launcher.icons";
    private volatile long busyUntil;
    private boolean wasSyncing;

    LauncherScreen(AppManagerUI owner) {
        this.owner = owner;
        setFullScreenMode(true);
        refresh();
    }

    void refresh() {
        String keep = current() == null ? null : current().key();
        int was = selected;
        rebuild();
        selected = Math.max(0, Math.min(was, entries.size() - 1));
        select(keep);

        Hashtable live = new Hashtable();
        Vector all = allItems;
        for (int i = 0; i < all.size(); i++) {
            Integer id = new Integer(((AMSMidletCustomItem)all.elementAt(i)).msi.suiteId);
            Object ic = icons.get(id);
            /* a failed icon is retried, its JAR may have just been moved or replaced */
            if (ic != null && ic != NO_ICON) live.put(id, ic);
        }
        icons.clear();
        for (Enumeration en = live.keys(); en.hasMoreElements();) {
            Object id = en.nextElement();
            icons.put(id, live.get(id));
        }
        preload();
        if (mode == CONFIRM || mode == MOVE) mode = LIST;
        scrollToSelected();
        repaint();
    }

    void selectSuite(int suiteId) {
        String f = folderOf(suiteId);
        if (!f.equals(dir)) {
            dir = f;
            rebuild();
        }
        select("#" + suiteId);
        scrollToSelected();
        snap();
        repaint();
    }

    private void select(String key) {
        if (key == null) return;
        for (int i = 0; i < entries.size(); i++) {
            if (entry(i).key().equals(key)) {
                selected = i;
                return;
            }
        }
    }

    /** entries of the current folder: Back, subfolders by name, then games */
    private void rebuild() {
        Vector all = new Vector();
        collect(owner.rootFolder, all);
        allItems = all;
        for (;;) {
            entries.removeAllElements();
            Vector subs = new Vector(), games = new Vector();
            Hashtable seen = new Hashtable();
            for (int i = 0; i < all.size(); i++) {
                AMSMidletCustomItem it = (AMSMidletCustomItem)all.elementAt(i);
                String f = folderOf(it.msi.suiteId);
                if (f.equals(dir)) {
                    games.addElement(new Entry(GAME, it.msi.displayName, null, it));
                } else if (f.startsWith(dir)) {
                    String sub = f.substring(0, f.indexOf('/', dir.length()) + 1);
                    Entry e = (Entry)seen.get(sub);
                    if (e == null) {
                        e = new Entry(FOLDER, sub.substring(dir.length(), sub.length() - 1), sub, null);
                        seen.put(sub, e);
                        insertSorted(subs, e);
                    }
                    e.count++;
                }
            }
            String[] known = GameSync.folders();
            for (int i = 0; i < known.length; i++) {
                String f = known[i];
                if (!f.startsWith(dir) || f.length() == dir.length() || f.indexOf('/', dir.length()) != f.length() - 1
                        || seen.containsKey(f)) continue;
                Entry e = new Entry(FOLDER, f.substring(dir.length(), f.length() - 1), f, null);
                seen.put(f, e);
                insertSorted(subs, e);
            }
            if (dir.length() > 0 && subs.size() + games.size() == 0 && !exists(dir, known)) {
                dir = parent(dir);
                continue;
            }
            if (dir.length() > 0) entries.addElement(new Entry(BACK, "Back", parent(dir), null));
            for (int i = 0; i < subs.size(); i++) entries.addElement(subs.elementAt(i));
            for (int i = 0; i < games.size(); i++) entries.addElement(games.elementAt(i));
            return;
        }
    }

    private static boolean exists(String d, String[] known) {
        for (int i = 0; i < known.length; i++) if (known[i].equals(d)) return true;
        return false;
    }

    private static void insertSorted(Vector v, Entry e) {
        String n = e.name.toLowerCase();
        int i = 0;
        while (i < v.size() && ((Entry)v.elementAt(i)).name.toLowerCase().compareTo(n) <= 0) i++;
        v.insertElementAt(e, i);
    }

    private static String parent(String d) {
        int i = d.lastIndexOf('/', d.length() - 2);
        return i < 0 ? "" : d.substring(0, i + 1);
    }

    private static String folderOf(int suiteId) {
        String f = GameSync.folderOf(suiteId);
        return f == null ? "" : f;
    }

    private void collect(AMSFolderCustomItem folder, Vector out) {
        if (folder == null) return;
        if (folder.items != null) {
            for (int i = 0; i < folder.items.length; i++) {
                AMSMidletCustomItem it = folder.items[i];
                if (it != null && it.msi != null && it.msi.suiteId >= 0
                        && GameSync.visible(it.msi.suiteId)) out.addElement(it);
            }
        }
        if (folder.subfolders != null) {
            for (int i = 0; i < folder.subfolders.length; i++) collect(folder.subfolders[i], out);
        }
    }

    private Entry entry(int i) {
        return (Entry)entries.elementAt(i);
    }

    private Entry current() {
        return selected >= 0 && selected < entries.size() ? entry(selected) : null;
    }

    private AMSMidletCustomItem currentGame() {
        Entry e = current();
        return e != null && e.kind == GAME ? e.item : null;
    }

    private int cols() {
        return (getWidth() - 28 + GAP) / STEP;
    }

    private int gridLeft() {
        return (getWidth() - (cols() * STEP - GAP)) / 2;
    }

    private int gridTop() {
        int area = getHeight() - J2Style.TOP - J2Style.SOFT_H - NAME_H;
        return J2Style.TOP + (area - (ROWS * STEP - GAP)) / 2;
    }

    private int totalCols() {
        return (entries.size() + ROWS - 1) / ROWS;
    }

    private void clampScroll() {
        int max = Math.max(0, totalCols() - cols());
        if (firstCol > max) firstCol = max;
        if (firstCol < 0) firstCol = 0;
    }

    /** shows the selection at once, used when the grid content is replaced */
    private void snap() {
        curX = selected / ROWS * STEP * 16;
        curY = selected % ROWS * STEP * 16;
        scroll = firstCol * STEP * 16;
        lastFrame = 0;
    }

    private static int ease(int cur, int target, int dt) {
        int d = target - cur;
        if (d == 0) return cur;
        int s = d * dt / 48;
        if (s == 0) s = d > 0 ? Math.min(d, 8) : Math.max(d, -8);
        return cur + s;
    }

    /** moves the shown cursor and scroll by the time since the last frame, true while still moving */
    private boolean step() {
        long now = System.currentTimeMillis();
        int dt = lastFrame == 0 ? 16 : (int)Math.min(40, Math.max(1, now - lastFrame));
        int tx = selected / ROWS * STEP * 16, ty = selected % ROWS * STEP * 16;
        int ts = dragging ? dragScroll : firstCol * STEP * 16;
        curX = ease(curX, tx, dt);
        curY = ease(curY, ty, dt);
        scroll = dragging ? ts : ease(scroll, ts, dt);
        boolean moving = curX != tx || curY != ty || scroll != ts;
        lastFrame = moving ? now : 0;
        return moving;
    }

    private void scrollToSelected() {
        int col = selected / ROWS, n = cols();
        if (col < firstCol) firstCol = col;
        if (col >= firstCol + n) firstCol = col - n + 1;
        clampScroll();
    }

    private Image icon(AMSMidletCustomItem it) {
        Integer key = new Integer(it.msi.suiteId);
        Object cached = icons.get(key);
        if (cached == null) {
            if (iconBudget <= 0) {
                iconsPending = true;
                return null;
            }
            iconBudget--;
            icons.put(key, cached = decode(it));
        }
        return cached instanceof Image ? (Image)cached : null;
    }

    /** the suite icon scaled with nearest neighbour to fit the tile, whole steps when growing */
    private Object decode(AMSMidletCustomItem it) {
        Image img = it.icon;
        try {
            if (img == null) {
                MIDletSuiteStorage store = MIDletSuiteStorage.getMIDletSuiteStorage();
                if (it.msi.iconName == null) {
                    MIDletSuiteInfo info = store.getMIDletSuiteInfo(it.msi.suiteId);
                    it.msi.iconName = info.iconName;
                }
                it.msi.loadIcon(store);
                img = it.msi.icon;
                /* only the tile sized copy is kept */
                it.msi.icon = null;
            }
            if (img != null) {
                int w = img.getWidth(), h = img.getHeight(), nw, nh;
                if (w <= ICON && h <= ICON) {
                    int k = Math.min(ICON / w, ICON / h);
                    nw = w * k;
                    nh = h * k;
                } else {
                    int m = Math.max(w, h);
                    nw = Math.max(1, w * ICON / m);
                    nh = Math.max(1, h * ICON / m);
                }
                if (nw != w || nh != h) {
                    int[] src = new int[w * h], dst = new int[nw * nh];
                    img.getRGB(src, 0, w, 0, 0, w, h);
                    for (int y = 0; y < nh; y++) {
                        int sy = y * h / nh * w;
                        for (int x = 0; x < nw; x++) dst[y * nw + x] = src[sy + x * w / nw];
                    }
                    img = Image.createRGBImage(dst, nw, nh, true);
                }
            }
        } catch (Throwable t) {
            img = null;
        }
        return img != null ? (Object)img : NO_ICON;
    }

    /* records of id, JAR size, width, height and ARGB pixels, all big endian ints */
    private void readIconCache() {
        byte[] b = GameSync.readStore(ICON_CACHE);
        if (b == null) return;
        try {
            int[] all = new int[b.length / 4];
            J2Vita.unpackInts(b, 0, all, all.length);
            b = null;
            for (int p = 0; p + 4 <= all.length;) {
                int id = all[p], size = all[p + 1], w = all[p + 2], h = all[p + 3];
                p += 4;
                if (w <= 0 || h <= 0 || w > ICON || h > ICON || p + w * h > all.length) return;
                Integer key = new Integer(id);
                if (size >= 0 && size == GameSync.jarSizeOf(id) && icons.get(key) == null) {
                    int[] px = new int[w * h];
                    System.arraycopy(all, p, px, 0, px.length);
                    icons.put(key, Image.createRGBImage(px, w, h, true));
                }
                p += w * h;
            }
        } catch (Throwable t) {
        }
    }

    private static int put(byte[] b, int p, int v) {
        b[p] = (byte)(v >> 24);
        b[p + 1] = (byte)(v >> 16);
        b[p + 2] = (byte)(v >> 8);
        b[p + 3] = (byte)v;
        return p + 4;
    }

    private void writeIconCache(Vector all) {
        try {
            Vector keep = new Vector();
            int len = 0;
            for (int i = 0; i < all.size(); i++) {
                Object ic = icons.get(new Integer(((AMSMidletCustomItem)all.elementAt(i)).msi.suiteId));
                if (!(ic instanceof Image)) continue;
                keep.addElement(all.elementAt(i));
                len += 16 + ((Image)ic).getWidth() * ((Image)ic).getHeight() * 4;
            }
            byte[] b = new byte[len];
            int p = 0;
            for (int i = 0; i < keep.size(); i++) {
                int id = ((AMSMidletCustomItem)keep.elementAt(i)).msi.suiteId;
                Image img = (Image)icons.get(new Integer(id));
                int w = img.getWidth(), h = img.getHeight();
                int[] px = new int[w * h];
                img.getRGB(px, 0, w, 0, 0, w, h);
                p = put(b, p, id);
                p = put(b, p, GameSync.jarSizeOf(id));
                p = put(b, p, w);
                p = put(b, p, h);
                for (int k = 0; k < px.length; k++) p = put(b, p, px[k]);
            }
            GameSync.writeStore(ICON_CACHE, b);
        } catch (Throwable t) {
        }
    }

    /** Decodes every game's icon in the background, so scrolling never waits on a JAR. */
    private void preload() {
        synchronized (this) {
            preloadAgain = true;
            if (preloader != null) return;
            preloader = new Thread(new Runnable() {
                public void run() {
                    preloadLoop();
                }
            });
        }
        preloader.setPriority(Thread.MIN_PRIORITY);
        preloader.start();
    }

    private void preloadLoop() {
        try {
            for (;;) {
                synchronized (this) {
                    if (!preloadAgain || !isShown()) {
                        preloader = null;
                        return;
                    }
                    preloadAgain = false;
                }
                loadIconCache();
                while (owner.vitaSyncing() && isShown()) Thread.sleep(100);
                Vector all = allItems;
                int done = 0;
                for (int i = 0; i < all.size(); i++) {
                    AMSMidletCustomItem it = (AMSMidletCustomItem)all.elementAt(i);
                    Integer key = new Integer(it.msi.suiteId);
                    if (icons.get(key) != null) continue;
                    /* scrolling and syncing come first, a game in front stops it until the launcher is back */
                    while (owner.vitaSyncing() || System.currentTimeMillis() < busyUntil) {
                        if (!isShown()) break;
                        Thread.sleep(100);
                    }
                    if (!isShown()) {
                        preloadAgain = true;
                        break;
                    }
                    icons.put(key, decode(it));
                    if (++done % 8 == 0) repaint();
                    Thread.yield();
                }
                if (done > 0) {
                    repaint();
                    writeIconCache(all);
                }
            }
        } catch (Throwable t) {
            synchronized (this) {
                preloader = null;
            }
        }
    }

    /* the icons of the last boot, before the first sync so they come with the first frame */
    private synchronized void loadIconCache() {
        if (cacheRead) return;
        cacheRead = true;
        readIconCache();
    }

    protected void showNotify() {
        refresh();
        loadIconCache();
        snap();
        owner.vitaSync();
        /* the native settings menu can switch the theme, and JARs can come and go over FTP or USB */
        if (watch == null) {
            ticks = 0;
            watch = new Timer();
            watch.schedule(new TimerTask() {
                public void run() {
                    boolean s = owner.vitaSyncing();
                    if (J2Style.stale() || s != wasSyncing) repaint();
                    wasSyncing = s;
                    if (++ticks % 4 == 0 && GameSync.changed()) owner.vitaSync();
                }
            }, 500, 500);
        }
    }

    protected void hideNotify() {
        if (watch != null) {
            watch.cancel();
            watch = null;
        }
    }

    protected void paint(Graphics g) {
        int w = getWidth(), h = getHeight();
        boolean moving = step();
        if (moving || dragging) busyUntil = System.currentTimeMillis() + 300;
        iconBudget = 2;
        iconsPending = false;
        String title = dir.length() == 0 ? "J2Vita" : dir.substring(0, dir.length() - 1);
        if (owner.vitaSyncing()) title = title + "  (updating)";
        J2Style.frame(g, w, h, title);

        Entry cur = current();
        if (entries.size() == 0) {
            paintEmpty(g, w, h);
        } else {
            paintGrid(g, w, h);
            String name = cur.kind == FOLDER ? cur.name + " (" + cur.count + ")" : cur.name;
            if (cur.kind == GAME && !cur.item.msi.enabled) name = name + " (disabled)";
            J2Style.strip(g, 24, h - J2Style.SOFT_H - NAME_H, w - 48, NAME_H, name);
        }

        if (mode == OPTIONS) {
            paintOptions(g, w, h);
            J2Style.softkeys(g, w, h, J2Style.CROSS, "Select", J2Style.CIRCLE, "Back");
        } else if (mode == MOVE) {
            paintMove(g, w, h);
            J2Style.softkeys(g, w, h, J2Style.CROSS, "Move", J2Style.CIRCLE, "Cancel");
        } else if (mode == CONFIRM) {
            paintConfirm(g, w, h);
            J2Style.softkeys(g, w, h, J2Style.CROSS, "Delete", J2Style.CIRCLE, "Cancel");
        } else if (cur == null) {
            J2Style.softkeys(g, w, h, J2Style.TRIANGLE, "Options", J2Style.NO_GLYPH, null);
        } else {
            J2Style.softkeys(g, w, h, J2Style.TRIANGLE, "Options", J2Style.CROSS,
                    cur.kind == GAME ? "Play" : cur.kind == FOLDER ? "Open" : "Back");
        }
        if (moving || iconsPending) repaint();
    }

    private void paintEmpty(Graphics g, int w, int h) {
        int y = J2Style.TOP + 40;
        g.setColor(J2Style.TEXT);
        g.drawString("No games yet", w / 2, y, Graphics.HCENTER | Graphics.TOP);
        J2Style.wrap(g, "Copy .jar or .zip files to ux0:data/j2vita/games/ and they show up here.",
                w / 2, y + 34, w - 80, 4, J2Style.DIM, Graphics.HCENTER);
    }

    private void paintGrid(Graphics g, int w, int h) {
        int n = cols(), left = gridLeft(), top = gridTop(), sc = (scroll + 8) >> 4;
        /* tiles slide in and out between the arrows */
        int cx = 20, cw = w - 40, cy0 = top - 6, ch = ROWS * STEP + 12;
        g.setClip(cx, cy0, cw, ch);
        for (int c = Math.max(0, sc / STEP - 1); c <= sc / STEP + n; c++) {
            int x = left + c * STEP - sc;
            if (x + TILE + 4 < cx || x - 4 > cx + cw) continue;
            for (int r = 0; r < ROWS; r++) {
                int i = c * ROWS + r;
                if (i >= entries.size()) break;
                int y = top + r * STEP;
                J2Style.tile(g, x, y, TILE, false);
                paintTile(g, entry(i), x, y);
                g.setClip(cx, cy0, cw, ch);
            }
        }
        J2Style.cursor(g, left + ((curX + 8) >> 4) - sc, top + ((curY + 8) >> 4), TILE);
        g.setClip(0, 0, w, h);
        g.setColor(J2Style.DIM);
        int cy = top + (ROWS * STEP - GAP) / 2;
        if (firstCol > 0) g.fillTriangle(10, cy, 16, cy - 6, 16, cy + 6);
        if (firstCol + n < totalCols()) g.fillTriangle(w - 10, cy, w - 16, cy - 6, w - 16, cy + 6);
    }

    private void paintTile(Graphics g, Entry e, int x, int y) {
        int cx = x + TILE / 2, cy = y + TILE / 2;
        if (e.kind == FOLDER) {
            g.setColor(J2Style.ACCENT);
            g.fillRoundRect(x + 7, y + 10, 12, 6, 4, 4);
            g.fillRoundRect(x + 7, y + 13, TILE - 14, TILE - 23, 5, 5);
            g.setColor(J2Style.mix(J2Style.ACCENT, 0xFFFFFF, 1, 3));
            g.drawLine(x + 9, y + 16, x + TILE - 10, y + 16);
            return;
        }
        if (e.kind == BACK) {
            g.setColor(J2Style.TEXT);
            g.fillTriangle(cx - 11, cy, cx - 2, cy - 9, cx - 2, cy + 9);
            g.fillRect(cx - 3, cy - 3, 13, 7);
            return;
        }
        g.clipRect(x + 2, y + 2, TILE - 4, TILE - 4);
        Image ic = icon(e.item);
        if (ic == null && icons.get(new Integer(e.item.msi.suiteId)) == null) return;
        if (ic != null) {
            g.drawImage(ic, cx, cy, Graphics.HCENTER | Graphics.VCENTER);
        } else {
            String name = e.item.msi.displayName;
            g.setColor(J2Style.TEXT);
            g.drawString(name != null && name.length() > 0 ? name.substring(0, 1).toUpperCase() : "?",
                    cx, J2Style.textY(y, TILE), Graphics.HCENTER | Graphics.TOP);
        }
    }

    /** Options for what is selected: games, folders and the folder list itself */
    private void buildOptions() {
        Entry e = current();
        int[] o;
        if (e != null && e.kind == GAME) o = new int[] {A_OPEN, A_MOVE, A_DELETE, A_NEWDIR, A_SETTINGS, A_QUIT};
        else if (e != null && e.kind == FOLDER) o = new int[] {A_OPEN, A_RENAME, A_RMDIR, A_NEWDIR, A_SETTINGS, A_QUIT};
        else if (e != null) o = new int[] {A_OPEN, A_NEWDIR, A_SETTINGS, A_QUIT};
        else o = new int[] {A_NEWDIR, A_SETTINGS, A_QUIT};
        opts = o;
    }

    private boolean optEnabled(int i) {
        if (i < 0 || i >= opts.length) return false;
        Entry e = current();
        /* only empty folders can go, games inside are moved out first */
        if (opts[i] == A_RMDIR) return e != null && e.kind == FOLDER && e.count == 0;
        return true;
    }

    private String optLabel(int i) {
        Entry e = current();
        if (opts[i] == A_OPEN && e != null) return e.kind == GAME ? "Play" : e.kind == BACK ? "Back" : "Open";
        return A_LABELS[opts[i]];
    }

    private int optTop(int h) {
        return h - J2Style.SOFT_H - 6 - (J2Style.TITLE_H + opts.length * OPT_ROW + 8);
    }

    private void paintOptions(Graphics g, int w, int h) {
        int x = 8, y = optTop(h);
        int ph = J2Style.TITLE_H + opts.length * OPT_ROW + 8;
        J2Style.dialog(g, x, y, OPT_W, ph, "Options");
        for (int i = 0; i < opts.length; i++) {
            int ry = y + J2Style.TITLE_H + 4 + i * OPT_ROW;
            if (i == opt) J2Style.selection(g, x + 5, ry, OPT_W - 10, OPT_ROW);
            g.setColor(!optEnabled(i) ? J2Style.DISABLED : i == opt ? J2Style.SEL_TEXT : J2Style.TEXT);
            g.drawString(J2Style.fit(optLabel(i), OPT_W - 28), x + 14, J2Style.textY(ry, OPT_ROW),
                    Graphics.LEFT | Graphics.TOP);
        }
    }

    private int moveTopY(int h) {
        int ph = J2Style.TITLE_H + Math.min(MOVE_ROWS, dests.length) * OPT_ROW + 8;
        return J2Style.TOP + (h - J2Style.TOP - J2Style.SOFT_H - ph) / 2;
    }

    private void paintMove(Graphics g, int w, int h) {
        int n = Math.min(MOVE_ROWS, dests.length), x = (w - MOVE_W) / 2, y = moveTopY(h);
        J2Style.dialog(g, x, y, MOVE_W, J2Style.TITLE_H + n * OPT_ROW + 8, "Move to");
        if (moveSel < moveTop) moveTop = moveSel;
        if (moveSel >= moveTop + n) moveTop = moveSel - n + 1;
        for (int r = 0; r < n; r++) {
            int i = moveTop + r, ry = y + J2Style.TITLE_H + 4 + r * OPT_ROW;
            if (i == moveSel) J2Style.selection(g, x + 5, ry, MOVE_W - 10, OPT_ROW);
            String d = dests[i];
            String label = d.length() == 0 ? "J2Vita" : d.substring(0, d.length() - 1);
            g.setColor(i == moveSel ? J2Style.SEL_TEXT : J2Style.TEXT);
            g.drawString(J2Style.fit(label, MOVE_W - 28), x + 14, J2Style.textY(ry, OPT_ROW),
                    Graphics.LEFT | Graphics.TOP);
        }
    }

    private void openMove() {
        AMSMidletCustomItem it = currentGame();
        if (it == null) return;
        String from = folderOf(it.msi.suiteId);
        String[] known = GameSync.folders();
        Vector v = new Vector();
        if (from.length() > 0) v.addElement("");
        for (int i = 0; i < known.length; i++) {
            if (known[i].equals(from)) continue;
            int k = 0;
            while (k < v.size() && ((String)v.elementAt(k)).toLowerCase().compareTo(known[i].toLowerCase()) <= 0) k++;
            v.insertElementAt(known[i], k);
        }
        if (v.size() == 0) return;
        dests = new String[v.size()];
        v.copyInto(dests);
        moveSel = moveTop = 0;
        mode = MOVE;
        repaint();
    }

    private void doMove() {
        AMSMidletCustomItem it = currentGame();
        mode = LIST;
        if (it != null && moveSel < dests.length && GameSync.moveGame(it.msi.suiteId, dests[moveSel])) {
            owner.vitaSync();
        }
        repaint();
    }

    /* Vita keyboard for a folder name, the action runs on its own thread once it is typed */
    private void askName(final String title, final String initial, final String renameFrom) {
        J2Vita.askText(title, initial);
        new Thread(new Runnable() {
            public void run() {
                int st;
                while ((st = J2Vita.textState()) == J2Vita.TEXT_BUSY) {
                    try { Thread.sleep(100); } catch (InterruptedException e) { }
                }
                if (st != J2Vita.TEXT_OK) return;
                String name = cleanName(J2Vita.textResult());
                if (name.length() == 0) return;
                boolean ok = renameFrom == null ? GameSync.makeFolder(dir + name)
                        : GameSync.renameFolder(renameFrom, dir + name);
                if (ok) owner.vitaSync();
            }
        }).start();
    }

    private static String cleanName(String s) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; s != null && i < s.length(); i++) {
            char c = s.charAt(i);
            if (c >= ' ' && "/\\:*?\"<>|".indexOf(c) < 0) b.append(c);
        }
        String r = b.toString().trim();
        while (r.startsWith(".")) r = r.substring(1);
        return r;
    }

    private void paintConfirm(Graphics g, int w, int h) {
        int dw = 320, dh = 120, x = (w - dw) / 2, y = J2Style.TOP + (h - J2Style.TOP - J2Style.SOFT_H - dh) / 2;
        AMSMidletCustomItem it = currentGame();
        J2Style.dialog(g, x, y, dw, dh, "Delete game");
        g.setColor(J2Style.TEXT);
        g.drawString(J2Style.fit(it == null ? "" : it.msi.displayName, dw - 28), w / 2, y + J2Style.TITLE_H + 14,
                Graphics.HCENTER | Graphics.TOP);
        J2Style.wrap(g, "The game and its saved data will be removed.", w / 2, y + J2Style.TITLE_H + 40,
                dw - 28, 2, J2Style.DIM, Graphics.HCENTER);
    }

    private void moveTo(int i) {
        if (i < 0 || i >= entries.size() || i == selected) return;
        selected = i;
        scrollToSelected();
        repaint();
    }

    private void move(int action) {
        int n = entries.size(), row = selected % ROWS;
        if (n == 0) return;
        if (action == UP && row > 0) moveTo(selected - 1);
        else if (action == DOWN && row < ROWS - 1) moveTo(selected + 1);
        else if (action == LEFT) moveTo(selected - ROWS);
        else if (action == RIGHT) {
            /* the last column can be short, land on its last tile */
            if (selected + ROWS < n) moveTo(selected + ROWS);
            else if (selected / ROWS < (n - 1) / ROWS) moveTo(n - 1);
        }
    }

    private void activate(Entry e) {
        if (e == null) return;
        if (e.kind == GAME) {
            owner.vitaLaunch(e.item);
        } else if (e.kind == FOLDER) {
            dir = e.path;
            rebuild();
            selected = 0;
            firstCol = 0;
            scrollToSelected();
            snap();
            repaint();
        } else {
            goUp();
        }
    }

    private void goUp() {
        if (dir.length() == 0) return;
        String from = dir;
        dir = parent(dir);
        rebuild();
        selected = 0;
        select("/" + from);
        scrollToSelected();
        snap();
        repaint();
    }

    private void moveOpt(int d) {
        for (int k = 0; k < opts.length; k++) {
            opt = (opt + d + opts.length) % opts.length;
            if (optEnabled(opt)) break;
        }
        repaint();
    }

    private void runOption(int i) {
        if (!optEnabled(i)) return;
        Entry e = current();
        mode = LIST;
        repaint();
        switch (opts[i]) {
        case A_OPEN: activate(e); break;
        case A_MOVE: openMove(); break;
        case A_DELETE: mode = CONFIRM; break;
        case A_NEWDIR: askName("New folder", "", null); break;
        case A_RENAME: askName("Rename folder", e.name, e.path); break;
        case A_RMDIR:
            if (GameSync.removeFolder(e.path)) owner.vitaSync();
            break;
        case A_SETTINGS: J2Vita.openSettings(); break;
        case A_QUIT: J2Vita.quit(); break;
        }
    }

    private void openOptions() {
        buildOptions();
        mode = OPTIONS;
        opt = 0;
        repaint();
    }

    protected void keyPressed(int key) {
        int action = 0;
        try { action = getGameAction(key); } catch (Throwable t) {}
        /* number keys first, some keymaps also report them as game actions */
        boolean soft1 = key == -6, back = key == -7;

        if (mode == CONFIRM) {
            if (action == FIRE && key != KEY_NUM5) {
                AMSMidletCustomItem it = currentGame();
                mode = LIST;
                if (it != null) owner.vitaDeleteNow(it);
                refresh();
            } else if (back || key == KEY_POUND) {
                mode = LIST;
                repaint();
            }
            return;
        }
        if (mode == OPTIONS) {
            if (back || key == KEY_POUND || soft1) {
                mode = LIST;
                repaint();
            } else if (action == UP) moveOpt(-1);
            else if (action == DOWN) moveOpt(1);
            else if (action == FIRE) runOption(opt);
            return;
        }
        if (mode == MOVE) {
            if (back || key == KEY_POUND) {
                mode = LIST;
                repaint();
            } else if (action == UP || action == DOWN) {
                moveSel = (moveSel + (action == UP ? -1 : 1) + dests.length) % dests.length;
                repaint();
            } else if (action == FIRE) doMove();
            return;
        }
        if (key == KEY_POUND || soft1) openOptions();
        else if (back) goUp();
        else if (key >= KEY_NUM0 && key <= KEY_NUM9) return;
        else if (action == UP || action == DOWN || action == LEFT || action == RIGHT) move(action);
        else if (action == FIRE) activate(current());
    }

    protected void keyRepeated(int key) {
        int action = 0;
        try { action = getGameAction(key); } catch (Throwable t) {}
        if (mode == LIST) {
            if (action == UP || action == DOWN || action == LEFT || action == RIGHT) move(action);
        } else if (mode == OPTIONS && (action == UP || action == DOWN)) {
            moveOpt(action == UP ? -1 : 1);
        } else if (mode == MOVE && (action == UP || action == DOWN)) {
            moveSel = (moveSel + (action == UP ? -1 : 1) + dests.length) % dests.length;
            repaint();
        }
    }

    protected void pointerPressed(int x, int y) {
        pressX = x;
        pressY = y;
        pressScroll = scroll;
        dragging = false;
    }

    /** a sideways drag moves the grid under the finger */
    protected void pointerDragged(int x, int y) {
        int dx = x - pressX;
        if (mode != LIST) return;
        if (!dragging && (Math.abs(dx) <= 8 || Math.abs(dx) <= Math.abs(y - pressY))) return;
        dragging = true;
        int max = Math.max(0, totalCols() - cols()) * STEP * 16;
        dragScroll = Math.max(0, Math.min(max, pressScroll - dx * 16));
        repaint();
    }

    protected void pointerReleased(int x, int y) {
        int w = getWidth(), h = getHeight();
        int dx = x - pressX, dy = y - pressY;
        boolean softbar = y >= h - J2Style.SOFT_H;

        if (dragging) {
            /* settle on the nearest column and keep the selection on screen */
            dragging = false;
            firstCol = (dragScroll / 16 + STEP / 2) / STEP;
            clampScroll();
            int col = selected / ROWS;
            if (col < firstCol || col >= firstCol + cols()) {
                selected = Math.min(entries.size() - 1, firstCol * ROWS + selected % ROWS);
            }
            repaint();
            return;
        }
        if (Math.abs(dy) > 12 || Math.abs(dx) > 12) return;

        if (mode == CONFIRM) {
            if (softbar && x < w / 2) keyPressed(-5);
            else if (softbar) keyPressed(-7);
            return;
        }
        if (mode == MOVE) {
            int top = moveTopY(h) + J2Style.TITLE_H + 4, x0 = (w - MOVE_W) / 2;
            int n = Math.min(MOVE_ROWS, dests.length);
            if (x >= x0 && x < x0 + MOVE_W && y >= top && y < top + n * OPT_ROW) {
                moveSel = moveTop + (y - top) / OPT_ROW;
                doMove();
            } else {
                mode = LIST;
                repaint();
            }
            return;
        }
        if (mode == OPTIONS) {
            int top = optTop(h) + J2Style.TITLE_H + 4;
            if (x >= 8 && x < 8 + OPT_W && y >= top && y < top + opts.length * OPT_ROW) {
                opt = (y - top) / OPT_ROW;
                runOption(opt);
            } else {
                mode = LIST;
                repaint();
            }
            return;
        }
        if (softbar) {
            if (x < w / 2) openOptions();
            else activate(current());
            return;
        }
        int gx = x - gridLeft(), gy = y - gridTop();
        if (gx < 0 || gy < 0 || gx % STEP >= TILE || gy % STEP >= TILE) return;
        int c = gx / STEP, r = gy / STEP;
        if (c >= cols() || r >= ROWS) return;
        int i = (firstCol + c) * ROWS + r;
        if (i >= entries.size()) return;
        if (i == selected) activate(current());
        else moveTo(i);
    }
}
