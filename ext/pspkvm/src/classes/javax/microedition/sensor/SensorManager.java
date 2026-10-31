package javax.microedition.sensor;

/* no JSR-256 sensors on J2Vita, games fall back to keys */
public final class SensorManager {
    private SensorManager() {}

    public static SensorInfo[] findSensors(String quantity, String contextType) {
        if (contextType != null && !contextType.equals(SensorInfo.CONTEXT_TYPE_AMBIENT)
                && !contextType.equals(SensorInfo.CONTEXT_TYPE_DEVICE)
                && !contextType.equals(SensorInfo.CONTEXT_TYPE_USER)
                && !contextType.equals(SensorInfo.CONTEXT_TYPE_VEHICLE)) {
            throw new IllegalArgumentException();
        }
        return new SensorInfo[0];
    }

    public static SensorInfo[] findSensors(String url) {
        if (url == null) throw new NullPointerException();
        return new SensorInfo[0];
    }

    public static void addSensorListener(SensorListener listener, SensorInfo info) {
        if (listener == null || info == null) throw new NullPointerException();
    }

    public static void addSensorListener(SensorListener listener, String quantity) {
        if (listener == null || quantity == null) throw new NullPointerException();
    }

    public static void removeSensorListener(SensorListener listener) {
        if (listener == null) throw new NullPointerException();
    }
}
