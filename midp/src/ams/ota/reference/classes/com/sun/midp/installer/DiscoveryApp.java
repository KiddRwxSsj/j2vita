/*
 *
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */

package com.sun.midp.installer;

import java.io.*;

import java.util.*;

import javax.microedition.io.*;

import javax.microedition.lcdui.*;

import javax.microedition.midlet.*;

import javax.microedition.rms.*;

import com.sun.midp.i18n.Resource;
import com.sun.midp.i18n.ResourceConstants;

import com.sun.midp.midlet.*;
import com.pspkvm.system.VMSettings;

import com.sun.midp.log.Logging;
import com.sun.midp.log.LogChannels;
import com.sun.midp.io.j2me.storage.RandomAccessStream;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.CustomItem;
/**
 * The Graphical MIDlet suite Discovery Application.
 * <p>
 * Let the user install a suite from a list of suites
 * obtained using an HTML URL given by the user. This list is derived by
 * extracting the links with hrefs that are in quotes and end with ".jad" from
 * the HTML page. An href in an extracted link is assumed to be an absolute
 * URL for a MIDP application descriptor. The selected URL is then passed to
 * graphical Installer.
 */
public class DiscoveryApp extends MIDlet implements ItemCommandListener, CommandListener {

    /** Display for this MIDlet. */
    private Display display;
    /** Contains the default URL for the install list. */
    private String defaultInstallListUrl = "";
    /** Contains the URL the user typed in. */
    private TextBox urlTextBox;
    /** Displays the progress of the install. */
    private InstallProgressCanvas progressForm;
    /** Gauge for progress form index. */
    /** URL for progress form index. */
    /** Keeps track of when the display last changed, in milliseconds. */
    private long lastDisplayChange;
    /** Displays a list of suites to install to the user. */
    private InstallBrowserCanvas installListBox;
    /** Contains a list of suites to install. */
    private Vector installList;

    private boolean http_install;

    private String lastInstallMidletName;

    /** Command object for URL screen to go and discover available suites. */
    private Command discoverCmd =
        new Command(Resource.getString(ResourceConstants.GOTO),
                    Command.SCREEN, 1);
    /** Command object for "Install" command in the suite list form . */
    private Command installCmd = new Command(
        Resource.getString(ResourceConstants.INSTALL), Command.ITEM, 1);
    /** Command object for "Back" command in the suite list form. */
    private Command backCmd = new Command(Resource.getString
                                          (ResourceConstants.BACK),
                                          Command.BACK, 1);
    /** Command object for URL screen to save the URL for suites. */
    private Command saveCmd =
        new Command(Resource.getString(ResourceConstants.SAVE),
                    Command.SCREEN, 2);

    /** Command object for "Back" command in the URL form. */
    private Command endCmd = new Command(Resource.getString
                                         (ResourceConstants.BACK),
                                         Command.BACK, 1);


    private static final String LOCAL_INSTALL_ROOT = "ux0:data/j2vita/games/";
    private String last_dir = LOCAL_INSTALL_ROOT;
    
    /**
     * Create and initialize a new discovery application MIDlet.
     * The saved URL is retrieved and the list of MIDlets are retrieved.
     */
    public DiscoveryApp() {
        String storageName;

        display = Display.getDisplay(this);

        GraphicalInstaller.initSettings();


        
        // no network in this build, go straight to the JAR list on ux0
        activateInstallSource(0);
    }


    private void activateInstallSource(int selectedIndex) {
        if (selectedIndex == 0) {
            http_install = false;
            defaultInstallListUrl = LOCAL_INSTALL_ROOT;
        } else {
            http_install = true;
            defaultInstallListUrl = "http://";
        }

        restoreSettings();

        if (http_install) {
            getUrl();
        } else {
            if (!defaultInstallListUrl.startsWith(LOCAL_INSTALL_ROOT)) {
                defaultInstallListUrl = LOCAL_INSTALL_ROOT;
            }

            last_dir = defaultInstallListUrl;
            discoverSuitesToInstall(defaultInstallListUrl);
        }
    }


