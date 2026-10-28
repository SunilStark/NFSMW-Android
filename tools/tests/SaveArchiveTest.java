package com.nfsmw.android;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

public final class SaveArchiveTest {
    static void check(boolean ok) { if (!ok) throw new AssertionError(); }
    static void write(File root, String path, String text) throws IOException {
        File target = new File(root, path); Files.createDirectories(target.getParentFile().toPath()); Files.writeString(target.toPath(), text);
    }
    static String read(File root, String path) throws IOException { return Files.readString(new File(root, path).toPath()); }
    static byte[] zip(File root) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(); SaveArchive.export(root, out); return out.toByteArray();
    }
    static byte[] entries(String... names) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        try (ZipOutputStream z = new ZipOutputStream(out)) {
            for (String name : names) {
                z.putNextEntry(new ZipEntry(name)); z.write("malformed".getBytes()); z.closeEntry();
            }
        }
        return out.toByteArray();
    }
    static void rejects(File root, File backups, byte[] data) throws Exception {
        String before = read(root, "saves/Driver/actual/data");
        try { SaveArchive.restore(root, new ByteArrayInputStream(data), backups); throw new AssertionError("Accepted invalid ZIP"); }
        catch (IOException expected) { }
        check(read(root, "saves/Driver/actual/data").equals(before));
        check(read(root, "nfsmw.toml").equals("configuration"));
    }
    static void erase(Path root) throws IOException {
        try (var files = Files.walk(root)) {
            for (Path p : files.sorted(Comparator.reverseOrder()).toList()) Files.delete(p);
        }
    }
    public static void main(String[] args) throws Exception {
        Path folder = Files.createTempDirectory("nfsmw-save-test-");
        try {
            File source = folder.resolve("source").toFile(), root = folder.resolve("user").toFile(), backups = folder.resolve("backups").toFile();
            write(source, "saves/Driver/actual/data", "imported");
            write(source, "0009000000000000/profile/setting", "profile");
            write(source, "achievements/list.json", "achievements");
            write(source, "nfsmw.toml", "must-not-export"); write(source, "cache/pipelines", "cache");
            write(source, "logs/session.log", "log"); write(source, "gpu_drivers/driver.so", "driver");
            byte[] valid = zip(source);
            try (ZipInputStream z = new ZipInputStream(new ByteArrayInputStream(valid))) {
                int count = 0;
                for (ZipEntry e; (e = z.getNextEntry()) != null;) { check(!e.getName().contains("cache") && !e.getName().contains(".toml") && !e.getName().contains("logs") && !e.getName().contains("driver.so")); ++count; }
                check(count == 5);
            }
            write(root, "saves/Driver/actual/data", "original");
            write(root, "nfsmw.toml", "configuration"); write(root, "cache/pipelines", "keep-cache");
            Files.createDirectory(folder.resolve("alias-parent"));
            SaveArchive.recover(folder.resolve("alias-parent/../user").toFile());
            File backup = SaveArchive.restore(root, new ByteArrayInputStream(valid), backups);
            check(backup.isFile() && read(root, "saves/Driver/actual/data").equals("imported"));
            check(read(root, "nfsmw.toml").equals("configuration") && read(root,"cache/pipelines").equals("keep-cache"));
            check(read(root,"0009000000000000/profile/setting").equals("profile"));
            SaveArchive.restore(root, new FileInputStream(backup), backups);
            check(read(root,"saves/Driver/actual/data").equals("original"));
            rejects(root, backups, Arrays.copyOf(valid, 40));
            rejects(root, backups, entries("../escape", "nfsmw_checksums.sha256"));
            rejects(root, backups, entries("saves/../nfsmw.toml"));
            rejects(root, backups, entries("saves/payload.xex"));
            rejects(root, backups, entries("saves/Name", "saves/name"));
            rejects(root, backups, entries("saves/data"));
            // Valid ZIP CRC but tampered payload and original SHA-256 manifest.
            ByteArrayOutputStream corrupt = new ByteArrayOutputStream();
            try (ZipInputStream z = new ZipInputStream(new ByteArrayInputStream(valid)); ZipOutputStream out = new ZipOutputStream(corrupt)) {
                for (ZipEntry e; (e=z.getNextEntry())!=null;) {
                    byte[] bytes = z.readAllBytes();
                    if (e.getName().equals("saves/Driver/actual/data")) bytes[0] ^= 1;
                    out.putNextEntry(new ZipEntry(e.getName())); out.write(bytes); out.closeEntry();
                }
            }
            rejects(root, backups, corrupt.toByteArray());
            // Small compressed input expanding past the 64 MiB per-file cap must fail before touching saves.
            ByteArrayOutputStream oversized = new ByteArrayOutputStream();
            try (ZipOutputStream out = new ZipOutputStream(oversized)) {
                out.putNextEntry(new ZipEntry(SaveArchive.MARKER)); out.write("NFSMW-Evolved-Saves/1\n".getBytes()); out.closeEntry();
                out.putNextEntry(new ZipEntry("saves/Driver/oversized"));
                byte[] zeros = new byte[8192];
                for (int i=0;i<8193;++i) out.write(zeros);
                out.closeEntry();
            }
            try { SaveArchive.restore(root, new ByteArrayInputStream(oversized.toByteArray()), backups); throw new AssertionError("Accepted oversized save"); }
            catch (IOException expected) { check(expected.getMessage().contains("limit")); }
            check(read(root,"saves/Driver/actual/data").equals("original"));
            // Simulate process death after moving the original, before installing the new directory.
            File work = folder.resolve(".nfsmw-save-transaction").toFile(); Files.createDirectory(work.toPath());
            Files.move(root.toPath(), new File(work,"old").toPath());
            SaveArchive.recover(root);
            check(read(root,"saves/Driver/actual/data").equals("original") && !work.exists());
            // Simulate death after installation but before writing the commit marker: roll back.
            Files.createDirectory(work.toPath()); Files.move(root.toPath(), new File(work,"old").toPath());
            write(root,"saves/Driver/actual/data","uncommitted"); SaveArchive.recover(root);
            check(read(root,"saves/Driver/actual/data").equals("original"));
            // Completed commit remains installed; only its old tree is removed.
            Files.createDirectory(work.toPath()); Files.move(root.toPath(), new File(work,"old").toPath());
            write(root,"saves/Driver/actual/data","committed"); write(work,"committed","1"); SaveArchive.recover(root);
            check(read(root,"saves/Driver/actual/data").equals("committed") && !work.exists());
            System.out.println("Save archives: round trip, backups, exclusions, integrity, traversal, duplicates, interrupted swaps and committed recovery passed.");
        } finally { erase(folder); }
    }
}
