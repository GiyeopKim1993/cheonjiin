#!/usr/bin/env python3
"""spec/hanja-dict.json -> 각 언어 사전 소스 생성 (단일 출처 원칙)"""
import json, os
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# 이 사전은 한국어 위키낱말사전 파생 → CC BY-SA 4.0 (저장소 기본 라이선스와 다름).
# 생성물 상단에 반드시 고지가 박히도록 여기서 정의한다.
LICENSE_NOTE = (
    "⚠️ 라이선스: CC BY-SA 4.0 (저장소 기본 PolyForm Strict 와 다름)\n"
    "   한국어 위키낱말사전(https://ko.wiktionary.org) 파생 데이터 포함.\n"
    "   재사용 시 출처 표기 + 변경 명시 + 동일 조건 배포 필수. LICENSE-DICT.md 참조."
)
def _note(prefix):
    return "\n".join(prefix + line for line in LICENSE_NOTE.split("\n"))

d=json.load(open(f'{ROOT}/spec/hanja-dict.json'))
words, chars = d['words'], d['chars']

# ---------- JavaScript ----------
js=f'''/*!
{_note(" * ")}
 *
 * 천지인 한자 사전 — 자동 생성 (tools/gen_hanja.py). 직접 수정 금지.
 * 출처: {d["source"]}
 * 정렬: {d["sort"]}
 * 한자 {d["charCount"]}자 / 음절 {d["syllableCount"]} / 단어 {d["wordCount"]}
 */
(function (root, factory) {{
  const api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (typeof window !== 'undefined') window.HanjaDict = api;
}})(this, function () {{
  'use strict';
  const WORDS = {json.dumps(words, ensure_ascii=False, separators=(',',':'))};
  const CHARS = {json.dumps(chars, ensure_ascii=False, separators=(',',':'))};
  /** 단어 우선 -> 단일 글자 (docs/00 §5) */
  function lookup(target) {{
    if (WORDS[target]) return [WORDS[target]];
    if (Array.from(target).length === 1 && CHARS[target]) return CHARS[target].slice();
    return [];
  }}
  return {{ lookup, WORDS, CHARS,
    charCount: {d["charCount"]}, syllableCount: {d["syllableCount"]}, wordCount: {d["wordCount"]} }};
}});
'''
open(f'{ROOT}/ime/core/hanja.js','w').write(js)

# ---------- Java ----------
# 상수 풀 한계(65,535)를 피하려고 개별 리터럴 대신 큰 문자열 블롭을 파싱한다.
# 블롭 포맷:  key\x1fvalue\x1e key\x1fvalue\x1e ...
def blob(d):
    return "\x1e".join(k + "\x1f" + ("".join(v) if isinstance(v, list) else v)
                        for k, v in d.items())

def jchunks(text, size=20000):
    """자바 소스 리터럴 길이 제한(65535 바이트) 회피용 분할"""
    return [text[i:i+size] for i in range(0, len(text), size)]

def jesc(s):
    out = []
    for ch in s:
        if ch == '\\': out.append('\\\\')
        elif ch == '"': out.append('\\"')
        elif ch == '\n': out.append('\\n')
        elif ch == '\x1e': out.append('\\u001e')
        elif ch == '\x1f': out.append('\\u001f')
        elif ord(ch) < 0x20: out.append('\\u%04x' % ord(ch))
        else: out.append(ch)
    return "".join(out)

wblob, cblob = blob(words), blob(chars)
wparts, cparts = jchunks(wblob), jchunks(cblob)

java = f'''/*\n{_note(" * ")}\n */\npackage com.cuime;

import java.util.*;

/**
 * 천지인 한자 사전 — 자동 생성 (tools/gen_hanja.py). 직접 수정 금지.
 * 출처: {d["source"]}
 * 정렬: {d["sort"]}
 * 한자 {d["charCount"]}자 / 음절 {d["syllableCount"]} / 단어 {d["wordCount"]}
 *
 * 구현 메모: 단어가 3만 규모라 개별 문자열 리터럴로 두면 클래스 상수 풀
 * 한계(65,535)를 초과한다. 그래서 큰 블롭을 런타임에 1회 파싱한다.
 */
public final class HanjaDict {{
    public static final int CHAR_COUNT = {d["charCount"]};
    public static final int SYLLABLE_COUNT = {d["syllableCount"]};
    public static final int WORD_COUNT = {d["wordCount"]};

    private static final Map<String,String> W = new HashMap<>({max(len(words)*2,16)});
    private static final Map<String,String> C = new HashMap<>({max(len(chars)*2,16)});

'''
for i, part in enumerate(wparts):
    java += f'    private static final String W{i} = "{jesc(part)}";\n'
