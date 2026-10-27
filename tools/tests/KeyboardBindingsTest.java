package com.nfsmw.android;

import android.view.KeyEvent;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The launcher's keyboard binds against the SDK they are passed to: every button is one of the keyboard
 * driver's keybind_* cvars with its default (sdk/src/input/mnk/mnk_input_driver.cpp), and every key name the
 * launcher can write is one keybinds.cpp knows. Needs the SDK's android.jar for KeyEvent's constants:
 *   javac -encoding UTF-8 -cp $ANDROID_HOME/platforms/android-35/android.jar -d out/keyboard-tests \
 *       android/app/src/main/java/com/nfsmw/android/KeyboardBindings.java tools/tests/KeyboardBindingsTest.java
 *   java -cp out/keyboard-tests com.nfsmw.android.KeyboardBindingsTest
 */
public final class KeyboardBindingsTest {
    static void check(boolean ok, String what) { if (!ok) throw new AssertionError(what); }

    public static void main(String[] args) throws Exception {
        String driver = Files.readString(Path.of("sdk/src/input/mnk/mnk_input_driver.cpp"));
        String keybinds = Files.readString(Path.of("sdk/src/ui/keybinds.cpp"));

        Map<String, String> sdk = new LinkedHashMap<>();
        Matcher bind = Pattern.compile("REXCVAR_DEFINE_STRING\\((keybind_\\w+),\\s*\"([^\"]*)\"").matcher(driver);
        while (bind.find()) sdk.put(bind.group(1), bind.group(2));
        Set<String> names = new HashSet<>();
        Matcher name = Pattern.compile("\\{\"(\\w+)\",\\s*VirtualKey::").matcher(keybinds);
        while (name.find()) names.add(name.group(1));
        check(!sdk.isEmpty() && names.size() > 50, "the SDK's tables were read");

        // every button the launcher shows is a cvar of the driver, with the driver's default
        Set<String> shown = new HashSet<>();
        for (KeyboardBindings.Bind b : KeyboardBindings.ALL) {
            check(sdk.containsKey(b.cvar), b.cvar + " is a keybind of the driver");
            check(sdk.get(b.cvar).equals(b.defaultKeys), b.cvar + " default " + b.defaultKeys + " = " + sdk.get(b.cvar));
            check(shown.add(b.cvar), b.cvar + " shown once");
        }
        // every controller button but Guide (not a button the game reads) is shown
        Set<String> missing = new TreeSet<>(sdk.keySet());
        missing.removeAll(shown);
        check(missing.equals(Set.of("keybind_guide")), "every button shown: missing " + missing);

        // every name the launcher writes is one keybinds.cpp parses
        int mapped = 0;
        // (KeyEvent's constants are compiled in; its methods are stubs outside Android)
        for (int code = 0; code <= 400; ++code) {
            String key = KeyboardBindings.keyName(code);
            if (key == null) continue;
            check(names.contains(key), "keybinds.cpp knows " + key + " (key code " + code + ")");
            ++mapped;
        }
        check(KeyboardBindings.keyName(KeyEvent.KEYCODE_W).equals("W"), "W");
        check(KeyboardBindings.keyName(KeyEvent.KEYCODE_DPAD_UP).equals("Up"), "the arrow keys");
        check(KeyboardBindings.keyName(KeyEvent.KEYCODE_ENTER).equals("Return"), "Enter");
        check(KeyboardBindings.keyName(KeyEvent.KEYCODE_DEL).equals("Backspace"), "Backspace");
        check(KeyboardBindings.keyName(KeyEvent.KEYCODE_F12).equals("F12"), "F12");
        check(KeyboardBindings.keyName(KeyEvent.KEYCODE_SHIFT_LEFT) == null, "a modifier alone is no bind");
        check(KeyboardBindings.isModifier(KeyEvent.KEYCODE_SHIFT_LEFT) && !KeyboardBindings.isModifier(KeyEvent.KEYCODE_A),
                "modifiers wait for their key");
        // and the defaults themselves are names it knows, after their modifiers
        for (String keys : sdk.values()) {
            for (String token : keys.split(",")) {
                if (token.isEmpty()) continue;
                check(names.contains(token.replaceAll("^((Shift|Ctrl|Control|Alt)\\+)+", "")), "default key " + token);
            }
        }
        System.out.println("Keyboard: " + KeyboardBindings.ALL.length + " buttons match the driver's keybinds and defaults; "
                + mapped + " Android keys map to names keybinds.cpp parses.");
    }
}