    /**
     * Start.
     */
    public void startApp() {
    }

    /**
     * Pause; there are no resources that need to be released.
     */
    public void pauseApp() {
    }

    /**
     * Destroy cleans up.
     *
     * @param unconditional is ignored; this object always
     * destroys itself when requested.
     */
    public void destroyApp(boolean unconditional) {
    }

    /**
     * Respond to a command issued on any Screen.
     *
     * @param c command activated by the user
     * @param s the Displayable the command was on.
     */
    public void commandAction(Command c, Displayable s) {
        if (c == discoverCmd) {
            discoverSuitesToInstall(urlTextBox.getString());
        } else if (c == backCmd) {
            if (urlTextBox != null) {
                display.setCurrent(urlTextBox);
            } else {
                notifyDestroyed();
            }
        } else if (c == saveCmd) {
            saveURLSetting(null);
        } else if (c == endCmd || c == Alert.DISMISS_COMMAND) {
            notifyDestroyed();
        }
    }
 
    public void commandAction(Command c, Item item) {
        // v0.1.81 browser handles selection directly in InstallBrowserCanvas.
    }
    
    private void activateInstallListEntry(int selectedIndex) {
        if (installList == null ||
                selectedIndex < 0 ||
                selectedIndex >= installList.size()) {
            return;
        }

        SuiteDownloadInfo info =
                (SuiteDownloadInfo)installList.elementAt(selectedIndex);

        if (info.dir) {
            String installDir;

            if (info.label.equals("..")) {
                installDir = info.url.substring(0, info.url.lastIndexOf('/'));
                installDir = installDir.substring(
                        0, installDir.lastIndexOf('/') + 1);
            } else {
                installDir = info.url + "/";
            }

            last_dir = installDir;
            discoverSuitesToInstall(installDir);
            return;
        }

        if (!http_install) {
            saveURLSetting(info.label);
        }

        installSuite(selectedIndex);
    }

    private void browserBack(String sourceUrl) {
        if (http_install) {
            if (urlTextBox != null) {
                display.setCurrent(urlTextBox);
            } else {
                notifyDestroyed();
            }
            return;
        }

        if (sourceUrl != null &&
                sourceUrl.startsWith(LOCAL_INSTALL_ROOT) &&
                !sourceUrl.equals(LOCAL_INSTALL_ROOT)) {
            try {
                String p = sourceUrl;
                if (p.endsWith("/")) {
                    p = p.substring(0, p.length() - 1);
                }
                int slash = p.lastIndexOf('/');
                if (slash >= LOCAL_INSTALL_ROOT.length() - 1) {
                    p = p.substring(0, slash + 1);
                    last_dir = p;
                    discoverSuitesToInstall(p);
                    return;
                }
            } catch (Throwable ignored) {
            }
        }

        notifyDestroyed();
    }

    /**
     * Get the settings the Manager saved for the user.
     */
    private void restoreSettings() {
        ByteArrayInputStream bas;
        DataInputStream dis;
        byte[] data;
        RecordStore settings = null;

        /**
         * ams.url = "" or null when running OTA from command line /
         *           OTA provisioning
         * ams.url = <some url> when running OTA from KToolbar */
        String amsUrl = System.getProperty("ams.url");
        
        if (amsUrl != null && !amsUrl.equals("")) {
            defaultInstallListUrl = amsUrl.trim();
            return;
        }

        try {
            settings = RecordStore.openRecordStore(
                       GraphicalInstaller.SETTINGS_STORE, false);

            data = settings.getRecord(http_install?1:3);
            if (data != null) {
                bas = new ByteArrayInputStream(data);
                dis = new DataInputStream(bas);
                defaultInstallListUrl = dis.readUTF();
            }

            data = settings.getRecord(4);
            if (data != null) {
                bas = new ByteArrayInputStream(data);
                dis = new DataInputStream(bas);
                lastInstallMidletName = dis.readUTF();
            }

        } catch (RecordStoreException e) {
            if (Logging.REPORT_LEVEL <= Logging.WARNING) {
                Logging.report(Logging.WARNING, LogChannels.LC_AMS,
                               "restoreSettings threw a RecordStoreException");
            }
        } catch (IOException e) {
            if (Logging.REPORT_LEVEL <= Logging.WARNING) {
                Logging.report(Logging.WARNING, LogChannels.LC_AMS,
                               "restoreSettings threw an IOException");
            }
        } finally {
            if (settings != null) {
                try {
                    settings.closeRecordStore();
                } catch (RecordStoreException e) {
                    if (Logging.REPORT_LEVEL <= Logging.WARNING) {
                        Logging.report(Logging.WARNING, LogChannels.LC_AMS,
                        "closeRecordStore threw a RecordStoreException");
                    }
                }
            }
        }
    }

