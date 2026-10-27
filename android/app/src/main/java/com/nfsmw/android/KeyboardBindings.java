package com.nfsmw.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.InputDevice;
import android.view.KeyEvent;

import java.util.ArrayList;
import java.util.List;

/**
 * Playing with a keyboard: the SDK's keyboard driver (sdk/src/input/mnk/mnk_input_driver.cpp) makes it a
 * controller for player 1 when mnk_mode is on. Each controller button is a keybind_* cvar holding key
 * names (sdk/src/ui/keybinds.cpp), alternatives separated by commas, each optionally after "Shift+",
 * "Ctrl+" or "Alt+". GameActivity passes the choice on the command line; a button left at its default is
 * not passed, so the SDK's own default applies.
 */
final class KeyboardBindings {
    static final class Bind {
        final String cvar;
        final String label;
        /** The SDK's default (mnk_input_driver.cpp), shown and restored by the launcher. */
        final String defaultKeys;

        Bind(String cvar, String label, String defaultKeys) {
            this.cvar = cvar;
            this.label = label;
            this.defaultKeys = defaultKeys;
        }
    }

    static final Bind[] ALL = {
            new Bind("keybind_right_trigger", "Gatillo derecho (RT) · acelerar", "E,O"),
            new Bind("keybind_left_trigger", "Gatillo izquierdo (LT) · frenar", "Q,I"),
            new Bind("keybind_lstick_left", "Stick izquierdo ← · girar", "A"),
            new Bind("keybind_lstick_right", "Stick izquierdo → · girar", "D"),
            new Bind("keybind_lstick_up", "Stick izquierdo ↑", "W"),
            new Bind("keybind_lstick_down", "Stick izquierdo ↓", "S"),
            new Bind("keybind_lstick_press", "Pulsar stick izquierdo (L3)", "F"),
            new Bind("keybind_a", "A", "Semicolon,Space"),
            new Bind("keybind_b", "B", "Quote,Backspace"),
            new Bind("keybind_x", "X", "L"),
            new Bind("keybind_y", "Y", "P"),
            new Bind("keybind_left_shoulder", "LB", "1"),
            new Bind("keybind_right_shoulder", "RB", "3"),
            new Bind("keybind_rstick_up", "Stick derecho ↑", "Up"),
            new Bind("keybind_rstick_down", "Stick derecho ↓", "Down"),
            new Bind("keybind_rstick_left", "Stick derecho ←", "Left"),
            new Bind("keybind_rstick_right", "Stick derecho →", "Right"),
            new Bind("keybind_rstick_press", "Pulsar stick derecho (R3)", "K"),
            new Bind("keybind_dpad_up", "Cruceta ↑", "Shift+Up"),
            new Bind("keybind_dpad_down", "Cruceta ↓", "Shift+Down"),
            new Bind("keybind_dpad_left", "Cruceta ←", "Shift+Left"),
            new Bind("keybind_dpad_right", "Cruceta →", "Shift+Right"),
            new Bind("keybind_start", "START", "X,Return"),
            new Bind("keybind_back", "BACK", "Z,Tab"),
    };

    private static final String PREFS = "nfsmw_keyboard";
    private static final String ENABLED = "enabled";

    private KeyboardBindings() {
    }

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static boolean enabled(Context context) {
        return prefs(context).getBoolean(ENABLED, false);
    }

    static void setEnabled(Context context, boolean enabled) {
        prefs(context).edit().putBoolean(ENABLED, enabled).apply();
    }

    static String keys(Context context, Bind bind) {
        return prefs(context).getString(bind.cvar, bind.defaultKeys);
    }

    /** The keys as the launcher shows them: "Space, Shift+Up", or "(ninguna)". */
    static String describe(String keys) {
        return keys.isEmpty() ? "(ninguna)" : keys.replace(",", ", ");
    }

    static void reset(Context context) {
        SharedPreferences.Editor edit = prefs(context).edit();
        for (Bind bind : ALL) {
            edit.remove(bind.cvar);
        }
        edit.apply();
    }

    /**
     * Gives the key to the button alone: it replaces the button's keys and leaves every other button that
     * had it. Returns the labels of those buttons, for the launcher to say so.
     */
    static List<String> assign(Context context, Bind target, String key) {
        List<String> taken = new ArrayList<>();
        SharedPreferences.Editor edit = prefs(context).edit();
        for (Bind bind : ALL) {
            if (bind == target) {
                continue;
            }
            List<String> rest = new ArrayList<>();
            boolean had = false;
            for (String token : keys(context, bind).split(",")) {
                if (token.isEmpty()) {
                    continue;
                }
                if (token.equals(key)) {
                    had = true;
                } else {
                    rest.add(token);
                }
            }
            if (had) {
                edit.putString(bind.cvar, String.join(",", rest));
                taken.add(bind.label);
            }
        }
        edit.putString(target.cvar, key);
        edit.apply();
        return taken;
    }

    /** mnk_mode and the buttons that differ from the SDK's defaults, for GameActivity's command line. */
    static void arguments(Context context, List<String> args) {
        boolean on = enabled(context);
        args.add("--mnk_mode=" + on);
        if (!on) {
            return;
        }
        for (Bind bind : ALL) {
            String keys = keys(context, bind);
            if (!keys.equals(bind.defaultKeys)) {
                args.add("--" + bind.cvar + "=" + keys);
            }
        }
    }

