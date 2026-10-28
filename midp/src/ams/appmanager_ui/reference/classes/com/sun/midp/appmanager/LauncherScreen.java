package com.sun.midp.appmanager;

import java.util.Hashtable;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Vector;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

import com.pspkvm.system.J2Vita;
import com.sun.midp.installer.J2Style;
import com.sun.midp.midletsuite.MIDletSuiteInfo;
import com.sun.midp.midletsuite.MIDletSuiteStorage;

/**
 * J2Vita game list, drawn at 480x272 in the classic phone style of J2Style.
 * Cross plays, Triangle (or L / Start) opens Options, Square installs.
 * The frontend sends Fire for Cross, '*' for Square, '#' for Triangle,
 * Soft1 for L/Start and Soft2 for Circle/R.
 */
final class LauncherScreen extends Canvas {
    private static final int ROW_H = 34;
    private static final int ICON = 30;

    private static final int LIST = 0, OPTIONS = 1, CONFIRM = 2;
    private static final int OPT_PLAY = 0, OPT_INSTALL = 1, OPT_DELETE = 2, OPT_SETTINGS = 3, OPT_QUIT = 4;
    private static final String[] OPT_LABELS = {"Play", "Install games", "Delete game", "Settings", "Quit J2Vita"};
    private static final int OPT_W = 200, OPT_ROW = 24;

    private final AppManagerUI owner;
    private final Vector games = new Vector();
    private final Hashtable icons = new Hashtable();
    private static final Object NO_ICON = new Object();
    private int selected, first, mode = LIST, opt;
    private int pressX, pressY;
    private Timer themeWatch;

    LauncherScreen(AppManagerUI owner) {
        this.owner = owner;
        setFullScreenMode(true);
        refresh();
    }

    void refresh() {
        int keep = selectedSuite();
        games.removeAllElements();
        collect(owner.rootFolder);
        selected = 0;
        for (int i = 0; i < games.size(); i++) {
            if (item(i).msi.suiteId == keep) selected = i;
        }
        mode = LIST;
        scrollToSelected();
        repaint();
    }

    void selectSuite(int suiteId) {
        for (int i = 0; i < games.size(); i++) {
            if (item(i).msi.suiteId == suiteId) {
                selected = i;
                scrollToSelected();
                repaint();
                return;
            }
        }
    }

    private void collect(AMSFolderCustomItem folder) {
        if (folder == null) return;
        if (folder.items != null) {
            for (int i = 0; i < folder.items.length; i++) {
                AMSMidletCustomItem it = folder.items[i];
                if (it != null && it.msi != null && it.msi.suiteId >= 0) games.addElement(it);
            }
        }
        if (folder.subfolders != null) {
            for (int i = 0; i < folder.subfolders.length; i++) collect(folder.subfolders[i]);
        }
    }

    private AMSMidletCustomItem item(int i) {
        return (AMSMidletCustomItem)games.elementAt(i);
    }

    private AMSMidletCustomItem current() {
        return games.size() == 0 ? null : item(selected);
    }

    private int selectedSuite() {
        AMSMidletCustomItem it = current();
        return it == null ? -1 : it.msi.suiteId;
    }

    private int rows() {
        return (getHeight() - J2Style.TOP - J2Style.SOFT_H - 4) / ROW_H;
    }

    private void scrollToSelected() {
        int n = rows();
        if (selected < first) first = selected;
        if (selected >= first + n) first = selected - n + 1;
        if (first > games.size() - n) first = games.size() - n;
        if (first < 0) first = 0;
    }

    /** the suite icon, blown up 2x when it is tiny so it fills the frame */
    private Image icon(AMSMidletCustomItem it) {
        Integer key = new Integer(it.msi.suiteId);
        Object cached = icons.get(key);
        if (cached != null) return cached instanceof Image ? (Image)cached : null;
        Image img = it.icon;
        try {
            if (img == null) {
                MIDletSuiteStorage store = MIDletSuiteStorage.getMIDletSuiteStorage();
                if (it.msi.iconName == null) {
                    MIDletSuiteInfo info = store.getMIDletSuiteInfo(it.msi.suiteId);
                    it.msi.iconName = info.iconName;
                }
                it.msi.loadIcon(store);
                img = it.icon = it.msi.icon;
            }
            if (img != null && img.getWidth() * 2 <= ICON && img.getHeight() * 2 <= ICON) {
                int w = img.getWidth(), h = img.getHeight();
                int[] src = new int[w * h], dst = new int[w * h * 4];
                img.getRGB(src, 0, w, 0, 0, w, h);
                for (int y = 0; y < h * 2; y++) {
                    for (int x = 0; x < w * 2; x++) dst[y * w * 2 + x] = src[(y / 2) * w + x / 2];
                }
                img = Image.createRGBImage(dst, w * 2, h * 2, true);
            }
        } catch (Throwable t) {
        }
        icons.put(key, img != null ? (Object)img : NO_ICON);
        return img;
    }

