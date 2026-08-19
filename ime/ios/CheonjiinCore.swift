//
//  CheonjiinCore.swift
//  천지인 IME 코어 — Swift 참조 구현
//
//  설계 계약 (docs/02-기술스펙.md §1.1):
//    - 코어는 시간을 모른다. 타이머는 셸이 소유하며 TIMEOUT/COMMIT 키로 주입한다.
//    - press(state, key) -> state 는 순수 함수. I/O·전역상태 없음.
//
//  JS/C++/Java/Python 참조 구현과 **전수 동일**해야 한다.
//  검증: 11,172자 키 시퀀스 MD5 == 372100db39c6a5e64a6473f451823935
//
//  UIKit 비의존 — Linux 에서도 컴파일·테스트된다.
//
import Foundation

public enum Cheonjiin {

    // MARK: - 유니코드 한글

    public static let CHO  = Array("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ")
    public static let JUNG = Array("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ")
    public static let JONG = Array(" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ")

    public static func compose(_ cho: Character, _ jung: Character,
                               _ jong: Character?) -> String {
        let ci = CHO.firstIndex(of: cho) ?? 0
        let vi = JUNG.firstIndex(of: jung) ?? 0
        let ti = JONG.firstIndex(of: jong ?? " ") ?? 0
        let code = 0xAC00 + (ci * 21 + vi) * 28 + ti
        return String(UnicodeScalar(code)!)
    }

    public static func decompose(_ ch: Character)
        -> (cho: Character, jung: Character, jong: Character?)? {
        guard let scalar = ch.unicodeScalars.first else { return nil }
        let c = Int(scalar.value) - 0xAC00
        guard c >= 0 && c <= 11171 else { return nil }
        let jongIdx = c % 28
        return (CHO[c / 588], JUNG[(c % 588) / 28],
                jongIdx == 0 ? nil : JONG[jongIdx])
    }

    // MARK: - 키맵 (4×4)

    public static let VOWEL_KEYS: [String: Character] = ["K1": "ㅣ", "K2": "ㆍ", "K3": "ㅡ"]

    public static let CONSONANT_CYCLE: [String: [Character]] = [
        "K4": ["ㄱ", "ㅋ", "ㄲ"],
        "K5": ["ㄴ", "ㄹ"],
        "K6": ["ㄷ", "ㅌ", "ㄸ"],
        "K7": ["ㅂ", "ㅍ", "ㅃ"],
        "K8": ["ㅅ", "ㅎ", "ㅆ"],
        "K9": ["ㅈ", "ㅊ", "ㅉ"],
        "K0": ["ㅇ", "ㅁ"],
    ]

    /// 롱프레스 = 순환열의 마지막 항목 (docs/07 §3.1)
    public static let LONGPRESS: [String: Character] = {
        var m = [String: Character]()
        for (k, cyc) in CONSONANT_CYCLE { m[k] = cyc.last! }
        return m
    }()

    // MARK: - 모음 오토마타

