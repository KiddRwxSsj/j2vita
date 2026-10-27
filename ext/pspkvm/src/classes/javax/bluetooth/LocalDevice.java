package javax.bluetooth;

import javax.microedition.io.Connection;

/* games probe Bluetooth for multiplayer; report it as switched off */
public class LocalDevice {
    private LocalDevice() {}

    public static LocalDevice getLocalDevice() throws BluetoothStateException {
        throw new BluetoothStateException("Bluetooth not available");
    }
    public static boolean isPowerOn() { return false; }
    public static String getProperty(String property) { return null; }

    public DiscoveryAgent getDiscoveryAgent() { return null; }
    public String getFriendlyName() { return null; }
    public DeviceClass getDeviceClass() { return null; }
    public boolean setDiscoverable(int mode) throws BluetoothStateException {
        throw new BluetoothStateException("Bluetooth not available");
    }
    public int getDiscoverable() { return DiscoveryAgent.NOT_DISCOVERABLE; }
    public String getBluetoothAddress() { return null; }
    public ServiceRecord getRecord(Connection notifier) { throw new IllegalArgumentException(); }
    public void updateRecord(ServiceRecord srvRecord) throws ServiceRegistrationException {
        throw new ServiceRegistrationException("Bluetooth not available");
    }
}
