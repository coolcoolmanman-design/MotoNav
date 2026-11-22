package com.example.motonav;
/** Map source IDs: 3 Amap (default), 1 Tianditu vec_w hybrid (restored for iOS tip parity). */
public final class MapProviderPolicy {
    private MapProviderPolicy() {}
    public static int normalize(int source) {
        if (source == 1) return 1; // SRC_TDT
        return 3; // SRC_AMAP
    }
    public static boolean usesAmap(int source, int theme) {
        return source == 3 && MapStylePolicy.usesBasemap(theme);
    }
    public static boolean usesTdt(int source, int theme) {
        return source == 1 && MapStylePolicy.usesBasemap(theme);
    }
}
