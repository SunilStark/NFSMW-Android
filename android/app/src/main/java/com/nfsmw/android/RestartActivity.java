package com.nfsmw.android;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;

/**
 * Brings the launcher back after the player leaves the game.
 *
 * The game cannot start twice in one process (SDL and the recompiled game keep native state; see
 * GameActivity.canLoad), so leaving it ends its process. This activity runs in a process of its own
 * (":restart" in the manifest): the game's process starts it and ends, and it opens the launcher in a
 * fresh process, as a visible activity may, then ends too.
 */
public final class RestartActivity extends Activity {
    /** Called from the game's process: opens this activity and ends that process. */
    static void restartLauncher(Activity from) {
        from.startActivity(new Intent(from, RestartActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NO_ANIMATION));
        from.finishAffinity();
        Process.killProcess(Process.myPid());
        System.exit(0);
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        Context context = getApplicationContext();
        // Give the game's process a moment to be gone, so the launcher starts in a new one.
        getWindow().getDecorView().postDelayed(() -> {
            startActivity(new Intent(context, MainActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK));
            finish();
            Process.killProcess(Process.myPid());
        }, 400);
    }
}
