package com.pspkvm.system;

/** Bridge to the J2Vita native frontend (per-game display/controls config). */
public class J2Vita {
	/**
	 * Tell the frontend which game is going to the foreground (null for the
	 * launcher) and get back the device profile it should run with. hint is
	 * free text (manifest size attributes, JAR name) used to guess the screen
	 * size the first time a game runs.
	 */
	public static native int beginGame(String name, String hint);

	/** Open the frontend's global settings menu (screen, filter, controls...). */
	public static native void openSettings();

	/** The game drew a full screen rectangle of this size, bigger than the screen. */
	public static native void screenHint(int w, int h);

	/** Colour slot of the current theme as 0xRRGGBB (slots listed in J2Style). */
	public static native int themeColor(int slot);

	/** Changes every time the player picks another theme. */
	public static native int themeSerial();

	/** Close J2Vita and go back to the LiveArea. */
	public static native void quit();

	public static final int TEXT_IDLE = 0, TEXT_BUSY = 1, TEXT_OK = 2, TEXT_CANCEL = 3;

	/** Open the Vita keyboard; poll textState() until it is no longer TEXT_BUSY. */
	public static native void askText(String title, String initial);

	public static native int textState();

	/** What was typed, valid once textState() is TEXT_OK. */
	public static native String textResult();

	public static native boolean makeDir(String path);

	/** Removes an empty folder. */
	public static native boolean removeDir(String path);

	/** Renames a file or folder; false when the target already exists. */
	public static native boolean move(String from, String to);

	/** n big endian ints of src from byte off into dst. */
	public static native void unpackInts(byte[] src, int off, int[] dst, int n);

	/** Decodes len UTF-8 bytes of src from off, null on bad arguments. */
	public static native String utf8(byte[] src, int off, int len);
}
