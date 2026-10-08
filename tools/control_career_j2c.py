#!/usr/bin/env python3
"""Mechanical Java -> C translation of Extended's Control.preform
(the nfm web port's decompilation/extended/java-src/Control.java) into
native/core/control_career.inc, the C port's career AI.

Usage:
  python tools/control_career_j2c.py <nfm>/decompilation/extended/java-src/Control.java \
      native/core/control_career.inc

The mapping it applies is documented above the #include in native/core/control.c.
Only the constructs that method actually uses are handled; anything else is
left in place (and reported on stderr) so the C compiler rejects it rather
than it being guessed. native/tests/control_career_test.c checks the result
against web/ext/Control.js.
"""
import re
import sys

JAVA, OUT = sys.argv[1], sys.argv[2]
lines = open(JAVA, encoding='utf-8').read().split('\n')

start = next(i for i, l in enumerate(lines) if 'public void preform(' in l)
end = next(i for i, l in enumerate(lines) if 'public void reset(' in l)
# body: between the method's opening line and its closing brace
body = lines[start + 1:end]
while body and body[-1].strip() == '':
    body.pop()
assert body[-1].strip() == '}', body[-1]
body = body[:-1]
first_java_line = start + 2  # 1-based number of body[0]


def find_close(s, i):
    """s[i] == '(' or '['; return index of the matching close."""
    open_c = s[i]
    close_c = ')' if open_c == '(' else ']'
    depth = 0
    for j in range(i, len(s)):
        if s[j] == open_c:
            depth += 1
        elif s[j] == close_c:
            depth -= 1
            if depth == 0:
                return j
    raise ValueError('unbalanced: ' + s)


def cast_int(s):
    """(int)X -> JTI(X), X a parenthesised expression or a postfix chain."""
    while True:
        k = s.find('(int)')
        if k < 0:
            return s
        i = k + 5
        while s[i] == ' ':
            i += 1
        if s[i] == '(':
            j = find_close(s, i)
            operand = s[i + 1:j]
            rest = s[j + 1:]
        else:
            j = i
            while j < len(s):
                if s[j].isalnum() or s[j] in '_.':
                    j += 1
                elif s[j] in '([':
                    j = find_close(s, j) + 1
                else:
                    break
            operand = s[i:j]
            rest = s[j:]
        s = s[:k] + 'JTI(' + operand + ')' + rest


ORDERED = [
    # Medium / Math / own helpers
    ('this.m.random()', 'medium_random(c->m)'),
    ('this.m.sin(', 'medium_sin(c->m, '),
    ('this.m.cos(', 'medium_cos(c->m, '),
    ('Math.random()', 'nfm_random()'),
    ('Math.abs(', 'abs('),
    ('Math.atan(', 'atan('),
    ('Math.sqrt(', 'sqrt('),
    ('this.pys(', 'control_pys('),
    ('this.py(', 'control_py('),
    # per-car-number tables of Madness: CarDefine (port numbering) or the
    # career context (per slot)
    ('madness.maxmag[madness.cn]', 'mad->cd->maxmag[mad->cn]'),
    ('madness.swits[madness.cn]', 'mad->cd->swits[mad->cn]'),
    ('madness.moment[madness.cn]', 'mad->cd->moment[mad->cn]'),
    ('usermad.moment[usermad.cn]', 'um->cd->moment[um->cn]'),
    ('madness.aistrsp[madness.cn]', 'cx->aistrsp[mad->im]'),
    ('usermad.aistrsp[usermad.cn]', 'cx->aistrsp[um->im]'),
    ('usermad.level[usermad.cn]', 'cx->level[um->im]'),
    ('madness.beast[madness.im]', 'cx->beast[mad->im]'),
    ('madness.shadowcar', 'cx->shadowcar[mad->im]'),
    ('madness.groundlevel', 'cx->groundlevel[mad->im]'),
    ('madness.nostunts', 'cx->nostunts[mad->im]'),
    ('madness.nofix', 'cx->nofix[mad->im]'),
    ('madness.cn', 'mcn'),
    ('usermad.cn', 'ucn'),
    ('madness.', 'mad->'),
    ('usermad.', 'um->'),
    ('conto.floorguardian', 'cx->floorguardian[mad->im]'),
    ('conto.guardswitch', 'cx->guardswitch[mad->im]'),
    ('conto.', 'contO->'),
    ('checkpoints.', 'cp->'),
    ('trackers.', 'trackers->'),
    ('xtgraphics.unlocked[1]', 'cx->unlocked'),
    ('xtgraphics.bonstage', '(cx->bonus != 0)'),
    ('xtgraphics.nplayers', 'nplayers'),
    ('xtgraphics.classicmode', 'xt->classicmode'),
    ('xtgraphics.ptmatch', 'xt->ptmatch'),
    ('xtgraphics.dontdisplay', 'dontdisplay'),
    ('xtgraphics.', 'cx->'),
    ('bots.botbreak', 'cx->botbreak'),
    ('this.variable.', 'cv->'),
    ('this.avoidnlev', 'c->xavoidnlev'),
    ('this.', 'c->'),
]

