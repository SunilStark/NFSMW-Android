package com.nfsmw.android;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.ConsoleMessage;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Builds nfsmw_shaders.nfsp on the phone from the player's own game files.
 *
 * It runs the nfsmw-nx installer's shader pipeline (XenosRecomp, DXC and the packer compiled to WebAssembly,
 * copied to assets/shaders) in a hidden WebView. The page and the game files are served under
 * https://appassets.androidplatform.net/ (a secure origin, which WebCrypto and ES modules need): the pages from
 * the APK assets, the game files streamed from the game folder. The finished library is checked against the
 * SHA-256 expected for the Android library in the page and written next to default.xex, with its version
 * (LIBRARY_VERSION) beside it.
 */
final class ShaderBuilder {
    static final String LIBRARY = "nfsmw_shaders.nfsp";
    // Version of assets/shaders/shader_common.h the library was built with, kept next to it. A library from an
    // older version is built again: "sin-punteros-1" (v0.3.7) dropped the 64-bit pointer path, so the shaders
    // run on Vulkan 1.1 and on drivers without shaderInt64.
    static final String LIBRARY_VERSION = "sin-punteros-1";
    // The OpenGL ES backend's library (nfsmw_nativo_api = gles): the same shaders with one texture per sampler
    // register instead of descriptor heaps, built from the containers of the Vulkan one (assets/shaders/build.js).
    static final String LIBRARY_GLES = "nfsmw_shaders_gles.nfsp";
    static final String LIBRARY_GLES_VERSION = "gles-1";
    private static final String TAG = "NFSMW";
    private static final String HOST = "appassets.androidplatform.net";

    interface Listener {
        void onProgress(float fraction, String text);

        void onDone(File library);

        void onError(String message);
    }

    private final Activity activity;
    private final File gameRoot;
    private final Listener listener;
    private final boolean gles;
    private final Handler main = new Handler(Looper.getMainLooper());
    private WebView web;
    private boolean finished;

    /** gles: build LIBRARY_GLES, which needs a current LIBRARY. */
    ShaderBuilder(Activity activity, File gameRoot, boolean gles, Listener listener) {
        this.activity = activity;
        this.gameRoot = gameRoot;
        this.gles = gles;
        this.listener = listener;
    }

    private static String name(boolean gles) {
        return gles ? LIBRARY_GLES : LIBRARY;
    }

    private static String version(boolean gles) {
        return gles ? LIBRARY_GLES_VERSION : LIBRARY_VERSION;
    }

    private static String version(File root, boolean gles) {
        String hash = GameEdition.hash(new File(root, "default.xex"));
        if (hash == null) return "missing-xex";
        // Keep the existing PAL cache valid; other executables must never reuse it.
        return GameEdition.SUPPORTED_SHA256.equals(hash) ? version(gles) : version(gles) + ":" + hash;
    }

    /** Whether the game folder already has a library of the current version and XEX. */
    static boolean hasLibrary(File gameRoot) {
        return hasLibrary(gameRoot, false);
    }

    static boolean hasLibrary(File gameRoot, boolean gles) {
        File f = new File(gameRoot, name(gles));
        if (!f.isFile() || f.length() <= 24) {
            return false;
        }
        try (InputStream in = new FileInputStream(new File(gameRoot, name(gles) + ".version"))) {
            byte[] buffer = new byte[160];
            int n = in.read(buffer);
            return n > 0 && version(gameRoot, gles).equals(new String(buffer, 0, n, "UTF-8").trim());
        } catch (IOException e) {
            return false;
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    void start() {
        web = new WebView(activity);
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        web.addJavascriptInterface(new Bridge(), "NfsmwShaders");
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onConsoleMessage(ConsoleMessage message) {
                Log.i(TAG, "[shaders] " + message.message());
                return true;
            }
        });
        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                return serve(request.getUrl());
            }