    /// 삽입 순서가 PREV(백스페이스 역추적)를 결정하므로 배열로 보존한다.
    /// Dictionary 로 만들면 순회 순서가 비결정적이라 참조 구현과 어긋난다.
    /// (실제로 펌웨어에서 이 실수로 `나`⌫ 가 `니` 대신 `내` 가 된 적이 있다)
    static let V_ORDER: [(String, [(Character, String)])] = [
        ("",     [("ㅣ", "ㅣ"), ("ㆍ", "ㆍ"), ("ㅡ", "ㅡ")]),
        ("ㅣ",   [("ㆍ", "ㅏ")]),
        ("ㆍ",   [("ㅣ", "ㅓ"), ("ㅡ", "ㅗ"), ("ㆍ", "ㆍㆍ")]),
        ("ㆍㆍ", [("ㅣ", "ㅕ"), ("ㅡ", "ㅛ"), ("ㆍ", "ㆍ")]),
        ("ㅡ",   [("ㆍ", "ㅜ"), ("ㅣ", "ㅢ")]),
        ("ㅏ",   [("ㆍ", "ㅑ"), ("ㅣ", "ㅐ")]),
        ("ㅑ",   [("ㅣ", "ㅒ"), ("ㆍ", "ㅏ")]),
        ("ㅓ",   [("ㅣ", "ㅔ")]),
        ("ㅕ",   [("ㅣ", "ㅖ")]),
        ("ㅐ",   [("ㆍ", "ㅒ")]),
        ("ㅗ",   [("ㅣ", "ㅚ")]),
        ("ㅚ",   [("ㆍ", "ㅘ")]),
        ("ㅘ",   [("ㅣ", "ㅙ")]),
        ("ㅜ",   [("ㅣ", "ㅟ"), ("ㆍ", "ㅠ")]),
        ("ㅠ",   [("ㅣ", "ㅝ")]),
        ("ㅝ",   [("ㅣ", "ㅞ")]),
        ("ㅟ",   [("ㆍ", "ㅝ")]),
        ("ㅛ", []), ("ㅒ", []), ("ㅔ", []), ("ㅖ", []),
        ("ㅙ", []), ("ㅞ", []), ("ㅢ", []),
    ]

    static let V: [String: [Character: String]] = {
        var m = [String: [Character: String]]()
        for (st, trans) in V_ORDER {
            var t = [Character: String]()
            for (j, nx) in trans { t[j] = nx }
            m[st] = t
        }
        return m
    }()

    /// 화면에 출력되지 않는 중간 상태
    static let PENDING: Set<String> = ["ㆍ", "ㆍㆍ"]

    /// 백스페이스 역추적용 (삽입 순서 기준 최초 부모)
    static let PREV: [String: String] = {
        var p = [String: String]()
        for (st, trans) in V_ORDER {
            for (_, nx) in trans where p[nx] == nil {
                p[nx] = st
            }
        }
        return p
    }()

    // MARK: - 종성

    static let JONG_COMBINE: [String: Character] = [
        "ㄱ|ㅅ": "ㄳ", "ㄴ|ㅈ": "ㄵ", "ㄴ|ㅎ": "ㄶ", "ㄹ|ㄱ": "ㄺ", "ㄹ|ㅁ": "ㄻ",
        "ㄹ|ㅂ": "ㄼ", "ㄹ|ㅅ": "ㄽ", "ㄹ|ㅌ": "ㄾ", "ㄹ|ㅍ": "ㄿ", "ㄹ|ㅎ": "ㅀ",
        "ㅂ|ㅅ": "ㅄ",
    ]
    static let JONG_SPLIT: [Character: (Character, Character)] = {
        var m = [Character: (Character, Character)]()
        for (k, v) in JONG_COMBINE {
            let parts = k.split(separator: "|")
            m[v] = (Character(String(parts[0])), Character(String(parts[1])))
        }
        return m
    }()
    static let VALID_JONG: Set<Character> = Set(JONG.filter { $0 != " " })

    // MARK: - 상태

    public struct State {
        public var cho: Character?  = nil
        public var vstate: String   = ""
        public var jong: Character? = nil
        public var lastKey: String? = nil
        public var tap: Int         = 0
        public var snap: Snapshot?  = nil
        public var committed: String = ""
        public var slot: String     = "cho"
        public init() {}
    }

    public struct Snapshot {
        let cho: Character?
        let vstate: String
        let jong: Character?
        let committed: String
        let slot: String
    }

    public static func newState() -> State { State() }

    public static func preedit(_ s: State) -> String {
        let v = PENDING.contains(s.vstate) ? "" : s.vstate
        if let c = s.cho, !v.isEmpty {
            return compose(c, Character(v), s.jong)
        }
        if let c = s.cho { return String(c) }
        return v
    }

    public static func text(_ s: State) -> String { s.committed + preedit(s) }