label_open = re.compile(r'^(\s*)(Label_\w+): \{$')
decl_arr = re.compile(r'^(\s*)(?:final )?int\[\] (\w+) = new int\[(\w+)\];$')

out = []
stack = []  # one entry per '{': label name or None
labels_seen = set()
for n, raw in enumerate(body):
    jl = first_java_line + n
    s = raw
    # comments the decompiler left: keep them
    comment = ''
    ci = s.find('//')
    if ci >= 0:
        comment = ' ' + s[ci:]
        s = s[:ci].rstrip()
    m = label_open.match(s)
    if m:
        name = m.group(2)
        assert name not in labels_seen, name
        labels_seen.add(name)
        stack.append(name)
        out.append(m.group(1) + '{' + comment)
        continue
    m = decl_arr.match(s)
    if m:
        ind, name, size = m.groups()
        cap = size if size.isdigit() else 'CTL_SORT_MAX'
        s = f'{ind}int32_t {name}[{cap}] = {{0}};'
        if not size.isdigit():
            s += f'  // new int[{size}]'
    s = s.replace("'\\0'", '0').replace("'´'", '180').replace("'Z'", '90')
    s = re.sub(r'^(\s*)(?:final )?int\[\] (\w+) = \{', r'\1const int32_t \2[] = {', s)
    s = re.sub(r'\bfinal ', '', s)
    s = re.sub(r'\b(int|char|byte|short)\s+(?=[A-Za-z_])', 'int32_t ', s)
    s = re.sub(r'\bboolean\s+(?=[A-Za-z_])', 'bool ', s)
    s = cast_int(s)
    for a, b in ORDERED:
        s = s.replace(a, b)
    s = re.sub(r'break (Label_\w+);', r'goto \1;', s)
    s = re.sub(r'cx->bonusstage\[(\d+)\]', r'BONUSSTAGE(\1)', s)
    # braces: closing ones may end a labeled block
    res = ''
    for ch in s:
        if ch == '{':
            stack.append(None)
            res += ch
        elif ch == '}':
            top = stack.pop()
            res += ch
            if top is not None:
                res += ' ' + top + ':;'
        else:
            res += ch
    out.append(res + comment)

assert not stack, stack

text = '\n'.join(out)
# operand-order hazards: two Medium draws in one comparison
text = text.replace('medium_random(c->m) > medium_random(c->m)', 'ctl_rand_gt(c->m)')
text = text.replace('medium_random(c->m) <= medium_random(c->m)', 'ctl_rand_le(c->m)')
text = text.replace('Arrays.sort(match2);', 'ctl_sort_ints(match2, fixroutes);')
text = text.replace('Arrays.sort(match);', 'ctl_sort_ints(match, numfixes);')
text = text.replace('Arrays.sort(cx->endsp);', 'ctl_sort_ints(cx->endsp, XT_CAREER_ENDSP);')

leftover = [l for l in text.split('\n') if 'medium_random(c->m)' in l and l.count('medium_random') > 1]
for l in leftover:
    print('TWO DRAWS:', l.strip(), file=sys.stderr)
for pat in ['Math.', 'this.', 'Arrays', 'new ', 'xtgraphics', 'madness', 'checkpoints', 'conto.', 'usermad', '(int)']:
    for l in text.split('\n'):
        if pat in l.split('//')[0]:
            print('LEFT', pat, ':', l.strip(), file=sys.stderr)

def reindent(l):
    k = len(l) - len(l.lstrip(' '))
    return ' ' * (2 + (k - 8) // 2) + l.lstrip(' ') if l.strip() else ''


HEAD = '''// GENERATED by tools/control_career_j2c.py from Extended's Control.java
// (nfm: decompilation/extended/java-src/Control.java:%d-%d, the body of
// preform; line N here is its line N + %d) -- do not edit by hand.
// #included by control.c, which defines what it uses (JTI, BONUSSTAGE,
// ctl_rand_gt/le, ctl_sort_ints, ext_cn) and documents the mapping.

// Locals the decompiled method sets and never reads stay, as in the original.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

static void control_preform_career(Control *c, Mad *mad, ContO *contO, CheckPoints *cp, Trackers *trackers) {
  XtGraphicsStub *xt = mad->xt;
  XtCareerAI *cx = &xt->career;
  XtContva *cv = &cx->contva;
  Mad *um = cx->usermad != NULL ? cx->usermad : mad;
  const int32_t mcn = ext_cn(mad->cn), ucn = ext_cn(um->cn);
  const int32_t nplayers = cx->nplayers > 0 ? cx->nplayers : cp->nplayers;
  const bool dontdisplay = false;   // the Premier Tournament is never career
'''
with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    head_lines = HEAD.count('\n')
    f.write(HEAD % (first_java_line, first_java_line + len(body) - 1, first_java_line - (head_lines + 1)))
    f.write('\n'.join(reindent(l) for l in text.split('\n')) + '\n}\n\n#pragma GCC diagnostic pop\n')
print('body lines', len(body), 'java', first_java_line, '-', first_java_line + len(body) - 1, 'labels', len(labels_seen))
