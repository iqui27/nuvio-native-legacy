#ifndef NV_REGEXJS_H
#define NV_REGEXJS_H
#include <stddef.h>

// MOTOR DE REGEX NO DIALETO DO JAVASCRIPT, proprio e pequeno (um so arquivo,
// sem dependencia). Existe para os PACOTES DE SELOS: o pacote do Nuvio traz cada
// regra como `new RegExp(pattern, flags)` do web, e na TV nao ha motor de JS.
//
// Suporta: alternancia, grupos (com e sem captura; a captura nao e guardada),
// classes [..] com faixas e \d \w \s, \d \D \w \W \s \S \b \B, ^ $, ponto,
// quantificadores gulosos e preguicosos (* + ? {n} {n,} {n,m}), lookahead
// (?= ?!) e lookbehind (?<= ?<!), flags i m s e grupos (?i:...) / (?-i:...). Texto em UTF-8 (a classe e o
// ponto consomem um caractere inteiro; maiuscula/minuscula so em ASCII).
// NAO suporta: retrorreferencias (\1, \k<n>) e propriedades \p{..}: padrao com
// elas nao compila (e o chamador o descarta).
//
// Protecao: o casamento tem teto de passos e de profundidade, e estourar conta
// como "nao casou". Nenhum padrao derruba o app nem o trava.
#define REGEXJS_I 1
#define REGEXJS_M 2
#define REGEXJS_S 4

typedef struct RegexJs RegexJs;

// NULL se o padrao e invalido; `erro` (opcional) recebe o motivo.
RegexJs *regexjs_compilar(const char *padrao, int flags, char *erro, size_t errotam);
// Como o web: aceita os grupos de flags em linha no INICIO, "(?i)", "(?ims)"
// (Python/Java), e os vira flags antes de compilar.
RegexJs *regexjs_compilar_web(const char *padrao, char *erro, size_t errotam);
// Imported packs can contain 80 KB release-group rules. Only this entry point
// allows a larger program; normal autoplay regex limits remain unchanged.
#define REGEXJS_BADGE_MAX (128 * 1024)
RegexJs *regexjs_compilar_badge(const char *padrao, char *erro, size_t errotam);
size_t regexjs_memoria(const RegexJs *re); // owned compiled storage, excluding match scratch
// 1 se o padrao casa em QUALQUER ponto do texto (RegExp.prototype.test).
int regexjs_testar(const RegexJs *re, const char *texto, size_t n);
void regexjs_liberar(RegexJs *re);
#endif
