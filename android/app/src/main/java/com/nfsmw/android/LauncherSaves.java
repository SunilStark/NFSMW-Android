// SPDX-License-Identifier: GPL-3.0-only
package com.nfsmw.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import java.io.*;
import java.text.DateFormat;
import java.util.*;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

/** SAF documents, private backups and background transactions. No storage permission or automatic upload. */
final class LauncherSaves {
    private static final int EXPORT = 81, IMPORT = 82;
    private final Activity activity;
    private final Executor worker;
    private final Consumer<Boolean> setBusy;
    private final File root, backups;
    private boolean busy, failedRecovery;
    private final Handler main = new Handler(Looper.getMainLooper());
    LauncherSaves(Activity activity, Executor worker, Consumer<Boolean> setBusy) {
        this.activity = activity; this.worker = worker; this.setBusy = setBusy;
        root = new File(activity.getFilesDir(), "nfsmw/user");
        backups = new File(activity.getFilesDir(), "save-backups");
    }
    boolean blocked() { return busy || failedRecovery; }
    private void message(String text) {
        if (activity.isFinishing() || activity.isDestroyed()) return;
        new AlertDialog.Builder(activity).setTitle("Saved games").setMessage(text).setPositiveButton("OK", null).show();
    }
    void recover() {
        if (GameActivity.gameRunning) return;
        busy = true; setBusy.accept(true);
        worker.execute(() -> {
            String error = null;
            try { SaveArchive.recover(root); }
            catch (IOException failure) { error = "An interrupted import could not be recovered. Keep the app's data and the backups. " + failure.getMessage(); }
            String report = error;
            main.post(() -> { busy = false; failedRecovery = report != null; setBusy.accept(failedRecovery); if (report != null) message(report); });
        });
    }
    private File latest() {
        File[] files = backups.listFiles(file -> file.isFile() && file.getName().endsWith(".zip"));
        return files == null ? null : Arrays.stream(files).max(Comparator.comparingLong(File::lastModified)).orElse(null);
    }
    void menu() {
        if (blocked()) { message("Wait for the saved-game recovery to finish."); return; }
        if (GameActivity.gameRunning) { message("Quit the game before importing or exporting saved games."); return; }
        File last = latest();
        String info = last == null ? "There are no local backups yet." : "Last backup: " + DateFormat.getDateTimeInstance().format(new Date(last.lastModified()));
        new AlertDialog.Builder(activity).setTitle("Saved games")
            .setItems(new String[]{"Export ZIP", "Create local backup", "Import ZIP", "Restore last backup"}, (dialog, which) -> {
                if (which == 0) picker(EXPORT);
                else if (which == 1) run(() -> "Backup created and verified: " + SaveArchive.backup(root, backups).getName() + ". You can export a ZIP to keep it outside the app.");
                else if (which == 2) picker(IMPORT);
                else if (last == null) message(info);
                else confirm(() -> run(() -> { SaveArchive.restore(root, new FileInputStream(last), backups); return "Backup restored. The previous copy was backed up too."; }));
            }).setNeutralButton("Information", (dialog, which) -> message(info + "\nLocal backups are lost when the app is uninstalled; export a ZIP to keep them. Copies are verified with SHA-256. Only ZIPs created by this feature can be imported."))
            .setNegativeButton("Close", null).show();
    }
    private void picker(int request) {
        Intent intent = new Intent(request == EXPORT ? Intent.ACTION_CREATE_DOCUMENT : Intent.ACTION_OPEN_DOCUMENT)
                .addCategory(Intent.CATEGORY_OPENABLE).setType("application/zip");
        if (request == EXPORT) intent.putExtra(Intent.EXTRA_TITLE, "NFSMW-partidas-" + System.currentTimeMillis() + ".zip");
        try { activity.startActivityForResult(intent, request); }
        catch (Exception error) { message("The document picker could not be opened."); }
    }
    private void confirm(Runnable action) {
        new AlertDialog.Builder(activity).setTitle("Import saved games")
            .setMessage("Profiles, saved games and achievements will be replaced. The ZIP is verified first and the current ones are backed up. Settings and shaders are kept.")
            .setPositiveButton("Import with backup", (dialog, which) -> action.run()).setNegativeButton("Cancel", null).show();
    }
    boolean result(int request, int result, Intent data) {
        if (request != EXPORT && request != IMPORT) return false;
        if (result != Activity.RESULT_OK || data == null || data.getData() == null) return true;
        Uri uri = data.getData();
        if (request == EXPORT) run(() -> {
            OutputStream out = activity.getContentResolver().openOutputStream(uri, "w");
            if (out == null) throw new IOException("The document cannot be written.");
            int count = SaveArchive.export(root, out);
            return "ZIP exported: " + count + " saved-game files. Settings, shaders, caches and drivers are left out.";
        });
        else confirm(() -> run(() -> {
            InputStream in = activity.getContentResolver().openInputStream(uri);
            if (in == null) throw new IOException("The document cannot be read.");
            File backup = SaveArchive.restore(root, in, backups);
            return "Saved games imported and verified. Previous backup: " + backup.getName();
        }));
        return true;
    }
    interface Job { String run() throws IOException; }
    private void run(Job job) {
        if (blocked() || GameActivity.gameRunning) { message("Quit the game and wait for the current operation to finish."); return; }
        busy = true; setBusy.accept(true);
        worker.execute(() -> {
            String result;
            try { SaveArchive.recover(root); result = job.run(); }
            catch (IOException error) { result = "The operation did not complete: " + error.getMessage(); }
            boolean recoveryFailed = false;
            try { SaveArchive.recover(root); }
            catch (IOException error) { recoveryFailed = true; result += " Keep the app's data: a recovery is pending."; }
            String report = result;
            boolean blocked = recoveryFailed;
            main.post(() -> { busy = false; failedRecovery = blocked; setBusy.accept(blocked); message(report); });
        });
    }
}
