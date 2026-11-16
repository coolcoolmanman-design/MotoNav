import Foundation

/// Minimal RideRecorder stand-in so TripStatsEngine can compile without SwiftUI.
enum RideRecorder {
    struct Fix {
        let lat: Double
        let lng: Double
        let time: Date
        let speed: Double
    }

    struct Ride: Identifiable {
        var id = UUID()
        let started: Date
        var ended: Date?
        var mode: String
        var fixes: [Fix] = []
        var meters = 0.0
    }

    final class Shared {
        var rides: [Ride] = []
    }

    static let shared = Shared()
}
