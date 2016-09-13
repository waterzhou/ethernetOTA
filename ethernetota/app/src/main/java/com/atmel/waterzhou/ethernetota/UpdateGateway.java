package com.atmel.waterzhou.ethernetota;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.text.InputType;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;
import java.util.Arrays;
import java.util.zip.CRC32;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.CipherOutputStream;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.SecretKey;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;


/**
 * Created by water.zhou on 12/4/2014.
 */
public class UpdateGateway extends Activity {

    private static final String TAG = "UpdateGateway";

    private EditText mPath;
    private TextView mTextVersion;
    private Button mButtonUpdate;
    private ImageView mButtonCheck;
    private static final int PICKFILE_RESULT_CODE = 1;
    private String dataSource;
    private TCPClient mTcpClient;
    private Integer SERVERPORT = 8899;
    byte crypto_key[] = {(byte) 0xfe, (byte) 0xff, (byte) 0xe9, (byte) 0x92,
            (byte) 0x86, (byte) 0x65, (byte) 0x73, (byte) 0x1c,
            (byte) 0x6d, (byte) 0x6a, (byte) 0x8f, (byte) 0x94,
            (byte) 0x67, (byte) 0x30, (byte) 0x83, (byte) 0x08};
    byte crypto_iv[] = {(byte) 0xfe, (byte) 0xff, (byte) 0xe9, (byte) 0x92,
            (byte) 0x86, (byte) 0x65, (byte) 0x73, (byte) 0x1c,
            (byte) 0x6d, (byte) 0x6a, (byte) 0x8f, (byte) 0x94,
            (byte) 0x67, (byte) 0x30, (byte) 0x83, (byte) 0x08};
    private byte crypto_aad[] = {
            (byte) 0xab, (byte) 0x6e, (byte) 0x47, (byte) 0xd4, (byte) 0x2c, (byte) 0xec, (byte) 0x13, (byte) 0xbd,
            (byte) 0xf5, (byte) 0x3a, (byte) 0x67, (byte) 0xb2, (byte) 0x12, (byte) 0x57, (byte) 0xbd, (byte) 0xdf,
            (byte) 0x58, (byte) 0xe2, (byte) 0xfc, (byte) 0xce, (byte) 0xfa, (byte) 0x7e, (byte) 0x30, (byte) 0x61,
            (byte) 0x36, (byte) 0x7f, (byte) 0x1d, (byte) 0x57, (byte) 0xa4, (byte) 0xe7, (byte) 0x45, (byte) 0x5a};

    private static final SecureRandom secureRandom = new SecureRandom();
    private Handler mHandler;
    private static final int SENDDATAFRAME = 1003;
    private static final int TRANSFER2NEWFIRMWARE = 1004;
    private static final int UPDATEOVER = 1005;
    private static final int CONTINUEDATA = 1006;
    private static final int SENDGETVERSION = 1007;
    private static final int DISPLAYVERSION = 1008;
    private static final int CONNECTIONLOST = 1009;


