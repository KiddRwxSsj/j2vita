package com.nokia.mid.ui;

public class DeviceControl
{
    
    public static void flashLights(long duration)
    {
        // PS Vita has no equivalent notification light. Intentionally a no-op.
    }
    
    public static void setLights(int num,int level)
    {
        // Games can call this every frame. Never log from this hot path.
    }
    
    public static void startVibra(int freq, long duration) 
    {
        // PS Vita handheld has no vibration motor. Intentionally a no-op.
    }
    
    public static void stopVibra()  
    {
        // No-op; see startVibra().
    }
    
}
