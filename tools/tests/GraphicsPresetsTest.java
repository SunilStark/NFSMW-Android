package com.nfsmw.android;
import java.util.*;
public final class GraphicsPresetsTest {
    static void check(boolean ok) { if (!ok) throw new AssertionError(); }
    public static void main(String[] args) {
        Map<String,String> personal = GraphicsPresets.values(3); personal.put("fps","90");
        Map<String,String> battery = GraphicsPresets.values(0), performance = GraphicsPresets.values(2);
        check(GraphicsPresets.preserve(personal, Map.of(), null).equals(personal));
        check(GraphicsPresets.preserve(battery, battery, personal).equals(personal));
        check(GraphicsPresets.preserve(performance, performance, personal).equals(personal));
        Map<String,String> changed = new HashMap<>(battery); changed.put("aa","fxaa");
        check(GraphicsPresets.preserve(changed, battery, personal).equals(changed));
        for (int i=0;i<4;++i) {
            check(GraphicsPresets.values(i).size()==6);
            check(!GraphicsPresets.values(i).containsKey("graphics_api") && !GraphicsPresets.values(i).containsKey("language"));
        }
        System.out.println("Presets: personal values survive consecutive presets; manual changes become the next snapshot; API and language untouched.");
    }
}
