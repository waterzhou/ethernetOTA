package com.atmel.waterzhou.ethernetota;

import android.util.Log;

import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketTimeoutException;

/**
 * Created by water.zhou on 12/5/2014.
 */
public class TCPClient {
    private static final String TAG = "TCPClient:";
    //private char[] serverMessage = new char[1024];
    private byte[] serverMessage = new byte[1024];
    public static String SERVERIP;
    public static int SERVERPORT;
    private OnMessageReceived mMessageListener = null;
    private boolean mRun = false;
    private Socket socket;
    //OutputStream out;
    DataOutputStream out;
    InputStream in;
    private int timeout = 10000;

    /**
     * Constructor of the class. OnMessagedReceived listens for the messages received from server
     */
    public TCPClient(OnMessageReceived listener, String IP, int port) {
        mMessageListener = listener;
        SERVERIP = IP;
        SERVERPORT = port;
    }
/*
    public void sendBytes(byte[] myByteArray) throws IOException {
        try {
            if (socket != null) {
                out.write(myByteArray);
                out.flush();
            } else {
                conn();
            }

        } catch (Exception e) {
            Log.e(TAG, "send error");
            e.printStackTrace();
        } finally {
            Log.d(TAG, "send successfully");
        }
    }*/

    public void sendBytes(byte[] myByteArray) throws IOException {
        sendBytes(myByteArray, 0, myByteArray.length);
    }

    public void sendBytes(byte[] myByteArray, int start, int len) throws IOException {
        if (len < 0)
            throw new IllegalArgumentException("Negative length not allowed");
        if (start < 0 || start >= myByteArray.length)
            throw new IndexOutOfBoundsException("Out of bounds: " + start);
        // Other checks if needed.

        // May be better to save the streams in the support class;
        // just like the socket variable.
        out.writeInt(len);
        if (len > 0) {
            out.write(myByteArray, start, len);
        }
    }


    public void stopClient() {
        if (!mRun)
            return;
        try {
            socket.close();
            socket = null;
        }catch (Exception e) {
            Log.e(TAG, "socket close error " + e);
        }
        mRun = false;
    }
    public void startClient() {
        if(mRun)
            return;
        conn();
    }

    public void conn() {
        Log.d(TAG, "Thread Connecting..." + SERVERIP);
        try {
            InetAddress serverAddr = InetAddress.getByName(SERVERIP);
            socket = new Socket(serverAddr, SERVERPORT);
            socket.setSoTimeout(timeout);
            //out = socket.getOutputStream();
            out = new DataOutputStream(socket.getOutputStream());
            in = socket.getInputStream();
        } catch (Exception e) {
            Log.e(TAG, "C: Error", e);
        }
        mRun = true;
    }
    public void run() {
        conn();
        while (mRun) {
            try {
                if (socket != null) {
                    int size = 0;
                    while ((size = in.read(serverMessage)) > 0) {
                        byte[] res = new byte[size];
                        System.arraycopy(serverMessage, 0, res, 0, size);
                        if (mMessageListener != null) {
                            //call the method messageReceived from MyActivity class
                            mMessageListener.messageReceived(res);
                        }
                    }
                } else {
                    conn();
                }
            } catch (SocketTimeoutException e) {

                Log.e(TAG, "Read: timeout", e);
                if (mMessageListener != null) {
                    //call the method messageReceived from MyActivity class
                    byte[] res1 = {0x74, 0x69, 0x6D, 0x65, 0x6f, 0x75, 0x74};
                    mMessageListener.messageReceived(res1);
                }

            } catch (IOException e) {
                Log.e(TAG, "Read: IO exception", e);

            }finally {
                //the socket must be closed. It is not possible to reconnect to this socket
                // after it is closed, which means a new socket instance has to be created.
               /* try {
                    socket.close();
                    socket = null;
                }catch (Exception e) {
                    Log.e(TAG, "socket close error " + e);
                }*/
            }

        }
    }

    //Declare the interface. The method messageReceived(String message) will must be implemented in the MyActivity
    //class at on asynckTask doInBackground
    public interface OnMessageReceived {
        public void messageReceived(byte[] message);
    }
}