    public static func flush(_ s: State) -> State {
        var t = s
        t.committed = s.committed + preedit(s)
        t.cho = nil; t.vstate = ""; t.jong = nil
        t.lastKey = nil; t.tap = 0; t.snap = nil; t.slot = "cho"
        return t
    }

    static func snapshot(_ s: State) -> Snapshot {
        Snapshot(cho: s.cho, vstate: s.vstate, jong: s.jong,
                 committed: s.committed, slot: s.slot)
    }
    static func restore(_ s: State, _ snap: Snapshot) -> State {
        var t = s
        t.cho = snap.cho; t.vstate = snap.vstate; t.jong = snap.jong
        t.committed = snap.committed; t.slot = snap.slot
        return t
    }

    /// 자음 c 를 현재 조합에 결합 (멀티탭 tap 관리는 호출측 책임)
    static func attach(_ s: State, _ c: Character) -> State {
        var t: State
        if s.slot == "cho" && s.cho == nil {
            t = s; t.cho = c; t.slot = "cho"; return t
        }
        // 종성이 이미 있음 -> 겹받침 시도
        if s.slot == "jong", let j = s.jong {
            if let comb = JONG_COMBINE["\(j)|\(c)"] {
                t = s; t.jong = comb; t.slot = "jong"; return t
            }
            t = flush(s); t.cho = c; t.slot = "cho"; return t
        }
        // 초성+중성 완성 -> 종성 자리
        if s.cho != nil && !s.vstate.isEmpty && !PENDING.contains(s.vstate) {
            if VALID_JONG.contains(c) {
                t = s; t.jong = c; t.slot = "jong"; return t
            }
            t = flush(s); t.cho = c; t.slot = "cho"; return t
        }
        t = flush(s); t.cho = c; t.slot = "cho"; return t
    }

    static func pullJong(_ jong: Character) -> (Character, Character?) {
        if let sp = JONG_SPLIT[jong] { return (sp.1, sp.0) }
        return (jong, nil)
    }

    // MARK: - press

    public static func press(_ state: State, _ key: String) -> State {
        var s = state
        var t: State

        /* 모음 */
        if let jamo = VOWEL_KEYS[key] {
            // 받침 뒤 모음 -> 연음(도깨비불)
            if let j = s.jong {
                let (moved, rest) = pullJong(j)
                var b = s; b.jong = rest
                b = flush(b)
                b.cho = moved; b.slot = "jung"
                s = b
            }
            var nxt = V[s.vstate]?[jamo]
            if nxt == nil {
                s = flush(s)
                nxt = V[""]![jamo]!
            }
            t = s
            t.vstate = nxt!; t.lastKey = nil; t.tap = 0; t.snap = nil; t.slot = "jung"
            return t
        }

        /* 자음 (멀티탭 — 스냅샷 되감기) */
        if let cyc = CONSONANT_CYCLE[key] {
            let tap: Int
            var base: State
            if s.lastKey == key, let sn = s.snap {
                tap = (s.tap + 1) % cyc.count
                base = restore(s, sn)
            } else {
                tap = 0; base = s
            }
            let snap = snapshot(base)
            t = attach(base, cyc[tap])
            t.lastKey = key; t.tap = tap; t.snap = snap
            return t
        }

        /* 롱프레스 = 순환열 마지막 항목 (docs/07 §3.1) */
        if key.hasPrefix("LONG_") {
            let k = String(key.dropFirst(5))
            guard let lp = LONGPRESS[k] else { return s }
            t = attach(s, lp)
            t.lastKey = nil; t.tap = 0; t.snap = nil   // 멀티탭 세션 종료
            return t
        }

        /* 방향키 — 조합 중이면 확정, 아니면 세션만 종료 */
        if key == "KRIGHT" || key == "KLEFT" {
            if !preedit(s).isEmpty { return flush(s) }
            t = s; t.lastKey = nil; t.tap = 0; t.snap = nil; return t
        }

        /* 셸이 주입하는 타임아웃 — 멀티탭 세션만 종료 */
        if key == "TIMEOUT" {
            t = s; t.lastKey = nil; t.tap = 0; t.snap = nil; return t
        }

        if key == "SPACE" { t = flush(s); t.committed += " ";  return t }
        if key == "ENTER" { t = flush(s); t.committed += "\n"; return t }
        if key == "COMMIT" { return flush(s) }
        if key == "BACK"   { return backspace(s) }

        /* 임의 문자 직접 삽입 (기호/영문/숫자 레이어) */
        if key.hasPrefix("CHAR_") {
            t = flush(s); t.committed += String(key.dropFirst(5)); return t
        }
        fatalError("unknown key: \(key)")
    }

