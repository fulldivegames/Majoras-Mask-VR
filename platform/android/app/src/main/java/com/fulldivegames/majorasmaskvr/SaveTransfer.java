package com.fulldivegames.majorasmaskvr;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import org.json.JSONObject;

/** The Android picker never writes active saves. Native code validates and
 * commits the selected JSON on the game thread, at file selection only. */
final class SaveTransfer {
    static final int REQUEST = 48;
    private final Activity activity;
    private boolean busy, exporting;
    private String result = "";

    SaveTransfer(Activity activity) { this.activity = activity; }

    synchronized String takeResult() { String value=result; result=""; return value; }
    private synchronized void finish(String value) { result=value; busy=false; }

    synchronized void request(boolean export) {
        if (busy) return;
        busy=true; exporting=export; result="";
        activity.runOnUiThread(() -> {
            try {
                Intent intent = new Intent(export ? Intent.ACTION_CREATE_DOCUMENT : Intent.ACTION_OPEN_DOCUMENT)
                    .setType(export ? "application/json" : "*/*").addCategory(Intent.CATEGORY_OPENABLE);
                intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
                    DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "primary:Download"));
                if (export) intent.putExtra(Intent.EXTRA_TITLE,"Majoras-Mask-save.json");
                activity.startActivityForResult(intent,REQUEST);
            } catch (Exception error) { finish("Cannot open file picker: " + error.getMessage()); }
        });
    }

    void selected(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            finish("cancelled"); return;
        }
        final Uri uri=data.getData();
        final boolean export;
        synchronized (this) { if (!busy) return; export=exporting; }
        new Thread(() -> {
            File stage=null;
            try {
                File root=activity.getExternalFilesDir(null);
                if (root==null) throw new IOException("App storage unavailable");
                if (export) {
                    File source=new File(root,"save-export.json");
                    try (InputStream in=new FileInputStream(source);
                         OutputStream out=activity.getContentResolver().openOutputStream(uri,"wt")) {
                        if (out==null) throw new IOException("Cannot write the selected document");
                        byte[] bytes=new byte[65536];int count;
                        while ((count=in.read(bytes))!=-1) out.write(bytes,0,count);
                        out.flush();
                    }
                } else {
                    stage=new File(root,"save-import.selected.json.pending");
                    SetupActivity.copy(activity.getContentResolver().openInputStream(uri),stage,32L*1024*1024);
                    JSONObject save=new JSONObject(SetupActivity.read(new FileInputStream(stage)));
                    if (!"2S2H_SAVE".equals(save.optString("type")))
                        throw new IOException("Choose a normal 2Ship save JSON, not a save state");
                    Files.move(stage.toPath(),new File(root,"save-import.selected.json").toPath(),
                        StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
                }
                finish("ready");
            } catch (Exception error) {
                if (stage!=null) stage.delete();
                finish("File transfer failed: " + error.getMessage());
            }
        },"MMVR-save-transfer").start();
    }
}
