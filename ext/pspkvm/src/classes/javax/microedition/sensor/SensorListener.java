package javax.microedition.sensor;

public interface SensorListener {
    void sensorAvailable(SensorInfo info);
    void sensorUnavailable(SensorInfo info);
}
