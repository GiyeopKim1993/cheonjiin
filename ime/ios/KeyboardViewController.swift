//
//  KeyboardViewController.swift
//  천지인 iOS 키보드 확장 (Keyboard Extension)
//
//  ⚠️ 설계 변경 (docs/02 §7.5 개정):
//  기존 문서는 "UITextDocumentProxy 는 marked text API 를 제공하지 않는다"는
//  전제로 delete-and-reinsert 우회를 규정하고 iOS 를 v1.2 로 미뤘다.
//  이 전제는 **현재 사실이 아니다**. Apple 공식 문서 기준
//  `setMarkedText(_:selectedRange:)` / `unmarkText()` 는 iOS 13.0+ 에서
//  UITextDocumentProxy 의 **Required** 멤버다.
//    https://developer.apple.com/documentation/uikit/uitextdocumentproxy
//  따라서 iOS 도 다른 플랫폼과 동일하게 **정식 preedit(밑줄 조합)** 을 쓴다.
//  delete-and-reinsert 는 iOS 12 이하 폴백으로만 남긴다.
//
//  조합 로직은 CheonjiinCore.swift(= 5개 언어와 전수 동일) 를 그대로 쓴다.
//  이 파일은 UIKit 바인딩과 타이머만 담당한다.
//
import UIKit

/// UITextDocumentProxy 어댑터
final class ProxyOutput: TextOutput {
    private let proxy: UITextDocumentProxy
    init(_ p: UITextDocumentProxy) { proxy = p }

    var supportsMarkedText: Bool {
        if #available(iOS 13.0, *) { return true }
        return false
    }
    var contextBefore: String? { proxy.documentContextBeforeInput }

    func insertText(_ text: String) { proxy.insertText(text) }
    func deleteBackward() { proxy.deleteBackward() }

    func setMarked(_ text: String) {
        if #available(iOS 13.0, *) {
            // 조합 중 밑줄 표시. 커서는 조합 문자열 끝.
            proxy.setMarkedText(text, selectedRange: NSRange(location: text.count, length: 0))
        }
    }
    func unmark() {
        if #available(iOS 13.0, *) { proxy.unmarkText() }
    }
}

// MARK: - 키보드 뷰 컨트롤러

open class KeyboardViewController: UIInputViewController {

    private var bridge: CompositionBridge!
    private var output: ProxyOutput!
    private var keypad: KeypadView!

    private var multitapTimer: Timer?
    private var longPressTimer: Timer?
    private var longPressFired = false

    /// docs/00 §10 확정값
    private let multitapMs: TimeInterval = 0.8
    private let longpressMs: TimeInterval = 0.3

    open override func viewDidLoad() {
        super.viewDidLoad()
        output = ProxyOutput(textDocumentProxy)
        bridge = CompositionBridge(output: output)

        keypad = KeypadView(frame: .zero)
        keypad.translatesAutoresizingMaskIntoConstraints = false
        keypad.onKeyDown = { [weak self] key in self?.handleDown(key) }
        keypad.onKeyUp   = { [weak self] key in self?.handleUp(key) }
        view.addSubview(keypad)
        NSLayoutConstraint.activate([
            keypad.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            keypad.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            keypad.topAnchor.constraint(equalTo: view.topAnchor),
            keypad.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            keypad.heightAnchor.constraint(equalToConstant: 260),
        ])
    }

    /// 다른 키보드로 전환되거나 입력창을 떠날 때 조합을 흘리지 않는다
    open override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        cancelTimers()
        bridge.finish()
    }

    open override func textWillChange(_ textInput: UITextInput?) { }
    open override func textDidChange(_ textInput: UITextInput?) { }

    private func cancelTimers() {
        multitapTimer?.invalidate(); multitapTimer = nil
        longPressTimer?.invalidate(); longPressTimer = nil
    }

    private func handleDown(_ key: String) {
        longPressTimer?.invalidate()
        // 롱프레스 예약 — 자음키에만
        if Cheonjiin.CONSONANT_CYCLE[key] != nil {
            longPressTimer = Timer.scheduledTimer(withTimeInterval: longpressMs,
                                                  repeats: false) { [weak self] _ in
                guard let self = self else { return }
                self.longPressTimer = nil
                self.longPressFired = true
                // key_down 에서 이미 1탭이 들어갔으므로 코어가 되감는다
                self.bridge.press("LONG_\(key)")
                self.multitapTimer?.invalidate()
                self.multitapTimer = nil
            }
        }
        longPressFired = false
        bridge.press(key)
        rearmMultitap()
    }

    private func handleUp(_ key: String) {
        longPressTimer?.invalidate(); longPressTimer = nil
    }

    /// 멀티탭 만료 -> 코어에 TIMEOUT 주입 (코어는 시간을 모른다)
    private func rearmMultitap() {
        multitapTimer?.invalidate()
        multitapTimer = Timer.scheduledTimer(withTimeInterval: multitapMs,
                                             repeats: false) { [weak self] _ in
            self?.multitapTimer = nil
            self?.bridge.press("TIMEOUT")
        }
    }
}
