package javax.bluetooth;

public class UUID {
    private static final String BASE = "00001000800000805F9B34FB";
    private final String hex; /* 32 upper-case hex digits */

    public UUID(long uuidValue) {
        if (uuidValue < 0 || uuidValue > 0xFFFFFFFFL) throw new IllegalArgumentException();
        hex = pad(Long.toString(uuidValue, 16), 8) + BASE;
    }

    public UUID(String uuidValue, boolean shortUUID) {
        if (uuidValue == null) throw new NullPointerException();
        int n = uuidValue.length();
        if (n == 0 || (shortUUID ? n > 8 : n > 32)) throw new IllegalArgumentException();
        for (int i = 0; i < n; i++) {
            if (Character.digit(uuidValue.charAt(i), 16) < 0) throw new NumberFormatException();
        }
        hex = shortUUID ? pad(uuidValue, 8) + BASE : pad(uuidValue, 32);
    }

    private static String pad(String s, int len) {
        StringBuffer b = new StringBuffer();
        for (int i = s.length(); i < len; i++) b.append('0');
        return b.append(s).toString().toUpperCase();
    }

    public String toString() {
        int i = 0;
        while (i < 31 && hex.charAt(i) == '0') i++;
        return hex.substring(i);
    }
    public boolean equals(Object value) {
        return value instanceof UUID && ((UUID)value).hex.equals(hex);
    }
    public int hashCode() { return hex.hashCode(); }
}