    /**
     * Save the URL setting the user entered in to the urlTextBox.
     */
    private void saveURLSetting(String midlet) {
        String temp;
        Exception ex;

        temp = http_install?urlTextBox.getString():last_dir;

        ex = GraphicalInstaller.saveSettings(http_install, temp, MIDletSuite.INTERNAL_SUITE_ID, midlet);
        if (ex != null) {
            displayException(Resource.getString
                             (ResourceConstants.EXCEPTION), ex.toString());
            return;
        }

        defaultInstallListUrl = temp;

        displaySuccessMessage(Resource.getString
                              (ResourceConstants.AMS_MGR_SAVED));
    }

    /**
     * Alert the user that an action was successful.
     *
     * @param successMessage message to display to user
     */
    private void displaySuccessMessage(String successMessage) {
        Image icon;
        Alert successAlert;

        icon = GraphicalInstaller.getImageFromInternalStorage("_dukeok8");

        successAlert = new Alert(null, successMessage, icon, null);

        successAlert.setTimeout(GraphicalInstaller.ALERT_TIMEOUT);

        // We need to prevent "flashing" on fast development platforms.
        while (System.currentTimeMillis() - lastDisplayChange <
               GraphicalInstaller.ALERT_TIMEOUT);

        lastDisplayChange = System.currentTimeMillis();
        display.setCurrent(successAlert);
    }

    /**
     * Let the user select a suite to install. The suites that are listed
     * are the links on a web page that end with .jad.
     *
     * @param url where to get the list of suites to install.
     */
    private void discoverSuitesToInstall(String url) {
        new Thread(new BackgroundInstallListGetter(this, url)).start();
    }

    /**
     * Display the connecting form to the user, let call set actions.
     *
     * @param action action to put in the form's title
     * @param name name to in the form's title
     * @param url URL of a JAD
     * @param size 0 if unknown, else size of object to download in K bytes
     * @param gaugeLabel label for progress gauge
     *
     * @return displayed form
     */
    private InstallProgressCanvas displayProgressForm(String action, String name,
            String url, int size, String gaugeLabel) {
        progressForm = new InstallProgressCanvas(
                action == null ? "" : action,
                name == null ? "" : name,
                url == null ? "" : url,
                gaugeLabel == null ? "" : gaugeLabel,
                size);

        display.setCurrent(progressForm);
        lastDisplayChange = System.currentTimeMillis();
        return progressForm;
    }

