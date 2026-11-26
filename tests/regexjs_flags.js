// Casos para tests/regexjs_flags.sh: as flags s (dotAll) e m (multiline) com
// "^", "$" e ".", em todas as formas que o motor otimiza (primeiro byte do
// casamento, "^" so na posicao 0), contra o V8. Imprime
// "esperado<TAB>flags<TAB>padrao<TAB>texto" (quebra de linha escrita como \n, \r como \r).
"use strict";
const PADROES = [
  "^.$", ".", "^.", ".$", "a.b", "^a.b$", "(?:.|x)", "(?:.)", "^(?:.|x)$", "[^a]", "^[^a]$", "\\s", "[\\s\\S]", "^[\\s\\S]$",
  "^$", "^b", "a$", "^a$", "^a\\nb$", "^a.b$", "^.*b", ".*b$", "a.*", "^.+$", "^.?$", "^.{2}$", "^.*$",
  "(?:^|x)a", "(?:^b)", "(?:a|^b)", "(?=.)a", "^(?=.)", "(?<=\\n).", "(?<!.)a", ".\\b", "\\b.", "^(?:.)*$",
  "(?!.)", "^(?!.)", "(?=.$)a", ".a", "x.*?y", "^x.*?y$", "(?:\\n|.)a",
];
// RegExp modifiers are new in V8; older runner Node versions still execute
// the deterministic scoped-modifier cases in regexjs_badge.c.
try {
  new RegExp("(?i:a)");
  PADROES.push("(?i:a)", "(?-i:a)", "^(?-i:[A-Z]+)$", "(?-i:[a-z]*)B",
    "^(?i:a(?-i:b(?i:c))d)$", "(?m:^a$)", "(?-m:^a$)", "(?s:.)", "(?-s:.)",
    "^(?s:a.*b).$", "^(?s-m:.*)$", "(?i-m:^a$)", "(?-i:a)b", "(?i:A)(?-i:B)C",
    "(?<=(?-i:Ab))C", "(?=(?-i:Ab))a.", "^(?s:.+)(?-s:.)$", "^(?-i:a|Ab|ABC)$");
} catch (_) { /* C expectations remain mandatory on these runners. */ }
const TEXTOS = ["", "\n", "\r", "a", "ab", "a\nb", "a\rb", "\na", "a\n", "\n\n", "a\r\nb", "xa\nb", "b", "a\nb\nc", "x\ny", "x\r\ny", "ab\n", "A", "AB", "Ab", "ABC", "AbCD", "aBc", "Abc", "aaab", "AAAb", "a\nb\n"];
const FLAGS = ["", "s", "m", "ms"];
const esc = (t) => t.replace(/\n/g, "\\n").replace(/\r/g, "\\r");
const out = [];
for (const f of FLAGS)
  for (const p of PADROES) {
    let re; try { re = new RegExp(p, "i" + f); } catch (_) { continue; }
    for (const t of TEXTOS) out.push([re.test(t) ? 1 : 0, f || "-", p, esc(t)].join("\t"));
  }
process.stdout.write(out.join("\n") + "\n");
