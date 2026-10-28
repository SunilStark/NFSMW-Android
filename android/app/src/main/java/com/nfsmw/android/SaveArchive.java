// SPDX-License-Identifier: GPL-3.0-only
// Save ZIP workflow inspired by victorgbd/NFSMW-Recompiled-Mobile/Partidas.java (dd76acb).
// Uses our existing user folder, a strict save allowlist, SHA-256 and recoverable directory swaps.
package com.nfsmw.android;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

final class SaveArchive {
    static final String MARKER = "nfsmw_partidas.txt", HASHES = "nfsmw_checksums.sha256";
    private static final String FORMAT = "NFSMW-Evolved-Saves/1\n";
    private static final long MAX_TOTAL = 512L << 20, MAX_FILE = 64L << 20;
    private static final int MAX_FILES = 4096;

    static boolean saveRoot(String name) {
        return name.matches("[0-9A-Fa-f]{16}") || name.equals("achievements") || name.equals("saves");
    }
    private static String path(String name) throws IOException {
        if (name.length() > 1024 || name.startsWith("/") || name.contains("\\") || name.contains(":"))
            throw new IOException("Invalid saved-game path.");
        String[] parts = name.split("/", -1);
        if (parts.length < 2 || parts.length > 16 || !saveRoot(parts[0])) throw new IOException("The ZIP contains files that are not saved games.");
        for (String part : parts) {
            if (part.isEmpty() || part.equals(".") || part.equals("..") || part.startsWith(".") || part.chars().anyMatch(c -> c < 32))
                throw new IOException("Invalid saved-game path.");
        }
        String lower = name.toLowerCase(Locale.ROOT);
        if (lower.matches(".*\\.(xex|iso|nfsp|apk|so|dll|big|bun|viv|wmv|zip)$")) throw new IOException("The ZIP contains resources that are not saved games.");
        return name;
    }
    private static List<File> children(File root) throws IOException {
        if (!root.exists()) return Collections.emptyList();
        File[] files = root.listFiles();
        if (files == null) throw new IOException("The saved-games folder cannot be read.");
        Arrays.sort(files, Comparator.comparing(File::getName));
        return Arrays.asList(files);
    }
    private static void regular(File file) throws IOException {
        if (Files.isSymbolicLink(file.toPath()) || (!file.isDirectory() && !file.isFile())) throw new IOException("Links are not allowed in saved games.");
    }
    private static String hex(byte[] bytes) {
        StringBuilder value = new StringBuilder();
        for (byte b : bytes) value.append(String.format(Locale.ROOT, "%02x", b & 255));
        return value.toString();
    }
    private static MessageDigest sha() throws IOException {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (Exception error) { throw new IOException(error); }
    }
    private static long copy(InputStream in, OutputStream out, MessageDigest hash, long limit) throws IOException {
        byte[] buffer = new byte[16384]; long total = 0;
        for (int count; (count = in.read(buffer)) != -1;) {
            total += count;
            if (total > limit) throw new IOException("The copy exceeds the size limit.");
            out.write(buffer, 0, count);
            if (hash != null) hash.update(buffer, 0, count);
        }
        return total;
    }
    static synchronized int export(File root, OutputStream destination) throws IOException {
        StringBuilder hashes = new StringBuilder();
        long[] size = {0, 0};
        try (ZipOutputStream zip = new ZipOutputStream(destination)) {
            zip.putNextEntry(new ZipEntry(MARKER)); zip.write(FORMAT.getBytes(StandardCharsets.UTF_8)); zip.closeEntry();
            for (File file : children(root)) if (saveRoot(file.getName())) add(zip, file, file.getName(), hashes, size);
            zip.putNextEntry(new ZipEntry(HASHES)); zip.write(hashes.toString().getBytes(StandardCharsets.UTF_8)); zip.closeEntry();
        }
        return (int) size[0];
    }
    private static void add(ZipOutputStream zip, File file, String name, StringBuilder hashes, long[] size) throws IOException {
        regular(file);
        if (name.split("/").length > 16) throw new IOException("Saved-games folder nested too deep.");
        if (file.isDirectory()) {
            for (File child : children(file)) add(zip, child, name + "/" + child.getName(), hashes, size);
            return;
        }
        path(name);
        if (++size[0] > MAX_FILES) throw new IOException("Too many saved-game files.");
        zip.putNextEntry(new ZipEntry(name)); MessageDigest hash = sha();
        try (InputStream input = new FileInputStream(file)) { size[1] += copy(input, zip, hash, MAX_FILE); }
        if (size[1] > MAX_TOTAL) throw new IOException("Saved games too large.");
        zip.closeEntry(); hashes.append(hex(hash.digest())).append("  ").append(name).append('\n');
    }
    private static String text(ZipInputStream zip, int limit) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(); copy(zip, out, null, limit);
        return new String(out.toByteArray(), StandardCharsets.UTF_8);
    }
    private static int extract(InputStream source, File root) throws IOException {
        Map<String, String> actual = new TreeMap<>(), expected = new TreeMap<>();
        Set<String> seen = new HashSet<>(); long total = 0; int entries = 0;
        boolean marker = false, checksums = false;
        try (ZipInputStream zip = new ZipInputStream(source)) {
            for (ZipEntry entry; (entry = zip.getNextEntry()) != null;) {
                String name = entry.getName();
                if (++entries > MAX_FILES + 2 || !seen.add(name.toLowerCase(Locale.ROOT))) throw new IOException("ZIP with repeated or too many entries.");
                if (entry.isDirectory()) throw new IOException("This format only uses file entries.");
                if (name.equals(MARKER)) { marker = FORMAT.equals(text(zip, 128)); if (!marker) throw new IOException("Unrecognized backup format."); }
                else if (name.equals(HASHES)) {
                    checksums = true;
                    for (String line : text(zip, 5 << 20).split("\n")) {
                        if (line.isEmpty()) continue;
                        if (line.length() < 67 || !line.substring(0, 64).matches("[0-9a-f]{64}") || !line.substring(64,66).equals("  ")) throw new IOException("Invalid integrity table.");
                        if (expected.put(path(line.substring(66)), line.substring(0, 64)) != null) throw new IOException("Repeated integrity table.");
                    }
                } else {
                    path(name);
                    File file = new File(root, name);
                    if (!file.getCanonicalPath().startsWith(root.getCanonicalPath() + File.separator)) throw new IOException("Path outside the allowed folder.");
                    Files.createDirectories(file.getParentFile().toPath()); MessageDigest hash = sha();
                    try (OutputStream out = new FileOutputStream(file)) { total += copy(zip, out, hash, MAX_FILE); }
                    if (total > MAX_TOTAL) throw new IOException("Saved-games ZIP too large.");
                    actual.put(name, hex(hash.digest()));
                }
                zip.closeEntry(); // Also verifies ZIP CRC before any live files are changed.
            }
        }
        if (!marker || !checksums || !actual.equals(expected)) throw new IOException("The backup's integrity does not match.");
        return actual.size();
    }
    private static File work(File root) throws IOException {
        // Android may expose /data/user/0 through a trusted system alias. Resolve ancestors, reject the user
        // directory itself being a link, and place the transaction beside the actual user directory.
        if (root.getParentFile() == null || Files.isSymbolicLink(root.toPath())) throw new IOException("Invalid user folder.");
        return new File(root.getCanonicalFile().getParentFile(), ".nfsmw-save-transaction");
    }
    private static void move(File from, File to) throws IOException { Files.move(from.toPath(), to.toPath(), StandardCopyOption.ATOMIC_MOVE); }
    private static void erase(File file) throws IOException {
        if (!file.exists()) return;
        regular(file);
        if (file.isDirectory()) for (File child : children(file)) erase(child);
        Files.delete(file.toPath());
    }
    static synchronized void recover(File root) throws IOException {
        File work = work(root), old = new File(work, "old");
        if (!work.exists()) return;
        regular(work);
        if (!work.isDirectory()) throw new IOException("The recovery folder is not valid.");
        if (old.exists() && !new File(work, "committed").exists()) {
            // A crash between the two renames (or before the commit marker) rolls back the whole user tree.
            erase(root); move(old, root);
        }
        if (!root.isDirectory()) throw new IOException("Recovery pending: keep the backup folder.");
        erase(work);
    }
    private static void keep(File source, File target, int depth) throws IOException {
        if (depth > 32) throw new IOException("User folder nested too deep.");
        regular(source);
        if (source.isDirectory()) {
            Files.createDirectories(target.toPath());
            for (File child : children(source)) keep(child, new File(target, child.getName()), depth + 1);
        } else {
            try { Files.createLink(target.toPath(), source.toPath()); }
            catch (IOException | UnsupportedOperationException error) { Files.copy(source.toPath(), target.toPath(), StandardCopyOption.COPY_ATTRIBUTES); }
        }
    }
    static synchronized File backup(File root, File backups) throws IOException {
        Files.createDirectories(backups.toPath());
        File file = new File(backups, "partidas-" + System.currentTimeMillis() + "-" + UUID.randomUUID() + ".zip");
        File temporary = work(root); Files.createDirectories(temporary.toPath());
        File partial = Files.createTempFile(temporary.toPath(), "backup-", ".tmp").toFile();
        File check = Files.createTempDirectory(temporary.toPath(), "verify-").toFile();
        try {
            try (FileOutputStream out = new FileOutputStream(partial)) { export(root, out); }
            try (InputStream in = new FileInputStream(partial)) { extract(in, check); }
            move(partial, file);
            return file;
        } finally { erase(check); Files.deleteIfExists(partial.toPath()); }
    }
    static synchronized File restore(File root, InputStream source, File backups) throws IOException {
        try (InputStream owned = source) { return restoreChecked(root, owned, backups); }
    }
    private static File restoreChecked(File root, InputStream source, File backups) throws IOException {
        Files.createDirectories(root.toPath()); recover(root);
        File work = work(root), next = new File(work, "next"), old = new File(work, "old");
        Files.createDirectories(next.toPath()); File backup = null;
        try {
            if (extract(source, next) == 0) throw new IOException("The backup contains no saved games.");
            if (root.getUsableSpace() < (16L << 20)) throw new IOException("Not enough free space for the backup.");
            backup = backup(root, backups);
            // Retain settings, shaders, caches and logs. Hard links avoid copying large caches on the same filesystem.
            for (File child : children(root)) if (!saveRoot(child.getName())) keep(child, new File(next, child.getName()), 0);
            move(root, old); move(next, root);
            File marker = new File(work, "commit.tmp");
            try (FileOutputStream committed = new FileOutputStream(marker)) { committed.write(1); committed.getFD().sync(); }
            move(marker, new File(work, "committed"));
        } catch (IOException error) {
            try { recover(root); } catch (IOException recovery) { error.addSuppressed(recovery); }
            throw error;
        }
        // Cleanup may be retried at the next launcher start; a valid committed import is already usable.
        try { recover(root); } catch (IOException ignored) { }
        return backup;
    }
}
