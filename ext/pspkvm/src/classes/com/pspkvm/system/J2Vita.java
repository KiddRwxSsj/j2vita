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

	/** Colour slot of the current theme as 0xRRGGBB (slots listed in J2Style). */
	public static native int themeColor(int slot);

	/** Changes every time the player picks another theme. */
	public static native int themeSerial();

	/** Close J2Vita and go back to the LiveArea. */
	public static native void quit();
}
