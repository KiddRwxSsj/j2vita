package javax.microedition.sensor;

import java.io.IOException;

import javax.microedition.io.Connection;

public interface SensorConnection extends Connection {
    int STATE_CLOSED = 4;
    int STATE_LISTENING = 2;
    int STATE_OPENED = 1;

    Channel getChannel(ChannelInfo channelInfo);
    Data[] getData(int bufferSize) throws IOException;
    Data[] getData(int bufferSize, long bufferingPeriod, boolean isTimestampIncluded,
            boolean isUncertaintyIncluded, boolean isValidityIncluded) throws IOException;
    SensorInfo getSensorInfo();
    int getState();
    void removeDataListener();
    void setDataListener(DataListener listener, int bufferSize);
    void setDataListener(DataListener listener, int bufferSize, long bufferingPeriod,
            boolean isTimestampIncluded, boolean isUncertaintyIncluded, boolean isValidityIncluded);
}