    protected void showNotify() {
        refresh();
        /* the native settings menu can switch the theme while we are shown */
        if (themeWatch == null) {
            themeWatch = new Timer();
            themeWatch.schedule(new TimerTask() {
                public void run() {
                    if (J2Style.stale()) repaint();
                }
            }, 500, 500);
        }
    }

    protected void hideNotify() {
        if (themeWatch != null) {
            themeWatch.cancel();
            themeWatch = null;
        }
    }

    protected void paint(Graphics g) {
        int w = getWidth(), h = getHeight();
        J2Style.frame(g, w, h, "Java Games");

        if (games.size() == 0) paintEmpty(g, w, h);
        else paintList(g, w, h);

        if (mode == OPTIONS) {
            paintOptions(g, w, h);
            J2Style.softkeys(g, w, h, J2Style.CROSS, "Select", J2Style.CIRCLE, "Back");
        } else if (mode == CONFIRM) {
            paintConfirm(g, w, h);
            J2Style.softkeys(g, w, h, J2Style.CROSS, "Delete", J2Style.CIRCLE, "Cancel");
        } else if (games.size() == 0) {
            J2Style.softkeys(g, w, h, J2Style.TRIANGLE, "Options", J2Style.SQUARE, "Install");
        } else {
            J2Style.softkeys(g, w, h, J2Style.TRIANGLE, "Options", J2Style.CROSS, "Play");
        }
    }

    private void paintEmpty(Graphics g, int w, int h) {
        int y = J2Style.TOP + 40;
        g.setColor(J2Style.TEXT);
        g.drawString("No games installed", w / 2, y, Graphics.HCENTER | Graphics.TOP);
        J2Style.wrap(g, "Copy .jar files to ux0:data/j2vita/games/ and press Square to install them.",
                w / 2, y + 34, w - 80, 4, J2Style.DIM, Graphics.HCENTER);
    }

    private void paintList(Graphics g, int w, int h) {
        int n = rows(), y0 = J2Style.TOP + 2;
        boolean scroll = games.size() > n;
        int right = scroll ? w - 14 : w - 6;
        for (int r = 0; r < n && first + r < games.size(); r++) {
            int i = first + r, y = y0 + r * ROW_H;
            boolean sel = i == selected;
            AMSMidletCustomItem it = item(i);
            if (sel) {
                J2Style.selection(g, 6, y + 1, right - 6, ROW_H - 2);
            } else if (i + 1 != selected && r < n - 1 && i < games.size() - 1) {
                J2Style.separator(g, 50, y + ROW_H - 1, right - 56);
            }

            int ix = 12, iy = y + (ROW_H - ICON) / 2;
            g.setColor(sel ? J2Style.SEL_FILL : J2Style.WELL);
            g.fillRoundRect(ix, iy, ICON, ICON, 8, 8);
            Image ic = icon(it);
            g.setClip(ix + 1, iy + 1, ICON - 2, ICON - 2);
            if (ic != null) {
                g.drawImage(ic, ix + ICON / 2, iy + ICON / 2, Graphics.HCENTER | Graphics.VCENTER);
            } else {
                String name = it.msi.displayName;
                g.setColor(J2Style.TEXT);
                g.drawString(name != null && name.length() > 0 ? name.substring(0, 1).toUpperCase() : "?",
                        ix + ICON / 2, J2Style.textY(iy, ICON), Graphics.HCENTER | Graphics.TOP);
            }
            g.setClip(0, 0, w, h);
            g.setColor(sel ? J2Style.SEL_EDGE : J2Style.WELL_EDGE);
            g.drawRoundRect(ix, iy, ICON - 1, ICON - 1, 8, 8);

            String name = it.msi.displayName;
            if (!it.msi.enabled) name = name + " (disabled)";
            g.setColor(sel ? J2Style.SEL_TEXT : J2Style.TEXT);
            g.drawString(J2Style.fit(name, right - 54 - 8), 52, J2Style.textY(y, ROW_H),
                    Graphics.LEFT | Graphics.TOP);
        }
        if (scroll) {
            int top = y0 + 2, track = n * ROW_H - 4;
            int thumb = Math.max(16, track * n / games.size());
            int pos = top + (track - thumb) * first / (games.size() - n);
            g.setColor(J2Style.WELL);
            g.fillRoundRect(w - 10, top, 5, track, 4, 4);
            g.setColor(J2Style.SEL_FILL);
            g.fillRoundRect(w - 10, pos, 5, thumb, 4, 4);
        }
    }

    private boolean optEnabled(int i) {
        return games.size() > 0 || i == OPT_INSTALL || i == OPT_SETTINGS || i == OPT_QUIT;
    }

    private int optTop(int h) {
        return h - J2Style.SOFT_H - 6 - (J2Style.TITLE_H + OPT_LABELS.length * OPT_ROW + 8);
    }

