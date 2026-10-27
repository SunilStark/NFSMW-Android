package com.nfsmw.android;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Only editions whose native modules were included by the build may run. */
final class CompiledEditions {
    static final class Entry {
        final String id, name, sha256, library;
        final int language, country;
        Entry(String[] f) {
            id = f[0]; name = f[1]; sha256 = f[2];
            language = Integer.parseInt(f[3]); country = Integer.parseInt(f[4]); library = f[5];
        }
    }

    // Table format adapted from victorgbd/NFSMW-Recompiled-Mobile, GPL-3.0.
    static List<Entry> parse(String table) {
        List<Entry> entries = new ArrayList<>();
        for (String row : table.split(";", -1)) {
            String[] f = row.split("\\|", -1);
            if (f.length != 6 || !f[0].matches("[a-z][a-z0-9_]{0,31}")
                    || f[1].isEmpty() || !f[2].matches("[a-f0-9]{64}")
                    || !f[3].matches("[0-9]{1,3}") || !f[4].matches("[0-9]{1,3}")
                    || !(f[5].equals("main") || f[5].equals("main_" + f[0]))) {
                throw new IllegalArgumentException("Invalid compiled edition registry");
            }
            Entry entry = new Entry(f);
            for (Entry previous : entries) {
                if (previous.id.equals(entry.id) || previous.sha256.equals(entry.sha256)
                        || previous.library.equals(entry.library)) {
                    throw new IllegalArgumentException("Duplicate compiled edition");
                }
            }
            entries.add(entry);
        }
        return Collections.unmodifiableList(entries);
    }

    static Entry find(List<Entry> entries, String hash) {
        for (Entry entry : entries) if (entry.sha256.equals(hash)) return entry;
        return null;
    }
}
