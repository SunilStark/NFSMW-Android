package com.nfsmw.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.util.DisplayMetrics;
import android.view.Display;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * The graphics options of the launcher. Each one is a game cvar: GameActivity passes them on the command
 * line, which wins over nfsmw.toml. Values must be ones the cvar allows (nfsmw_ajustes_graficos.cpp and
 * friends), or the game ignores them.
 */
final class GameOptions {
    static final class Option {
        final String key;
        final String title;
        final String cvar;
        final String[] values;
        final String[] labels;
        final String defaultValue;

        Option(String key, String title, String cvar, String defaultValue, String[] values, String[] labels) {
            this.key = key;
            this.title = title;
            this.cvar = cvar;
            this.defaultValue = defaultValue;
            this.values = values;
            this.labels = labels;
        }

        String label(String value) {
            for (int i = 0; i < values.length; i++) {
                if (values[i].equals(value)) {
                    return labels[i];
                }
            }
            return value;
        }
    }

    static final String RESOLUTION = "resolution";
    static final String FPS = "fps";
    static final String RENDERER = "renderer";
    static final String API = "graphics_api";
    static final String ASPECT = "aspect";
    /** The ASPECT value that GameActivity turns into the screen's own proportion. */
    static final String SCREEN_ASPECT = "pantalla";

