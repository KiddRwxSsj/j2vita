package javax.microedition.sensor;

public interface ChannelInfo {
    int TYPE_DOUBLE = 1;
    int TYPE_INT = 2;
    int TYPE_OBJECT = 4;

    float getAccuracy();
    int getDataType();
    MeasurementRange[] getMeasurementRanges();
    String getName();
    int getScale();
    Unit getUnit();
}