    private void paintOptions(Graphics g, int w, int h) {
        int x = 8, y = optTop(h);
        int ph = J2Style.TITLE_H + OPT_LABELS.length * OPT_ROW + 8;
        J2Style.dialog(g, x, y, OPT_W, ph, "Options");
        for (int i = 0; i < OPT_LABELS.length; i++) {
            int ry = y + J2Style.TITLE_H + 4 + i * OPT_ROW;
            if (i == opt) J2Style.selection(g, x + 5, ry, OPT_W - 10, OPT_ROW);
            g.setColor(!optEnabled(i) ? J2Style.DISABLED : i == opt ? J2Style.SEL_TEXT : J2Style.TEXT);
            g.drawString(J2Style.fit(OPT_LABELS[i], OPT_W - 28), x + 14, J2Style.textY(ry, OPT_ROW),
                    Graphics.LEFT | Graphics.TOP);
        }
    }

    private void paintConfirm(Graphics g, int w, int h) {
        int dw = 320, dh = 120, x = (w - dw) / 2, y = J2Style.TOP + (h - J2Style.TOP - J2Style.SOFT_H - dh) / 2;
        AMSMidletCustomItem it = current();
        J2Style.dialog(g, x, y, dw, dh, "Delete game");
        g.setColor(J2Style.TEXT);
        g.drawString(J2Style.fit(it == null ? "" : it.msi.displayName, dw - 28), w / 2, y + J2Style.TITLE_H + 14,
                Graphics.HCENTER | Graphics.TOP);
        J2Style.wrap(g, "The game and its saved data will be removed.", w / 2, y + J2Style.TITLE_H + 40,
                dw - 28, 2, J2Style.DIM, Graphics.HCENTER);
    }

    private void move(int d) {
        if (games.size() == 0) return;
        selected = Math.max(0, Math.min(games.size() - 1, selected + d));
        scrollToSelected();
        repaint();
    }

    private void moveOpt(int d) {
        for (int k = 0; k < OPT_LABELS.length; k++) {
            opt = (opt + d + OPT_LABELS.length) % OPT_LABELS.length;
            if (optEnabled(opt)) break;
        }
        repaint();
    }

    private void runOption(int i) {
        if (!optEnabled(i)) return;
        mode = LIST;
        repaint();
        switch (i) {
        case OPT_PLAY: owner.vitaLaunch(current()); break;
        case OPT_INSTALL: owner.vitaInstall(); break;
        case OPT_DELETE: mode = CONFIRM; break;
        case OPT_SETTINGS: J2Vita.openSettings(); break;
        case OPT_QUIT: J2Vita.quit(); break;
        }
    }

    private void openOptions() {
        mode = OPTIONS;
        opt = games.size() > 0 ? OPT_PLAY : OPT_INSTALL;
        repaint();
    }

    protected void keyPressed(int key) {
        int action = 0;
        try { action = getGameAction(key); } catch (Throwable t) {}
        /* number keys first, some keymaps also report them as game actions */
        boolean soft1 = key == -6, back = key == -7;

        if (mode == CONFIRM) {
            if (action == FIRE && key != KEY_NUM5) {
                AMSMidletCustomItem it = current();
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
        if (key == KEY_STAR) owner.vitaInstall();
        else if (key == KEY_POUND || soft1) openOptions();
        else if (key >= KEY_NUM0 && key <= KEY_NUM9) return;
        else if (action == UP) move(-1);
        else if (action == DOWN) move(1);
        else if (action == LEFT) move(-rows());
        else if (action == RIGHT) move(rows());
        else if (action == FIRE) owner.vitaLaunch(current());
    }

    protected void keyRepeated(int key) {
        int action = 0;
        try { action = getGameAction(key); } catch (Throwable t) {}
        if (action != UP && action != DOWN) return;
        if (mode == LIST) move(action == UP ? -1 : 1);
        else if (mode == OPTIONS) moveOpt(action == UP ? -1 : 1);
    }

    protected void pointerPressed(int x, int y) {
        pressX = x;
        pressY = y;
    }

    protected void pointerReleased(int x, int y) {
        int w = getWidth(), h = getHeight();
        if (Math.abs(y - pressY) > 12 || Math.abs(x - pressX) > 12) return;
        boolean softbar = y >= h - J2Style.SOFT_H;

        if (mode == CONFIRM) {
            if (softbar && x < w / 2) keyPressed(-5);
            else if (softbar) keyPressed(-7);
            return;
        }
        if (mode == OPTIONS) {
            int top = optTop(h) + J2Style.TITLE_H + 4;
            if (x >= 8 && x < 8 + OPT_W && y >= top && y < top + OPT_LABELS.length * OPT_ROW) {
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
            else if (games.size() == 0) owner.vitaInstall();
            else owner.vitaLaunch(current());
            return;
        }
        int y0 = J2Style.TOP + 2;
        if (y < y0) return;
        int r = (y - y0) / ROW_H, i = first + r;
        if (r >= rows() || i >= games.size()) return;
        if (i == selected) owner.vitaLaunch(current());
        else {
            selected = i;
            repaint();
        }
    }
}
