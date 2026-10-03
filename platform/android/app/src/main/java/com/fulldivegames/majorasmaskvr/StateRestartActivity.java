package com.fulldivegames.majorasmaskvr;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ResultReceiver;
import android.os.SystemClock;
import android.widget.TextView;

/** Lives outside SDL's process. Never kills the game or starts a second native engine. */
public final class StateRestartActivity extends Activity {
    static final String READY = "mmvr.restart.ready";
    static final String OWNER = "mmvr.restart.owner";
    static final String COMMIT = "mmvr.restart.commit";
    private final Handler handler = new Handler(Looper.getMainLooper());
    private StateRestartPolicy policy;
    private IBinder owner;
    private TextView status;
    private final Runnable poll = this::check;

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        status = new TextView(this);
        status.setTextSize(24);
        status.setPadding(48, 48, 48, 48);
        status.setText("Restarting to restore saved settings...");
        setContentView(status);
        Bundle request = getIntent().getExtras();
        owner = request == null ? null : request.getBinder(OWNER);
        ResultReceiver ready = request == null ? null : request.getParcelable(READY);
        // The Activity is internal and has no intent filter. The Binder proves
        // which particular originating process must finish, avoiding PID reuse.
        if (owner == null || ready == null || !owner.isBinderAlive()) {
            fail("Restart request expired. Open the game again manually.");
            return;
        }
        policy = new StateRestartPolicy(SystemClock.elapsedRealtime());
        ResultReceiver commit = new ResultReceiver(handler) {
            @Override protected void onReceiveResult(int code, Bundle data) {
                if (code == Activity.RESULT_OK) policy.commit(SystemClock.elapsedRealtime());
                else policy.cancel();
                check();
            }
        };
        Bundle response = new Bundle();
        response.putParcelable(COMMIT, commit);
        ready.send(Activity.RESULT_OK, response);
        check();
    }

    private void check() {
        if (isFinishing() || policy == null) return;
        handler.removeCallbacks(poll);
        switch (policy.poll(SystemClock.elapsedRealtime(), owner.isBinderAlive())) {
            case WAIT:
                handler.postDelayed(poll, 100);
                break;
            case LAUNCH:
                try {
                    // Setup mounts the saved enabled pack set in a fresh main
                    // process. Native pending-state validation runs afterward.
                    startActivity(new Intent(this, SetupActivity.class)
                            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP));
                    finish();
                } catch (RuntimeException failure) {
                    fail("Automatic restart failed. Open the game again manually.");
                }
                break;
            case STOP:
                if (owner.isBinderAlive()) {
                    android.util.Log.w("MMVR-Lifecycle", "State restart timed out or was cancelled; game was not killed");
                    finish(); // Return to the still-running game if setup failed.
                } else fail("Restart request expired. Open the game again manually.");
                break;
        }
    }

    private void fail(String message) {
        handler.removeCallbacks(poll);
        status.setText(message);
        android.util.Log.w("MMVR-Lifecycle", message);
    }

    @Override protected void onDestroy() {
        handler.removeCallbacks(poll);
        super.onDestroy();
    }
}
