//
//  CompositionBridge 검증 — UIKit 없이 Linux 에서 실행된다.
//
//  핵심 검증: marked text 경로와 iOS12 폴백 경로가 **같은 최종 텍스트**를
//  만들어야 한다. (docs/02 §7.5 개정 근거)
//
import Foundation

/// 텍스트 필드를 흉내내는 가짜 출력.
/// marked text 를 실제 iOS 처럼 "확정 전 임시 영역"으로 취급한다.
final class FakeOutput: TextOutput {
    var committed = ""      // 확정된 본문
    var marked = ""         // 조합 중(밑줄) 영역
    let supportsMarkedText: Bool
    var deleteCount = 0
    var insertCount = 0

    init(markedSupported: Bool) { supportsMarkedText = markedSupported }

    var contextBefore: String? { committed }
    /// 사용자가 보는 최종 화면
    var visible: String { committed + marked }

    func insertText(_ text: String) {
        committed += text
        insertCount += 1
    }
    func deleteBackward() {
        if !marked.isEmpty { marked.removeLast() }
        else if !committed.isEmpty { committed.removeLast() }
        deleteCount += 1
    }
    func setMarked(_ text: String) { marked = text }
    func unmark() {
        // iOS 동작: unmark 하면 조합 내용이 본문으로 확정된다.
        // 우리 bridge 는 unmark 직후 확정 문자열을 따로 insert 하므로
        // 여기서는 조합 영역만 비운다.
        marked = ""
    }
}

var failures = 0
func check(_ name: String, _ got: String, _ want: String) {
    if got == want {
        print("✅ \(name)")
    } else {
        print("❌ \(name)\n   got =[\(got)]\n   want=[\(want)]")
        failures += 1
    }
}
func checkInt(_ name: String, _ got: Int, _ want: Int) {
    if got == want { print("✅ \(name)") }
    else { print("❌ \(name) got=\(got) want=\(want)"); failures += 1 }
}

// 키 시퀀스를 브리지에 흘려넣고 최종 화면을 돌려준다
func run(_ keys: [String], marked: Bool) -> FakeOutput {
    let out = FakeOutput(markedSupported: marked)
    let bridge = CompositionBridge(output: out)
    for k in keys { bridge.press(k) }
    bridge.finish()
    return out
}

print("── iOS CompositionBridge 검증 ──")

// 1. marked text 경로 (iOS 13+)
let nam = Cheonjiin.encode("나") + Cheonjiin.encode("무")
check("marked: 나무", run(nam, marked: true).visible, "나무")

// 2. 폴백 경로 (iOS 12 이하) — 같은 결과여야 한다
check("폴백:   나무", run(nam, marked: false).visible, "나무")

// 3. 두 경로 동치성 — 전 음절 전수 대조
var mismatch = 0
var firstBad = ""
for code in 0xAC00...0xD7A3 {
    let ch = Character(UnicodeScalar(code)!)
    let keys = Cheonjiin.encode(ch)
    let a = run(keys, marked: true).visible
    let b = run(keys, marked: false).visible
    if a != b || a != String(ch) {
        mismatch += 1
        if firstBad.isEmpty { firstBad = "\(ch): marked=[\(a)] fallback=[\(b)]" }
    }
}
checkInt("marked/폴백 전수 동치 11,172자 불일치", mismatch, 0)
if !firstBad.isEmpty { print("   최초 불일치: \(firstBad)") }

// 4. 조합 중에는 확정되지 않아야 한다 (marked 영역에만 존재)
do {
    let out = FakeOutput(markedSupported: true)
    let b = CompositionBridge(output: out)
    b.press("K4")            // ㄱ
    check("조합중 본문 비어있음", out.committed, "")
    check("조합중 marked=ㄱ", out.marked, "ㄱ")
    b.press("K1"); b.press("K2")   // ㅏ -> 가
    check("조합중 marked=가", out.marked, "가")
    b.finish()
    check("확정 후 본문=가", out.committed, "가")
    check("확정 후 marked 비움", out.marked, "")
}

// 5. 폴백은 재삽입을 위해 삭제를 쓴다 (marked 경로는 삭제 0)
do {
    let m = run(Cheonjiin.encode("가"), marked: true)
    let f = run(Cheonjiin.encode("가"), marked: false)
    checkInt("marked 경로 deleteBackward 호출 0", m.deleteCount, 0)
    if f.deleteCount == 0 {
        print("❌ 폴백 경로가 삭제를 전혀 안 함 — 재삽입 로직 미동작")
        failures += 1
    } else {
        print("✅ 폴백 경로 deleteBackward \(f.deleteCount)회 (재삽입 동작)")
    }
}

// 6. 백스페이스 — 자모 단위 역추적
do {
    let out = FakeOutput(markedSupported: true)
    let b = CompositionBridge(output: out)
    for k in Cheonjiin.encode("나") { b.press(k) }
    b.press("BACK")
    check("나 ⌫ -> 니", out.marked, "니")
}

// 7. 스페이스/엔터 확정
do {
    let out = FakeOutput(markedSupported: true)
    let b = CompositionBridge(output: out)
    for k in Cheonjiin.encode("가") { b.press(k) }
    b.press("SPACE")
    check("가 + 공백", out.visible, "가 ")
}

print("")
if failures == 0 {
    print("✅ iOS 브리지 전 항목 통과 (marked text + 폴백 동치)")
    exit(0)
} else {
    print("❌ 실패 \(failures)건")
    exit(1)
}