    /**
     * A key of a keyboard with letters: not a controller's, the device's own buttons (volume, power) or the
     * system's back gesture.
     */
    static boolean fromKeyboard(KeyEvent event) {
        InputDevice device = event.getDevice();
        return device != null && !device.isVirtual() &&
                device.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC &&
                event.isFromSource(InputDevice.SOURCE_KEYBOARD) &&
                !event.isFromSource(InputDevice.SOURCE_GAMEPAD);
    }

    /** Whether the key is a modifier alone (Shift, Ctrl, Alt), which the SDK's binds cannot fire on. */
    static boolean isModifier(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_SHIFT_LEFT:
            case KeyEvent.KEYCODE_SHIFT_RIGHT:
            case KeyEvent.KEYCODE_CTRL_LEFT:
            case KeyEvent.KEYCODE_CTRL_RIGHT:
            case KeyEvent.KEYCODE_ALT_LEFT:
            case KeyEvent.KEYCODE_ALT_RIGHT:
            case KeyEvent.KEYCODE_META_LEFT:
            case KeyEvent.KEYCODE_META_RIGHT:
                return true;
            default:
                return false;
        }
    }

    /**
     * The bind for a key pressed in the launcher, as keybinds.cpp names it, with the modifiers held
     * ("Shift+Up"); null for a key the SDK has no name for.
     */
    static String bindFor(KeyEvent event) {
        String name = keyName(event.getKeyCode());
        if (name == null) {
            return null;
        }
        StringBuilder bind = new StringBuilder();
        if (event.isShiftPressed()) bind.append("Shift+");
        if (event.isCtrlPressed()) bind.append("Ctrl+");
        if (event.isAltPressed()) bind.append("Alt+");
        return bind.append(name).toString();
    }

    /** keybinds.cpp's name of an Android key code, or null. */
    static String keyName(int keyCode) {
        if (keyCode >= KeyEvent.KEYCODE_A && keyCode <= KeyEvent.KEYCODE_Z) {
            return String.valueOf((char) ('A' + keyCode - KeyEvent.KEYCODE_A));
        }
        if (keyCode >= KeyEvent.KEYCODE_0 && keyCode <= KeyEvent.KEYCODE_9) {
            return String.valueOf((char) ('0' + keyCode - KeyEvent.KEYCODE_0));
        }
        if (keyCode >= KeyEvent.KEYCODE_F1 && keyCode <= KeyEvent.KEYCODE_F12) {
            return "F" + (1 + keyCode - KeyEvent.KEYCODE_F1);
        }
        if (keyCode >= KeyEvent.KEYCODE_NUMPAD_0 && keyCode <= KeyEvent.KEYCODE_NUMPAD_9) {
            return "Numpad" + (keyCode - KeyEvent.KEYCODE_NUMPAD_0);
        }
        switch (keyCode) {
            case KeyEvent.KEYCODE_GRAVE: return "Backtick";
            case KeyEvent.KEYCODE_MINUS: return "Minus";
            case KeyEvent.KEYCODE_EQUALS: return "Plus";
            case KeyEvent.KEYCODE_COMMA: return "Comma";
            case KeyEvent.KEYCODE_PERIOD: return "Period";
            case KeyEvent.KEYCODE_SEMICOLON: return "Semicolon";
            case KeyEvent.KEYCODE_SLASH: return "Slash";
            case KeyEvent.KEYCODE_BACKSLASH: return "Backslash";
            case KeyEvent.KEYCODE_LEFT_BRACKET: return "LBracket";
            case KeyEvent.KEYCODE_RIGHT_BRACKET: return "RBracket";
            case KeyEvent.KEYCODE_APOSTROPHE: return "Quote";
            case KeyEvent.KEYCODE_ESCAPE: return "Escape";
            case KeyEvent.KEYCODE_ENTER: return "Return";
            case KeyEvent.KEYCODE_SPACE: return "Space";
            case KeyEvent.KEYCODE_TAB: return "Tab";
            case KeyEvent.KEYCODE_DEL: return "Backspace";
            case KeyEvent.KEYCODE_FORWARD_DEL: return "Delete";
            case KeyEvent.KEYCODE_INSERT: return "Insert";
            case KeyEvent.KEYCODE_MOVE_HOME: return "Home";
            case KeyEvent.KEYCODE_MOVE_END: return "End";
            case KeyEvent.KEYCODE_PAGE_UP: return "PageUp";
            case KeyEvent.KEYCODE_PAGE_DOWN: return "PageDown";
            case KeyEvent.KEYCODE_DPAD_LEFT: return "Left";
            case KeyEvent.KEYCODE_DPAD_RIGHT: return "Right";
            case KeyEvent.KEYCODE_DPAD_UP: return "Up";
            case KeyEvent.KEYCODE_DPAD_DOWN: return "Down";
            case KeyEvent.KEYCODE_NUMPAD_ENTER: return "NumpadEnter";
            case KeyEvent.KEYCODE_NUMPAD_ADD: return "NumpadPlus";
            case KeyEvent.KEYCODE_NUMPAD_SUBTRACT: return "NumpadMinus";
            case KeyEvent.KEYCODE_NUMPAD_MULTIPLY: return "NumpadStar";
            case KeyEvent.KEYCODE_NUMPAD_DIVIDE: return "NumpadSlash";
            case KeyEvent.KEYCODE_CAPS_LOCK: return "CapsLock";
            default: return null;
        }
    }
}
