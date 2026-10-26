// SPDX-License-Identifier: GPL-3.0-only
// Adapted from ServidorLanService.java of victorgbd/NFSMW-Recompiled-Mobile dd76acb (GPL-3.0).
package com.nfsmw.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import java.io.File;
import java.io.IOException;

/**
 * The LAN's EA lobby server (ServidorEa) on this phone, so it can host without a PC: the others join this
 * phone's IP. A foreground service in its own process (:lan, see the manifest), so it keeps serving while the
 * game runs, after the game exits and with the launcher in the background. Started by the launcher in host
 * mode or plays in it, stopped when the mode changes or from the notification.
 */
public class LanServerService extends Service {
    private static final String TAG = "NFSMW";
    private static final String CHANNEL = "lan_server";
    private static final int NOTIFICATION = 1;
    private static final String STOP = "com.nfsmw.android.STOP_LAN_SERVER";

    private final Handler mainThread = new Handler(Looper.getMainLooper());
    private ServidorEa server;
    private boolean running;
    // Without it some Wi-Fi drivers drop broadcasts before they reach the app, and the others' server search
    // (BuscadorServidores) would not find this phone.
    private WifiManager.MulticastLock broadcasts;

    static void start(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            context.startForegroundService(new Intent(context, LanServerService.class));
        } else {
            context.startService(new Intent(context, LanServerService.class));
        }
    }

    static void stop(Context context) {
        context.stopService(new Intent(context, LanServerService.class));
    }

    @Override
    public void onCreate() {
        super.onCreate();
        NotificationManager nm = getSystemService(NotificationManager.class);
        NotificationChannel channel = new NotificationChannel(CHANNEL, "Servidor LAN",
                NotificationManager.IMPORTANCE_LOW);
        channel.setShowBadge(false);
        nm.createNotificationChannel(channel);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && STOP.equals(intent.getAction())) {
            // From the notification. The launcher starts it again only when the player chooses host mode or
            // plays in it (its preferences live in the main process).
            stopSelf();
            return START_NOT_STICKY;
        }
        // Always, and at once: startForegroundService requires it.
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIFICATION, notification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        } else {
            startForeground(NOTIFICATION, notification());
        }
        if (!running) {
            running = true;
            WifiManager wifi = getApplicationContext().getSystemService(WifiManager.class);
            if (wifi != null) {
                broadcasts = wifi.createMulticastLock("lan_server");
                broadcasts.setReferenceCounted(false);
                broadcasts.acquire();
            }
            // Opening ports is network work: off the main thread.
            new Thread(this::open, "servidor-ea").start();
        }
        return START_STICKY;
    }

    private void open() {
        try {
            File data = new File(getFilesDir(), "lan_server");
            ServidorEa s = new ServidorEa(data, new File(data, "logs"),
                    () -> mainThread.post(this::refresh), this::serverName);
            s.arrancar();
            mainThread.post(() -> {
                if (running) {
                    server = s;
                    refresh();
                } else {
                    new Thread(s::parar, "servidor-ea-parar").start();
                }
            });
        } catch (IOException e) {
            // Usually the ports are taken (another server copy on this phone).
            Log.w(TAG, "[lan] the server could not open its ports", e);
            mainThread.post(this::stopSelf);
        }
    }

    /** What the others see when they search: this phone's gamertag, or its model. */
    private String serverName() {
        String g = LanSettings.gamertag(this);
        return g.isEmpty() ? Build.MODEL : g;
    }

    private void refresh() {
        if (running) {
            getSystemService(NotificationManager.class).notify(NOTIFICATION, notification());
        }
    }

    private Notification notification() {
        String ip = ServidorEa.ipLocal();
        int players = server != null ? server.jugadores() : 0;
        String text = ip == null ? "Sin Wi-Fi ni punto de acceso: nadie puede unirse"
                : "IP " + ip + " · " + players + (players == 1 ? " jugador" : " jugadores");
        PendingIntent open = PendingIntent.getActivity(this, 0,
                new Intent(this, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
                PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop = PendingIntent.getService(this, 1,
                new Intent(this, LanServerService.class).setAction(STOP), PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this, CHANNEL)
                .setSmallIcon(android.R.drawable.stat_sys_upload_done)
                .setContentTitle("Servidor LAN de NFSMW")
                .setContentText(text)
                .setContentIntent(open)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .addAction(new Notification.Action.Builder(null, "Detener", stop).build())
                .build();
    }

    @Override
    public void onDestroy() {
        running = false;
        if (broadcasts != null) {
            broadcasts.release();
            broadcasts = null;
        }
        mainThread.removeCallbacksAndMessages(null);
        if (server != null) {
            ServidorEa s = server;
            server = null;
            // Closing sockets is network work too.
            new Thread(s::parar, "servidor-ea-parar").start();
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