    static final Option[] ALL = {
            new Option(RENDERER, "Renderer", "nfsmw_renderizador", "nativo",
                    new String[] {"nativo", "xenos"},
                    new String[] {"Native", "Compatibility · experimental"}),
            // The native renderer's OpenGL ES backend (from NFSMW-Recompiled-Mobile), for GPUs whose Vulkan driver
            // lacks what the Vulkan backend needs (Mali-T8xx/G71/G72, Adreno 5xx...). It uses its own shader library.
            new Option(API, "Graphics API", "nfsmw_nativo_api", "vulkan",
                    new String[] {"vulkan", "gles"},
                    new String[] {"Vulkan · recommended", "OpenGL ES 3.2 · GPUs without a compatible Vulkan (experimental)"}),
            new Option("gpu_stability", "Graphics stability", "nfsmw_consultas_oclusion", "auto",
                    new String[] {"auto", "off", "on"},
                    new String[] {"Automatic · Mali protection", "Maximum compatibility · no sun flare",
                            "Full effects · experimental"}),
            // GPUs without BC (Mali, PowerVR), Vulkan: the BC textures kept compressed as ETC2/EAC, from a disk
            // cache the game fills in the background (nfsmw_nativo_bc_a_etc2). Off decodes them, as before.
            new Option("etc2", "Textures on GPUs without BC", "nfsmw_nativo_bc_a_etc2", "true",
                    new String[] {"true", "false"},
                    new String[] {"ETC2 · less memory and bandwidth (Vulkan)", "Decompressed · as before"}),
            // The pixel shaders' color math at 16 bits (nfsmw_nativo_media_precision): Mali and Adreno do twice the
            // arithmetic per cycle. The shadow map, specular pow and texture coordinates stay at 32 bits.
            new Option("half_precision", "Color precision", "nfsmw_nativo_media_precision", "false",
                    new String[] {"false", "true"},
                    new String[] {"32 bits · as always", "16 bits · faster on Mali (test)"}),
            // android_ritmo.cpp: before the system throttles, the 30 FPS guard's shadow steps (one map, then no cast
            // shadows) until the device cools down.
            new Option("thermal", "Thermal control", "nfsmw_android_termico", "true",
                    new String[] {"true", "false"},
                    new String[] {"Automatic · trims shadows before it overheats", "Off"}),
            // android_ritmo.cpp: each frame's work against its budget, so Android sets the clocks for it (13+).
            new Option("adpf", "System performance hints (ADPF)", "nfsmw_android_adpf", "true",
                    new String[] {"true", "false"},
                    new String[] {"On · Android 13 or later", "Off"}),
            // android_rendimiento.cpp: the game only on the fast cores, or wherever Android puts it.
            new Option("big_cores", "Game cores", "nfsmw_android_nucleos_grandes", "true",
                    new String[] {"true", "false"},
                    new String[] {"Fast cores only", "Let Android decide"}),
            // 1056x660 and 1280x800 are 16:10 (nfsmw_tamano_pantalla.h): the game draws at that proportion. 1056x660
            // is exactly half of a 2112x1320 tablet screen.
            new Option(RESOLUTION, "Internal resolution", "nfsmw_resolucion_interna", "1280x720",
                    new String[] {"640x360", "1024x576", "1280x720", "1920x1080", "1056x660", "1280x800"},
                    new String[] {"640x360 · maximum performance", "1024x576 · performance", "1280x720 · balanced",
                            "1920x1080 · maximum quality", "1056x660 · 16:10 tablets, half the screen",
                            "1280x800 · 16:10 tablets"}),
            // nfsmw_proporcion_pantalla: 16:9 as on the Xbox 360, or the screen's own, measured when the game starts.
            // Wider screens see more at the sides, narrower ones (16:10) more above and below. HUD stretched.
            new Option(ASPECT, "Aspect ratio", "nfsmw_proporcion_pantalla", "0",
                    new String[] {"0", SCREEN_ASPECT},
                    new String[] {"16:9 · as on the Xbox 360", "The screen's · no bars, no distortion (prototype)"}),
            new Option(FPS, "FPS limit", "nfsmw_limite_fps", "60",
                    new String[] {"30", "45", "60", "90", "120"},
                    new String[] {"30 FPS · saves battery", "45 FPS · even on 90 Hz screens, when 60 does not hold",
                            "60 FPS", "90 FPS · experimental", "120 FPS · experimental"}),
            new Option("aa", "Antialiasing", "nfsmw_antialiasing", "apagado",
                    new String[] {"apagado", "fxaa"},
                    new String[] {"Off", "FXAA"}),
            new Option("shadows", "Shadows", "nfsmw_sombras_cada", "1",
                    new String[] {"1", "2"},
                    new String[] {"Every frame", "Every 2 frames · more performance"}),
            // Faces of the car's environment map redrawn per race frame (nfsmw_recortes_carrera.cpp). 0 redraws one
            // face every 30 frames in turn (nfsmw_cubemap_refresco_lento): almost no cost, and the paint's
            // reflection follows the car slowly (tunnels, night) instead of keeping the race start's; the rear-view
            // mirror, one of those faces, only updates then too. Off by default in this copy: the owner plays in
            // the chase camera and wants the GPU time for frame rate.
            new Option("car_reflections", "Car reflections", "nfsmw_cubemap_caras_max", "0",
                    new String[] {"6", "2", "1", "0"},
                    new String[] {"High", "Medium", "Low · more performance",
                            "Off · slow reflection, most performance (mirror barely updates)"}),
            new Option("road_reflection", "Wet road reflection", "nfsmw_reflejo_carretera", "true",
                    new String[] {"true", "false"},
                    new String[] {"On", "Off · more performance"}),
            new Option("sky", "Sky glow", "nfsmw_resplandor_cielo", "natural",
                    new String[] {"original", "natural", "suave"},
                    new String[] {"Original (Xbox 360)", "Natural", "Soft"}),
            // The PAL discs carry the text of these ten languages (app/src/nfsmw_idioma.cpp). English by default in
            // this copy; speech and movies stay in the disc's language (Spanish on the PAL Spain disc).
            new Option("language", "Text language", "nfsmw_idioma", "0",
                    new String[] {"-1", "4", "0", "1", "2", "3", "5", "6", "7", "12", "13"},
                    new String[] {"The edition's", "Español", "English", "Français", "Deutsch", "Italiano",
                            "Nederlands", "Svenska", "Dansk", "Polski", "Suomi"}),
            new Option("volume", "Game volume", "audio_ganancia_pct", "100",
                    new String[] {"100", "125", "150", "200"},
                    new String[] {"Normal", "Loud", "Very loud", "Maximum · may clip"}),
            // SDL pump frames queued ahead (5.3 ms each): more of them ride over hitches without audio cuts.
            new Option("audio_buffer", "Audio buffer", "audio_sdl_bomba_cola", "12",
                    new String[] {"12", "20"},
                    new String[] {"Normal · 64 ms", "Large · 107 ms, if the sound cuts out"}),
            new Option("filter", "Image filter", "nfsmw_posproceso", "apagado",
                    new String[] {"apagado", "cine", "vivo", "calido", "frio", "sepia", "noir", "crt"},
                    new String[] {"No filter", "Cinema", "Vivid", "Warm", "Cold", "Sepia", "Black and white",
                            "CRT"}),
    };

