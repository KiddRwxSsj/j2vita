package javax.microedition.sensor;

public interface Channel {
    ChannelInfo getChannelInfo();
    String getChannelUrl();
    void removeAllConditions();
}
