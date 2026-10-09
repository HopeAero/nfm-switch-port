# Generates native/core/i18n_es.c, the Spanish tables tr() (core/i18n.c)
# reads, from the web port's dictionaries -- web/i18n.js and the files it
# merges (i18n-ext.js, i18n-careditor.js, i18n-stagemaker.js).
#
#   python3 tools/gen_i18n_es.py path/to/nfm-master/web
#
# The dictionaries are JS with computed parts (spread maps, keys built from
# the stage maker's part descriptions, a RegExp built from a word list), so
# rather than re-implement JS here the script asks node to import i18n.js and
# print its merged ES map and ES_PATTERNS list as JSON. node >= 18 must be on
# PATH; nothing in the web tree is modified (a temporary copy of i18n.js that
# exports the two is written beside it and removed).
#
# Exact entries become one table sorted by their UTF-8 bytes (binary search).
# Patterns keep the web's order (first match wins) and are converted from JS
# regex source to the token strings i18n.c matches (see I18N_T_* in i18n.h):
#   literal text, (.*) (.*?) (.+) (\d+) (-?\d+) (\d) (\s*) ([\d.]+)
#   (\d+(?:\.\d+)?) as captures, \s* \s+ uncaptured, `c?` an optional
#   character, `c{n}` a repeat, (?!literal) a negative lookahead, and
#   [Xx] / (a|b|c) alternatives (expanded into one pattern each; a captured
#   alternative's $n becomes its literal).
# Anything else, and every pattern whose replacement is a JS function, is
# skipped and listed on stdout.
import itertools
import json
import os
import re
import subprocess
import sys
import tempfile

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
web = os.path.abspath(sys.argv[1])

DUMP = r"""
import { readFileSync, writeFileSync, unlinkSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
const web = process.argv[2];
const tmp = path.join(web, '.i18n-dump-' + process.pid + '.mjs');
writeFileSync(tmp, readFileSync(path.join(web, 'i18n.js'), 'utf8') + '\nexport { ES as __ES, ES_PATTERNS as __PAT };\n');
try {
  const m = await import(pathToFileURL(tmp).href);
  const pats = m.__PAT.map(([re, to]) => ({ src: re.source, flags: re.flags, to: typeof to === 'string' ? to : null }));
  process.stdout.write(JSON.stringify({ exact: m.__ES, patterns: pats }));
} finally { unlinkSync(tmp); }
"""

with tempfile.TemporaryDirectory() as tmpdir:
    script = os.path.join(tmpdir, "dump.mjs")
    with open(script, "w", encoding="utf-8") as f:
        f.write(DUMP)
    data = json.loads(subprocess.run(["node", script, web], check=True, capture_output=True).stdout.decode("utf-8"))

# Token bytes; must match I18N_T_* in native/core/i18n.h.
T_ANY, T_LAZY, T_ANY1, T_DIGITS, T_SDIGITS, T_DIGIT, T_SPACES, T_NUM = range(1, 9)
T_WS, T_WS1, T_OPT, T_NOT, T_END = 14, 15, 16, 17, 18
CAPS = {".*": T_ANY, ".*?": T_LAZY, ".+": T_ANY1, r"\d+": T_DIGITS, r"-?\d+": T_SDIGITS,
        r"\d": T_DIGIT, r"\s*": T_SPACES, r"[\d.]+": T_NUM, r"\d+(?:\.\d+)?": T_NUM}
SPECIAL = set("^$.*+?()[]{}|\\")


class Unsupported(Exception):
    pass


def literal(src):
    """A regex fragment that is plain text (escapes allowed) -> the text."""
    out, i = [], 0
    while i < len(src):
        c = src[i]
        if c == "\\":
            if i + 1 >= len(src) or src[i + 1].isalnum():
                raise Unsupported(src)
            out.append(src[i + 1])
            i += 2
        elif c in SPECIAL:
            raise Unsupported(src)
        else:
            out.append(c)
            i += 1
    return "".join(out)


def close_paren(src, i):
    depth = 0
    while i < len(src):
        c = src[i]
        if c == "\\":
            i += 2
            continue
        if c == "[":
            i = src.index("]", i)
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise Unsupported("unbalanced")


