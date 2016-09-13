package com.atmel.waterzhou.ethernetota;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v4.widget.SwipeRefreshLayout;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ListView;

import java.net.DatagramPacket;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;


public class OtaActivity extends Activity {
    private static final String TAG = "Wifi";
    private static final int AddNode = 1001;
    private static final int START_SEARCH = 1003;
    private static final int DELAY = 1000;
    private int mCount = 0;
    private int mMaxTime = 15;
    private ListView listView_Node;
    private Menu optionsMenu;

    private ItemListBaseAdapter mAdapter;
    ArrayList<ItemDetails> image_details = new ArrayList<ItemDetails>();
    private Handler mHandler;
    private String dataSource = null;
    private udpbroadcast udpBroadcast = null;
    private SwipeRefreshLayout mSwipeRefreshLayout;
    private static AppPreferences mAppPreferences;

    private void startSearch() {
        mHandler.sendEmptyMessageDelayed(START_SEARCH, DELAY);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_ota);
        mAppPreferences = new AppPreferences(getApplicationContext());
        setTitle("Ethernet-OTA:try scanning...");
        mSwipeRefreshLayout = (SwipeRefreshLayout) findViewById(R.id.swipe);
        mSwipeRefreshLayout.setColorScheme(android.R.color.holo_red_light, android.R.color.holo_blue_light, android.R.color.holo_green_light, android.R.color.holo_green_light);
        mSwipeRefreshLayout.setOnRefreshListener(new SwipeRefreshLayout.OnRefreshListener() {
            @Override
            public void onRefresh() {
                mSwipeRefreshLayout.setRefreshing(true);
                Log.d(TAG, "Refreshing Number");
                (new Handler()).postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        mSwipeRefreshLayout.setRefreshing(false);
                        image_details.clear();
                        mCount = 0;
                        mAdapter.notifyDataSetChanged();
                    }
                }, 2000);
            }
        });

        listView_Node = (ListView) findViewById(R.id.listView_Node);
         /*1. add listview adapter*/
        mAdapter = new ItemListBaseAdapter(this, image_details);
        listView_Node.setAdapter(mAdapter);
        listView_Node.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> a, View v, int position, long id) {
                String ipAddress = null;
                Object temp = listView_Node.getItemAtPosition(position);
                ItemDetails obj_itemDetails = (ItemDetails) temp;
                mHandler.removeMessages(START_SEARCH);
                udpBroadcast.close();
                ipAddress = obj_itemDetails.getItemDescription();
                Intent intent = new Intent(OtaActivity.this, UpdateGateway.class);
                intent.putExtra("ipaddress", ipAddress);
                intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
                startActivity(intent);
            }
        });

        /*2. add a handler*/
        mHandler = new Handler() {
            public void handleMessage(android.os.Message msg) {
                switch (msg.what) {
                    case AddNode: {
                        setTitle("Ethernet-OTA");
                        mAdapter.notifyDataSetChanged();
                        break;
                    }
                    case START_SEARCH: {
                        if( mCount++ > mMaxTime && image_details.isEmpty()) {
                                mCount = 0;
                                mMaxTime = 200;
                                //udpBroadcast.close();
                                mHandler.removeMessages(START_SEARCH);
                                new AlertDialog.Builder(OtaActivity.this)
                                        .setMessage("Please check network, then try again")
                                        .setPositiveButton("Ok",new DialogInterface.OnClickListener() {
                                            public void onClick(DialogInterface dialog, int id) {
                                                // if this button is clicked, close
                                                // current activity
                                                System.exit(0);
                                            }
                                        })
                                        .show();

                        }

                        if (udpBroadcast.isClosed()) {
                            udpBroadcast.open();
                            udpBroadcast.setPort(48899);
                        }
                        udpBroadcast.send("HF-A11ASSISTHREAD");
                        udpBroadcast.receive();
                        mHandler.sendEmptyMessageDelayed(START_SEARCH, DELAY);
                        break;
                    }
                }

            }
        };

        /*3. Create one udpbroadcast object*/
        udpBroadcast = new udpbroadcast() {

            public boolean isIP(String str) {
                Pattern pattern = Pattern.compile("\\b((?!\\d\\d\\d)\\d+|1\\d\\d|2[0-4]\\d|25[0-5])" +
                        "\\.((?!\\d\\d\\d)\\d+|1\\d\\d|2[0-4]\\d|25[0-5])\\." +
                        "((?!\\d\\d\\d)\\d+|1\\d\\d|2[0-4]\\d|25[0-5])\\." +
                        "((?!\\d\\d\\d)\\d+|1\\d\\d|2[0-4]\\d|25[0-5])\\b");
                return pattern.matcher(str).matches();
            }

            public boolean isMAC(String str) {

                str = str.trim();
                if (str.length() != 12) {
                    return false;
                }

                char[] chars = new char[12];
                str.getChars(0, 12, chars, 0);
                for (int i = 0; i < chars.length; i++) {
                    if (!((chars[i] >= '0' && chars[i] <= '9') || (chars[i] >= 'A' && chars[i] <= 'F') || (chars[i] >= 'a' && chars[i] <= 'f'))) {
                        return false;
                    }
                }
                return true;
            }

            private void parseReceiveData(String response) {
                if (response == null) {
                    return;
                }
                String[] array = response.split(",");
                if (array == null || (array.length < 2 && array.length > 3) ||
                        !isIP(array[0]) || !isMAC(array[1])) {
                    return;
                }
                if (mAppPreferences != null) {
                    mAppPreferences.setIpAddress(array[0]);
                    mAppPreferences.setMacAddress(array[1]);
                }
                dataSource = array[2];
            }

            private boolean IsExsits(String Name) {
                for (ItemDetails tmp : image_details) {
                    Log.d(TAG, "list node is " + Name);
                    if (tmp.getName().equals(Name))
                        return true;
                }
                return false;
            }

            @Override
            public void onReceived(List<DatagramPacket> packets) {

                for (DatagramPacket packet : packets) {

                    String data = new String(packet.getData(), 0, packet.getLength());
                    parseReceiveData(data);
                    if (IsExsits(dataSource) == false) {
                        ItemDetails item_details = new ItemDetails();
                        item_details.setName(dataSource);
                        item_details.setItemDescription(mAppPreferences.getIpAddress());
                        item_details.setMac(mAppPreferences.getMacAddress());
                        item_details.setImageNumber(1);
                        image_details.add(item_details);
                        Message msg = mHandler.obtainMessage(AddNode);
                        mHandler.sendMessage(msg);
                    }
                    /*Parse ssid*/

                }
            }
        };
        /*4. Start auto update search*/
        startSearch();
    }


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.ota, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        if (id == R.id.action_settings) {
            System.exit(0);
            return true;
        } else if (id == R.id.action_fresh) {
            startSearch();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onStop() {
        udpBroadcast.close();
        mHandler.removeMessages(START_SEARCH);
        super.onStop();
    }

    @Override
    protected void onResume() {
        startSearch();
        super.onResume();
    }
}