    /// 자모 단위 역순 삭제 (docs/02 §5.4)
    public static func backspace(_ s: State) -> State {
        var t: State
        if let j = s.jong {
            t = s
            t.jong = JONG_SPLIT[j].map { $0.0 }
            if t.jong == nil { t.slot = "jung" }
            t.lastKey = nil; t.tap = 0; t.snap = nil
            return t
        }
        if !s.vstate.isEmpty {
            let prev = PREV[s.vstate] ?? ""
            t = s
            t.vstate = prev; t.slot = prev.isEmpty ? "cho" : "jung"
            t.lastKey = nil; t.tap = 0; t.snap = nil
            return t
        }
        if s.cho != nil {
            t = s; t.cho = nil; t.slot = "cho"
            t.lastKey = nil; t.tap = 0; t.snap = nil
            return t
        }
        if !s.committed.isEmpty {
            t = s; t.committed = String(s.committed.dropLast()); return t
        }
        return s
    }

    /// 편의: 키 배열 -> 확정 문자열
    public static func typeKeys(_ keys: [String]) -> String {
        var s = newState()
        for k in keys { s = press(s, k) }
        return flush(s).committed
    }

    // MARK: - 인코더 (글자 -> 키 시퀀스)

    /// 모음 상태로 가는 최단 키 경로 (BFS)
    static let VOWEL_PATH: [String: [String]] = {
        var dist: [String: [String]] = ["": []]
        var queue: [String] = [""]
        var head = 0
        let keyOf: [Character: String] = ["ㅣ": "K1", "ㆍ": "K2", "ㅡ": "K3"]
        while head < queue.count {
            let cur = queue[head]; head += 1
            guard let trans = V_ORDER.first(where: { $0.0 == cur })?.1 else { continue }
            for (jamo, nx) in trans where dist[nx] == nil {
                dist[nx] = dist[cur]! + [keyOf[jamo]!]
                queue.append(nx)
            }
        }
        return dist
    }()

    /// 한 글자를 키 시퀀스로. longpress=true 면 쌍자음 등을 롱프레스로 낸다.
    public static func encode(_ ch: Character, longpress: Bool = false) -> [String] {
        guard let d = decompose(ch) else { return [] }
        var keys: [String] = []
        keys += encodeConsonant(d.cho, longpress: longpress)
        keys += VOWEL_PATH[String(d.jung)] ?? []
        if let jong = d.jong {
            if let sp = JONG_SPLIT[jong] {
                keys += encodeConsonant(sp.0, longpress: longpress)
                keys += encodeConsonant(sp.1, longpress: longpress)
            } else {
                keys += encodeConsonant(jong, longpress: longpress)
            }
        }
        return keys
    }

    static func encodeConsonant(_ c: Character, longpress: Bool) -> [String] {
        for (k, cyc) in CONSONANT_CYCLE.sorted(by: { $0.key < $1.key }) {
            guard let idx = cyc.firstIndex(of: c) else { continue }
            if longpress && idx == cyc.count - 1 && cyc.count > 1 {
                return ["LONG_\(k)"]
            }
            return Array(repeating: k, count: idx + 1)
        }
        return []
    }
}
