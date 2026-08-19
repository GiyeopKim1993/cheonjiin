//
//  UIKitStub.swift
//  Linux 에서 iOS 코드를 **타입체크**하기 위한 최소 UIKit 스텁.
//
//  파싱만으로는 부족하다 — Android 에서 스텁에 메서드가 빠져 있어
//  `private override` 위반을 놓친 적이 있다. 실제 API 시그니처를
//  그대로 옮겨야 의미가 있다.
//
//  ⚠️ 실제 UIKit 과 시그니처가 다르면 이 검증은 무의미하다.
//     Apple 문서 기준으로 정확히 맞출 것.
//
import Foundation

// MARK: - 기본 타입

public typealias CGFloat = Double

public struct CGPoint { public var x: CGFloat; public var y: CGFloat
    public init(x: CGFloat, y: CGFloat) { self.x = x; self.y = y } }
public struct CGSize { public var width: CGFloat; public var height: CGFloat
    public init(width: CGFloat, height: CGFloat) { self.width = width; self.height = height } }
public struct CGRect {
    public var origin: CGPoint; public var size: CGSize
    public init(x: CGFloat, y: CGFloat, width: CGFloat, height: CGFloat) {
        origin = CGPoint(x: x, y: y); size = CGSize(width: width, height: height)
    }
    public static let zero = CGRect(x: 0, y: 0, width: 0, height: 0)
    public var width: CGFloat { size.width }
    public var height: CGFloat { size.height }
}

public struct NSRange {
    public var location: Int; public var length: Int
    public init(location: Int, length: Int) { self.location = location; self.length = length }
}

open class NSObject { public init() {} }
public protocol NSObjectProtocol: AnyObject {}
open class NSCoder: NSObject {}

// MARK: - UIKit 최소 셋

open class UIColor {
    public init(white: CGFloat, alpha: CGFloat) {}
    public static let black = UIColor(white: 0, alpha: 1)
    public static let white = UIColor(white: 1, alpha: 1)
}

open class UIFont {
    public struct Weight { public static let regular = Weight() }
    public static func systemFont(ofSize: CGFloat, weight: Weight) -> UIFont { UIFont() }
}

open class CALayer { public var cornerRadius: CGFloat = 0 }

public struct UIControlState: OptionSet {
    public let rawValue: Int
    public init(rawValue: Int) { self.rawValue = rawValue }
    public static let normal = UIControlState(rawValue: 1)
}
public struct UIControlEvents: OptionSet {
    public let rawValue: Int
    public init(rawValue: Int) { self.rawValue = rawValue }
    public static let touchDown = UIControlEvents(rawValue: 1)
    public static let touchUpInside = UIControlEvents(rawValue: 2)
    public static let touchUpOutside = UIControlEvents(rawValue: 4)
    public static let touchCancel = UIControlEvents(rawValue: 8)
}

open class UILabel { public var font: UIFont? }

open class NSLayoutAnchor<T> {
    public func constraint(equalTo: NSLayoutAnchor<T>) -> NSLayoutConstraint { NSLayoutConstraint() }
    public func constraint(equalToConstant: CGFloat) -> NSLayoutConstraint { NSLayoutConstraint() }
}
open class NSLayoutConstraint {
    public var isActive = false
    public static func activate(_ c: [NSLayoutConstraint]) {}
}

/// 실제 UITouch 는 NSObject 라 Hashable 이다. Linux 스텁에서는 직접 준수시킨다.
open class UITouch: NSObject, Hashable {
    public static func == (a: UITouch, b: UITouch) -> Bool { a === b }
    public func hash(into h: inout Hasher) { h.combine(ObjectIdentifier(self)) }
}
open class UIEvent: NSObject {}

open class UIView: NSObject {
    public var frame: CGRect = .zero
    public var bounds: CGRect = .zero
    public var backgroundColor: UIColor?
    public var tag: Int = 0
    public var translatesAutoresizingMaskIntoConstraints = true
    public let layer = CALayer()

    public let leadingAnchor = NSLayoutAnchor<UIView>()
    public let trailingAnchor = NSLayoutAnchor<UIView>()
    public let topAnchor = NSLayoutAnchor<UIView>()
    public let bottomAnchor = NSLayoutAnchor<UIView>()
    public let heightAnchor = NSLayoutAnchor<UIView>()

    public init(frame: CGRect) { super.init() ; self.frame = frame }
    public required init?(coder: NSCoder) { super.init() }
    open func addSubview(_ v: UIView) {}
    open func layoutSubviews() {}
    open func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {}
    open func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {}
    open func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {}
    /// 실제 UIView 에 존재하는 public 메서드
    open func setNeedsDisplay() {}
}

/// Linux Swift 에는 ObjC 런타임이 없어 Selector 가 없다. 최소 대체.
public struct Selector: ExpressibleByStringLiteral {
    public init(stringLiteral value: String) {}
    public init(_ s: String) {}
}

open class UIControl: UIView {
    open func addTarget(_ target: Any?, action: Selector, for: UIControlEvents) {}
}

public enum UIButtonType { case custom, system }

open class UIButton: UIControl {
    public var titleLabel: UILabel? = UILabel()
    public init(type: UIButtonType) { super.init(frame: .zero) }
    public required init?(coder: NSCoder) { super.init(coder: coder) }
    open func setTitle(_ t: String?, for: UIControlState) {}
    open func setTitleColor(_ c: UIColor?, for: UIControlState) {}
}

open class UIViewController: NSObject {
    public var view: UIView = UIView(frame: .zero)
    open func viewDidLoad() {}
    open func viewWillDisappear(_ animated: Bool) {}
}

public protocol UITextInput: AnyObject {}
public protocol UIKeyInput: AnyObject {
    func insertText(_ text: String)
    func deleteBackward()
}
public protocol UITextInputTraits: AnyObject {}

/// UITextDocumentProxy — Apple 문서 기준 시그니처
/// setMarkedText / unmarkText 는 iOS 13.0+ **Required** 멤버다.
public protocol UITextDocumentProxy: UIKeyInput, UITextInputTraits {
    var documentContextBeforeInput: String? { get }
    var documentContextAfterInput: String? { get }
    var selectedText: String? { get }
    func adjustTextPosition(byCharacterOffset: Int)
    func setMarkedText(_ markedText: String, selectedRange: NSRange)
    func unmarkText()
}

open class UIInputViewController: UIViewController {
    public var textDocumentProxy: UITextDocumentProxy = DummyProxy()
    open func textWillChange(_ textInput: UITextInput?) {}
    open func textDidChange(_ textInput: UITextInput?) {}
}

final class DummyProxy: UITextDocumentProxy {
    var documentContextBeforeInput: String? { nil }
    var documentContextAfterInput: String? { nil }
    var selectedText: String? { nil }
    func adjustTextPosition(byCharacterOffset: Int) {}
    func setMarkedText(_ markedText: String, selectedRange: NSRange) {}
    func unmarkText() {}
    func insertText(_ text: String) {}
    func deleteBackward() {}
}

// Timer 는 Foundation 에 있으므로 그대로 쓴다.
