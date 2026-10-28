package com.nfsmw.android;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.IOException;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.util.List;

/**
 * LAN multiplayer (experimental). The game's Xbox Live goes to an EA lobby server on the local network: a PC
 * running nfsmw-server-lan, or a phone hosting ServidorEa (LanServerService). The engine side is the SDK's
 * grant_user_privileges, red_lan, red_live and red_dns cvars (xam_user.cpp, xam_net.cpp). Off by default: the
 * single-player game never sees any of it.
 */
final class LanSettings {
    static final String MODE_OFF = "off";
    static final String MODE_JOIN = "join";
    static final String MODE_HOST = "host";

    private static final String PREFS = "nfsmw_lan";

    private LanSettings() {
    }

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static String mode(Context context) {
        String m = prefs(context).getString("mode", MODE_OFF);
        return MODE_JOIN.equals(m) || MODE_HOST.equals(m) ? m : MODE_OFF;
    }

    static void mode(Context context, String mode) {
        prefs(context).edit().putString("mode", mode).apply();
    }

    /** The server address typed or found for MODE_JOIN (an IPv4 or a host name). */
    static String server(Context context) {
        return prefs(context).getString("server", "");
    }

    static void server(Context context, String server) {
        prefs(context).edit().putString("server", server.trim()).apply();
    }

    /** Profile name (user_gamertag). Empty = the SDK's "User". Each phone in a race needs its own. */
    static String gamertag(Context context) {
        return prefs(context).getString("gamertag", "");
    }

    static void gamertag(Context context, String gamertag) {
        prefs(context).edit().putString("gamertag", gamertag.trim()).apply();
    }

    /** What Xbox 360 accepts: up to 15 letters, digits and spaces, starting with a letter. Empty is allowed. */
    static boolean validGamertag(String g) {
        return g.isEmpty() || g.matches("[A-Za-z][A-Za-z0-9 ]{0,14}");
    }

    static boolean validIp(String ip) {
        String[] parts = ip.split("\\.", -1);
        if (parts.length != 4) {
            return false;
        }
        for (String part : parts) {
            if (part.isEmpty() || part.length() > 3 || !part.matches("[0-9]+") || Integer.parseInt(part) > 255) {
                return false;
            }
        }
        return true;
    }

    /** An IPv4 or a host name ("my-pc", "server.lan"). Only digits and dots must be a whole IP. */
    static boolean validAddress(String d) {
        if (d == null || d.isEmpty() || d.length() > 253) {
            return false;
        }
        if (d.matches("[0-9.]+")) {
            return validIp(d);
        }
        return d.matches("(?i)[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?(\\.[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?)*");
    }

    /** The IPv4 of an address, resolving a name; null if not found. Blocks: call it off the main thread. */
    static String resolveIpv4(String address) {
        if (validIp(address)) {
            return address;
        }
        try {
            for (InetAddress a : InetAddress.getAllByName(address)) {
                if (a instanceof Inet4Address) {
                    return a.getHostAddress();
                }
            }
        } catch (IOException | SecurityException ignored) {
        }
        return null;
    }

    /**
     * The server the game talks to: this phone's own LAN IP when it hosts (never 127.0.0.1: the server puts
     * that address in the game record the others connect to), or the chosen one. Null = no LAN play. Blocks
     * when the chosen server is a name.
     */
    static String effectiveServer(Context context) {
        String mode = mode(context);
        if (MODE_HOST.equals(mode)) {
            return ServidorEa.ipLocal();
        }
        if (MODE_JOIN.equals(mode) && validAddress(server(context))) {
            return resolveIpv4(server(context));
        }
        return null;
    }

    /** Adds the engine arguments. server: effectiveServer(), already resolved. */
    static void arguments(Context context, String server, List<String> args) {
        String gamertag = gamertag(context);
        if (validGamertag(gamertag) && !gamertag.isEmpty()) {
            args.add("--user_gamertag=" + gamertag);
        }
        if (server != null && validIp(server)) {
            args.add("--grant_user_privileges=true");
            args.add("--red_lan=true");
            args.add("--red_live=true");
            args.add("--red_dns=" + server);
        }
    }

    static String describe(Context context) {
        switch (mode(context)) {
            case MODE_HOST:
                String ip = ServidorEa.ipLocal();
                return "Host on this device" + (ip != null ? " · " + ip : " · no local network");
            case MODE_JOIN:
                String s = server(context);
                return s.isEmpty() ? "Join · no server set" : "Join " + s;
            default:
                return "Off";
        }
    }
}
