package com.sun.midp.installer;

import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;

import com.pspkvm.system.J2Vita;

/**
 * Look shared by every J2Vita system screen (launcher, installer, dialogs):
 * a classic phone menu with a title bar, a list and a soft key bar. Screens
 * are 480x272 and the system font is 8 px per character, so every string goes
 * through fit() or wrap() before it is drawn. Colours come from the theme the
 * player picked (palettes live in vita/theme.c); frame() reloads them.
 */
public final class J2Style {
    public static final Font FONT = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_MEDIUM);

    public static final int TITLE_H = 28;
    public static final int SOFT_H = 24;
    /** first pixel row under the title bar */
    public static final int TOP = TITLE_H;

    /* theme slots, same order as the TC_* enum in vita/j2vita.h */
    private static final int BG_TOP = 0, BG_BOT = 1, BAR_TOP = 2, BAR_BOT = 3, BAR_LIGHT = 4,
            BAR_EDGE = 5, BAR_TEXT = 6, C_TEXT = 7, C_DIM = 8, C_LINE = 9, SEL_TOP = 10,
            SEL_BOT = 11, C_SEL_EDGE = 12, C_SEL_TEXT = 13, PANEL_TOP = 14, PANEL_BOT = 15,
            PANEL_EDGE = 16, SHADOW = 17, C_ACCENT = 18, GLYPH = 19, C_WELL = 20,
            C_WELL_EDGE = 21, C_DANGER = 22, C_OK = 23, C_DISABLED = 24, SLOTS = 25;
    private static final int[] c = new int[SLOTS];
    private static int serial = -1;

    public static int TEXT, DIM, SEL_TEXT, ACCENT, DANGER, LINE, OK, DISABLED, WELL, WELL_EDGE,
            SEL_EDGE, SEL_FILL;

    static {
        refresh();
    }

    /** true when the theme changed since the colours were last loaded */
    public static boolean stale() {
        try {
            return J2Vita.themeSerial() != serial;
        } catch (Throwable t) {
            return false;
        }
    }

    public static void refresh() {
        try {
            serial = J2Vita.themeSerial();
            for (int i = 0; i < SLOTS; i++) c[i] = J2Vita.themeColor(i);
        } catch (Throwable t) {
        }
        TEXT = c[C_TEXT];
        DIM = c[C_DIM];
        SEL_TEXT = c[C_SEL_TEXT];
        ACCENT = c[C_ACCENT];
        DANGER = c[C_DANGER];
        LINE = c[C_LINE];
        OK = c[C_OK];
        DISABLED = c[C_DISABLED];
        WELL = c[C_WELL];
        WELL_EDGE = c[C_WELL_EDGE];
        SEL_EDGE = c[C_SEL_EDGE];
        SEL_FILL = c[SEL_BOT];
    }

    public static final int NO_GLYPH = -1, CROSS = 0, CIRCLE = 1, SQUARE = 2, TRIANGLE = 3;

    private J2Style() {}

    /** a + (b - a) * t / n per channel */
    public static int mix(int a, int b, int t, int n) {
        if (n <= 0) return a;
        int r = ((a >> 16) & 0xFF) + ((((b >> 16) & 0xFF) - ((a >> 16) & 0xFF)) * t) / n;
        int gr = ((a >> 8) & 0xFF) + ((((b >> 8) & 0xFF) - ((a >> 8) & 0xFF)) * t) / n;
        int bl = (a & 0xFF) + (((b & 0xFF) - (a & 0xFF)) * t) / n;
        return (r << 16) | (gr << 8) | bl;
    }

    public static void gradient(Graphics g, int x, int y, int w, int h, int top, int bottom) {
        for (int i = 0; i < h; i++) {
            g.setColor(mix(top, bottom, i, h - 1));
            g.drawLine(x, y + i, x + w - 1, y + i);
        }
    }

    /** text baseline-free vertical position that centres FONT in a box of height h */
    public static int textY(int y, int h) {
        return y + (h - FONT.getHeight()) / 2 + 1;
    }

    /** background and title bar; the content area is TOP .. h - SOFT_H */
    public static void frame(Graphics g, int w, int h, String title) {
        if (stale()) refresh();
        g.setFont(FONT);
        gradient(g, 0, TOP, w, h - TOP - SOFT_H, c[BG_TOP], c[BG_BOT]);
        gradient(g, 0, 0, w, TITLE_H, c[BAR_TOP], c[BAR_BOT]);
        g.setColor(c[BAR_LIGHT]);
        g.drawLine(0, 0, w - 1, 0);
        g.setColor(c[BAR_EDGE]);
        g.drawLine(0, TOP - 1, w - 1, TOP - 1);
        g.setColor(c[BAR_TEXT]);
        g.drawString(fit(title, w - 24), w / 2, textY(0, TITLE_H), Graphics.HCENTER | Graphics.TOP);
    }

    /** bottom bar; a null label leaves that side empty */
    public static void softkeys(Graphics g, int w, int h, int leftGlyph, String left,
                                int rightGlyph, String right) {
        int y = h - SOFT_H, half = w / 2 - 16;
        gradient(g, 0, y, w, SOFT_H, c[BAR_TOP], c[BAR_BOT]);
        g.setColor(c[BAR_EDGE]);
        g.drawLine(0, y, w - 1, y);
        g.setFont(FONT);
        int ty = textY(y, SOFT_H), cy = y + SOFT_H / 2;
        if (left != null) {
            int x = 10;
            if (leftGlyph != NO_GLYPH) {
                glyph(g, leftGlyph, x + 5, cy);
                x += 16;
            }
            g.setColor(c[BAR_TEXT]);
            g.drawString(fit(left, half - x), x, ty, Graphics.LEFT | Graphics.TOP);
        }
        if (right != null) {
            int maxw = half - (rightGlyph != NO_GLYPH ? 16 : 0) - 10;
            String s = fit(right, maxw);
            int x = w - 10 - FONT.stringWidth(s);
            g.setColor(c[BAR_TEXT]);
            g.drawString(s, x, ty, Graphics.LEFT | Graphics.TOP);
            if (rightGlyph != NO_GLYPH) glyph(g, rightGlyph, x - 11, cy);
        }
    }

    /**
     * Rows from..to-1 of a w x h box with corner radius r, filled with a
     * vertical gradient running from top (row from) to bottom (row to-1).
     */
    public static void roundRows(Graphics g, int x, int y, int w, int h, int r,
                                 int from, int to, int top, int bottom) {
        for (int i = from; i < to; i++) {
            int dy = i < r ? r - i : (i > h - 1 - r ? i - (h - 1 - r) : 0), off = 0;
            if (dy > 0) {
                int k = 0;
                while ((k + 1) * (k + 1) <= r * r - dy * dy) k++;
                off = r - k;
            }
            g.setColor(mix(top, bottom, i - from, to - from - 1));
            g.drawLine(x + off, y + i, x + w - 1 - off, y + i);
        }
    }

    /** highlighted list row, the light bar from the mock-up */
    public static void selection(Graphics g, int x, int y, int w, int h) {
        g.setColor(c[SHADOW]);
        g.fillRoundRect(x + 1, y + 2, w, h, 10, 10);
        roundRows(g, x, y, w, h, 5, 0, h, c[SEL_TOP], c[SEL_BOT]);
        g.setColor(c[C_SEL_EDGE]);
        g.drawRoundRect(x, y, w - 1, h - 1, 10, 10);
        g.drawLine(x + 5, y + 1, x + w - 6, y + 1);
    }

    public static void separator(Graphics g, int x, int y, int w) {
        g.setColor(LINE);
        g.drawLine(x, y, x + w - 1, y);
    }

    /** rounded panel with its own title bar, used by dialogs and pop-up menus */
    public static void dialog(Graphics g, int x, int y, int w, int h, String title) {
        int t = title != null ? TITLE_H : 0;
        g.setColor(c[SHADOW]);
        g.fillRoundRect(x + 3, y + 4, w, h, 12, 12);
        if (t > 0) roundRows(g, x, y, w, h, 6, 0, t, c[BAR_TOP], c[BAR_BOT]);
        roundRows(g, x, y, w, h, 6, t, h, c[PANEL_TOP], c[PANEL_BOT]);
        g.setColor(c[PANEL_EDGE]);
        g.drawRoundRect(x, y, w - 1, h - 1, 12, 12);
        if (t > 0) {
            g.setColor(c[BAR_EDGE]);
            g.drawLine(x + 1, y + t - 1, x + w - 2, y + t - 1);
            g.setFont(FONT);
            g.setColor(c[BAR_TEXT]);
            g.drawString(fit(title, w - 20), x + w / 2, textY(y, t), Graphics.HCENTER | Graphics.TOP);
        }
    }

    /** PlayStation face button outline centred on cx, cy, plain grey in every theme */
    public static void glyph(Graphics g, int type, int cx, int cy) {
        g.setColor(c[GLYPH]);
        switch (type) {
        case CROSS:
            g.drawLine(cx - 4, cy - 4, cx + 4, cy + 4);
            g.drawLine(cx - 3, cy - 4, cx + 4, cy + 3);
            g.drawLine(cx + 4, cy - 4, cx - 4, cy + 4);
            g.drawLine(cx + 3, cy - 4, cx - 4, cy + 3);
            break;
        case CIRCLE:
            g.drawArc(cx - 5, cy - 5, 10, 10, 0, 360);
            g.drawArc(cx - 4, cy - 4, 8, 8, 0, 360);
            break;
        case SQUARE:
            g.drawRect(cx - 4, cy - 4, 8, 8);
            g.drawRect(cx - 3, cy - 3, 6, 6);
            break;
        case TRIANGLE:
            g.drawLine(cx, cy - 5, cx - 5, cy + 4);
            g.drawLine(cx, cy - 5, cx + 5, cy + 4);
            g.drawLine(cx - 5, cy + 4, cx + 5, cy + 4);
            g.drawLine(cx, cy - 3, cx - 4, cy + 3);
            g.drawLine(cx, cy - 3, cx + 4, cy + 3);
            break;
        }
    }

    public static void progress(Graphics g, int x, int y, int w, int h, int value, int max) {
        g.setColor(c[C_WELL]);
        g.fillRoundRect(x, y, w, h, h, h);
        g.setColor(c[C_WELL_EDGE]);
        g.drawRoundRect(x, y, w - 1, h - 1, h, h);
        int fill;
        if (max > 0) {
            fill = (int)(((long)(w - 4) * Math.min(value, max)) / max);
        } else {
            /* unknown size: a block that walks across the bar */
            int step = (int)((System.currentTimeMillis() / 60) % (w - 4));
            int len = (w - 4) / 4;
            g.setClip(x + 2, y, w - 4, h);
            gradient(g, x + 2 + step - len / 2, y + 2, len, h - 4, c[SEL_TOP], c[SEL_BOT]);
            g.setClip(0, 0, 4096, 4096);
            return;
        }
        if (fill > 0) gradient(g, x + 2, y + 2, fill, h - 4, c[SEL_TOP], c[SEL_BOT]);
    }

    /** s cut down with "..." so that it is at most max pixels wide */
    public static String fit(String s, int max) {
        if (s == null) return "";
        if (FONT.stringWidth(s) <= max) return s;
        String t = s;
        while (t.length() > 1 && FONT.stringWidth(t + "...") > max) t = t.substring(0, t.length() - 1);
        return FONT.stringWidth(t + "...") <= max ? t + "..." : "";
    }

    /** word-wraps text into at most maxLines lines; returns the y under the last line */
    public static int wrap(Graphics g, String text, int x, int y, int maxW, int maxLines,
                           int color, int anchor) {
        if (text == null || text.length() == 0) return y;
        g.setFont(FONT);
        g.setColor(color);
        String n = text.replace('\n', ' ');
        int p = 0, line = 0, lh = FONT.getHeight() + 2;
        while (p < n.length() && line < maxLines) {
            while (p < n.length() && n.charAt(p) == ' ') p++;
            if (p >= n.length()) break;
            int e = p, sp = -1;
            while (e < n.length()) {
                if (n.charAt(e) == ' ') sp = e;
                if (FONT.stringWidth(n.substring(p, e + 1)) > maxW) break;
                e++;
            }
            if (e < n.length()) {
                if (sp > p) e = sp;
                else if (e == p) e = p + 1;
            }
            String z = n.substring(p, e).trim();
            if (line == maxLines - 1 && e < n.length()) z = fit(z + "...", maxW);
            g.drawString(z, x, y + line * lh, anchor | Graphics.TOP);
            p = e;
            line++;
        }
        return y + line * lh;
    }
}