    private byte[] randomIv;
    private CRC32 CRC = new CRC32();
    private short FrameSequence = 0;
    private boolean bDataSend = false;
    private boolean bCommandSend = false;
    private boolean bCommandGetverison = false;
    private boolean bDataAckError = false;
    private boolean bFileReadOver = false;
    private boolean bUpdate = false;
    private boolean bChooseFile = false;
    private boolean bFileFormat = false;
    private enum CommandType {GetVersion, GetCRC, ExeFirmware};
    private long mFileCRC;
    private ByteArrayOutputStream baos;
    private FileInputStream fis;
    private String version;
    private ProgressDialog pd_config_refresh;
    private long mFilesize;
    private connectTask socketTask = null;
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_updategateway);
        Bundle bundle = this.getIntent().getExtras();
        dataSource = bundle.getString("ipaddress");
        setTitle(dataSource);
        mTextVersion = (TextView)findViewById(R.id.textView_version);
        mPath = (EditText) findViewById(R.id.path_edit);
        mButtonUpdate = (Button) findViewById(R.id.button_update);
        mButtonUpdate.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                if (bFileFormat) {
                    bChooseFile = false;
                    // Perform action on click
                    try {
                        bUpdate = true;
                        sendAuthenticationFrame();
                    } catch (Exception e) {
                        Log.e(TAG, "sendAuthenticationFrame Error", e);
                    }
                }
            }
        });
        //mButtonCheck = (Button) findViewById(R.id.button_check);
        mButtonCheck= (ImageView) findViewById(R.id.imageView_check);
        mButtonCheck.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                    bChooseFile = false;
                    try {
                        bUpdate = false;
                        sendAuthenticationFrame();
                    } catch (Exception e) {
                        Log.e(TAG, "sendAuthenticationFrame Error", e);
                    }
                }
        });
        mPath.setInputType(InputType.TYPE_NULL);
        mPath.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showMyDialog();
            }
        });
        mPath.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (hasFocus) {
                    //showMyDialog();
                }
            }
        });

        socketTask = new connectTask();
        socketTask.execute("gateway".getBytes());
        mHandler = new Handler() {
            public void handleMessage(android.os.Message msg) {
                switch (msg.what) {
                    case SENDDATAFRAME: {
                        File file = null;
                        try {
                            file = new File(mPath.getText().toString());
                        } catch (Exception e) {
                            Log.e(TAG, "New file error:" + e);
                            return;
                        }
                        if (!file.exists() || !file.isFile() || !file.canRead()) {
                            Toast.makeText(UpdateGateway.this, "File access error", Toast.LENGTH_SHORT).show();
                            return;
                        }

                        try {
                            fis = new FileInputStream(file);
                            baos = new ByteArrayOutputStream();
                            mFilesize = fis.available();
                            if (mFilesize == 0)
                            {
                                new AlertDialog.Builder(UpdateGateway.this)
                                        .setMessage("No file is chosen")
                                        .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
                                            public void onClick(DialogInterface dialog,int id) {
                                                // if this button is clicked, close
                                                // current activity
                                                UpdateGateway.this.finish();
                                            }
                                        })
                                        .show();
                            }
                            if (mFilesize % 1024 == 0) {
                                mFilesize = mFilesize/1024;
                            } else {
                                mFilesize = mFilesize/1024 + 1;
                            }
                            int count;
                            byte buffer[] = new byte[1024];
                            if ((count = fis.read(buffer)) > 0 && !bDataAckError && !bFileReadOver) {
                                //If count not to 1024, pad it using 0xff
                                if (count < 1024){
                                    for (int i = count; i < 1024; i++) {
                                        buffer[i] = (byte)0xff;
                                        bFileReadOver = true;
                                    }
                                }
                                try {
                                    byte[] dataFrame = generate_data_frame(buffer,FrameSequence++);
                                    mTcpClient.sendBytes(dataFrame);
                                } catch (Exception e) {
                                    Log.e(TAG, "Generate data frame error");
                                }
                                baos.write(buffer, 0, 1024);
                            }

                        } catch (Exception e) {
                            Log.e(TAG, "File read error:" + e);
                        }
                        bDataSend = true;
                        break;
                    }
                    case SENDGETVERSION:{
                        Log.d(TAG, "Send command get version");
                        // Send get version command
                        try {
                            mTcpClient.sendBytes(generate_command_frame(CommandType.GetVersion));
                            bCommandGetverison = true;
                        }catch (Exception e) {
                            Log.e(TAG, "Send command getversion error");
                        }

                        break;
                    }
                    case CONTINUEDATA: {
                        showProcessDialog((int)(FrameSequence*100/mFilesize));
                        if (bFileReadOver) {
                            CRC.reset();
                            CRC.update(baos.toByteArray());
                            mFileCRC = CRC.getValue();
                            if (bDataAckError)
                                return;
                            try {
                            fis.close();
                            baos.flush();
                            baos.close();
                            }catch (Exception e) {
                                Log.e(TAG, "Close baos error");
                            }
                            bDataSend = false;
                            Log.d(TAG, "Send command CRC");
                            // Send get CRC command
                            try {
                                mTcpClient.sendBytes(generate_command_frame(CommandType.GetCRC));
                                bCommandSend = true;
                            }catch (Exception e) {
                                Log.e(TAG, "Send command CRC error");
                            }
                            break;
                        } else {
                            try {
                                int count;
                                byte buffer[] = new byte[1024];
                                if ((count = fis.read(buffer)) > 0 && !bDataAckError && !bFileReadOver) {
                                    //If count not to 1024, pad it using 0xff
                                    if (count < 1024){
                                        for (int i = count; i < 1024; i++) {
                                            buffer[i] = (byte)0xff;
                                            bFileReadOver = true;
                                        }
                                    }
                                    try {
                                        byte[] dataFrame = generate_data_frame(buffer,FrameSequence++);
                                        mTcpClient.sendBytes(dataFrame);
                                    } catch (Exception e) {
                                        Log.e(TAG, "Generate data frame error");
                                    }
                                    baos.write(buffer, 0, 1024);
                                }

                            } catch (Exception e) {
                                Log.e(TAG, "File read error:" + e);
                            }

                        }
                        break;
                    }
                    case TRANSFER2NEWFIRMWARE:{
                        // Send ExeFirmware command
                        dismissProcessDialog();
                        try {
                            mTcpClient.sendBytes(generate_command_frame(CommandType.ExeFirmware));
                            bCommandSend = true;
                        }catch (Exception e) {
                            Log.e(TAG, "Send command exec error");
                        }
                        break;
                    }

                    case UPDATEOVER:{
                        new AlertDialog.Builder(UpdateGateway.this)
                                .setMessage("Update OK, wait for 3 seconds for ethernet reboot")
                                .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,int id) {
                                        // if this button is clicked, close
                                        // current activity
                                        UpdateGateway.this.finish();
                                        System.exit(0);
                                    }
                                })
                                .show();
                        break;
                    }
                    case DISPLAYVERSION:{
                        mTextVersion.setText("Running Version:" + version);
                        break;
                    }
                    case CONNECTIONLOST: {
                        new AlertDialog.Builder(UpdateGateway.this)
                                .setMessage("Timeout, check your network")
                                .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
                                    public void onClick(DialogInterface dialog,int id) {
                                        // if this button is clicked, close
                                        // current activity
                                        UpdateGateway.this.finish();
                                        System.exit(0);
                                    }
                                })
                                .show();
                        break;
                    }
                }

            }
        };

    }

    //Below is for crypto
    private static byte[] encryptWithAesGcm(byte[] plaintext, byte[] randomKeyBytes, byte[] randomIvBytes) throws IOException, InvalidKeyException,
            InvalidAlgorithmParameterException, NoSuchAlgorithmException, NoSuchProviderException, NoSuchPaddingException {

        SecretKey randomKey = new SecretKeySpec(randomKeyBytes, "AES");
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding", "BC");
        final int blockSize = cipher.getBlockSize();
        AlgorithmParameterSpec spec = new IvParameterSpec(randomIvBytes);
        cipher.init(Cipher.ENCRYPT_MODE, randomKey, spec);
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        CipherOutputStream cipherOutputStream = new CipherOutputStream(byteArrayOutputStream, cipher);
        cipherOutputStream.write(plaintext);
        cipherOutputStream.close();
        return byteArrayOutputStream.toByteArray();
    }

    // Create Random num
    private static byte[] createRandomArray(int size) {
        byte[] randomByteArray = new byte[size];
        secureRandom.nextBytes(randomByteArray);
        return randomByteArray;
    }

    private void sendAuthenticationFrame() throws InvalidKeyException, InvalidAlgorithmParameterException, IOException, NoSuchAlgorithmException,
            NoSuchProviderException, NoSuchPaddingException, IllegalBlockSizeException, BadPaddingException {
        randomIv = createRandomArray(16);
        // byte[] data = "Data".getBytes();
        byte[] AADCiphertext = encryptWithAesGcm(crypto_aad, crypto_key, randomIv);
        byte[] tag = Arrays.copyOfRange(AADCiphertext, AADCiphertext.length - 16, AADCiphertext.length);
        // [UPGRADE
        byte[] header = {(byte) 0x5B, (byte) 0x55, (byte) 0x50, (byte) 0x47, (byte) 0x52, (byte) 0x41, (byte) 0x44, (byte) 0x45};
        byte[] type = {(byte) 0x08, (byte) 0x00};
        byte[] tail = {(byte) 0x5D};

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bos.write(header);
        bos.write(type);
        bos.write(randomIv);
        bos.write(tag);
        bos.write(tail);
        mTcpClient.sendBytes(bos.toByteArray());
    }

    public static byte[] shortToByteArray(short a)
    {
        byte[] ret = new byte[2];
        ret[1] = (byte) (a & 0xFF);
        ret[0] = (byte) ((a >> 8) & 0xFF);
        return ret;
    }
    /*
    *  Encapsulate plain data in 1024 block size unit. If not to 1024 for last packet, pad it using 0xff
    *  Note: For last check CRC command, it need to take pad 0xff into consideration.
    * */
    private byte[] generate_data_frame(byte[] plaintext, short num) throws InvalidKeyException, InvalidAlgorithmParameterException, IOException, NoSuchAlgorithmException,
            NoSuchProviderException, NoSuchPaddingException, IllegalBlockSizeException, BadPaddingException {
        // [UPGRADE
        byte[] header = {(byte) 0x5B, (byte) 0x55, (byte) 0x50, (byte) 0x47, (byte) 0x52, (byte) 0x41, (byte) 0x44, (byte) 0x45};
        // Type
        byte[] type = {(byte) 0x08, (byte) 0x01};
        //Encrypted data for test only, so remain it here.
       /* byte[] plaintext = new byte[1024];
        for (int i = 0; i <  1024; i++) {
            plaintext[i] = (byte)i;
        }*/
        // Result from AES-GCM includes 16 bytes tag. so just copy 1024 here.
        byte[] Cipher = new byte[1024];
        System.arraycopy(encryptWithAesGcm(plaintext, crypto_key, randomIv), 0, Cipher, 0, 1024);
        byte[] sequence = shortToByteArray(num);
        byte[] length = {(byte) 0x04, (byte) 0x00};
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bos.write(header);
        bos.write(type);
        bos.write(sequence);
        bos.write(length);
        bos.write(Cipher);
        CRC.reset();
        CRC.update(bos.toByteArray());
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream data = new DataOutputStream(output);
        try {
            data.writeLong(CRC.getValue());
        } catch (IOException e) {
            Log.e(TAG, "Convert long to byte error");
        }
        byte[] crc = new byte[4];
        System.arraycopy(output.toByteArray(), 4, crc, 0, crc.length);
        byte[] tail = {(byte) 0x5D};
        bos.write(crc);
        bos.write(tail);
        return bos.toByteArray();
    }


    private byte[] generate_command_frame(CommandType commandType) throws InvalidKeyException, InvalidAlgorithmParameterException, IOException, NoSuchAlgorithmException,
            NoSuchProviderException, NoSuchPaddingException, IllegalBlockSizeException, BadPaddingException {
        // [UPGRADE
        byte[] header = {(byte) 0x5B, (byte) 0x55, (byte) 0x50, (byte) 0x47, (byte) 0x52, (byte) 0x41, (byte) 0x44, (byte) 0x45};
        // Type
        byte[] type = {(byte) 0x08, (byte) 0x02};
        byte[] command = new byte[4];
        command[0] = (byte)0x55;
        command[1] = (byte)0xAA;
        command[2] = (byte)0x04;
        if (commandType == CommandType.GetVersion) {
            command[3] = (byte)0x00;
        } else if (commandType == CommandType.GetCRC) {
            command[3] = (byte)0x01;
        } else if (commandType == CommandType.ExeFirmware) {
            command[3] = (byte)0x02;
        }
        byte[] tail = {(byte) 0x5D};
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        bos.write(header);
        bos.write(type);
        bos.write(command);
        bos.write(tail);
        return bos.toByteArray();
    }


    private void showMyDialog() {
        bChooseFile = true;
        Intent mediaIntent = new Intent(Intent.ACTION_GET_CONTENT);
        mediaIntent.setType("file/*"); //set mime type as per requirement
        startActivityForResult(mediaIntent, PICKFILE_RESULT_CODE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(TAG, "will reset bChooseFile to false");
        bChooseFile = false;
        // TODO Auto-generated method stub
        switch (requestCode) {
            case PICKFILE_RESULT_CODE:
                if (resultCode == RESULT_OK) {
                    String FilePath = data.getData().getPath();
                    mPath.setText(FilePath);

                    Log.d(TAG, "onActivityResult");
                    PreCheckVersion();
                }
                break;

        }
    }

    public void PreCheckVersion(){
        File file = null;
        try {
            file = new File(mPath.getText().toString());
        } catch (Exception e) {
            Log.e(TAG, "New file error:" + e);
            return;
        }
        if (!file.exists() || !file.isFile() || !file.canRead()) {
            Toast.makeText(UpdateGateway.this, "File access error", Toast.LENGTH_SHORT).show();
            return;
        }

        try {
            byte temp[] = new byte[1024];
            int count;
            final FileInputStream ftemp = new FileInputStream(file);
            if ((count = ftemp.read(temp)) > 0 ) {
                if (temp[28] != 'A' || temp[29] != 'T') {
                    Log.d(TAG, "file version is " + temp[30] + temp[31]);
                    new AlertDialog.Builder(UpdateGateway.this)
                            .setMessage("Note:file format is wrong")
                            .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface dialog,int id) {
                                    // if this button is clicked, close
                                    // current activity
                                    try {
                                        ftemp.close();
                                    }catch (Exception e) {
                                        Log.e(TAG, "file input stream close error");
                                    }
                                }
                            })
                            .show();
                } else {
                    bFileFormat = true;
                    new AlertDialog.Builder(UpdateGateway.this)
                            .setMessage("Note:chosen file version is V" + temp[30] + "." + temp[31])
                            .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface dialog, int id) {
                                    // if this button is clicked, close
                                    // current activity
                                    try {
                                        ftemp.close();
                                    }catch (Exception e) {
                                        Log.e(TAG, "file input stream close error");
                                    }
                                }
                            })
                            .show();
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "pre read error");
        }

    }
    public class connectTask extends AsyncTask<byte[], String, TCPClient> {

        @Override
        protected TCPClient doInBackground(byte[]... message) {
            Log.d(TAG, "start do in background");
            //we create a TCPClient object and
            mTcpClient = new TCPClient(new TCPClient.OnMessageReceived() {
                @Override
                //here the messageReceived method is implemented
                public void messageReceived(byte[] message) {
                    if (message[0] == 0x74 && message[1] == 0x69 && message[2] == 0x6d && message[3] == 0x65) {
                        if (bDataSend) {
                            mHandler.sendEmptyMessage(CONNECTIONLOST);
                        }
                        return;
                    }
                    //Ack frame
                    if (message[8] == 0x08 && message[9] == 0x03) {
                        if (message[10] == (byte)0xFF && message[11] == (byte)0xFF) {
                            if (message[12] == 0 && message[13] == 0) {
                                if (bCommandSend) {
                                    Log.d(TAG, "Successfully in switch to new firmware");
                                    mHandler.sendEmptyMessage(UPDATEOVER);
                                } else {
                                    Log.d(TAG, "Authentication passed, begin trans data");
                                    if (bUpdate)
                                        mHandler.sendEmptyMessage(SENDDATAFRAME);
                                    else
                                        mHandler.sendEmptyMessage(SENDGETVERSION);
                                }
                            } else if (message[13] == 0x01) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                    Log.d(TAG, "Authentication failed");
                                } else {
                                    Log.d(TAG, "Authentication failed");
                                }
                            } else if (message[13] == 0x02) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                }
                                Log.d(TAG, "Packet invalid");
                            } else if (message[13] == 0x03) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                }
                                Log.d(TAG, "Packet type error");
                            } else if (message[13] == 0x04) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                }
                                Log.d(TAG, "sequence error");
                            }else if (message[13] == 0x05) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                }
                                Log.d(TAG, "length error");
                            }else if (message[13] == 0x06) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                }
                                Log.d(TAG, "CRC error");
                            }else if (message[13] == 0x07) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                }
                                Log.d(TAG, "decrypt error");
                            }else if (message[13] == 0x08) {
                                if (bDataSend) {
                                    bDataAckError = true;
                                }
                                Log.d(TAG, "Flash write error");
                            }
                        } else {
                            /* Data ACK frame: first 2 bytes is sequence of the current Data frame
                             *                 last 2bytes is length of the current Data frame
                             */

                            if (bDataSend) {
                                byte[] bytes_temp = {0, 0, message[10], message[11]};
                                ByteBuffer buffer = ByteBuffer.wrap(bytes_temp);
                                int length = buffer.getInt();
                                Log.d(TAG, "Data ACK frame: " + length);
                                mHandler.sendEmptyMessage(CONTINUEDATA);
                            }
                            if (bCommandSend) {
                                byte[] bytes_temp = {0, 0, 0, 0,message[10], message[11], message[12], message[13]};
                                ByteBuffer buffer = ByteBuffer.wrap(bytes_temp);
                                long crc = buffer.getLong();
                                Log.d(TAG, "Get CRC value from peer is " + crc);
                                if (mFileCRC == crc){
                                    Log.d(TAG, "CRC has no problem, continue...");
                                    mHandler.sendEmptyMessage(TRANSFER2NEWFIRMWARE);
                                }
                            }
                            if (bCommandGetverison) {
                                bCommandGetverison = false;
                                byte[] bytes_main = {0, 0, message[10], message[11]};
                                byte[] bytes_minor = {0, 0, message[12], message[13]};
                                ByteBuffer buffer_main = ByteBuffer.wrap(bytes_main);
                                ByteBuffer buffer_minor = ByteBuffer.wrap(bytes_minor);
                                int main = buffer_main.getInt();
                                int minor = buffer_minor.getInt();
                                version = "V" + main + "." + minor;
                                Log.d(TAG, "Get version from peer is " + version);
                                mHandler.sendEmptyMessage(DISPLAYVERSION);
                            }

                        }
                    }
                }
            }, dataSource, SERVERPORT);
            mTcpClient.run();

            return null;
        }
    }


    private void dismissProcessDialog() {
        runOnUiThread(new Runnable() {
            public void run() {
                if (pd_config_refresh != null) {
                    pd_config_refresh.dismiss();
                }
                pd_config_refresh = null;
            }
        });
    }

    /*Show process dialog*/
    private void showProcessDialog(final int progress) {
        runOnUiThread(new Runnable() {
            public void run() {
                if (pd_config_refresh == null) {
                    pd_config_refresh = new ProgressDialog(UpdateGateway.this);
                }
                pd_config_refresh.setMessage("Updating...");
                pd_config_refresh.setCancelable(false);
                pd_config_refresh.setProgress(progress);
                pd_config_refresh.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
                pd_config_refresh.setMax(100);
                pd_config_refresh.show();
            }
        });
    }
    @Override
    protected void onStop() {
        if (mTcpClient != null) {
            if (!bChooseFile) {
                Log.d(TAG, "will stop client");
                mTcpClient.stopClient();
            }
        }
        Log.d(TAG, "on stop bChooseFile=" + bChooseFile);
        super.onStop();
    }

    @Override
    protected void onResume() {
        Log.d(TAG, "on resume");
        if (mTcpClient != null) {
            if (!bChooseFile) {
                Log.d(TAG, "will start client");
                mTcpClient.startClient();
            }
        }
        super.onResume();
    }
}
