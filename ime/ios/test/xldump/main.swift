import Foundation

// 교차 언어 동일성 덤프.
// tools/xl/dump_core.js 와 **완전히 같은 형식**이어야 한다:
//   CHO×JUNG×JONG 순회, 각 줄 = "글자\t키시퀀스"
// 사용법: dump [출력파일] [--longpress]
let args = CommandLine.arguments
let path = args.count > 1 && !args[1].hasPrefix("--") ? args[1] : "/tmp/xl/swift.txt"
let useLong = args.contains("--longpress")

var lines: [String] = []
lines.reserveCapacity(11172)

for c in Cheonjiin.CHO {
    for v in Cheonjiin.JUNG {
        for j in Cheonjiin.JONG {
            let jong: Character? = (j == " ") ? nil : j
            let ch = Cheonjiin.compose(c, v, jong)
            let keys = Cheonjiin.encode(Character(ch), longpress: useLong)
            let seq: String = keys.joined(separator: " ")
            lines.append(ch + "\t" + seq)
        }
    }
}

let out = lines.joined(separator: "\n") + "\n"
try! out.write(toFile: path, atomically: true, encoding: .utf8)
FileHandle.standardError.write("wrote \(path) (\(lines.count) lines)\n".data(using: .utf8)!)
