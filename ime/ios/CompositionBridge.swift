//
//  CompositionBridge.swift
//  코어 상태 -> 텍스트 출력 동기화. **UIKit 비의존**.
//
//  UIKit 없이 Linux 에서 전수 테스트할 수 있도록 분리했다.
//  실제 iOS 에서는 ProxyOutput(UITextDocumentProxy) 를 주입한다.
//
import Foundation

// MARK: - 텍스트 출력 추상화
// UIKit 없이 로직을 테스트하기 위해 프록시를 프로토콜로 감싼다.

public protocol TextOutput: AnyObject {
    func insertText(_ text: String)
    func deleteBackward()
    func setMarked(_ text: String)
    func unmark()
    var supportsMarkedText: Bool { get }
    var contextBefore: String? { get }
}

// MARK: - 조합 상태 <-> 텍스트 출력 동기화

public final class CompositionBridge {
    public var core = Cheonjiin.newState()
    private unowned let out: TextOutput

    /// iOS 12 이하 폴백에서 "지금 화면에 몇 글자를 넣어놨는지"
    private var reinsertedCount = 0
    /// marked text 를 쓰고 있는가
    private var marking = false

    /// 이미 텍스트 필드로 내보낸 확정 문자열의 길이.
    /// core.committed 중 이 길이만큼은 이미 화면에 있다.
    private var flushedLen = 0

    public init(output: TextOutput) { self.out = output }

    /// 멀티탭 세션이 살아 있는가 = 다음 키가 확정을 되돌릴 수 있는가
    private var multitapLive: Bool { core.lastKey != nil && core.snap != nil }

    /// 코어 상태 변화를 실제 텍스트로 반영한다.
    public func sync() {
        // 1) 확정분 처리.
        //
        // 핵심: core.committed 를 **가로채지 않는다**. 참조 구현은 멀티탭
        // 되감기 시 committed 까지 되돌리는데(갆 = ...K8 K8 에서 "간"이
        // 취소된다), 미리 빼내면 코어가 되돌릴 대상을 잃는다.
        // 실제로 그렇게 했다가 1,197자가 "간갆"처럼 깨졌다.
        //
        // 따라서 멀티탭 세션이 살아 있는 동안에는 아무것도 내보내지 않고,
        // 세션이 끝난 뒤 아직 안 보낸 몫만 내보낸다.
        var emit = ""
        if !multitapLive && core.committed.count > flushedLen {
            emit = String(core.committed.dropFirst(flushedLen))
            flushedLen = core.committed.count
        }

        if !emit.isEmpty {
            if marking {
                out.unmark()
                marking = false
            } else if reinsertedCount > 0 {
                for _ in 0..<reinsertedCount { out.deleteBackward() }
                reinsertedCount = 0
            }
            out.insertText(emit)
        }

        // 2) 조합 중 문자열 표시.
        //    아직 안 내보낸 확정분(되감기 가능성이 남은 부분)은 조합 영역에
        //    함께 보여준다. 사용자에게는 입력된 것처럼 보이면서 되돌릴 수 있다.
        let unflushed = core.committed.count > flushedLen
            ? String(core.committed.dropFirst(flushedLen)) : ""
        let pre = unflushed + Cheonjiin.preedit(core)

        if out.supportsMarkedText {
            if pre.isEmpty {
                if marking { out.unmark(); marking = false }
            } else {
                out.setMarked(pre)          // 밑줄 조합 — 시스템 키보드와 동일
                marking = true
            }
        } else {
            // iOS 12 이하 폴백: 삭제 후 재삽입 (docs/02 §7.5 원안)
            for _ in 0..<reinsertedCount { out.deleteBackward() }
            reinsertedCount = 0
            if !pre.isEmpty {
                out.insertText(pre)
                reinsertedCount = pre.count
            }
        }
    }

    /// 조합을 강제 확정 (포커스 이동·키보드 전환 등)
    public func finish() {
        core = Cheonjiin.press(core, "COMMIT")   // 멀티탭 세션도 함께 종료된다
        sync()
        // 다음 입력을 위해 초기화
        core.committed = ""
        flushedLen = 0
    }

    public func press(_ key: String) {
        core = Cheonjiin.press(core, key)
        sync()
    }

    public var preedit: String { Cheonjiin.preedit(core) }
}
