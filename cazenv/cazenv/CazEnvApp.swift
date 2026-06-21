import SwiftUI

@main
struct CazEnvApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .frame(minWidth: 1100, minHeight: 760)
        }
        .windowStyle(.hiddenTitleBar)
    }
}