            @Override
            public boolean onRenderProcessGone(WebView view, android.webkit.RenderProcessGoneDetail detail) {
                finish(null, "the shader compiler closed (not enough memory)");
                return true;
            }
        });
        // It must be attached to run at full speed; 1x1 and behind everything.
        ViewGroup root = activity.findViewById(android.R.id.content);
        root.addView(web, 0, new FrameLayout.LayoutParams(1, 1));
        web.loadUrl("https://" + HOST + "/shaders/index.html");
    }

    void cancel() {
        finish(null, null);
    }

    // ---- Serving ---------------------------------------------------------------------------------------------

    private WebResourceResponse serve(Uri uri) {
        if (!HOST.equals(uri.getHost()) || uri.getPath() == null) {
            return null;
        }
        String path = uri.getPath();
        try {
            InputStream in;
            long length;
            if (path.startsWith("/shaders/")) {
                String asset = path.substring(1);
                if (asset.contains("..")) {
                    return notFound();
                }
                in = activity.getAssets().open(asset);
                length = -1;
            } else if (path.startsWith("/game/")) {
                File file = gameFile(path.substring("/game/".length()));
                if (file == null) {
                    return notFound();
                }
                in = new FileInputStream(file);
                length = file.length();
            } else {
                return notFound();
            }
            Map<String, String> headers = new HashMap<>();
            headers.put("Access-Control-Allow-Origin", "*");
            headers.put("Cache-Control", "no-store");
            if (length >= 0) {
                headers.put("Content-Length", Long.toString(length));
            }
            return new WebResourceResponse(mime(path), null, 200, "OK", headers, in);
        } catch (IOException e) {
            return notFound();
        }
    }

    /** Only default.xex, the Vulkan library and the files of NFS/, never anything outside the game folder. */
    private File gameFile(String relative) throws IOException {
        File file = new File(gameRoot, relative).getCanonicalFile();
        File root = gameRoot.getCanonicalFile();
        if (!file.toPath().startsWith(root.toPath()) || !file.isFile()) {
            return null;
        }
        String rel = root.toPath().relativize(file.toPath()).toString().replace('\\', '/');
        boolean allowed = rel.equalsIgnoreCase("default.xex") || rel.equals(LIBRARY) ||
                (rel.toLowerCase(Locale.ROOT).startsWith("nfs/") && rel.indexOf('/', 4) < 0);
        return allowed ? file : null;
    }

    private static WebResourceResponse notFound() {
        return new WebResourceResponse("text/plain", "utf-8", 404, "Not Found", null, null);
    }

    private static String mime(String path) {
        if (path.endsWith(".mjs") || path.endsWith(".js")) return "text/javascript";
        if (path.endsWith(".wasm")) return "application/wasm";
        if (path.endsWith(".html")) return "text/html";
        if (path.endsWith(".json")) return "application/json";
        if (path.endsWith(".h")) return "text/plain";
        return "application/octet-stream";
    }

    // ---- Page interface --------------------------------------------------------------------------------------

    private final class Bridge {
        @JavascriptInterface
        public String listDiscFiles() {
            // NFS/*.bin sorted by lower-case path, as the installer does: the container names depend on it.
            List<String[]> list = new ArrayList<>();
            File[] children = new File(gameRoot, "NFS").listFiles();
            if (children != null) {
                for (File f : children) {
                    long size = f.length();
                    if (f.isFile() && f.getName().toLowerCase(Locale.ROOT).endsWith(".bin") &&
                            size >= 24 && size <= 1024L * 1024 * 1024) {
                        list.add(new String[] {"NFS/" + f.getName(), Long.toString(size)});
                    }
                }
            }
            Collections.sort(list, (a, b) -> a[0].toLowerCase(Locale.ROOT).compareTo(b[0].toLowerCase(Locale.ROOT)));
            JSONArray out = new JSONArray();
            try {
                for (String[] f : list) {
                    out.put(new JSONObject().put("path", f[0]).put("size", Long.parseLong(f[1])));
                }
            } catch (JSONException ignored) {
                // Unreachable: plain strings and numbers.
            }
            return out.toString();
        }

        @JavascriptInterface
        public boolean gles() {
            return gles;
        }

        @JavascriptInterface
        public void progress(float fraction, String text) {
            main.post(() -> {
                if (!finished) {
                    listener.onProgress(fraction, text);
                }
            });
        }

        @JavascriptInterface
        public void done(String base64, String sha256, String edition) {
            String library = name(gles);
            File target = new File(gameRoot, library);
            File temp = new File(gameRoot, library + ".tmp");
            try {
                byte[] bytes = Base64.decode(base64, Base64.DEFAULT);
                try (FileOutputStream out = new FileOutputStream(temp)) {
                    out.write(bytes);
                    out.getFD().sync();
                }
                if (!temp.renameTo(target)) {
                    throw new IOException("could not save " + library);
                }
                try (FileOutputStream out = new FileOutputStream(new File(gameRoot, library + ".version"))) {
                    out.write(version(gameRoot, gles).getBytes("UTF-8"));
                    out.getFD().sync();
                }
                Log.i(TAG, "[shaders] " + library + " " + bytes.length + " bytes, SHA-256 " + sha256 +
                        (edition.isEmpty() ? "" : " (" + edition + ")"));
                main.post(() -> finish(target, null));
            } catch (IOException | IllegalArgumentException e) {
                temp.delete();
                main.post(() -> finish(null, e.getMessage()));
            }
        }

        @JavascriptInterface
        public void fail(String message) {
            main.post(() -> finish(null, message));
        }
    }

    private void finish(File library, String error) {
        if (finished) {
            return;
        }
        finished = true;
        if (web != null) {
            ViewGroup parent = (ViewGroup) web.getParent();
            if (parent != null) {
                parent.removeView(web);
            }
            web.destroy();
            web = null;
        }
        if (library != null) {
            listener.onDone(library);
        } else if (error != null) {
            listener.onError(error);
        }
    }
}
