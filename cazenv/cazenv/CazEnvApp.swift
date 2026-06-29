import SwiftUI

@main
struct CazEnvApp: App {
    var body: some Scene {
#if os(macOS)
        WindowGroup {
            ContentView()
                .frame(minWidth: 1100, minHeight: 760)
        }
        .windowStyle(.hiddenTitleBar)
#else
        WindowGroup {
            ContentView()
        }
#endif
    }
}
