import SwiftUI

@main
struct CazMacApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .frame(minWidth: 1080, minHeight: 680)
        }
        .windowStyle(.hiddenTitleBar)
    }
}