for i, part in enumerate(cparts):
    java += f'    private static final String C{i} = "{jesc(part)}";\n'

java += '''
    private static String join(String[] parts) {
        int cap = 0;
        for (String p : parts) cap += p.length();
        StringBuilder sb = new StringBuilder(cap);
        for (String p : parts) sb.append(p);
        return sb.toString();
    }

    private static void parse(String blob, Map<String,String> into) {
        int i = 0, n = blob.length();
        while (i < n) {
            int rs = blob.indexOf('\\u001e', i);
            if (rs < 0) rs = n;
            int fs = blob.indexOf('\\u001f', i);
            if (fs >= 0 && fs < rs) into.put(blob.substring(i, fs), blob.substring(fs + 1, rs));
            i = rs + 1;
        }
    }

    static {
'''
java += '        parse(join(new String[]{' + ','.join(f'W{i}' for i in range(len(wparts))) + '}), W);\n'
java += '        parse(join(new String[]{' + ','.join(f'C{i}' for i in range(len(cparts))) + '}), C);\n'
java += '''    }

    /** 단어 우선 -> 단일 글자 (docs/00 §5) */
    public static List<String> lookup(String target) {
        String w = W.get(target);
        if (w != null) return Collections.singletonList(w);
        if (target.codePointCount(0, target.length()) == 1) {
            String s = C.get(target);
            if (s != null) {
                List<String> out = new ArrayList<>();
                for (int i = 0; i < s.length(); ) {
                    int cp = s.codePointAt(i);
                    out.add(new String(Character.toChars(cp)));
                    i += Character.charCount(cp);
                }
                return out;
            }
        }
        return Collections.emptyList();
    }

    private HanjaDict() {}
}
'''
open(f'{ROOT}/ime/android/src/main/java/com/cuime/HanjaDict.java','w').write(java)

# ---------- C++ ----------
cpp=f'''{_note("// ")}
//
// 천지인 한자 사전 — 자동 생성 (tools/gen_hanja.py). 직접 수정 금지.
// 출처: {d["source"]}
// 정렬: {d["sort"]}
// 한자 {d["charCount"]}자 / 음절 {d["syllableCount"]} / 단어 {d["wordCount"]}
#pragma once
#include <string>
#include <vector>
#include <map>

namespace cuime {{
namespace hanja {{

inline const std::map<std::string,std::string>& words() {{
    static const std::map<std::string,std::string> m = {{
'''
for k,v in words.items(): cpp+=f'        {{u8"{k}",u8"{v}"}},\n'
cpp+='''    };
    return m;
}

inline const std::map<std::string,std::string>& chars() {
    static const std::map<std::string,std::string> m = {
'''
for k,v in chars.items(): cpp+=f'        {{u8"{k}",u8"{"".join(v)}"}},\n'
cpp+='''    };
    return m;
}

inline std::vector<std::string> utf8split(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        size_t len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        out.push_back(s.substr(i, len));
        i += len;
    }
    return out;
}

/** 단어 우선 -> 단일 글자 (docs/00 §5) */
inline std::vector<std::string> lookup(const std::string& target) {
    auto& W = words();
    auto wi = W.find(target);
    if (wi != W.end()) return {wi->second};
    if (utf8split(target).size() == 1) {
        auto& C = chars();
        auto ci = C.find(target);
        if (ci != C.end()) return utf8split(ci->second);
    }
    return {};
}

'''
cpp+=f'inline constexpr int CHAR_COUNT = {d["charCount"]};\n'
cpp+=f'inline constexpr int SYLLABLE_COUNT = {d["syllableCount"]};\n'
cpp+=f'inline constexpr int WORD_COUNT = {d["wordCount"]};\n'
cpp+='\n} // namespace hanja\n} // namespace cuime\n'
open(f'{ROOT}/ime/native/hanja.hpp','w').write(cpp)

print(f"생성 완료: 한자 {d['charCount']}자 / 음절 {d['syllableCount']} / 단어 {d['wordCount']}")
for f in ['ime/core/hanja.js','ime/android/src/main/java/com/cuime/HanjaDict.java','ime/native/hanja.hpp']:
    print(f"  {f}  {os.path.getsize(ROOT+'/'+f):,} bytes")
