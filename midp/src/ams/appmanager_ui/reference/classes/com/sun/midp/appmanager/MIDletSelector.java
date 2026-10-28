package com.sun.midp.appmanager;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Graphics;

import com.sun.midp.installer.J2Style;
import com.sun.midp.midlet.MIDletSuite;
import com.sun.midp.midletsuite.MIDletInfo;
import com.sun.midp.midletsuite.MIDletSuiteStorage;

/** Lets the player pick which MIDlet to start when a suite has several. */
final class MIDletSelector extends Canvas {
    private static final int ROW_H = 30;

    private final RunningMIDletSuiteInfo suiteInfo;
    private final Display display;
    private final Displayable parentDisplayable;
    private final ApplicationManager manager;
    private MIDletInfo[] info = new MIDletInfo[20];
    private int count, selected, first;

    MIDletSelector(RunningMIDletSuiteInfo s, Display d, Displayable parent, ApplicationManager m)
            throws Throwable {
        suiteInfo = s;
        display = d;
        parentDisplayable = parent;
        manager = m;
        setFullScreenMode(true);
        read();
        display.setCurrent(this);
    }

    private void read() throws Throwable {
        MIDletSuite ms = MIDletSuiteStorage.getMIDletSuiteStorage().getMIDletSuite(suiteInfo.suiteId, false);
        if (ms == null) return;
        try {
            for (int n = 1; n < 100; n++) {
                String a = ms.getProperty("MIDlet-" + n);
                if (a == null || a.length() == 0) break;
                add(new MIDletInfo(a));
            }
        } finally {
            ms.close();
        }
    }

    private void add(MIDletInfo i) {
        if (count >= info.length) {
            MIDletInfo[] n = new MIDletInfo[count + 8];
            System.arraycopy(info, 0, n, 0, count);
            info = n;
        }
        info[count++] = i;
    }

    private int rows() {
        return (getHeight() - J2Style.TOP - J2Style.SOFT_H - 4) / ROW_H;
    }

    protected void paint(Graphics g) {
        int w = getWidth(), h = getHeight(), n = rows(), y0 = J2Style.TOP + 2;
        J2Style.frame(g, w, h, suiteInfo.displayName);
        for (int r = 0; r < n && first + r < count; r++) {
            int i = first + r, y = y0 + r * ROW_H;
            if (i == selected) J2Style.selection(g, 6, y + 1, w - 12, ROW_H - 2);
            else if (i + 1 != selected && i < count - 1) J2Style.separator(g, 14, y + ROW_H - 1, w - 28);
            g.setColor(i == selected ? J2Style.SEL_TEXT : J2Style.TEXT);
            g.drawString(J2Style.fit(info[i].name, w - 40), 18, J2Style.textY(y, ROW_H),
                    Graphics.LEFT | Graphics.TOP);
        }
        J2Style.softkeys(g, w, h, J2Style.CROSS, "Start", J2Style.CIRCLE, "Back");
    }

    private void move(int d) {
        if (count == 0) return;
        selected = Math.max(0, Math.min(count - 1, selected + d));
        if (selected < first) first = selected;
        if (selected >= first + rows()) first = selected - rows() + 1;
        repaint();
    }

    private void launch() {
        if (count == 0) return;
        manager.launchSuite(suiteInfo, info[selected].classname);
        if (parentDisplayable != null) display.setCurrent(parentDisplayable);
    }

    private void back() {
        if (parentDisplayable != null) display.setCurrent(parentDisplayable);
    }

    /* Cross arrives as Fire, Circle/R as Soft2, L/Start as Soft1 */
    protected void keyPressed(int k) {
        int a = 0;
        try { a = getGameAction(k); } catch (Throwable t) {}
        if (k == -7 || k == -6 || k == KEY_POUND) back();
        else if (a == UP) move(-1);
        else if (a == DOWN) move(1);
        else if (a == FIRE && k != KEY_NUM5) launch();
    }

    protected void pointerReleased(int x, int y) {
        int h = getHeight(), y0 = J2Style.TOP + 2;
        if (y >= h - J2Style.SOFT_H) {
            if (x < getWidth() / 2) launch();
            else back();
            return;
        }
        if (y < y0) return;
        int r = (y - y0) / ROW_H, i = first + r;
        if (r >= rows() || i >= count) return;
        if (i == selected) launch();
        else {
            selected = i;
            repaint();
        }
    }
}