    /**
     * Install a suite.
     *
     * @param selectedSuite index into the installList
     */
    private void installSuite(int selectedSuite) {
        MIDletStateHandler midletStateHandler =
            MIDletStateHandler.getMidletStateHandler();
        MIDletSuite midletSuite = midletStateHandler.getMIDletSuite();
        SuiteDownloadInfo suite;
        String displayName;

        suite = (SuiteDownloadInfo)installList.elementAt(selectedSuite);

        System.out.println("installSuite: "+ suite.url);

        midletSuite.setTempProperty(null, "arg-1", suite.url);
        midletSuite.setTempProperty(null, "arg-2", suite.label);
        if (http_install) {
            midletSuite.setTempProperty(null, "arg-0", "I");
            //The 3rd arg is for indicating file installer is used or not
            midletSuite.setTempProperty(null, "arg-3", "false");
        } else {
            midletSuite.setTempProperty(null, "arg-0", "FI");
            midletSuite.setTempProperty(null, "arg-3", "true");
        }

        displayName =
            Resource.getString(ResourceConstants.INSTALL_APPLICATION);
        try {
            midletStateHandler.startMIDlet(
                "com.sun.midp.installer.GraphicalInstaller", displayName);
            /*
             * Give the create MIDlet notification 1 second to get to
             * AMS.
             */
            Thread.sleep(1000);
            notifyDestroyed();
        } catch (Exception ex) {
            StringBuffer sb = new StringBuffer();

            sb.append(displayName);
            sb.append("\n");
            sb.append(Resource.getString(ResourceConstants.ERROR));
            sb.append(": ");
            sb.append(ex.toString());

            Alert a = new Alert(Resource.getString
                                (ResourceConstants.AMS_CANNOT_START),
                                sb.toString(), null, AlertType.ERROR);
            a.setTimeout(Alert.FOREVER);
            display.setCurrent(a, urlTextBox);
        }
    }

    /**
     * Update URL and gauge of the progress form.
     *
     * @param url new URL, null to remove, "" to not change
     * @param size 0 if unknown, else size of object to download in K bytes
     * @param gaugeLabel label for progress gauge
     */
    private void updateProgressForm(String url, int size, String gaugeLabel) {
        while (System.currentTimeMillis() - lastDisplayChange <
                GraphicalInstaller.ALERT_TIMEOUT);

        if (progressForm != null) {
            progressForm.update(
                    url,
                    size,
                    gaugeLabel == null ? "" : gaugeLabel);
        }

        lastDisplayChange = System.currentTimeMillis();
    }

    /**
     * Ask the user for the URL.
     */
    private void getUrl() {
        try {
            if (urlTextBox == null) {
                urlTextBox = new TextBox(Resource.getString
                                         (ResourceConstants.
                                          AMS_DISC_APP_WEBSITE_INSTALL),
                                         defaultInstallListUrl, 1024,
                                         TextField.ANY);
                urlTextBox.addCommand(endCmd);
                urlTextBox.addCommand(saveCmd);
                urlTextBox.addCommand(discoverCmd);
                urlTextBox.setCommandListener(this);
            }

            display.setCurrent(urlTextBox);
        } catch (Exception ex) {
            displayException(Resource.getString(ResourceConstants.EXCEPTION),
                             ex.toString());
        }
    }

    /**
     * Display an exception to the user, with a done command.
     *
     * @param title exception form's title
     * @param message exception message
     */
    private void displayException(String title, String message) {
        Alert a = new Alert(title, message, null, AlertType.ERROR);

        a.setTimeout(Alert.FOREVER);
        a.setCommandListener(this);

        display.setCurrent(a);
    }

    /** Shared key handling for the installer screens (see J2Style for the look). */
    private abstract class VitaInstallerCanvas extends Canvas {
        VitaInstallerCanvas() {
            setFullScreenMode(true);
        }

        /* Cross arrives as Fire, Circle/R as Soft2, L/Start as Soft1 */
        final boolean confirmKey(int keyCode, int action) {
            return action == FIRE && keyCode != KEY_NUM5;
        }

        final boolean backKey(int keyCode, int action) {
            return keyCode == -7 || keyCode == -6 || keyCode == KEY_NUM0 || keyCode == KEY_POUND;
        }
    }

    private final class InstallProgressCanvas extends VitaInstallerCanvas {
        private String action;
        private String name;
        private String url;
        private String status;
        private int size;
        private boolean running = true;

        InstallProgressCanvas(String a, String n, String u,
                              String s, int z) {
            action = a;
            name = n;
            url = u;
            status = s;
            size = z;
        }

        void update(String newUrl, int newSize, String newStatus) {
            if (newUrl == null) {
                url = "";
            } else if (newUrl.length() != 0) {
                url = newUrl;
            }
            size = newSize;
            status = newStatus;
            repaint();
        }

        void stopProgress() {
            running = false;
            repaint();
        }

