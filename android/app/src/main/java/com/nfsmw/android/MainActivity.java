package com.nfsmw.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.content.Intent;
import android.content.ClipData;
import android.content.ActivityNotFoundException;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.documentfile.provider.DocumentFile;
import androidx.core.content.FileProvider;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private static final int REQUEST_GAME_FOLDER = 41;
    private static final int REQUEST_STORAGE_ACCESS = 42;
    private static final int REQUEST_LEGACY_STORAGE = 43;
    private static final int REQUEST_SAVE_REPORT = 44;
    private static final int REQUEST_DRIVER_ZIP = 45;
    private static final int REQUEST_DRIVER_PROBE = 46;
    private boolean playAfterProbe;
    private boolean launcherResumed;
    private boolean checkingUpdates;
    private ReleaseUpdates.Result pendingUpdate;
    private boolean pendingUpdateManual;

    private static final String TREE_URI = "tree_uri";
    static final String GAME_FOLDER_NAME = "nsfmw-androidevolved";

    private TextView importStatus;
    private Button selectFolder;
    private Button launchGame;
    private Button sendReport;
    private File pendingReport;
    private boolean pendingGithub;
    private LauncherSaves launcherSaves;
    private final ExecutorService importer = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (state != null) {
            playAfterProbe = state.getBoolean("play_after_probe");
            String name = state.getString("pending_report");
            if (name != null && name.equals(new File(name).getName())) {
                pendingReport = new File(new File(getCacheDir(), "reports"), name);
                pendingGithub = state.getBoolean("pending_github");
            }
        }
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        setContentView(buildScreen());
        launcherSaves = new LauncherSaves(this, importer, busy -> {
            launchGame.setEnabled(!busy);
            selectFolder.setEnabled(!busy);
        });
        launcherSaves.recover();
        prepareSharedGameFolder();
        checkUpdates(false);
    }

    @Override
    protected void onResume() {
        super.onResume();
        launcherResumed = true;
        if (optionsList != null) {
            refreshOptions();
        }
        if (checksList != null && hasStorageAccess()) {
            showChecks(sharedGameRoot());
        }
        if (pendingUpdate != null) {
            ReleaseUpdates.Result result = pendingUpdate;
            pendingUpdate = null;
            showUpdate(result, pendingUpdateManual);
        }
    }

    @Override protected void onPause() {
        launcherResumed = false;
        super.onPause();
    }

    private void checkUpdates(boolean manual) {
        if (checkingUpdates) return;
        checkingUpdates = true;
        if (optionsList != null) refreshOptions();
        ReleaseUpdates.check(this, manual, result -> {
            if (isFinishing() || isDestroyed()) return;
            checkingUpdates = false;
            refreshOptions();
            if (!launcherResumed) {
                pendingUpdate = result; pendingUpdateManual = manual;
            } else showUpdate(result, manual);
        });
    }

    private void showUpdate(ReleaseUpdates.Result result, boolean manual) {
        if (result.error != null) {
            if (manual) reportError(result.error);
            return;  // Offline/rate-limited checks never affect playing.
        }
        if (!ReleaseVersion.newer(result.tag, ReleaseUpdates.installed(this))) {
            if (manual) new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle("Updates").setMessage("You have version " + ReleaseUpdates.installed(this)
                            + ". There is no newer stable version on GitHub.")
                    .setPositiveButton("OK", null).show();
            return;
        }
        android.content.SharedPreferences updates = ReleaseUpdates.prefs(this);
        if (!manual && result.tag.equals(updates.getString("notified_tag", ""))) return;
        updates.edit().putString("notified_tag", result.tag).apply();
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("New version available: " + result.tag)
                .setMessage("You can update from GitHub or keep playing this version. Install the new APK "
                        + "over the old one to keep your saves and settings.")
                .setPositiveButton("Update", (dialog, which) -> {
                    try { startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(result.url))); }
                    catch (ActivityNotFoundException error) { reportError("No app was found to open GitHub."); }
                })
                .setNegativeButton("Later", null).show();
    }

    // ---- Screen --------------------------------------------------------------------------------------------

    private static final int ACCENT = 0xFFFF8A00;
    private LinearLayout checksList;
    private LinearLayout optionsList;
    private LinearLayout shaderPanel;
    private ProgressBar shaderProgress;
    private TextView shaderText;
    private ShaderBuilder shaderBuilder;

    private View buildScreen() {
        FrameLayout screen = new FrameLayout(this);
        screen.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xFF0B0F14, 0xFF1C1407, 0xFF0B0F14}));

        LinearLayout columns = new LinearLayout(this);
        columns.setOrientation(LinearLayout.HORIZONTAL);
        columns.setPadding(dp(28), dp(16), dp(28), dp(16));
        screen.addView(columns, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));

        // Left: logo, game files and the two actions.
        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);
        left.setGravity(Gravity.CENTER_HORIZONTAL);
        ScrollView leftScroll = new ScrollView(this);
        leftScroll.setFillViewport(true);
        leftScroll.addView(left);
        columns.addView(leftScroll, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1.1f));

        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.banner);
        logo.setAdjustViewBounds(true);
        logo.setScaleType(ImageView.ScaleType.FIT_CENTER);
        left.addView(logo, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(92)));

        TextView subtitle = label("Xbox 360 · recompiled for Android", 13, 0x99FFFFFF, false);
        subtitle.setGravity(Gravity.CENTER);
        left.addView(subtitle, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        LinearLayout card = card();
        LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        cardParams.topMargin = dp(12);
        left.addView(card, cardParams);
        card.addView(label("GAME FILES", 12, ACCENT, true));
        importStatus = label("", 13, 0xDDFFFFFF, false);
        importStatus.setPadding(0, dp(4), 0, dp(6));
        card.addView(importStatus);
        checksList = new LinearLayout(this);
        checksList.setOrientation(LinearLayout.VERTICAL);
        card.addView(checksList);
        shaderPanel = new LinearLayout(this);
        shaderPanel.setOrientation(LinearLayout.VERTICAL);
        shaderPanel.setPadding(0, dp(8), 0, 0);
        shaderPanel.setVisibility(View.GONE);
        shaderText = label("", 13, 0xFFFFFFFF, false);
        shaderPanel.addView(shaderText);
        shaderProgress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        shaderProgress.setMax(1000);
        shaderProgress.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));
        shaderPanel.addView(shaderProgress, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(10)));
        card.addView(shaderPanel);

        launchGame = actionButton("PLAY", true);
        launchGame.setVisibility(View.GONE);
        launchGame.setOnClickListener(view -> play());
        LinearLayout.LayoutParams playParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(62));
        playParams.topMargin = dp(12);
        left.addView(launchGame, playParams);

        selectFolder = actionButton("Choose game folder", false);
        selectFolder.setOnClickListener(view -> selectGameFolder());
        LinearLayout.LayoutParams folderParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(46));
        folderParams.topMargin = dp(10);
        left.addView(selectFolder, folderParams);

        // Right: graphics options.
        LinearLayout right = card();
        LinearLayout.LayoutParams rightParams = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.MATCH_PARENT, 1f);
        rightParams.leftMargin = dp(24);
        columns.addView(right, rightParams);
        right.addView(label("GRAPHICS OPTIONS", 12, ACCENT, true));
        TextView note = label("Applied when the game starts.", 12, 0x88FFFFFF, false);
        note.setPadding(0, dp(2), 0, dp(4));
        right.addView(note);
        ScrollView scroll = new ScrollView(this);
        optionsList = new LinearLayout(this);
        optionsList.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(optionsList);
        right.addView(scroll, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));
        sendReport = actionButton("Send crash or log", false);
        sendReport.setOnClickListener(view -> chooseReportDestination());
        LinearLayout.LayoutParams reportParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(48));
        reportParams.topMargin = dp(8);
        right.addView(sendReport, reportParams);
        refreshOptions();
        return screen;
    }

    // ---- Shader library ------------------------------------------------------------------------------------

    private void play() {
        if (launcherSaves != null && launcherSaves.blocked()) return;
        if (GpuDrivers.selected(this) != null) {
            probeDriver(true);
            return;
        }
        playChecked();
    }

    private void playChecked() {
        launchGame.setEnabled(false);
        Diagnostics.recordLaunch(this);
        importer.execute(() -> {
            GameEdition edition = GameEdition.of(this, sharedGameRoot());
            String problem = "nativo".equals(GameOptions.get(this, GameOptions.RENDERER))
                    && !GameOptions.usesGles(this) ? Diagnostics.incompatibility(this) : null;
            mainHandler.post(() -> {
                if (isFinishing() || isDestroyed()) return;
                launchGame.setEnabled(true);
                if (edition == null || !edition.supported() || !GameActivity.canLoad(edition)) {
                    String reason = edition == null ? "default.xex cannot be read. Check your copy of the game."
                            : !edition.supported() ? "Your executable is from the " + edition.describe()
                                    + " edition. This APK includes: " + GameEdition.compiledNames()
                                    + ". Each edition needs its own build."
                            : "To change editions, close the app completely and open it again.";
                    new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                            .setTitle("This edition cannot be started")
                            .setMessage(reason)
                            .setPositiveButton("Back", null).show();
                    return;
                }
                continuePlay(problem);
            });
        });
    }

    private void continuePlay(String problem) {
        if (problem == null) {
            playCompatibleGame();
            return;
        }
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Try another renderer")
                .setMessage(problem + "\n\nYou can use the native renderer on OpenGL ES (experimental, for GPUs "
                        + "without a compatible Vulkan) or the compatibility mode, which still uses Vulkan. They may have "
                        + "graphical glitches or run slower.")
                .setPositiveButton("Use OpenGL ES", (dialog, which) -> {
                    GameOptions.set(this, GameOptions.RENDERER, "nativo");
                    GameOptions.set(this, GameOptions.API, "gles");
                    refreshOptions();
                    playCompatibleGame();
                })
                .setNeutralButton("Vulkan compatibility", (dialog, which) -> {
                    GameOptions.set(this, GameOptions.RENDERER, "xenos");
                    refreshOptions();
                    playCompatibleGame();
                })
                .setNegativeButton("Back", null).show();
    }

    private void playCompatibleGame() {
        if (launcherSaves != null && launcherSaves.blocked()) return;
        boolean xenos = "xenos".equals(GameOptions.get(this, GameOptions.RENDERER));
        if (!xenos && !ShaderBuilder.hasLibrary(sharedGameRoot())) {
            buildShaders(true, false);
        } else if (!xenos && GameOptions.usesGles(this) && !ShaderBuilder.hasLibrary(sharedGameRoot(), true)) {
            // The OpenGL ES library is made from the Vulkan one.
            buildShaders(true, true);
        } else {
            startGame();
        }
    }

    /** Starts the game; with LAN play, first the host server and the server's IP (a name is resolved here). */
    private void startGame() {
        if (LanSettings.MODE_OFF.equals(LanSettings.mode(this))) {
            startActivity(new Intent(this, GameActivity.class));
            return;
        }
        boolean host = LanSettings.MODE_HOST.equals(LanSettings.mode(this));
        if (host) {
            LanServerService.start(this);
        }
        launchGame.setEnabled(false);
        importer.execute(() -> {
            String server = LanSettings.effectiveServer(this);
            mainHandler.post(() -> {
                if (isFinishing() || isDestroyed()) return;
                launchGame.setEnabled(true);
                if (server == null) {
                    new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                            .setTitle("LAN multiplayer")
                            .setMessage(host
                                    ? "This device is not on any Wi-Fi and is not a hotspot: nobody could join."
                                    : "Server not found: " + LanSettings.server(this) + ".")
                            .setPositiveButton("Play without LAN", (d, w) ->
                                    startActivity(new Intent(this, GameActivity.class)))
                            .setNegativeButton("Back", null).show();
                    return;
                }
                startActivity(new Intent(this, GameActivity.class).putExtra(GameActivity.EXTRA_LAN_SERVER, server));
            });
        });
    }

    // ---- Tester reports ------------------------------------------------------------------------------------

    @Override
    protected void onSaveInstanceState(Bundle state) {
        if (pendingReport != null) {
            state.putString("pending_report", pendingReport.getName());
            state.putBoolean("pending_github", pendingGithub);
        }
        state.putBoolean("play_after_probe", playAfterProbe);
        super.onSaveInstanceState(state);
    }

    private void chooseReportDestination() {
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Send crash or log")
                .setMessage("The report includes the model, Android version, GPU, settings and this app's logs. Add what happened before sending it.")
                .setPositiveButton("Email", (dialog, which) -> prepareReport(0))
                .setNeutralButton("GitHub", (dialog, which) -> prepareReport(1))
                .setNegativeButton("Save ZIP", (dialog, which) -> prepareReport(2)).show();
    }

    private void reportError(String message) {
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Diagnostics").setMessage(message).setPositiveButton("OK", null).show();
    }

    private void prepareReport(int destination) {
        sendReport.setEnabled(false);
        sendReport.setText("Preparing diagnostics…");
        importer.execute(() -> {
            try {
                File report = Diagnostics.createReport(this);
                mainHandler.post(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    sendReport.setEnabled(true);
                    sendReport.setText("Send crash or log");
                    if (destination == 0) shareReportByEmail(report);
                    else saveReport(report, destination == 1);
                });
            } catch (Exception error) {
                mainHandler.post(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    sendReport.setEnabled(true);
                    sendReport.setText("Send crash or log");
                    reportError("The report could not be prepared: " + error.getMessage());
                });
            }
        });
    }

    private void shareReportByEmail(File report) {
        Uri uri = FileProvider.getUriForFile(this, getPackageName() + ".reports", report);
        Intent email = new Intent(Intent.ACTION_SEND);
        email.setType("application/zip");
        email.putExtra(Intent.EXTRA_EMAIL, new String[]{Diagnostics.EMAIL});
        email.putExtra(Intent.EXTRA_SUBJECT, "NFSMW Android: " + Build.MODEL + " — diagnostics");
        email.putExtra(Intent.EXTRA_TEXT, Diagnostics.summary(this) + "\nWhat happened:\n\nSteps to reproduce:\n\nGame edition:\n\nThe diagnostics are attached.");
        email.putExtra(Intent.EXTRA_STREAM, uri);
        email.setClipData(ClipData.newUri(getContentResolver(), "NFSMW diagnostics", uri));
        email.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try { startActivity(Intent.createChooser(email, "Send to " + Diagnostics.EMAIL)); }
        catch (ActivityNotFoundException error) { saveReport(report, false); }
    }

    private void saveReport(File report, boolean github) {
        pendingReport = report;
        pendingGithub = github;
        if (github) {
            new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle("Report on GitHub")
                    .setMessage("Save the ZIP first. Then the issue opens: describe the problem and attach the saved ZIP.")
                    .setPositiveButton("Save and open GitHub", (dialog, which) -> chooseReportFile())
                    .setNegativeButton("Cancel", (dialog, which) -> pendingReport = null).show();
        } else chooseReportFile();
    }

    private void chooseReportFile() {
        Intent save = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        save.addCategory(Intent.CATEGORY_OPENABLE);
        save.setType("application/zip");
        save.putExtra(Intent.EXTRA_TITLE, pendingReport.getName());
        try { startActivityForResult(save, REQUEST_SAVE_REPORT); }
        catch (ActivityNotFoundException error) {
            pendingReport = null;
            reportError("There is no app to save files. Try the Email option.");
        }
    }

    private void finishReportSave(Uri destination) {
        File report = pendingReport;
        boolean github = pendingGithub;
        pendingReport = null;
        if (report == null || !report.isFile()) {
            reportError("The report is no longer available. Create a new one.");
            return;
        }
        importer.execute(() -> {
            try (InputStream input = new FileInputStream(report);
                 java.io.OutputStream output = getContentResolver().openOutputStream(destination, "w")) {
                if (output == null) throw new IOException("The destination file could not be opened.");
                byte[] buffer = new byte[16384];
                for (int count; (count = input.read(buffer)) != -1;) output.write(buffer, 0, count);
            } catch (Exception error) {
                mainHandler.post(() -> {
                    if (!isFinishing() && !isDestroyed()) reportError("The ZIP could not be saved: " + error.getMessage());
                });
                return;
            }
            mainHandler.post(() -> {
                if (isFinishing() || isDestroyed()) return;
                if (github) {
                    String body = "### What happened\n\n### Steps to reproduce\n\n### Game edition\n\n### Device\n"
                            + Diagnostics.summary(this) + "\n### Diagnostics\nAttach here the file **" + report.getName() + "** you just saved.\n";
                    Uri issue = Uri.parse(Diagnostics.ISSUES).buildUpon()
                            .appendQueryParameter("title", "[Android] " + Build.MODEL + ": ")
                            .appendQueryParameter("body", body).build();
                    try { startActivity(new Intent(Intent.ACTION_VIEW, issue)); }
                    catch (ActivityNotFoundException error) { reportError("ZIP saved. Open " + Diagnostics.ISSUES + " and attach it to the issue."); }
                } else reportError("ZIP saved. You can attach it to an issue or send it to " + Diagnostics.EMAIL + ".");
            });
        });
    }

    /**
     * Builds nfsmw_shaders.nfsp from the game files (a minute or two, only once), or with gles
     * nfsmw_shaders_gles.nfsp from it.
     */
    private void buildShaders(boolean thenPlay, boolean gles) {
        if (shaderBuilder != null) {
            return;
        }
        launchGame.setEnabled(false);
        launchGame.setAlpha(.5f);
        selectFolder.setEnabled(false);
        shaderPanel.setVisibility(View.VISIBLE);
        shaderProgress.setProgress(0);
        shaderText.setText(gles ? "Generating the OpenGL ES shaders (first time only)…"
                : "Generating the native renderer's shaders (first time only)…");
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        shaderBuilder = new ShaderBuilder(this, sharedGameRoot(), gles, new ShaderBuilder.Listener() {
            @Override
            public void onProgress(float fraction, String text) {
                shaderProgress.setProgress(Math.round(fraction * 1000));
                shaderText.setText(text);
            }

            @Override
            public void onDone(File library) {
                shadersFinished();
                shaderPanel.setVisibility(View.GONE);
                showChecks(sharedGameRoot());
                if (thenPlay) {
                    playCompatibleGame();  // the next library, if one is missing, or the game
                }
            }

            @Override
            public void onError(String message) {
                shadersFinished();
                shaderText.setText("The shaders could not be generated: " + message);
                shaderProgress.setProgress(0);
            }
        });
        shaderBuilder.start();
    }

    private void shadersFinished() {
        shaderBuilder = null;
        getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        launchGame.setEnabled(true);
        launchGame.setAlpha(1f);
        selectFolder.setEnabled(true);
    }

    private void refreshOptions() {
        optionsList.removeAllViews();
        optionsList.addView(optionRow("Saved games", "Export, import and back up", () -> launcherSaves.menu()));
        optionsList.addView(optionRow("Graphics profile", profileLabel(), this::chooseProfile));
        GpuDrivers.Driver driver = GpuDrivers.selected(this);
        optionsList.addView(optionRow("Vulkan driver", driver == null ? "System" : driver.name,
                this::chooseDriver));
        optionsList.addView(optionRow("Test driver", "Check the GPU and Vulkan features", () -> probeDriver(false)));
        optionsList.addView(optionRow("Check for updates",
                checkingUpdates ? "Checking GitHub…" : "Version " + ReleaseUpdates.installed(this),
                () -> checkUpdates(true)));
        for (GameOptions.Option option : GameOptions.ALL) {
            String value = GameOptions.get(this, option.key);
            optionsList.addView(optionRow(option.title, option.label(value), () -> chooseOption(option)));
        }
        SharedPreferences controls = getSharedPreferences("nfsmw_controls", MODE_PRIVATE);
        boolean stretch = controls.getBoolean("stretch", true);
        optionsList.addView(optionRow("Image format",
                stretch ? "Stretched to the full screen" : "Original 16:9", () -> {
                    controls.edit().putBoolean("stretch", !stretch).apply();
                    refreshOptions();
                }));

        TextView keyboardTitle = label("KEYBOARD", 12, ACCENT, true);
        keyboardTitle.setPadding(0, dp(14), 0, dp(2));
        optionsList.addView(keyboardTitle);
        boolean keyboard = KeyboardBindings.enabled(this);
        optionsList.addView(optionRow("Play with keyboard",
                keyboard ? "On · it is player 1's controller" : "Off", () -> {
                    KeyboardBindings.setEnabled(this, !keyboard);
                    refreshOptions();
                }));
        if (keyboard) {
            optionsList.addView(optionRow("Keys", "Change the key of each button", this::chooseKeyboardBind));
        }

        TextView lan = label("LAN MULTIPLAYER · EXPERIMENTAL", 12, ACCENT, true);
        lan.setPadding(0, dp(14), 0, dp(2));
        optionsList.addView(lan);
        optionsList.addView(optionRow("LAN mode", LanSettings.describe(this), this::chooseLanMode));
        String gamertag = LanSettings.gamertag(this);
        optionsList.addView(optionRow("Gamertag", gamertag.isEmpty() ? "User (each device needs a different one)"
                : gamertag, this::editGamertag));
        if (LanSettings.MODE_JOIN.equals(LanSettings.mode(this))) {
            String server = LanSettings.server(this);
            optionsList.addView(optionRow("Server", server.isEmpty() ? "Search or type the IP" : server,
                    this::chooseLanServer));
        }
    }

    // ---- Keyboard ------------------------------------------------------------------------------------------

    /** The controller's buttons and their keys (KeyboardBindings); one tapped waits for its new key. */
    private void chooseKeyboardBind() {
        String[] items = new String[KeyboardBindings.ALL.length];
        for (int i = 0; i < items.length; i++) {
            KeyboardBindings.Bind bind = KeyboardBindings.ALL[i];
            items[i] = bind.label + ":  " + KeyboardBindings.describe(KeyboardBindings.keys(this, bind));
        }
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Keys")
                .setItems(items, (dialog, which) -> captureKey(KeyboardBindings.ALL[which]))
                .setNeutralButton("Restore", (dialog, which) -> {
                    KeyboardBindings.reset(this);
                    chooseKeyboardBind();
                })
                .setNegativeButton("Close", null)
                .show();
    }

    /**
     * Waits for a key of the keyboard and gives it to the button alone. Shift, Ctrl and Alt held with it make
     * a combination ("Shift+Up"); alone they wait for the key. The system's back and a controller's buttons
     * work the dialog as usual.
     */
    private void captureKey(KeyboardBindings.Bind bind) {
        AlertDialog dialog = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle(bind.label)
                .setMessage("Press a key on the keyboard (with Shift, Ctrl or Alt for a combination).\n\nNow: "
                        + KeyboardBindings.describe(KeyboardBindings.keys(this, bind)))
                .setNegativeButton("Cancel", null)
                .setOnCancelListener(d -> chooseKeyboardBind())
                .create();
        dialog.setOnKeyListener((d, keyCode, event) -> {
            if (keyCode == android.view.KeyEvent.KEYCODE_BACK || !KeyboardBindings.fromKeyboard(event)) {
                return false;
            }
            if (event.getAction() != android.view.KeyEvent.ACTION_DOWN || event.getRepeatCount() > 0
                    || KeyboardBindings.isModifier(keyCode)) {
                return true;
            }
            String key = KeyboardBindings.bindFor(event);
            if (key == null) {
                android.widget.Toast.makeText(this, "That key cannot be assigned.",
                        android.widget.Toast.LENGTH_SHORT).show();
                return true;
            }
            java.util.List<String> taken = KeyboardBindings.assign(this, bind, key);
            if (!taken.isEmpty()) {
                android.widget.Toast.makeText(this, key + " is no longer: " + String.join(", ", taken),
                        android.widget.Toast.LENGTH_LONG).show();
            }
            d.dismiss();
            chooseKeyboardBind();
            return true;
        });
        dialog.setOnShowListener(d -> dialog.getButton(AlertDialog.BUTTON_NEGATIVE)
                .setOnClickListener(v -> {
                    dialog.dismiss();
                    chooseKeyboardBind();
                }));
        dialog.show();
    }

    // ---- LAN multiplayer -----------------------------------------------------------------------------------

    private void chooseLanMode() {
        String[] modes = {LanSettings.MODE_OFF, LanSettings.MODE_JOIN, LanSettings.MODE_HOST};
        String[] labels = {"Off", "Join a server (PC or another device)",
                "Host: this device is the server"};
        int checked = java.util.Arrays.asList(modes).indexOf(LanSettings.mode(this));
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("LAN multiplayer")
                .setSingleChoiceItems(labels, checked, (dialog, which) -> {
                    dialog.dismiss();
                    String mode = modes[which];
                    LanSettings.mode(this, mode);
                    if (LanSettings.MODE_HOST.equals(mode)) {
                        // The server's notification (its IP, the players and the stop button).
                        if (Build.VERSION.SDK_INT >= 33) {
                            requestPermissions(new String[] {"android.permission.POST_NOTIFICATIONS"}, 47);
                        }
                        LanServerService.start(this);
                    } else {
                        LanServerService.stop(this);
                    }
                    refreshOptions();
                    if (LanSettings.MODE_JOIN.equals(mode) && LanSettings.server(this).isEmpty()) {
                        chooseLanServer();
                    } else if (!LanSettings.MODE_OFF.equals(mode)) {
                        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                                .setTitle("How to play on LAN")
                                .setMessage("Everyone on the same Wi-Fi (or on the host's hotspot), each "
                                        + "with a different gamertag. In the game: Xbox Live → Quick Match. One player "
                                        + "creates the match and the others join. It is experimental; single player "
                                        + "does not change.")
                                .setPositiveButton("Got it", null).show();
                    }
                })
                .setNegativeButton("Cancel", null).show();
    }

    private void editGamertag() {
        android.widget.EditText input = new android.widget.EditText(this);
        input.setSingleLine(true);
        input.setText(LanSettings.gamertag(this));
        input.setFilters(new android.text.InputFilter[] {new android.text.InputFilter.LengthFilter(15)});
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Gamertag")
                .setMessage("Up to 15 letters, numbers and spaces, starting with a letter. Your saved games do not change.")
                .setView(input)
                .setPositiveButton("Save", (dialog, which) -> {
                    String value = input.getText().toString().trim();
                    if (!LanSettings.validGamertag(value)) {
                        reportError("Invalid gamertag: up to 15 letters, numbers and spaces, starting with a letter.");
                        return;
                    }
                    LanSettings.gamertag(this, value);
                    refreshOptions();
                })
                .setNegativeButton("Cancel", null).show();
    }

    private void chooseLanServer() {
        android.widget.EditText input = new android.widget.EditText(this);
        input.setSingleLine(true);
        input.setHint("192.168.1.20");
        input.setText(LanSettings.server(this));
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("LAN server")
                .setMessage("The IP of the PC running nfsmw-server-lan, or of the host device.")
                .setView(input)
                .setPositiveButton("Save", (dialog, which) -> {
                    String value = input.getText().toString().trim();
                    if (!LanSettings.validAddress(value)) {
                        reportError("Invalid address.");
                        return;
                    }
                    LanSettings.server(this, value);
                    refreshOptions();
                })
                .setNeutralButton("Search the network", (dialog, which) -> searchLanServers())
                .setNegativeButton("Cancel", null).show();
    }

    private void searchLanServers() {
        AlertDialog searching = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setMessage("Searching for servers on the local network…").setCancelable(false).show();
        importer.execute(() -> {
            java.util.List<BuscadorServidores.Encontrado> found = BuscadorServidores.buscar();
            mainHandler.post(() -> {
                searching.dismiss();
                if (isFinishing() || isDestroyed()) return;
                if (found == null) {
                    reportError("This device is not on any local network.");
                    return;
                }
                if (found.isEmpty()) {
                    reportError("No server was found. Type the host's IP by hand.");
                    return;
                }
                String[] items = new String[found.size()];
                for (int i = 0; i < items.length; i++) {
                    BuscadorServidores.Encontrado e = found.get(i);
                    items[i] = (e.nombre.isEmpty() ? e.ip : e.nombre + " · " + e.ip) + " · " + e.jugadores
                            + (e.jugadores == 1 ? " player" : " players");
                }
                new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                        .setTitle("Servers found")
                        .setItems(items, (dialog, which) -> {
                            LanSettings.server(this, found.get(which).ip);
                            refreshOptions();
                        })
                        .setNegativeButton("Cancel", null).show();
            });
        });
    }

    private void chooseDriver() {
        java.util.List<GpuDrivers.Driver> drivers = GpuDrivers.list(this);
        String[] names = new String[drivers.size() + 1];
        names[0] = "System driver";
        int selected = 0;
        for (int i = 0; i < drivers.size(); ++i) {
            names[i + 1] = drivers.get(i).name;
            if (drivers.get(i).id.equals(GpuDrivers.selectedId(this))) selected = i + 1;
        }
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Vulkan driver")
                .setSingleChoiceItems(names, selected, (dialog, which) -> {
                    GpuDrivers.select(this, which == 0 ? "" : drivers.get(which - 1).id);
                    refreshOptions(); dialog.dismiss();
                })
                .setPositiveButton("Import ZIP", (dialog, which) -> {
                    Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*")
                            .addCategory(Intent.CATEGORY_OPENABLE);
                    startActivityForResult(picker, REQUEST_DRIVER_ZIP);
                })
                .setNeutralButton("Delete selected", (dialog, which) -> {
                    GpuDrivers.Driver driver = GpuDrivers.selected(this);
                    if (driver == null) return;
                    try { GpuDrivers.remove(this, driver); refreshOptions(); }
                    catch (IOException error) { reportError(error.getMessage()); }
                })
                .setNegativeButton("Back", null).show();
    }

    private void probeDriver(boolean playAfter) {
        playAfterProbe = playAfter;
        launchGame.setEnabled(false);
        startActivityForResult(new Intent(this, GpuProbeActivity.class), REQUEST_DRIVER_PROBE);
    }

    private void showDriverReport(String report) {
        try {
            Diagnostics.acceptGpu(this, report);
            org.json.JSONObject gpu = new org.json.JSONObject(report);
            boolean failed = gpu.optBoolean("driverLoadFailed") || gpu.has("probeError");
            String text = "GPU: " + gpu.optString("gpu", "not available")
                    + "\nVulkan: " + gpu.optString("vulkan", "not available")
                    + "\nDescriptor sets: " + gpu.optInt("maxBoundDescriptorSets")
                    + "\n" + (gpu.optBoolean("compatible") ? "The native renderer's features are available."
                            : "Missing features: " + gpu.optJSONArray("missing"));
            if (failed) text = "The driver could not be tested. " + gpu.optString("probeError", "Vulkan failed to load.");
            if (playAfterProbe && !failed) { playAfterProbe = false; playChecked(); return; }
            playAfterProbe = false;
            new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle("Vulkan driver test").setMessage(text)
                    .setPositiveButton("OK", null)
                    .setNeutralButton("Use system", (dialog, which) -> {
                        GpuDrivers.select(this, ""); refreshOptions();
                    }).show();
        } catch (Exception error) { playAfterProbe = false; reportError("Invalid diagnostics: " + error.getMessage()); }
    }

    private java.util.Map<String,String> profileValues(String prefix) {
        java.util.Map<String,String> values = new java.util.LinkedHashMap<>();
        for (String key : GraphicsPresets.KEYS) values.put(key, prefix.isEmpty() ? GameOptions.get(this, key)
                : GameOptions.prefs(this).getString(prefix + key, ""));
        return values;
    }

    private String profileLabel() {
        SharedPreferences prefs = GameOptions.prefs(this);
        return profileValues("").equals(profileValues("preset_applied_")) ? prefs.getString("preset_name", "Custom") : "Custom";
    }

    private void chooseProfile() {
        boolean saved = GameOptions.prefs(this).getBoolean("preset_has_backup", false);
        String[] names = saved ? new String[]{"Battery", "Balanced", "Performance", "Quality", "My saved settings"}
                : GraphicsPresets.NAMES;
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert).setTitle("Graphics profile")
                .setItems(names, (dialog, which) -> new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                        .setTitle(names[which]).setMessage(which == 4 ? "Your six saved graphics settings will be restored."
                                : "Resolution, FPS, antialiasing, shadows and reflections will be set. Your own values are saved so you can restore them. The result depends on the device; there are no battery or FPS measurements for this profile.")
                        .setPositiveButton("Apply on next start", (confirm, button) -> applyProfile(which))
                        .setNegativeButton("Cancel", null).show())
                .setNegativeButton("Close", null).show();
    }

    private void applyProfile(int profile) {
        SharedPreferences prefs = GameOptions.prefs(this);
        java.util.Map<String,String> next = profile == 4 ? profileValues("preset_backup_") : GraphicsPresets.values(profile);
        // Validate against the launcher's actual options before changing any preference.
        for (java.util.Map.Entry<String,String> entry : next.entrySet()) {
            if (!java.util.Arrays.asList(GameOptions.find(entry.getKey()).values).contains(entry.getValue())) {
                reportError("The saved settings do not match this version."); return;
            }
        }
        SharedPreferences.Editor edit = prefs.edit();
        if (profile != 4) {
            java.util.Map<String,String> previous = prefs.getBoolean("preset_has_backup", false) ? profileValues("preset_backup_") : null;
            java.util.Map<String,String> backup = GraphicsPresets.preserve(profileValues(""), profileValues("preset_applied_"), previous);
            for (java.util.Map.Entry<String,String> entry : backup.entrySet()) edit.putString("preset_backup_" + entry.getKey(), entry.getValue());
            edit.putBoolean("preset_has_backup", true).putString("preset_name", GraphicsPresets.NAMES[profile]);
        } else edit.putString("preset_name", "Custom");
        for (java.util.Map.Entry<String,String> entry : next.entrySet()) {
            edit.putString(entry.getKey(), entry.getValue());
            edit.putString("preset_applied_" + entry.getKey(), entry.getValue());
        }
        if (!edit.commit()) reportError("The settings could not be saved.");
        refreshOptions();
    }

    private void chooseOption(GameOptions.Option option) {
        String current = GameOptions.get(this, option.key);
        int checked = 0;
        for (int i = 0; i < option.values.length; i++) {
            if (option.values[i].equals(current)) checked = i;
        }
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle(option.title)
                .setSingleChoiceItems(option.labels, checked, (dialog, which) -> {
                    GameOptions.set(this, option.key, option.values[which]);
                    dialog.dismiss();
                    refreshOptions();
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private View optionRow(String title, String value, Runnable onClick) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(12), dp(10), dp(12), dp(10));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0x14FFFFFF);
        bg.setCornerRadius(dp(10));
        row.setBackground(bg);
        row.setClickable(true);
        row.setOnClickListener(v -> onClick.run());
        row.addView(label(title, 14, 0xFFFFFFFF, false),
                new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        TextView current = label(value + "  ›", 13, ACCENT, true);
        current.setGravity(Gravity.END);
        row.addView(current);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(6);
        row.setLayoutParams(lp);
        return row;
    }

    private void showChecks(File folder) {
        checksList.removeAllViews();
        addCheck("default.xex", new File(folder, "default.xex").isFile(), true);
        addCheck("NFS folder", new File(folder, "NFS").isDirectory(), true);
        addCheck("Movies folder", new File(folder, "Movies").isDirectory(), true);
        addCheck("Native renderer shaders", ShaderBuilder.hasLibrary(folder), false);
    }

    private void addCheck(String name, boolean ok, boolean required) {
        String mark = ok ? "✓  " : (required ? "✗  " : "!  ");
        String warning = ok || required ? "" : " · generated when you press PLAY";
        TextView row = label(mark + name + warning, 13,
                ok ? 0xFF7CD992 : (required ? 0xFFFF6B6B : 0xFFFFC857), false);
        row.setPadding(0, dp(2), 0, dp(2));
        checksList.addView(row);
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(12), dp(16), dp(12));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xCC121A22);
        bg.setCornerRadius(dp(16));
        bg.setStroke(dp(1), 0x33FFFFFF);
        card.setBackground(bg);
        return card;
    }

    private Button actionButton(String text, boolean primary) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(primary ? 0xFF1A1000 : Color.WHITE);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, primary ? 22 : 15);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setLetterSpacing(primary ? .12f : 0f);
        GradientDrawable bg = primary
                ? new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT, new int[] {0xFFFFB300, ACCENT})
                : new GradientDrawable();
        if (!primary) {
            bg.setColor(0x22FFFFFF);
            bg.setStroke(dp(1), 0x55FFFFFF);
        }
        bg.setCornerRadius(dp(14));
        b.setBackground(bg);
        b.setStateListAnimator(null);
        return b;
    }

    private TextView label(String text, int sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        if (bold) {
            t.setTypeface(Typeface.DEFAULT_BOLD);
            t.setLetterSpacing(.06f);
        }
        return t;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void selectGameFolder() {
        if (!hasStorageAccess()) {
            prepareSharedGameFolder();
            return;
        }
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        String saved = getPreferences(MODE_PRIVATE).getString(TREE_URI, null);
        if (saved != null && Build.VERSION.SDK_INT >= 26) {
            intent.putExtra("android.provider.extra.INITIAL_URI", Uri.parse(saved));
        }
        startActivityForResult(intent, REQUEST_GAME_FOLDER);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (launcherSaves != null && launcherSaves.result(requestCode, resultCode, data)) return;
        if (requestCode == REQUEST_DRIVER_PROBE) {
            launchGame.setEnabled(true);
            showDriverReport(resultCode == RESULT_OK && data != null ? data.getStringExtra("gpu_report")
                    : "{\"probeError\":\"The test closed or was cancelled. You can go back to the system driver.\",\"driverLoadFailed\":true}");
            return;
        }
        if (requestCode == REQUEST_DRIVER_ZIP) {
            if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
            Uri zip = data.getData();
            setImportStatus("Importing Vulkan driver…");
            importer.execute(() -> {
                try {
                    GpuDrivers.Driver driver = GpuDrivers.importZip(this, zip);
                    mainHandler.post(() -> {
                        if (isFinishing() || isDestroyed()) return;
                        GpuDrivers.select(this, driver.id); refreshOptions();
                        setImportStatus("Driver imported: " + driver.name + ". Use Test driver to check it.");
                    });
                } catch (Exception error) {
                    mainHandler.post(() -> reportError("The driver could not be imported: " + error.getMessage()));
                }
            });
            return;
        }
        if (requestCode == REQUEST_SAVE_REPORT) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) finishReportSave(data.getData());
            else pendingReport = null;
            return;
        }
        if (requestCode == REQUEST_STORAGE_ACCESS || requestCode == REQUEST_LEGACY_STORAGE) {
            if (hasStorageAccess()) prepareSharedGameFolder();
            else {
                selectFolder.setEnabled(true);
                setImportStatus("Allow storage access to use Internal storage/" + GAME_FOLDER_NAME + ".");
            }
            return;
        }
        if (requestCode != REQUEST_GAME_FOLDER || resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }

        Uri tree = data.getData();
        int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        try {
            getContentResolver().takePersistableUriPermission(tree, flags & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException error) {
            setImportStatus("The folder permission could not be saved: " + error.getMessage());
            return;
        }
        getPreferences(MODE_PRIVATE).edit().putString(TREE_URI, tree.toString()).apply();
        importGameFolder(tree);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_LEGACY_STORAGE) {
            if (hasStorageAccess()) prepareSharedGameFolder();
            else {
                selectFolder.setEnabled(true);
                setImportStatus("Allow storage access to use Internal storage/" + GAME_FOLDER_NAME + ".");
            }
        }
    }

    private boolean hasStorageAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) return Environment.isExternalStorageManager();
        return checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    private void prepareSharedGameFolder() {
        if (!hasStorageAccess()) {
            selectFolder.setEnabled(false);
            launchGame.setVisibility(View.GONE);
            setImportStatus("To use Internal storage/" + GAME_FOLDER_NAME + ", allow the file access Android asks for.");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                Intent settings = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivityForResult(settings, REQUEST_STORAGE_ACCESS);
            } else {
                requestPermissions(new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQUEST_LEGACY_STORAGE);
            }
            return;
        }

        selectFolder.setEnabled(false);
        launchGame.setVisibility(View.GONE);
        setImportStatus("Checking the game in internal storage...");
        importer.execute(() -> {
            File gameRoot = sharedGameRoot();
            File oldPrivateRoot = privateGameRoot();
            File parent = gameRoot.getParentFile();
            File staging = new File(parent, "." + GAME_FOLDER_NAME + ".importing");
            File previous = new File(parent, "." + GAME_FOLDER_NAME + ".previous");
            try {
                if (isValidGameFolder(gameRoot)) {
                    if (oldPrivateRoot.exists()) deleteRecursively(oldPrivateRoot);
                    mainHandler.post(() -> showSharedGameFolder(null));
                    return;
                }

                if (isValidGameFolder(oldPrivateRoot)) {
                    mainHandler.post(() -> setImportStatus("Moving the game to Internal storage/" + GAME_FOLDER_NAME + "..."));
                    deleteRecursively(staging);
                    deleteRecursively(previous);
                    copyDirectory(oldPrivateRoot, staging, staging.getCanonicalFile(), new long[]{0});
                    if (!isValidGameFolder(staging)) throw new IOException("The copy failed the file check.");
                    installStagedGame(staging, gameRoot, previous);
                    deleteRecursively(oldPrivateRoot);
                    deleteRecursively(previous);
                    mainHandler.post(() -> showSharedGameFolder("Game moved from private storage."));
                    return;
                }

                if (!gameRoot.mkdirs() && !gameRoot.isDirectory()) {
                    throw new IOException("Could not create " + gameRoot.getAbsolutePath());
                }
                mainHandler.post(() -> showSharedGameFolder(null));
            } catch (Exception error) {
                try { deleteRecursively(staging); } catch (IOException ignored) {}
                mainHandler.post(() -> {
                    setImportStatus("The game folder could not be prepared: " + error.getMessage());
                    selectFolder.setEnabled(true);
                });
            }
        });
    }

    private File sharedGameRoot() {
        return new File(Environment.getExternalStorageDirectory(), GAME_FOLDER_NAME);
    }

    private File privateGameRoot() {
        return new File(new File(getFilesDir(), "nfsmw"), "game_root");
    }

    private static boolean isValidGameFolder(File folder) {
        return new File(folder, "default.xex").isFile() &&
                new File(folder, "NFS").isDirectory() && new File(folder, "Movies").isDirectory();
    }

    private static void installStagedGame(File staging, File gameRoot, File previous) throws IOException {
        boolean movedPrevious = false;
        if (gameRoot.exists()) {
            Files.move(gameRoot.toPath(), previous.toPath(), StandardCopyOption.REPLACE_EXISTING);
            movedPrevious = true;
        }
        try {
            Files.move(staging.toPath(), gameRoot.toPath(), StandardCopyOption.REPLACE_EXISTING);
        } catch (IOException error) {
            if (movedPrevious) Files.move(previous.toPath(), gameRoot.toPath(), StandardCopyOption.REPLACE_EXISTING);
            throw error;
        }
    }

    private void importGameFolder(Uri tree) {
        selectFolder.setEnabled(false);
        launchGame.setVisibility(View.GONE);
        setImportStatus("Copying the game to Internal storage/" + GAME_FOLDER_NAME + "...");
        importer.execute(() -> {
            File gameRoot = sharedGameRoot();
            File parent = gameRoot.getParentFile();
            File staging = new File(parent, "." + GAME_FOLDER_NAME + ".importing");
            File previous = new File(parent, "." + GAME_FOLDER_NAME + ".previous");
            try {
                deleteRecursively(staging);
                deleteRecursively(previous);
                DocumentFile source = DocumentFile.fromTreeUri(this, tree);
                if (source == null || !source.isDirectory()) throw new IOException("The selected folder could not be opened.");
                copyDirectory(source, staging, staging.getCanonicalFile(), new long[]{0});
                if (!isValidGameFolder(staging)) {
                    throw new IOException("The folder must contain default.xex, NFS and Movies.");
                }
                installStagedGame(staging, gameRoot, previous);
                File oldPrivateRoot = privateGameRoot();
                if (oldPrivateRoot.exists()) deleteRecursively(oldPrivateRoot);
                deleteRecursively(previous);
                mainHandler.post(() -> {
                    showSharedGameFolder(null);
                    if ("nativo".equals(GameOptions.get(this, GameOptions.RENDERER)) &&
                            !ShaderBuilder.hasLibrary(sharedGameRoot())) buildShaders(false, false);
                });
            } catch (Exception error) {
                try { deleteRecursively(staging); } catch (IOException ignored) {}
                mainHandler.post(() -> setImportStatus("Import failed: " + error.getMessage()));
            } finally {
                mainHandler.post(() -> selectFolder.setEnabled(shaderBuilder == null));
            }
        });
    }

    private void copyDirectory(DocumentFile source, File destination, File safeRoot, long[] copiedBytes) throws IOException {
        if (!destination.getCanonicalFile().toPath().startsWith(safeRoot.toPath())) {
            throw new IOException("The game folder contains an invalid path.");
        }
        if (!destination.mkdirs() && !destination.isDirectory()) throw new IOException("Could not create " + destination);
        for (DocumentFile child : source.listFiles()) {
            String name = child.getName();
            if (name == null || name.isEmpty() || name.equals(".") || name.equals("..") ||
                    name.contains("/") || name.contains("\\")) {
                throw new IOException("The game folder contains an invalid name.");
            }
            File target = new File(destination, name);
            if (child.isDirectory()) {
                copyDirectory(child, target, safeRoot, copiedBytes);
            } else if (child.isFile()) {
                File parent = target.getParentFile();
                if (parent == null || !parent.getCanonicalFile().toPath().startsWith(safeRoot.toPath())) {
                    throw new IOException("A file tries to leave the import folder.");
                }
                if (!parent.mkdirs() && !parent.isDirectory()) throw new IOException("A game folder could not be created.");
                try (InputStream input = getContentResolver().openInputStream(child.getUri());
                     FileOutputStream output = new FileOutputStream(target)) {
                    if (input == null) throw new IOException("Could not read " + name);
                    byte[] buffer = new byte[1024 * 1024];
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        output.write(buffer, 0, count);
                        copiedBytes[0] += count;
                        if ((copiedBytes[0] & ((32L * 1024 * 1024) - 1)) < count) {
                            long total = copiedBytes[0];
                            mainHandler.post(() -> setImportStatus("Copied " + (total / (1024 * 1024)) + " MiB..."));
                        }
                    }
                    output.getFD().sync();
                }
            }
        }
    }

    private void copyDirectory(File source, File destination, File safeRoot, long[] copiedBytes) throws IOException {
        if (!destination.getCanonicalFile().toPath().startsWith(safeRoot.toPath())) {
            throw new IOException("The game folder contains an invalid path.");
        }
        if (!destination.mkdirs() && !destination.isDirectory()) throw new IOException("Could not create " + destination);
        File[] children = source.listFiles();
        if (children == null) throw new IOException("Could not read " + source.getAbsolutePath());
        byte[] buffer = new byte[1024 * 1024];
        for (File child : children) {
            File target = new File(destination, child.getName());
            if (child.isDirectory()) {
                copyDirectory(child, target, safeRoot, copiedBytes);
            } else if (child.isFile()) {
                try (InputStream input = new FileInputStream(child);
                     FileOutputStream output = new FileOutputStream(target)) {
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        output.write(buffer, 0, count);
                        copiedBytes[0] += count;
                        if ((copiedBytes[0] & ((128L * 1024 * 1024) - 1)) < count) {
                            long total = copiedBytes[0];
                            mainHandler.post(() -> setImportStatus("Moved " + (total / (1024 * 1024)) + " MiB..."));
                        }
                    }
                    output.getFD().sync();
                }
            }
        }
    }

    private void showSharedGameFolder(String notice) {
        File gameRoot = sharedGameRoot();
        selectFolder.setEnabled(true);
        showChecks(gameRoot);
        if (isValidGameFolder(gameRoot)) {
            setImportStatus((notice == null ? "" : notice + "\n") + "Internal storage/" + GAME_FOLDER_NAME);
            launchGame.setVisibility(View.VISIBLE);
            selectFolder.setText("Change game folder");
        } else {
            setImportStatus("Choose the extracted game folder (with default.xex, NFS and Movies). It will be copied to " +
                    "Internal storage/" + GAME_FOLDER_NAME + ".");
            launchGame.setVisibility(View.GONE);
            selectFolder.setText("Choose game folder");
        }
    }

    private void setImportStatus(String message) {
        if (importStatus != null) importStatus.setText(message);
    }

    private static void deleteRecursively(File file) throws IOException {
        if (!file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteRecursively(child);
        if (!file.delete()) throw new IOException("Could not delete " + file.getName());
    }

    @Override
    protected void onDestroy() {
        if (shaderBuilder != null) {
            shaderBuilder.cancel();
        }
        importer.shutdown();
        super.onDestroy();
    }
}