    private static final String PREFS = "nfsmw_game";

    private GameOptions() {
    }

    static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static Option find(String key) {
        for (Option o : ALL) {
            if (o.key.equals(key)) {
                return o;
            }
        }
        throw new IllegalArgumentException(key);
    }

    static String get(Context context, String key) {
        Option o = find(key);
        String value = prefs(context).getString(key, o.defaultValue);
        for (String allowed : o.values) {
            if (allowed.equals(value)) {
                return value;
            }
        }
        return o.defaultValue;
    }

    static void set(Context context, String key, String value) {
        prefs(context).edit().putString(key, value).apply();
    }

    /** The native renderer on its OpenGL ES backend: it needs nfsmw_shaders_gles.nfsp. */
    static boolean usesGles(Context context) {
        return "nativo".equals(get(context, RENDERER)) && "gles".equals(get(context, API));
    }

    static List<String> arguments(Context context) {
        List<String> args = new ArrayList<>();
        for (Option o : ALL) {
            if (o.key.equals(API) && !"nativo".equals(get(context, RENDERER))) {
                continue;  // the Xenos renderer is Vulkan only
            }
            String value = get(context, o.key);
            if (o.key.equals(ASPECT) && SCREEN_ASPECT.equals(value)) {
                value = screenAspect(context);
            }
            args.add("--" + o.cvar + "=" + value);
            // Off means no face redrawn: also the rear-view mirror's, which nfsmw.toml keeps every frame
            // (nfsmw_cubemap_caras_siempre = 4). In a race on the tablet that face alone was 230 draws and 6.2 ms of
            // GPU per frame, read by nothing in the chase camera.
            if (o.key.equals("car_reflections") && "0".equals(value)) {
                args.add("--nfsmw_cubemap_caras_siempre=0");
            }
        }
        if ("xenos".equals(get(context, RENDERER))) {
            // Use ordinary descriptor sets and host framebuffers on older drivers.
            // Xenos also decompresses unsupported BC texture formats on the GPU.
            args.add("--vulkan_native_shader_features=false");
            args.add("--render_target_path_vulkan=fbo");
            args.add("--vulkan_require_geometry_shader=false");
            args.add("--vulkan_require_fill_mode_non_solid=false");
            args.add("--async_shader_compilation=false");
            args.add("--nfsmw_d3d_registros_nativo=false");
            args.add("--nfsmw_d3d_marcador=false");
            args.add("--nfsmw_d3d_marcador_registro=false");
            args.add("--nfsmw_d3d_efectos_nativo=false");
            args.add("--nfsmw_render_sin_mosaico=false");
            args.add("--nfsmw_material_nativo=false");
            args.add("--nfsmw_visible_nativo=false");
            args.add("--nfsmw_matrices_nativo=false");
            args.add("--nfsmw_eview_nativo=false");
            args.add("--nfsmw_escenario_nativo=false");
            args.add("--nfsmw_efecto_pasada_nativo=false");
            args.add("--nfsmw_pegamento_nativo=false");
        }
        // Playing with a keyboard (the launcher's TECLADO section): mnk_mode and the changed keys.
        KeyboardBindings.arguments(context, args);
        GpuDrivers.arguments(context, args);
        return args;
    }

    /** The screen's long side over its short one, for nfsmw_proporcion_pantalla (the game plays in landscape). */
    static String screenAspect(Context context) {
        DisplayMetrics metrics = new DisplayMetrics();
        DisplayManager displays = context.getSystemService(DisplayManager.class);
        Display display = displays != null ? displays.getDisplay(Display.DEFAULT_DISPLAY) : null;
        if (display != null) {
            display.getRealMetrics(metrics);
        } else {
            metrics = context.getResources().getDisplayMetrics();
        }
        int longSide = Math.max(metrics.widthPixels, metrics.heightPixels);
        int shortSide = Math.min(metrics.widthPixels, metrics.heightPixels);
        return shortSide > 0 ? String.format(Locale.ROOT, "%.4f", (double) longSide / shortSide) : "0";
    }
}