        protected void paint(Graphics g) {
            int w = getWidth(), h = getHeight();
            int dw = 360, dh = 104, x = (w - dw) / 2;
            int y = J2Style.TOP + (h - J2Style.TOP - J2Style.SOFT_H - dh) / 2;

            J2Style.frame(g, w, h, "Install games");
            J2Style.dialog(g, x, y, dw, dh, "Please wait");
            g.setColor(J2Style.TEXT);
            g.drawString(J2Style.fit(status != null && status.length() > 0
                            ? status : "Looking for games...", dw - 32),
                    x + 16, y + J2Style.TITLE_H + 10, Graphics.LEFT | Graphics.TOP);
            J2Style.progress(g, x + 16, y + J2Style.TITLE_H + 36, dw - 32, 12, running ? 0 : 1, running ? 0 : 1);
            g.setColor(J2Style.DIM);
            g.drawString(J2Style.fit(url, dw - 32), x + 16, y + J2Style.TITLE_H + 54,
                    Graphics.LEFT | Graphics.TOP);
            J2Style.softkeys(g, w, h, J2Style.NO_GLYPH, null, J2Style.NO_GLYPH, null);
        }
    }

    private final class InstallBrowserCanvas extends VitaInstallerCanvas {
        private static final int ROW_H = 30;
        private final String sourceUrl;
        private int selectedIndex;
        private int firstVisible;
        private int pressX;
        private int pressY;

        InstallBrowserCanvas(String url, int initialIndex) {
            sourceUrl = url == null ? "" : url;
            selectedIndex = initialIndex;
            if (installList != null && selectedIndex >= installList.size()) {
                selectedIndex = installList.size() - 1;
            }
            if (selectedIndex < 0) selectedIndex = 0;
            ensureVisible();
        }

        private int listTop() {
            return J2Style.TOP + 22;
        }

        private int visible() {
            return (getHeight() - J2Style.SOFT_H - listTop() - 2) / ROW_H;
        }

        private int count() {
            return installList == null ? 0 : installList.size();
        }

        private void ensureVisible() {
            int n = visible();
            if (selectedIndex < firstVisible) firstVisible = selectedIndex;
            if (selectedIndex >= firstVisible + n) firstVisible = selectedIndex - n + 1;
            if (firstVisible > count() - n) firstVisible = count() - n;
            if (firstVisible < 0) firstVisible = 0;
        }

        private SuiteDownloadInfo currentEntry() {
            if (selectedIndex < 0 || selectedIndex >= count()) return null;
            return (SuiteDownloadInfo)installList.elementAt(selectedIndex);
        }

        private void move(int delta) {
            if (count() == 0) return;
            selectedIndex = Math.max(0, Math.min(count() - 1, selectedIndex + delta));
            ensureVisible();
            repaint();
        }

