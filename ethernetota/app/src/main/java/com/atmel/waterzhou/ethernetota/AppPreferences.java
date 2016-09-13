package com.atmel.waterzhou.ethernetota;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

/**
 * Created by water.zhou on 12/4/2014.
 */
public final class AppPreferences {
    private static final String TAG = "SharedPreference";
    /** Preference keys */
    public static final String KEY_IP_ADDRESS  = "GatewayIp";
    public static final String KEY_MAC_ADDRESS  = "GatewayMac";

    /** Preference members */
    private SharedPreferences mAppSharedPrefs;
    private SharedPreferences.Editor mAppPrefsEditor;

    public AppPreferences(Context context) {
        mAppSharedPrefs = context.getSharedPreferences("SmartConnectPreferences", 0);
        mAppPrefsEditor = mAppSharedPrefs.edit();
    }

    public void setIpAddress(String ipAddress) {
        Log.d(TAG, "Changing IP Address to " + ipAddress);
        mAppPrefsEditor.putString(KEY_IP_ADDRESS, ipAddress);
        mAppPrefsEditor.commit();
    }
    public void setMacAddress(String MacAddress) {
        Log.d(TAG, "Changing MAC Address to " + MacAddress);
        mAppPrefsEditor.putString(KEY_MAC_ADDRESS, MacAddress);
        mAppPrefsEditor.commit();
    }

    public String getIpAddress() {
        return mAppSharedPrefs.getString(KEY_IP_ADDRESS, null);
    }
    public String getMacAddress() {
        return mAppSharedPrefs.getString(KEY_MAC_ADDRESS, null);
    }

}
