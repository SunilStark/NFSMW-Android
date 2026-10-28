// SPDX-License-Identifier: GPL-3.0-only
// Reversible presets, following the fork's "Mis ajustes" idea without copying its launcher.
package com.nfsmw.android;
import java.util.*;

final class GraphicsPresets {
    static final String[] KEYS = {"resolution", "fps", "aa", "shadows", "car_reflections", "road_reflection"};
    static final String[] NAMES = {"Battery", "Balanced", "Performance", "Quality"};
    static Map<String,String> values(int preset) {
        String[][] data = {
            {"1024x576", "30", "apagado", "2", "1", "false"},
            {"1280x720", "60", "apagado", "1", "2", "true"},
            {"1024x576", "60", "apagado", "2", "2", "false"},
            {"1280x720", "60", "fxaa", "1", "6", "true"}
        };
        if (preset < 0 || preset >= data.length) throw new IllegalArgumentException("Invalid preset");
        Map<String,String> result = new LinkedHashMap<>();
        for (int i = 0; i < KEYS.length; ++i) result.put(KEYS[i], data[preset][i]);
        return result;
    }
    static Map<String,String> preserve(Map<String,String> current, Map<String,String> applied, Map<String,String> saved) {
        return new LinkedHashMap<>(current.equals(applied) && saved != null ? saved : current);
    }
}
