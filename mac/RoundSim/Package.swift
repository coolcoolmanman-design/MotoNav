// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "RoundSim",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "RoundSim", targets: ["RoundSim"]),
    ],
    targets: [
        .executableTarget(
            name: "RoundSim",
            path: "Sources/RoundSim"
        ),
    ]
)
