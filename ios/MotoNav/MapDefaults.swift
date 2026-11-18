import Foundation

/// Apply the requested AMap-building default once when upgrading, then preserve choices.
/// Trial (0.16.2.89+): allow Amap (3) or Tianditu (1) for round-screen basemap A/B; default stays Amap.
enum MapDefaults {
    /// Keep Amap / Tianditu; migrate obsolete AUTO/OSM/unknown → Amap. Never strip `key_tdt`.
    static func source(_ defaults: UserDefaults = .standard) -> Int {
        let raw = defaults.object(forKey: "map_src") as? Int
        if raw == MapFeeder.SRC_TDT || raw == MapFeeder.SRC_AMAP { return raw! }
        defaults.set(MapFeeder.SRC_AMAP, forKey: "map_src")
        return MapFeeder.SRC_AMAP
    }
    static let migrationKey = "amap_building_defaults_v013"
    static func buildings(_ defaults: UserDefaults = .standard) -> Bool {
        if !defaults.bool(forKey: migrationKey) {
            if let old = defaults.object(forKey: "map_src") { defaults.set(old, forKey: "map_src_before_building_default") }
            defaults.set(3, forKey: "map_src")
            defaults.set(true, forKey: "map_buildings")
            defaults.set(true, forKey: migrationKey)
        }
        return defaults.object(forKey: "map_buildings") as? Bool ?? true
    }
}