def parse(src):
    """JS regex source -> list of items: str (literal), ('tok', bytes, captures),
    ('alt', [str...], captured)."""
    if not (src.startswith("^") and src.endswith("$")) or src.endswith("\\$"):
        raise Unsupported("not anchored")
    s, i, items = src[1:-1], 0, []
    while i < len(s):
        c = s[i]
        nxt = s[i + 1] if i + 1 < len(s) else ""
        if c == "(":
            j = close_paren(s, i)
            inner = s[i + 1:j]
            i = j + 1
            if i < len(s) and s[i] in "?*+{":
                raise Unsupported("quantified group")
            if inner.startswith("?!"):
                items.append(("tok", bytes([T_NOT]) + literal(inner[2:]).encode() + bytes([T_END]), 0))
            elif inner in CAPS:
                items.append(("tok", bytes([CAPS[inner]]), 1))
            elif "|" in inner and not inner.startswith("?"):
                items.append(("alt", [literal(a) for a in inner.split("|")], True))
            else:
                raise Unsupported(inner)
        elif c == "\\" and nxt == "s" and i + 2 < len(s) and s[i + 2] in "*+":
            items.append(("tok", bytes([T_WS if s[i + 2] == "*" else T_WS1]), 0))
            i += 3
        elif c == "[":
            j = s.index("]", i)
            members = s[i + 1:j]
            if "\\" in members or "-" in members or "^" in members:
                raise Unsupported(members)
            i = j + 1
            if i < len(s) and s[i] in "?*+{":
                raise Unsupported("quantified class")
            items.append(("alt", list(members), False))
        elif c in "?*+{":
            if not items or not isinstance(items[-1], str):
                raise Unsupported("quantifier")
            prev = items.pop()
            ch, rest = prev[-1], prev[:-1]
            if rest:
                items.append(rest)
            if c == "?":
                if ord(ch) > 126:
                    raise Unsupported("optional non-ASCII")
                items.append(("tok", bytes([T_OPT]) + ch.encode(), 0))
                i += 1
            elif c == "{":
                m = re.match(r"\{(\d+)\}", s[i:])
                if not m:
                    raise Unsupported("range")
                items.append(ch * int(m.group(1)))
                i += m.end()
            else:
                raise Unsupported("repeat")
        else:
            if c == "\\":
                text, i = literal(s[i:i + 2]), i + 2
            elif c in SPECIAL:
                raise Unsupported(c)
            else:
                text, i = c, i + 1
            if items and isinstance(items[-1], str):
                items[-1] += text
            else:
                items.append(text)
    return items


def expand(items, to):
    """Every combination of the alternatives -> (token bytes, replacement)."""
    alts = [it[1] if isinstance(it, tuple) and it[0] == "alt" else [None] for it in items]
    out = []
    for combo in itertools.product(*alts):
        pat, groups = b"", []   # groups: per JS group, ('n', new number) or ('lit', text)
        n = 0
        for it, pick in zip(items, combo):
            if isinstance(it, str):
                pat += it.encode()
            elif it[0] == "tok":
                pat += it[1]
                if it[2]:
                    n += 1
                    groups.append(("n", n))
            else:
                pat += pick.encode()
                if it[2]:
                    groups.append(("lit", pick))
        def sub(m):
            k = int(m.group(1))
            if k > len(groups):
                raise Unsupported("$%d" % k)
            kind, v = groups[k - 1]
            return "$%d" % v if kind == "n" else v.replace("$", "$$")
        out.append((pat, re.sub(r"\$(\d)", sub, to)))
    return out


def c_str(b):
    out = []
    for i, x in enumerate(b):
        ch = chr(x)
        if ch == '"' or ch == "\\":
            out.append("\\" + ch)
        elif ch == "?":
            out.append("\\?")   # no trigraphs under -std=c11
        elif 32 <= x < 127:
            out.append(ch)
        else:
            out.append("\\%03o" % x)
    return '"' + "".join(out) + '"'


exact = sorted(((k.encode(), v.encode()) for k, v in data["exact"].items()), key=lambda e: e[0])
patterns, skipped = [], []
for p in data["patterns"]:
    if p["to"] is None:
        skipped.append((p["src"], "function replacement"))
        continue
    if p["flags"]:
        skipped.append((p["src"], "flags " + p["flags"]))
        continue
    try:
        patterns += expand(parse(p["src"]), p["to"])
    except Unsupported as e:
        skipped.append((p["src"], "unsupported: %s" % e))

out = ["// Generated by tools/gen_i18n_es.py from the web port's web/i18n*.js -- do not",
       "// edit; regenerate instead.",
       '#include "i18n.h"\n',
       "// English -> Spanish, sorted by the English's bytes (strcmp order)",
       "const I18nEntry I18N_ES[] = {"]
out += ["  {%s, %s}," % (c_str(k), c_str(v)) for k, v in exact]
out.append("};")
out.append("const int32_t I18N_ES_COUNT = (int32_t)(sizeof I18N_ES / sizeof I18N_ES[0]);\n")
out.append("// Strings built around a name or a number: I18N_T_* tokens, first match wins")
out.append("const I18nPattern I18N_ES_PATTERNS[] = {")
out += ["  {%s, %s}," % (c_str(p), c_str(t.encode())) for p, t in patterns]
out.append("};")
out.append("const int32_t I18N_ES_PATTERN_COUNT = (int32_t)(sizeof I18N_ES_PATTERNS / sizeof I18N_ES_PATTERNS[0]);")
with open(os.path.join(root, "native", "core", "i18n_es.c"), "w", newline="\n", encoding="ascii") as f:
    f.write("\n".join(out) + "\n")

sys.stdout.reconfigure(encoding="utf-8")
print("exact", len(exact), "patterns", len(patterns), "from", len(data["patterns"]) - len(skipped),
      "skipped", len(skipped))
for src, why in skipped:
    print("  skipped", src, "--", why)