        protected void paint(Graphics g) {
            int w = getWidth(), h = getHeight();
            int n = visible(), top = listTop();
            boolean scroll = count() > n;
            int right = scroll ? w - 14 : w - 6;

            J2Style.frame(g, w, h, "Install games");
            g.setColor(J2Style.DIM);
            g.drawString(J2Style.fit(sourceUrl, w - 24), 12, J2Style.textY(J2Style.TOP, 22),
                    Graphics.LEFT | Graphics.TOP);
            J2Style.separator(g, 0, top - 1, w);

            if (count() == 0) {
                J2Style.wrap(g, "No .jar files here. Copy games to ux0:data/j2vita/games/ first.",
                        w / 2, top + 40, w - 80, 3, J2Style.TEXT, Graphics.HCENTER);
            }
            for (int r = 0; r < n && firstVisible + r < count(); r++) {
                int idx = firstVisible + r, y = top + 1 + r * ROW_H;
                SuiteDownloadInfo info = (SuiteDownloadInfo)installList.elementAt(idx);
                boolean sel = idx == selectedIndex;
                if (sel) {
                    J2Style.selection(g, 6, y + 1, right - 6, ROW_H - 2);
                } else if (idx + 1 != selectedIndex && r < n - 1 && idx < count() - 1) {
                    J2Style.separator(g, 44, y + ROW_H - 1, right - 50);
                }
                drawEntryIcon(g, info, 14, y + (ROW_H - 22) / 2, sel);
                g.setColor(sel ? J2Style.SEL_TEXT : J2Style.TEXT);
                g.drawString(J2Style.fit(info.label, right - 44 - 8), 44, J2Style.textY(y, ROW_H),
                        Graphics.LEFT | Graphics.TOP);
            }
            if (scroll) {
                int track = n * ROW_H - 4;
                int thumb = Math.max(16, track * n / count());
                int pos = top + 2 + (track - thumb) * firstVisible / (count() - n);
                g.setColor(J2Style.WELL);
                g.fillRoundRect(w - 10, top + 2, 5, track, 4, 4);
                g.setColor(J2Style.SEL_FILL);
                g.fillRoundRect(w - 10, pos, 5, thumb, 4, 4);
            }

            SuiteDownloadInfo entry = currentEntry();
            J2Style.softkeys(g, w, h, J2Style.CROSS, entry == null ? null : entry.dir ? "Open" : "Install",
                    J2Style.CIRCLE, "Back");
        }

        /** 22 px tall: a folder, or a little JAR "file" */
        private void drawEntryIcon(Graphics g, SuiteDownloadInfo info, int x, int y, boolean sel) {
            int ink = sel ? J2Style.SEL_TEXT : J2Style.DIM;
            if (info.dir) {
                g.setColor(sel ? J2Style.SEL_TEXT : J2Style.ACCENT);
                g.fillRoundRect(x, y + 2, 10, 6, 3, 3);
                g.fillRoundRect(x, y + 5, 22, 15, 4, 4);
                return;
            }
            g.setColor(sel ? J2Style.SEL_EDGE : J2Style.WELL);
            g.fillRoundRect(x + 1, y, 20, 22, 5, 5);
            g.setColor(ink);
            g.drawRoundRect(x + 1, y, 19, 21, 5, 5);
            g.drawLine(x + 6, y + 6, x + 15, y + 6);
            g.drawLine(x + 6, y + 10, x + 15, y + 10);
            g.drawLine(x + 6, y + 14, x + 12, y + 14);
        }

        protected void keyPressed(int keyCode) {
            int action = 0;
            try {
                action = getGameAction(keyCode);
            } catch (Throwable ignored) {
            }

            if (action == UP || keyCode == KEY_NUM2) {
                move(-1);
            } else if (action == DOWN || keyCode == KEY_NUM8) {
                move(1);
            } else if (action == LEFT || keyCode == KEY_NUM4) {
                move(-visible());
            } else if (action == RIGHT || keyCode == KEY_NUM6) {
                move(visible());
            } else if (confirmKey(keyCode, action)) {
                if (currentEntry() != null) activateInstallListEntry(selectedIndex);
            } else if (backKey(keyCode, action)) {
                browserBack(sourceUrl);
            }
        }

        protected void keyRepeated(int keyCode) {
            int action = 0;
            try {
                action = getGameAction(keyCode);
            } catch (Throwable ignored) {
            }
            if (action == UP || keyCode == KEY_NUM2) move(-1);
            else if (action == DOWN || keyCode == KEY_NUM8) move(1);
        }

        protected void pointerPressed(int x, int y) {
            pressX = x;
            pressY = y;
        }

        protected void pointerReleased(int x, int y) {
            int w = getWidth(), h = getHeight();
            if (Math.abs(x - pressX) > 10 || Math.abs(y - pressY) > 10) return;
            if (y >= h - J2Style.SOFT_H) {
                if (x >= w / 2) browserBack(sourceUrl);
                else if (currentEntry() != null) activateInstallListEntry(selectedIndex);
                return;
            }
            int top = listTop() + 1;
            if (y < top) return;
            int r = (y - top) / ROW_H, idx = firstVisible + r;
            if (r >= visible() || idx >= count()) return;
            if (idx == selectedIndex) {
                activateInstallListEntry(idx);
            } else {
                selectedIndex = idx;
                repaint();
            }
        }
    }

