// Casos para tests/regexjs_pilha.sh: repeticao de GRUPO sobre texto longo, com a
// resposta do V8. "(?:ab |c)*" nao e um caractere so (esses viram O_STAR, sem
// recursao): cada volta do laco abre um nivel no retrocesso, e com o texto de uma
// fonte (ate 8 KB) o motor antigo recursava milhares de niveis de roda() e estourava
// a pilha de um fio de 64 KB (SIGBUS/SIGSEGV). Imprime "esperado<TAB>padrao<TAB>texto".
"use strict";
const R = String.raw;
const palavras = (n, p = "ab ") => p.repeat(n);
const PADROES = [
  // laco de grupo ancorado: decide pelo que vem depois das voltas
  [R`^(?:ab |c)*z`, [palavras(2500), palavras(2500) + "z", palavras(1500, "ab c ") + "z"]],
  [R`^(?:ab |c)*$`, [palavras(2700), palavras(2700) + "x"]],
  [R`^(?:\w+\s)*!`, [palavras(2500), palavras(2500) + "!", palavras(1200, "word ") + "!"]],
  [R`^(?:[a-z]+ )*end$`, [palavras(2500) + "end", palavras(2500) + "enx"]],
  // preguicoso e com alternancia dentro do laco
  [R`^(?:a|b|c|\s)*?z`, [palavras(2500) + "z", palavras(2500)]],
  [R`^(?:ab|c|\s)+?\d`, [palavras(2500) + "7", palavras(2500)]],
  // lacos aninhados
  [R`^(?:(?:ab)+ )*x`, [palavras(2500) + "x", palavras(2500)]],
  [R`^(?:(?:a|b)+ )+x`, [palavras(2500) + "x", palavras(2500)]],
  // lookahead e lookbehind dentro do laco
  [R`^(?:(?=a)\w+ )*!`, [palavras(2500) + "!", palavras(2500)]],
  [R`^(?:(?<=\s|^)ab\s)*!`, [palavras(2500) + "!", palavras(2500)]],
  [R`^(?:ab (?!z))*$`, [palavras(2500), palavras(2500) + "z"]],
  // laco no meio do texto (sem ^): o motor tenta varias posicoes de inicio
  [R`x(?:ab |c)*y`, ["x" + palavras(2500) + "y", "x" + palavras(2500)]],
  [R`(?:ab ){900}z`, [palavras(900) + "z", palavras(901)]],
  // 400 caracteres: varios lacos de grupo e alternancias seguidos
  [R`^(?:ab |cd |ef )*(?:gh |ij )*(?:\d+ )*(?:[a-z]+\. )*(?=.*\bend\b)(?!.*\bBAD\b)` + "(?:q|r|s|t|u|v|w)*".repeat(8) + R`.*$`,
   [palavras(1300, "ab cd ") + " 12 34 end", palavras(1300, "ab cd ") + " end BAD"]],
  [pad400R(R`^(?:ab |c)*(?:`, R`)*z`), [palavras(2500) + "z", palavras(2500)]],
];
function pad400R(ini, fim) {
  let alts = [];
  for (let i = 0; ini.length + alts.join("|").length + fim.length < 395; i++) alts.push("w" + String(i).padStart(3, "0"));
  return ini + alts.join("|") + fim;
}
const esc = (t) => t.replace(/\n/g, "\\n");
const out = [];
for (const [p, textos] of PADROES) {
  const re = new RegExp(p, "i");
  for (const t of textos) out.push([re.test(t) ? 1 : 0, p, esc(t)].join("\t"));
}
process.stdout.write(out.join("\n") + "\n");
