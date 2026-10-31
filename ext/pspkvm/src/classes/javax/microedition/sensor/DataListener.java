package javax.microedition.sensor;

public interface DataListener {
    void dataReceived(SensorConnection sensor, Data[] data, boolean isDataLost);
}