    /** A class to get the install list in a background thread. */
    private class BackgroundInstallListGetter implements Runnable {
        /** Parent application. */
        private DiscoveryApp parent;
        /** URL of the list. */
        private String url;

        /**
         * Construct a BackgroundInstallListGetter.
         *
         * @param theParent parent of this object
         * @param theUrl where to get the list of suites to install.
         */
        private BackgroundInstallListGetter(DiscoveryApp theParent,
                                            String theUrl) {
            parent = theParent;
            url = theUrl;
        }

        /**
         * Get the list of suites for the user to install.
         * The suites that are listed
         * are the links on a web page that end with .jad.
         */
        public void run() {
            StreamConnection conn = null;
            InputStreamReader in = null;
            String errorMessage = null;
            long startTime = System.currentTimeMillis();

            try {
                parent.displayProgressForm(
                        Resource.getString(
                        ResourceConstants.AMS_DISC_APP_GET_INSTALL_LIST),
                        "", url, 0,
                        Resource.getString(
                        ResourceConstants.AMS_GRA_INTLR_CONN_GAUGE_LABEL));

                if (!url.startsWith("http://") &&
                        !url.startsWith("https://")) {
                    http_install = false;
                }

                if (http_install) {
                    conn = (StreamConnection)Connector.open(url, Connector.READ);
                    in = new InputStreamReader(conn.openInputStream());
                }

                try {
                    parent.updateProgressForm(
                            "", 0,
                            Resource.getString(
                            ResourceConstants.AMS_DISC_APP_GAUGE_LABEL_DOWNLOAD));

                    if (http_install) {
                        parent.installList =
                                SuiteDownloadInfo.getDownloadInfoFromPage(in);
                    } else {
                        while (true) {
                            parent.installList =
                                    SuiteDownloadInfo.getDownloadInfoFromDir(url);

                            if (parent.installList.size() <= 0 &&
                                    !url.equals(LOCAL_INSTALL_ROOT)) {
                                url = LOCAL_INSTALL_ROOT;
                            } else {
                                break;
                            }
                        }
                    }

                    if (parent.installList.size() > 0) {
                        int initialIndex = 0;

                        for (int i = 0; i < parent.installList.size(); i++) {
                            SuiteDownloadInfo suite =
                                    (SuiteDownloadInfo)parent.installList.elementAt(i);

                            if (suite.http) {
                                if (!suite.url.startsWith("http://") &&
                                        !suite.url.startsWith("https://")) {
                                    int lastSlash = url.lastIndexOf('/');
                                    suite.url = url.substring(0, lastSlash + 1) +
                                            suite.url;
                                }
                            } else if (lastInstallMidletName != null &&
                                    lastInstallMidletName.equals(suite.label)) {
                                initialIndex = i;
                            }
                        }

                        while (System.currentTimeMillis() -
                                parent.lastDisplayChange <
                                GraphicalInstaller.ALERT_TIMEOUT);

                        parent.installListBox =
                                new InstallBrowserCanvas(url, initialIndex);
                        parent.display.setCurrent(parent.installListBox);
                        return;
                    }

                    errorMessage = Resource.getString(
                            ResourceConstants.AMS_DISC_APP_CHECK_URL_MSG);
                } catch (IllegalArgumentException ex) {
                    errorMessage = Resource.getString(
                            ResourceConstants.AMS_DISC_APP_URL_FORMAT_MSG);
                } catch (Exception ex) {
                    errorMessage = ex.getMessage();
                }
            } catch (Exception ex) {
                errorMessage = Resource.getString(
                        ResourceConstants.AMS_DISC_APP_CONN_FAILED_MSG);
            } finally {
                if (parent.progressForm != null) {
                    parent.progressForm.stopProgress();
                }

                try {
                    if (conn != null) conn.close();
                    if (in != null) in.close();
                } catch (Exception e) {
                    if (Logging.REPORT_LEVEL <= Logging.WARNING) {
                        Logging.report(Logging.WARNING,
                                LogChannels.LC_AMS,
                                "close threw an Exception");
                    }
                }
            }

            Alert a = new Alert(
                    Resource.getString(ResourceConstants.ERROR),
                    errorMessage, null, AlertType.ERROR);
            a.setTimeout(Alert.FOREVER);

            if (parent.urlTextBox != null) {
                parent.display.setCurrent(a, parent.urlTextBox);
            } else {
                notifyDestroyed();
            }
        }
    }

