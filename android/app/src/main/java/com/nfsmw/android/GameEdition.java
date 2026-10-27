package com.nfsmw.android;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.security.MessageDigest;

/** XEX identity and the native module actually included in this APK. */
final class GameEdition {
    static final String SUPPORTED_SHA256 = "aad15fc218d034de3131f746e0b763dd8049a40dd676efc87667fcaf4fc72694";
    static final String SUPPORTED_NAME = "PAL (España)";
    private static final String PREFS = "nfsmw_edition";

    private static final java.util.List<CompiledEditions.Entry> COMPILED =
            CompiledEditions.parse(BuildConfig.GAME_EDITIONS);
    final CompiledEditions.Entry compiled;
    final String sha256;
    final String name;  // the manifest's edition, or null if the executable is not in it

    private GameEdition(String sha256, String name) {
        this.compiled = CompiledEditions.find(COMPILED, sha256);
        this.sha256 = sha256;
        this.name = name;
    }

    boolean supported() {
        return compiled != null;
    }

    String describe() {
        if (supported()) return compiled.name;
        return name != null ? name : "desconocida";
    }

    /** The edition of the game folder's default.xex, or null if it cannot be read. Cached by size and date. */
    static GameEdition of(Context context, File gameRoot) {
        File xex = new File(gameRoot, "default.xex");
        if (!xex.isFile()) return null;
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String key = xex.getAbsolutePath() + ":" + xex.length() + ":" + xex.lastModified();
        String sha = key.equals(prefs.getString("key", "")) ? prefs.getString("sha256", null) : null;
        if (sha == null) {
            sha = hash(xex);
            if (sha == null) return null;
            prefs.edit().putString("key", key).putString("sha256", sha).apply();
        }
        return new GameEdition(sha, editionName(context, sha));
    }

    static String compiledNames() {
        StringBuilder names = new StringBuilder();
        for (CompiledEditions.Entry entry : COMPILED) {
            if (names.length() > 0) names.append(", ");
            names.append(entry.name);
        }
        return names.toString();
    }

    static String hash(File file) {
        try (InputStream in = new FileInputStream(file)) {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] buffer = new byte[1 << 16];
            for (int n; (n = in.read(buffer)) > 0;) digest.update(buffer, 0, n);
            StringBuilder hex = new StringBuilder();
            for (byte b : digest.digest()) hex.append(String.format("%02x", b));
            return hex.toString();
        } catch (Exception e) { return null; }
    }

    private static String editionName(Context context, String sha) {
        try (InputStream in = context.getAssets().open("shaders/release/manifest.json")) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            for (int n; (n = in.read(buffer)) > 0; ) out.write(buffer, 0, n);
            JSONArray builds = new JSONObject(out.toString("UTF-8")).getJSONArray("builds");
            for (int i = 0; i < builds.length(); i++) {
                JSONObject build = builds.getJSONObject(i);
                if (sha.equals(build.optString("xex_sha256"))) return build.optString("edition", null);
            }
        } catch (Exception ignored) {
            // Without the manifest only "supported or not" is known.
        }
        return null;
    }
}
