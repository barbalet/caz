import Foundation

extension CazProgramChoice {
    var sourceLines: [String] {
        switch self {
        case .curiousPatrol:
            return [
                "loop:",
                "  IN   A,(EAR_VOLUME)",
                "  CP   #BE",
                "  JP   NC,alarm",
                "  IN   A,(EYE_MOTION)",
                "  CP   #96",
                "  JP   NC,track_motion",
                "  IN   A,(EAR_PATTERN)",
                "  CP   #01",
                "  JP   Z,mouse",
                "  IN   A,(EYE_LUMA)",
                "  CP   #20",
                "  JP   C,night_listen",
                "  IN   A,(EYE_EDGE)",
                "  CP   #D2",
                "  JP   NC,close_object",
                "patrol:        gait=walk tail=level",
                "track_motion:  gait=walk ears=forward",
                "mouse:         gait=crouch eyelid=alert",
                "alarm:         gait=retreat vocal=hiss",
                "night_listen:  gait=loaf ears=swivel",
                "close_object:  gait=paw-test vocal=mrrp"
            ]
        case .napWatch:
            return [
                "loop:",
                "  IN   A,(EAR_VOLUME)",
                "  CP   #D2",
                "  JP   NC,startle",
                "  IN   A,(EYE_MOTION)",
                "  CP   #B9",
                "  JP   NC,open_eye",
                "  IN   A,(EAR_PATTERN)",
                "  CP   #02",
                "  JP   Z,greet",
                "nap:       gait=loaf vocal=purr eyelid=low",
                "open_eye:  gait=loaf ears=swivel",
                "startle:   gait=retreat tail=bottle",
                "greet:     gait=walk vocal=meow"
            ]
        case .farmyardMouser:
            return [
                "loop:",
                "  IN   A,(EAR_PATTERN)",
                "  CP   #04",
                "  JP   Z,machine",
                "  IN   A,(EAR_PATTERN)",
                "  CP   #01",
                "  JP   Z,prey",
                "  IN   A,(EAR_PATTERN)",
                "  CP   #02",
                "  JP   Z,human",
                "  IN   A,(EAR_PATTERN)",
                "  CP   #03",
                "  JP   Z,weather",
                "  IN   A,(EYE_LUMA)",
                "  CP   #DE",
                "  JP   NC,glare",
                "patrol:   gait=walk head=132",
                "machine:  gait=retreat ears=flat",
                "prey:     edge check then crouch/pounce",
                "human:    gait=walk vocal=purr",
                "weather:  gait=loaf tail=curl",
                "glare:    gait=walk eyelid=narrow"
            ]
        }
    }

    func highlightLine(for gait: UInt8, pattern: UInt8) -> Int? {
        switch self {
        case .curiousPatrol:
            if gait == 4 { return 19 }
            if gait == 2 { return 18 }
            if gait == 5 { return 21 }
            if gait == 0 { return 20 }
            if gait == 1 && pattern == 0 { return 16 }
            return 17
        case .napWatch:
            if gait == 4 { return 12 }
            if gait == 1 { return 13 }
            if pattern == 2 { return 13 }
            if gait == 0 && pattern != 0 { return 11 }
            return 10
        case .farmyardMouser:
            if pattern == 4 { return 18 }
            if pattern == 1 { return 19 }
            if pattern == 2 { return 20 }
            if pattern == 3 { return 21 }
            if gait == 1 { return 17 }
            return nil
        }
    }
}
