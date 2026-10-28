package com.nfsmw.android;

/** Host-side checks: javac -d out/... CompiledEditions.java CompiledEditionsTest.java */
public final class CompiledEditionsTest {
    private static final String PAL = "a".repeat(64), USA = "b".repeat(64);
    private static final String ROW = "pal_es|PAL Spain|" + PAL + "|5|31|main";
    private static void reject(String text) {
        try { CompiledEditions.parse(text); }
        catch (IllegalArgumentException expected) { return; }
        throw new AssertionError("Accepted invalid registry: " + text);
    }
    public static void main(String[] args) {
        var entries = CompiledEditions.parse(ROW + ";usa|USA|" + USA + "|1|103|main_usa");
        if (!CompiledEditions.find(entries, PAL).library.equals("main")) throw new AssertionError();
        if (!CompiledEditions.find(entries, USA).library.equals("main_usa")) throw new AssertionError();
        if (CompiledEditions.find(entries, "c".repeat(64)) != null) throw new AssertionError("Unknown fallback");
        if (CompiledEditions.find(CompiledEditions.parse(ROW), USA) != null) throw new AssertionError("Unbuilt edition");
        reject("");
        reject(ROW + ";" + ROW);
        reject(ROW + ";usa|USA|" + PAL + "|1|103|main_usa");
        reject(ROW + ";usa|USA|" + USA + "|1|103|main");
        reject(ROW.replace("|main", "|../../main"));
        reject(ROW.replace("|5|", "|-1|"));
        reject(ROW.replace(PAL, "a"));
        reject(ROW.replace("pal_es|", "../pal_es|"));
        System.out.println("CompiledEditionsTest passed");
    }
}
