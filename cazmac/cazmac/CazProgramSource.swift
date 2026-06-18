import Foundation

extension CazProgramChoice {
    var sourceLines: [String] {
        if let url = Bundle.main.url(forResource: fileBaseName, withExtension: "caz"),
           let text = try? String(contentsOf: url, encoding: .utf8) {
            return text.split(separator: "\n", omittingEmptySubsequences: false).map(String.init)
        }
        return ["; missing bundled source: \(fileBaseName).caz"]
    }

    func highlightLine(for gait: UInt8, pattern: UInt8, skill: UInt8) -> Int? {
        let label: String?
        switch self {
        case .curiousPatrol:
            if gait == 4 { label = "alarm" }
            else if gait == 2 { label = "mouse" }
            else if gait == 5 { label = "close_object" }
            else if gait == 0 { label = "night_listen" }
            else if gait == 1 && pattern == 0 { label = "patrol" }
            else { label = "track_motion" }
        case .napWatch:
            if gait == 4 { label = "startle" }
            else if gait == 1 || pattern == 2 { label = "greet" }
            else if gait == 0 && pattern != 0 { label = "open_eye" }
            else { label = "nap" }
        case .farmyardMouser:
            if pattern == 4 { label = "machine" }
            else if pattern == 1 && gait == 3 { label = "pounce" }
            else if pattern == 1 { label = "crouch" }
            else if pattern == 2 { label = "human" }
            else if pattern == 3 { label = "weather" }
            else if gait == 1 { label = "patrol" }
            else { label = nil }
        case .skillCycle:
            switch skill {
            case 2: label = "rest"
            case 3: label = "sit"
            case 4: label = "walk"
            case 5: label = "crawl"
            case 6: label = "pounce"
            case 7: label = "sniff"
            case 8: label = "scratch"
            default: label = "walk"
            }
        }

        guard let label else {
            return nil
        }
        return sourceLines.firstIndex { line in
            line.trimmingCharacters(in: .whitespaces).hasPrefix("\(label):")
        }
    }
}