    class FileEntryItem extends CustomItem {
    	private boolean hasFocus;
    	private boolean dir;
    	private String text;
    	private DiscoveryApp parent;
    	private final Font ENTRY_FONT;
    	
       FileEntryItem(String str, boolean isDir, DiscoveryApp theParent) {
       	super(null);
       	text = str;
       	dir = isDir;
       	hasFocus = false;
       	parent = theParent;
       	ENTRY_FONT = Font.getFont(Font.FACE_SYSTEM,
                                                         Font.STYLE_BOLD,
                                                         Font.SIZE_SMALL);
       }
    	
       /**
         * Gets the minimum width of a midlet representation in
         * the App Selector Screen.
         * @return the minimum width of a midlet representation
         *         in the App Selector Screen.
         */
        protected int getMinContentWidth() {
            return parent.installListBox.getWidth();
        }

        /**
         * Gets the minimum height of a midlet representation in
         * the App Selector Screen.
         * @return the minimum height of a midlet representation
         *         in the App Selector Screen.
         */
        protected int getMinContentHeight() {
            return ENTRY_FONT.getHeight();
        }

        /**
         * Gets the preferred width of a midlet representation in
         * the App Selector Screen based on the passed in height.
         * @param height the amount of height available for this Item
         * @return the minimum width of a midlet representation
         *         in the App Selector Screen.
         */
        protected int getPrefContentWidth(int height) {
            return parent.installListBox.getWidth();
        }

        /**
         * Gets the preferred height of a midlet representation in
         * the App Selector Screen based on the passed in width.
         * @param width the amount of width available for this Item
         * @return the minimum height of a midlet representation
         *         in the App Selector Screen.
         */
        protected int getPrefContentHeight(int width) {
            return ENTRY_FONT.getHeight();
        }

        /**
         * Paints the content of a midlet representation in
         * the App Selector Screen.
         * Note that icon representing that foreground was requested
         * is painted on to of the existing ickon.
         * @param g The graphics context where painting should be done
         * @param w The width available to this Item
         * @param h The height available to this Item
         */
        protected void paint(Graphics g, int w, int h) {
             g.setFont(ENTRY_FONT);
             if (dir) {
                 g.setColor(0, 0, 128);
             } else {
                 g.setColor(0, 0, 0);
             }
             g.drawString(text, 5, (h - ENTRY_FONT.getHeight())/2,
                            Graphics.LEFT | Graphics.TOP);           
        }

        /**
         * Handles traversal.
         * @param dir The direction of traversal (Canvas.UP, Canvas.DOWN,
         *            Canvas.LEFT, Canvas.RIGHT)
         * @param viewportWidth The width of the viewport in the AppSelector
         * @param viewportHeight The height of the viewport in the AppSelector
         * @param visRect_inout The return array that tells AppSelector
         *        which portion of the MidletCustomItem has to be made visible
         * @return true if traversal was handled in this method
         *         (this MidletCustomItem just got focus or there was an
         *         internal traversal), otherwise false - to transfer focus
         *         to the next item
         */
        protected boolean traverse(int dir,
                                   int viewportWidth, int viewportHeight,
                                   int visRect_inout[]) {
            if (!hasFocus) {
                hasFocus = true;
            }

            return false;
        }

        /**
         * Handles traversal out. This method is called when this
         * MidletCustomItem looses focus.
         */
        protected void traverseOut() {
            hasFocus = false;
        }

        boolean hasFocus() {
        	return hasFocus;
        }
       
    }
}
