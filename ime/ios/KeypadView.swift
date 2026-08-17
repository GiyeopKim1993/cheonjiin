//
//  KeypadView.swift
//  천지인 4×4 키패드 (docs/03-디자인.md)
//
//  확정 배치:
//     ㅣ    ㆍ    ㅡ    ⌫
//    ㄱㅋ  ㄴㄹ  ㄷㅌ   ↵
//    ㅂㅍ  ㅅㅎ  ㅈㅊ   漢
//     ◀   ㅇㅁ   ▶    ␣
//
import UIKit

/// 터치 이벤트를 클로저로 전달하는 버튼.
/// `#selector` 는 ObjC 런타임을 요구해 Linux 타입체크가 불가능하므로
/// 클로저 방식으로 대체했다(iOS 동작은 동일).
final class KeyButton: UIButton {
    var onDown: ((UIButton) -> Void)?
    var onUp: ((UIButton) -> Void)?

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(touches, with: event)
        onDown?(self)
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesEnded(touches, with: event)
        onUp?(self)
    }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesCancelled(touches, with: event)
        onUp?(self)
    }
}

public final class KeypadView: UIView {

    public var onKeyDown: ((String) -> Void)?
    public var onKeyUp: ((String) -> Void)?

    /// 논리 키 (코어가 아는 이름)
    static let KEYS: [[String]] = [
        ["K1", "K2", "K3", "BACK"],
        ["K4", "K5", "K6", "ENTER"],
        ["K7", "K8", "K9", "HANJA"],
        ["KLEFT", "K0", "KRIGHT", "SPACE"],
    ]

    static let LABELS_HANGUL: [[String]] = [
        ["ㅣ", "ㆍ", "ㅡ", "⌫"],
        ["ㄱㅋ", "ㄴㄹ", "ㄷㅌ", "↵"],
        ["ㅂㅍ", "ㅅㅎ", "ㅈㅊ", "漢"],
        ["◀", "ㅇㅁ", "▶", "␣"],
    ]
    static let LABELS_ENGLISH: [[String]] = [
        [".,?!", "ABC", "DEF", "⌫"],
        ["GHI", "JKL", "MNO", "↵"],
        ["PQRS", "TUV", "WXYZ", "漢"],
        ["◀", "⇧", "▶", "␣"],
    ]
    static let LABELS_NUMBER: [[String]] = [
        ["1", "2", "3", "⌫"],
        ["4", "5", "6", "↵"],
        ["7", "8", "9", "漢"],
        ["◀", "0", "▶", "␣"],
    ]

    public enum Layout { case hangul, english, number }
    public var layout: Layout = .hangul { didSet { relabel() } }

    private var buttons: [[KeyButton]] = []
    private let gap: CGFloat = 6
    private let pad: CGFloat = 4

    public override init(frame: CGRect) {
        super.init(frame: frame)
        build()
    }
    public required init?(coder: NSCoder) {
        super.init(coder: coder)
        build()
    }

    private func build() {
        backgroundColor = UIColor(white: 0.82, alpha: 1.0)
        for r in 0..<4 {
            var row: [KeyButton] = []
            for c in 0..<4 {
                let b = KeyButton(type: .custom)
                b.titleLabel?.font = .systemFont(ofSize: 22, weight: .regular)
                b.setTitleColor(.black, for: .normal)
                b.backgroundColor = isFunctionKey(r, c) ? UIColor(white: 0.72, alpha: 1)
                                                        : .white
                b.layer.cornerRadius = 5
                b.tag = r * 4 + c
                b.onDown = { [weak self] btn in self?.down(btn) }
                b.onUp   = { [weak self] btn in self?.up(btn) }
                addSubview(b)
                row.append(b)
            }
            buttons.append(row)
        }
        relabel()
    }

    private func isFunctionKey(_ r: Int, _ c: Int) -> Bool {
        return c == 3 || (r == 3 && (c == 0 || c == 2))
    }

    private func relabel() {
        let labels: [[String]]
        switch layout {
        case .hangul:  labels = Self.LABELS_HANGUL
        case .english: labels = Self.LABELS_ENGLISH
        case .number:  labels = Self.LABELS_NUMBER
        }
        for r in 0..<4 {
            for c in 0..<4 {
                buttons[r][c].setTitle(labels[r][c], for: .normal)
            }
        }
    }

    public override func layoutSubviews() {
        super.layoutSubviews()
        let w = (bounds.width - pad * 2 - gap * 3) / 4
        let h = (bounds.height - pad * 2 - gap * 3) / 4
        for r in 0..<4 {
            for c in 0..<4 {
                buttons[r][c].frame = CGRect(
                    x: pad + CGFloat(c) * (w + gap),
                    y: pad + CGFloat(r) * (h + gap),
                    width: w, height: h)
            }
        }
    }

    private func down(_ sender: UIButton) {
        let r = sender.tag / 4, c = sender.tag % 4
        sender.backgroundColor = UIColor(white: 0.62, alpha: 1)
        onKeyDown?(Self.KEYS[r][c])
    }

    private func up(_ sender: UIButton) {
        let r = sender.tag / 4, c = sender.tag % 4
        sender.backgroundColor = isFunctionKey(r, c) ? UIColor(white: 0.72, alpha: 1) : .white
        onKeyUp?(Self.KEYS[r][c])
    }
}
