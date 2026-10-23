#include "regexjs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Ver regexjs.h. Compila para um programa de VM com retrocesso (alternativas numa
// pilha na heap, ver roda), como o regexp.c do mujs mas escrito aqui: pequeno e sem
// licenca de terceiros.

typedef struct { unsigned lo, hi; } Faixa;
// `ascii`: a resposta pronta para os 128 primeiros codigos (com neg e sem-caixa
// ja aplicados), montada ao compilar. O "[\s\S]*" de cada lookahead passa por
// todo o texto, e conferir as ~20 faixas de \s/\S a cada caractere era quase
// metade do tempo de uma regex longa.
typedef struct { Faixa *f; int n, cap; int neg; unsigned char ascii[16]; int pronta; } Classe;

enum { N_VAZIO, N_BYTE, N_ANY, N_CLASSE, N_SEQ, N_ALT, N_REP, N_ASSERT, N_LOOK };
enum { A_BOL, A_EOL, A_WB, A_NWB };

typedef struct No {
  int tipo;
  int a, b;          // N_BYTE: byte; N_REP: min,max (-1 = infinito); N_LOOK: atras,neg; N_ASSERT: tipo
  int lazy;
  int classe;        // N_CLASSE: indice
  int f1, f2;        // filhos (N_SEQ/N_ALT: esquerda e direita; N_REP/N_LOOK: f1)
} No;

enum { O_BYTE, O_ANY, O_CLASSE, O_SPLIT, O_JMP, O_MARK, O_CHK, O_BOL, O_EOL, O_WB, O_NWB,
       O_LOOK, O_STAR, O_MATCH };
// `neg`: em O_LOOK, lookaround negativo; em O_BYTE/O_ANY/O_CLASSE, 1 = consome
// PARA TRAS (corpo de lookbehind, ver emiteNo). `lazy`: em O_STAR.
typedef struct { unsigned char op, neg, lazy; int x, y; } Ins;

struct RegexJs {
  Ins *prog; int nprog, capprog;
  Classe *cls; int ncls, capcls;
  int nreg;
  int flags;
  // MEMORIA DE ESTADOS (ver roda): slot[pc] = indice na tabela de memoria de
  // quem pode comecar uma chamada de roda em `pc`, ou -1 (nao memorizavel).
  int *slot; int nslot;
  // PRIMEIRO BYTE: quando todo casamento consome pelo menos um byte, o conjunto
  // dos bytes que podem abrir um casamento. Posicao de inicio com outro byte nao
  // e tentada (uma lista "\\bESP\\b|\\bLAT\\b|..." de 45 nomes so roda nas letras
  // iniciais deles, nao em cada byte do texto).
  unsigned char primeiro[32]; int temPrimeiro;
  // CONTINUACAO DE CADA O_STAR: o conjunto de bytes que a continuacao pode
  // consumir primeiro (seg[in->y - 1]; y = 0 = sem filtro). Recuando o
  // "[\s\S]*" de "(?![\s\S]*\bDTS)", so as posicoes com "D"/"d" chamam a
  // continuacao, e nao cada byte do texto.
  unsigned char (*seg)[32]; int nseg;
  // "^" logo no inicio (sem a flag m): so a posicao 0 pode casar.
  int ancorado;
};

typedef struct {
  const char *p, *fim;
  No *no; int nno, capno;
  Classe *cls; int ncls, capcls;
  int flags;
  char erro[96];
} Pr;

static int falha(Pr *pr, const char *m) { if (!pr->erro[0]) snprintf(pr->erro, sizeof pr->erro, "%s", m); return -1; }

static int novoNo(Pr *pr, int tipo) {
  if (pr->nno == pr->capno) {
    int c = pr->capno ? pr->capno * 2 : 32;
    No *n = realloc(pr->no, (size_t)c * sizeof *n);
    if (!n) return falha(pr, "out of memory");
    pr->no = n; pr->capno = c;
  }
  memset(&pr->no[pr->nno], 0, sizeof(No));
  pr->no[pr->nno].tipo = tipo;
  pr->no[pr->nno].f1 = pr->no[pr->nno].f2 = -1;
  return pr->nno++;
}

static int novaClasse(Pr *pr) {
  if (pr->ncls == pr->capcls) {
    int c = pr->capcls ? pr->capcls * 2 : 8;
    Classe *n = realloc(pr->cls, (size_t)c * sizeof *n);
    if (!n) return falha(pr, "out of memory");
    pr->cls = n; pr->capcls = c;
  }
  memset(&pr->cls[pr->ncls], 0, sizeof(Classe));
  return pr->ncls++;
}

static int addFaixa(Pr *pr, Classe *c, unsigned lo, unsigned hi) {
  if (c->n == c->cap) {
    int cap = c->cap ? c->cap * 2 : 8;
    Faixa *f = realloc(c->f, (size_t)cap * sizeof *f);
    if (!f) return falha(pr, "out of memory");
    c->f = f; c->cap = cap;
  }
  c->f[c->n].lo = lo; c->f[c->n].hi = hi; c->n++;
  return 0;
}

// \d \w \s (e o complemento), como faixas de codepoints.
static int addAtalho(Pr *pr, Classe *c, int k, int neg) {
  static const Faixa D[] = {{'0','9'}};
  static const Faixa W[] = {{'0','9'},{'A','Z'},{'_','_'},{'a','z'}};
  static const Faixa S[] = {{9,13},{32,32},{0xA0,0xA0},{0x1680,0x1680},{0x2000,0x200A},
                            {0x2028,0x2029},{0x202F,0x202F},{0x205F,0x205F},{0x3000,0x3000},{0xFEFF,0xFEFF}};
  const Faixa *t; int n, i;
  if (k == 'd') { t = D; n = 1; } else if (k == 'w') { t = W; n = 4; } else { t = S; n = 10; }
  if (!neg) { for (i = 0; i < n; i++) if (addFaixa(pr, c, t[i].lo, t[i].hi) < 0) return -1; return 0; }
  { unsigned ini = 0;
    for (i = 0; i < n; i++) {
      if (t[i].lo > ini && addFaixa(pr, c, ini, t[i].lo - 1) < 0) return -1;
      ini = t[i].hi + 1;
    }
    return addFaixa(pr, c, ini, 0x10FFFF); }
}

static unsigned decodifica(const unsigned char *s, const unsigned char *fim, int *len) {
  unsigned c = s[0];
  if (c < 0x80) { *len = 1; return c; }
  if (c >= 0xC2 && c < 0xE0 && s + 1 < fim && (s[1] & 0xC0) == 0x80) { *len = 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
  if (c >= 0xE0 && c < 0xF0 && s + 2 < fim && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
    *len = 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
  if (c >= 0xF0 && c < 0xF5 && s + 3 < fim && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
    *len = 4; return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); }
  *len = 1; return c;
}

static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

// Le o que vem DEPOIS de uma barra invertida e devolve um codepoint literal, ou
// -1 se for atalho/asserção (tratado pelo chamador, que olha *pr->p antes).
static int escapeLiteral(Pr *pr, unsigned *cp) {
  int c = (unsigned char)*pr->p++, i, v, h;
  switch (c) {
    case 'n': *cp = '\n'; return 0;
    case 'r': *cp = '\r'; return 0;
    case 't': *cp = '\t'; return 0;
    case 'v': *cp = 11; return 0;
    case 'f': *cp = 12; return 0;
    case '0': *cp = 0; return 0;
    case 'x':
      if (pr->fim - pr->p >= 2 && hexv(pr->p[0]) >= 0 && hexv(pr->p[1]) >= 0) {
        *cp = (unsigned)(hexv(pr->p[0]) * 16 + hexv(pr->p[1])); pr->p += 2; return 0; }
      *cp = 'x'; return 0;
    case 'u':
      if (pr->p < pr->fim && *pr->p == '{') {
        const char *q = pr->p + 1; unsigned x = 0; int nd = 0;
        while (q < pr->fim && hexv(*q) >= 0 && nd < 7) { x = x * 16 + (unsigned)hexv(*q); q++; nd++; }
        if (nd && q < pr->fim && *q == '}' && x <= 0x10FFFF) { *cp = x; pr->p = q + 1; return 0; }
        *cp = 'u'; return 0;
      }
      if (pr->fim - pr->p >= 4) {
        v = 0;
        for (i = 0; i < 4; i++) { h = hexv(pr->p[i]); if (h < 0) break; v = v * 16 + h; }
        if (i == 4) { *cp = (unsigned)v; pr->p += 4; return 0; }
      }
      *cp = 'u'; return 0;
    case 'c':
      if (pr->p < pr->fim && isalpha((unsigned char)*pr->p)) { *cp = (unsigned)(*pr->p++ & 31); return 0; }
      *cp = '\\'; pr->p--; return 0;
    default:
      if (c >= '1' && c <= '9') return falha(pr, "backreference unsupported");
      if (c == 'p' || c == 'P' || c == 'k') return falha(pr, "unsupported escape");
      pr->p--;
      { int l; *cp = decodifica((const unsigned char *)pr->p, (const unsigned char *)pr->fim, &l); pr->p += l; }
      return 0;
  }
}

static int parseAlt(Pr *pr, int prof);
static int lerChaves(Pr *pr, int *mn, int *mx);

static int parseClasse(Pr *pr) {   // depois do '['
  int ci = novaClasse(pr), neg = 0;
  if (ci < 0) return -1;
  if (pr->p < pr->fim && *pr->p == '^') { neg = 1; pr->p++; }
  for (;;) {
    unsigned lo, hi; int l, atalho = 0;
    if (pr->p >= pr->fim) return falha(pr, "unclosed class");
    if (*pr->p == ']') { pr->p++; break; }
    if (*pr->p == '\\') {
      char k;
      pr->p++;
      if (pr->p >= pr->fim) return falha(pr, "trailing backslash");
      k = *pr->p;
      if (k == 'd' || k == 'w' || k == 's' || k == 'D' || k == 'W' || k == 'S') {
        pr->p++;
        if (addAtalho(pr, &pr->cls[ci], tolower((unsigned char)k), isupper((unsigned char)k)) < 0) return -1;
        atalho = 1;
      } else if (k == 'b') { pr->p++; lo = 8; }
      else if (k == '-') { pr->p++; lo = '-'; }
      else if (escapeLiteral(pr, &lo) < 0) return -1;
    } else lo = decodifica((const unsigned char *)pr->p, (const unsigned char *)pr->fim, &l), pr->p += l;
    if (atalho) continue;
    hi = lo;
    if (pr->fim - pr->p >= 2 && pr->p[0] == '-' && pr->p[1] != ']') {
      pr->p++;
      if (*pr->p == '\\') {
        char k;
        pr->p++;
        if (pr->p >= pr->fim) return falha(pr, "trailing backslash");
        k = *pr->p;
        if (k == 'd' || k == 'w' || k == 's' || k == 'D' || k == 'W' || k == 'S') {
          // "a-\d": o hifen e literal.
          if (addFaixa(pr, &pr->cls[ci], lo, lo) < 0 || addFaixa(pr, &pr->cls[ci], '-', '-') < 0) return -1;
          pr->p++;
          if (addAtalho(pr, &pr->cls[ci], tolower((unsigned char)k), isupper((unsigned char)k)) < 0) return -1;
          continue;
        }
        if (k == 'b') { pr->p++; hi = 8; }
        else if (escapeLiteral(pr, &hi) < 0) return -1;
      } else hi = decodifica((const unsigned char *)pr->p, (const unsigned char *)pr->fim, &l), pr->p += l;
      if (hi < lo) return falha(pr, "reversed range");
    }
    if (addFaixa(pr, &pr->cls[ci], lo, hi) < 0) return -1;
  }
  pr->cls[ci].neg = neg;
  { int n = novoNo(pr, N_CLASSE);
    if (n < 0) return -1;
    pr->no[n].classe = ci;
    return n; }
}

static int parseAtomo(Pr *pr, int prof) {
  int c = (unsigned char)*pr->p, n, l;
  if (c == '(') {
    int look = 0, atras = 0, neg = 0, filho;
    pr->p++;
    if (pr->fim - pr->p >= 2 && pr->p[0] == '?') {
      char k = pr->p[1];
      if (k == ':') pr->p += 2;
      else if (k == '=' || k == '!') { look = 1; neg = k == '!'; pr->p += 2; }
      else if (k == '<' && pr->fim - pr->p >= 3 && (pr->p[2] == '=' || pr->p[2] == '!')) {
        look = 1; atras = 1; neg = pr->p[2] == '!'; pr->p += 3; }
      else if (k == '<') {   // bad group name: (?<nome>...)
        const char *q = pr->p + 2;
        while (q < pr->fim && *q != '>') q++;
        if (q >= pr->fim) return falha(pr, "bad group name");
        pr->p = q + 1;
      } else return falha(pr, "unknown group");
    }
    filho = parseAlt(pr, prof + 1);
    if (filho < 0) return -1;
    if (pr->p >= pr->fim || *pr->p != ')') return falha(pr, "unclosed group");
    pr->p++;
    if (!look) return filho;
    n = novoNo(pr, N_LOOK);
    if (n < 0) return -1;
    pr->no[n].a = atras; pr->no[n].b = neg; pr->no[n].f1 = filho;
    return n;
  }
  if (c == '[') { pr->p++; return parseClasse(pr); }
  if (c == '.') { pr->p++; return novoNo(pr, N_ANY); }
  if (c == '^' || c == '$') {
    pr->p++;
    n = novoNo(pr, N_ASSERT);
    if (n >= 0) pr->no[n].a = c == '^' ? A_BOL : A_EOL;
    return n;
  }
  if (c == '\\') {
    char k;
    pr->p++;
    if (pr->p >= pr->fim) return falha(pr, "trailing backslash");
    k = *pr->p;
    if (k == 'b' || k == 'B') {
      pr->p++;
      n = novoNo(pr, N_ASSERT);
      if (n >= 0) pr->no[n].a = k == 'b' ? A_WB : A_NWB;
      return n;
    }
    if (k == 'd' || k == 'w' || k == 's' || k == 'D' || k == 'W' || k == 'S') {
      int ci = novaClasse(pr);
      pr->p++;
      if (ci < 0 || addAtalho(pr, &pr->cls[ci], tolower((unsigned char)k), isupper((unsigned char)k)) < 0) return -1;
      n = novoNo(pr, N_CLASSE);
      if (n >= 0) pr->no[n].classe = ci;
      return n;
    }
    { unsigned cp;
      if (escapeLiteral(pr, &cp) < 0) return -1;
      // Um codepoint literal vira sequencia de bytes UTF-8 (atomica no AST).
      { unsigned char b[4]; int nb, i, seq = -1, ult = -1;
        if (cp < 0x80) { b[0] = (unsigned char)cp; nb = 1; }
        else if (cp < 0x800) { b[0] = (unsigned char)(0xC0 | cp >> 6); b[1] = (unsigned char)(0x80 | (cp & 63)); nb = 2; }
        else if (cp < 0x10000) { b[0] = (unsigned char)(0xE0 | cp >> 12); b[1] = (unsigned char)(0x80 | ((cp >> 6) & 63)); b[2] = (unsigned char)(0x80 | (cp & 63)); nb = 3; }
        else { b[0] = (unsigned char)(0xF0 | cp >> 18); b[1] = (unsigned char)(0x80 | ((cp >> 12) & 63)); b[2] = (unsigned char)(0x80 | ((cp >> 6) & 63)); b[3] = (unsigned char)(0x80 | (cp & 63)); nb = 4; }
        for (i = 0; i < nb; i++) {
          int bn = novoNo(pr, N_BYTE);
          if (bn < 0) return -1;
          pr->no[bn].a = b[i];
          if (nb == 1) return bn;
          if (seq < 0) seq = bn;
          else { int s = novoNo(pr, N_SEQ); if (s < 0) return -1; pr->no[s].f1 = ult; pr->no[s].f2 = bn; ult = s; continue; }
          ult = seq;
        }
        return ult; }
    }
  }
  if (c == '*' || c == '+' || c == '?') return falha(pr, "nothing to repeat");
  // "{3}" / "{2,5}" no lugar de um atomo (depois de outro quantificador, no
  // inicio, depois de "|"): o V8 recusa. "{" que nao forma quantificador ("x{",
  // "{a}") continua literal.
  if (c == '{') {
    int mn, mx; const char *p0 = pr->p;
    if (lerChaves(pr, &mn, &mx)) { pr->p = p0; return falha(pr, "nothing to repeat"); }
  }
  // literal (um codepoint UTF-8 inteiro, para o quantificador valer para ele todo)
  { int nb;
    decodifica((const unsigned char *)pr->p, (const unsigned char *)pr->fim, &l);
    nb = l;
    { int i, ult = -1;
      for (i = 0; i < nb; i++) {
        int bn = novoNo(pr, N_BYTE);
        if (bn < 0) return -1;
        pr->no[bn].a = (unsigned char)pr->p[i];
        if (ult < 0) ult = bn;
        else { int s = novoNo(pr, N_SEQ); if (s < 0) return -1; pr->no[s].f1 = ult; pr->no[s].f2 = bn; ult = s; }
      }
      pr->p += nb;
      return ult; }
  }
}

// {n}, {n,}, {n,m}: 1 se ha um quantificador valido em pr->p (e o consome).
static int lerChaves(Pr *pr, int *mn, int *mx) {
  const char *q = pr->p + 1;
  long a = 0, b;
  int tem = 0;
  while (q < pr->fim && *q >= '0' && *q <= '9') { a = a * 10 + (*q - '0'); if (a > 100000) a = 100000; q++; tem = 1; }
  if (!tem) return 0;
  if (q < pr->fim && *q == '}') { *mn = *mx = (int)a; pr->p = q + 1; return 1; }
  if (q >= pr->fim || *q != ',') return 0;
  q++;
  if (q < pr->fim && *q == '}') { *mn = (int)a; *mx = -1; pr->p = q + 1; return 1; }
  b = 0; tem = 0;
  while (q < pr->fim && *q >= '0' && *q <= '9') { b = b * 10 + (*q - '0'); if (b > 100000) b = 100000; q++; tem = 1; }
  if (!tem || q >= pr->fim || *q != '}') return 0;
  *mn = (int)a; *mx = (int)b; pr->p = q + 1;
  if (*mx < *mn) return -1;
  return 1;
}

static int parseTermo(Pr *pr, int prof) {
  const char *ini = pr->p;
  int at = parseAtomo(pr, prof), mn, mx, r;
  if (at < 0) return -1;
  if (pr->p >= pr->fim) return at;
  switch (*pr->p) {
    case '*': mn = 0; mx = -1; pr->p++; break;
    case '+': mn = 1; mx = -1; pr->p++; break;
    case '?': mn = 0; mx = 1; pr->p++; break;
    case '{':
      r = lerChaves(pr, &mn, &mx);
      if (r < 0) return falha(pr, "reversed quantifier");
      if (!r) return at;   // '{' solto e literal (sem a flag u)
      break;
    default: return at;
  }
  // Como o V8 (sem a flag u): "\b*" e "^+" sao erro, mas "(?:\b)*" e um grupo e
  // vale; lookahead pode ter quantificador (Anexo B), lookbehind nao.
  if (pr->no[at].tipo == N_ASSERT && *ini != '(') return falha(pr, "nothing to repeat");
  if (pr->no[at].tipo == N_LOOK && pr->no[at].a && !strncmp(ini, "(?<", 3)) return falha(pr, "nothing to repeat");
  { int n = novoNo(pr, N_REP);
    if (n < 0) return -1;
    pr->no[n].a = mn; pr->no[n].b = mx; pr->no[n].f1 = at;
    if (pr->p < pr->fim && *pr->p == '?') { pr->no[n].lazy = 1; pr->p++; }
    return n; }
}

// A sequencia vira uma arvore EQUILIBRADA de N_SEQ (a ordem das folhas e a do
// padrao). Em cadeia, uma regex de 500 caracteres tinha 500 niveis de N_SEQ, e
// emiteNo / minlen / primeiros recursavam 500 vezes (~45 KB de pilha, quase tudo
// de um fio de 64 KB); equilibrada sao 9 niveis.
static int seqEquilibrada(Pr *pr, const int *t, int a, int b) {
  int m, l, r, s;
  if (b - a == 1) return t[a];
  m = a + (b - a) / 2;
  if ((l = seqEquilibrada(pr, t, a, m)) < 0) return -1;
  if ((r = seqEquilibrada(pr, t, m, b)) < 0) return -1;
  if ((s = novoNo(pr, N_SEQ)) < 0) return -1;
  pr->no[s].f1 = l; pr->no[s].f2 = r;
  return s;
}

static int parseSeq(Pr *pr, int prof) {
  int *t = NULL, n = 0, cap = 0, r;
  while (pr->p < pr->fim && *pr->p != '|' && *pr->p != ')') {
    int termo = parseTermo(pr, prof);
    if (termo < 0) { free(t); return -1; }
    if (n == cap) {
      int *nt = realloc(t, (size_t)(cap ? cap * 2 : 16) * sizeof *t);
      if (!nt) { free(t); return falha(pr, "out of memory"); }
      t = nt; cap = cap ? cap * 2 : 16;
    }
    t[n++] = termo;
  }
  r = n ? seqEquilibrada(pr, t, 0, n) : novoNo(pr, N_VAZIO);
  free(t);
  return r;
}

static int parseAlt(Pr *pr, int prof) {
  int e;
  if (prof > 100) return falha(pr, "nested too deep");
  e = parseSeq(pr, prof);
  if (e < 0) return -1;
  while (pr->p < pr->fim && *pr->p == '|') {
    int d, a;
    pr->p++;
    d = parseSeq(pr, prof);
    if (d < 0) return -1;
    a = novoNo(pr, N_ALT);
    if (a < 0) return -1;
    pr->no[a].f1 = e; pr->no[a].f2 = d;
    e = a;
  }
  return e;
}

// ---- emissao ---------------------------------------------------------------
#define PROG_MAX 30000

// Trechos do programa cujo resultado depende de um registrador e nao so de
// (pc, posicao): o corpo de uma repeticao que pode casar vazio (entre O_MARK e
// O_CHK). Nada ali e memorizado. `atras` = emitindo o corpo de um lookbehind.
typedef struct { int ini, fim; } Trecho;
typedef struct { RegexJs *re; Pr *pr; int erro; int atras; Trecho *tr; int ntr, captr; } Em;

static void marcaTrecho(Em *em, int ini, int fim) {
  if (em->erro || fim < ini) return;
  if (em->ntr == em->captr) {
    int c = em->captr ? em->captr * 2 : 8;
    Trecho *t = realloc(em->tr, (size_t)c * sizeof *t);
    if (!t) { em->erro = 1; return; }
    em->tr = t; em->captr = c;
  }
  em->tr[em->ntr].ini = ini; em->tr[em->ntr].fim = fim; em->ntr++;
}

static int emite(Em *em, int op, int neg, int x, int y) {
  RegexJs *re = em->re;
  if (re->nprog >= PROG_MAX) { em->erro = 1; return 0; }
  if (re->nprog == re->capprog) {
    int c = re->capprog ? re->capprog * 2 : 64;
    Ins *n = realloc(re->prog, (size_t)c * sizeof *n);
    if (!n) { em->erro = 1; return 0; }
    re->prog = n; re->capprog = c;
  }
  re->prog[re->nprog].op = (unsigned char)op;
  re->prog[re->nprog].neg = (unsigned char)neg;
  re->prog[re->nprog].lazy = 0;
  re->prog[re->nprog].x = x; re->prog[re->nprog].y = y;
  return re->nprog++;
}

static int minlen(const Pr *pr, int n) {
  const No *o = &pr->no[n];
  switch (o->tipo) {
    case N_BYTE: case N_ANY: case N_CLASSE: return 1;
    case N_SEQ: return minlen(pr, o->f1) + minlen(pr, o->f2);
    case N_ALT: { int a = minlen(pr, o->f1), b = minlen(pr, o->f2); return a < b ? a : b; }
    case N_REP: return o->a > 0 ? minlen(pr, o->f1) : 0;
    default: return 0;
  }
}

static void emiteNo(Em *em, int n);

static void emiteRep(Em *em, const No *o) {
  RegexJs *re = em->re;
  int i, mn = o->a, mx = o->b, vazio = minlen(em->pr, o->f1) == 0;
  int *fins, nf = 0;
  if (mn > 1000 || mx > 1000) { em->erro = 1; return; }
  for (i = 0; i < mn && !em->erro; i++) emiteNo(em, o->f1);
  // REPETICAO DE UM CARACTERE SO (".*", "\\s*", "[^x]+", "a*"): O_STAR seguido
  // da instrucao do caractere. A VM come o maximo de uma vez e recua um a um,
  // sem uma chamada recursiva por caractere: o ".*" do "(?=.*...)" deixa de
  // custar a pilha do tamanho da linha (a TV roda isso num fio comum).
  if (mx < 0) {
    const No *c = &em->pr->no[o->f1];
    if (c->tipo == N_ANY || c->tipo == N_CLASSE || c->tipo == N_BYTE) {
      int st = emite(em, O_STAR, 0, 0, 0);
      if (em->erro) return;
      re->prog[st].lazy = (unsigned char)o->lazy;
      emiteNo(em, o->f1);
      re->prog[st].x = re->nprog;   // a continuacao
      return;
    }
  }
  if (mx < 0) {
    int l1 = emite(em, O_SPLIT, 0, 0, 0), reg = -1, corpo = re->nprog;
    if (em->erro) return;
    if (vazio) { reg = re->nreg++; emite(em, O_MARK, 0, reg, 0); }
    emiteNo(em, o->f1);
    if (vazio) { int chk = emite(em, O_CHK, 0, reg, 0); marcaTrecho(em, corpo + 1, chk); }
    emite(em, O_JMP, 0, l1, 0);
    if (em->erro) return;
    // x = proximo da preferencia, y = o outro
    if (!o->lazy) { re->prog[l1].x = corpo; re->prog[l1].y = re->nprog; }
    else { re->prog[l1].x = re->nprog; re->prog[l1].y = corpo; }
    return;
  }
  // {n,m}: ate m-n copias opcionais (".{0,600}" e valido no V8; antes um vetor
  // fixo de 512 recusava o padrao).
  if (mx <= mn) return;
  if (!(fins = malloc((size_t)(mx - mn) * sizeof *fins))) { em->erro = 1; return; }
  for (i = mn; i < mx && !em->erro; i++) {
    int sp = emite(em, O_SPLIT, 0, 0, 0);
    if (em->erro) break;
    fins[nf++] = sp;
    if (!o->lazy) re->prog[sp].x = re->nprog; else re->prog[sp].y = re->nprog;
    emiteNo(em, o->f1);
  }
  if (!em->erro)
    for (i = 0; i < nf; i++) {
      if (!o->lazy) re->prog[fins[i]].y = re->nprog; else re->prog[fins[i]].x = re->nprog;
    }
  free(fins);
}

static void emiteNo(Em *em, int n) {
  RegexJs *re = em->re;
  const No *o = &em->pr->no[n];
  int a, b;
  if (em->erro) return;
  switch (o->tipo) {
    case N_VAZIO: break;
    case N_BYTE: emite(em, O_BYTE, em->atras, o->a, 0); break;
    case N_ANY: emite(em, O_ANY, em->atras, 0, 0); break;
    case N_CLASSE: emite(em, O_CLASSE, em->atras, o->classe, 0); break;
    // Para tras, a sequencia casa da direita para a esquerda (como o V8).
    case N_SEQ:
      if (em->atras) { emiteNo(em, o->f2); emiteNo(em, o->f1); }
      else { emiteNo(em, o->f1); emiteNo(em, o->f2); }
      break;
    case N_ALT:
      a = emite(em, O_SPLIT, 0, 0, 0);
      if (em->erro) return;
      re->prog[a].x = re->nprog;
      emiteNo(em, o->f1);
      b = emite(em, O_JMP, 0, 0, 0);
      if (em->erro) return;
      re->prog[a].y = re->nprog;
      emiteNo(em, o->f2);
      re->prog[b].x = re->nprog;
      break;
    case N_REP: emiteRep(em, o); break;
    case N_ASSERT:
      emite(em, o->a == A_BOL ? O_BOL : o->a == A_EOL ? O_EOL : o->a == A_WB ? O_WB : O_NWB, 0, 0, 0);
      break;
    // LOOKAROUND: o corpo roda a partir da posicao atual e basta chegar ao
    // O_MATCH dele. O lookbehind e o MESMO, com o corpo compilado para tras
    // (como o V8 faz): nada de tentar cada inicio possivel e conferir se o fim
    // bate, o que custava o quadrado do texto e nao podia ser lembrado.
    // Lookahead dentro de lookbehind volta a andar para frente.
    case N_LOOK: {
      int antes = em->atras;
      a = emite(em, O_LOOK, o->b, 0, 0);
      if (em->erro) return;
      em->atras = o->a;
      emiteNo(em, o->f1);
      em->atras = antes;
      emite(em, O_MATCH, 0, 0, 0);
      if (!em->erro) re->prog[a].x = re->nprog;
      break; }
  }
}

// Bytes que podem abrir o casamento do no `n` (em `c`); devolve 1 se o no pode
// casar sem consumir nada. Asserções e lookarounds sao transparentes (nao
// consomem); conservador onde ha UTF-8 (todo byte >= 0x80 entra).
static int noClasse(const Classe *c, unsigned cp, int icase);
#define POE(c, b) ((c)[(b) >> 3] |= (unsigned char)(1u << ((b) & 7)))
static int primeiros(const Pr *pr, const Classe *cls, int flags, int n, unsigned char *c) {
  const No *o = &pr->no[n];
  int b;
  switch (o->tipo) {
    case N_BYTE:
      POE(c, o->a);
      if ((flags & REGEXJS_I) && isalpha(o->a)) { POE(c, tolower(o->a)); POE(c, toupper(o->a)); }
      return 0;
    case N_ANY:
      // Com a flag s o ponto casa tambem \n e \r (como o O_ANY de primeirosProg).
      for (b = 0; b < 256; b++) if ((flags & REGEXJS_S) || (b != '\n' && b != '\r')) POE(c, b);
      return 0;
    case N_CLASSE:
      for (b = 0; b < 128; b++) if (noClasse(&cls[o->classe], (unsigned)b, flags & REGEXJS_I)) POE(c, b);
      for (b = 128; b < 256; b++) POE(c, b);
      return 0;
    case N_SEQ:
      if (!primeiros(pr, cls, flags, o->f1, c)) return 0;
      return primeiros(pr, cls, flags, o->f2, c);
    case N_ALT: {
      int a = primeiros(pr, cls, flags, o->f1, c);
      return primeiros(pr, cls, flags, o->f2, c) | a; }
    case N_REP: {
      int v = primeiros(pr, cls, flags, o->f1, c);
      return v || o->a == 0; }
    default: return 1;   // vazio, asserção, lookaround
  }
}

void regexjs_liberar(RegexJs *re) {
  int i;
  if (!re) return;
  for (i = 0; i < re->ncls; i++) free(re->cls[i].f);
  free(re->cls); free(re->prog); free(re->slot); free(re->seg); free(re);
}

// Quem pode ser o `pc` de entrada de roda: o inicio do programa, o ramo x de
// cada O_SPLIT e o corpo de cada O_LOOK. Fora dos trechos dependentes de
// registrador, o resultado dessa chamada so depende de (pc, posicao) — o motor
// nao tem retrorreferencia —, entao pode ser lembrado (ver roda).
static int montaSlots(RegexJs *re, const Em *em) {
  unsigned char *fora = calloc((size_t)re->nprog + 1, 1);
  int i, j;
  if (!fora) return 0;
  re->slot = malloc(((size_t)re->nprog + 1) * sizeof *re->slot);
  if (!re->slot) { free(fora); return 0; }
  for (i = 0; i < em->ntr; i++)
    for (j = em->tr[i].ini; j <= em->tr[i].fim && j < re->nprog; j++) fora[j] = 1;
  for (i = 0; i < re->nprog; i++) re->slot[i] = -1;
  re->nslot = 0;
#define SLOT(pc) do { int p_ = (pc); \
    if (p_ >= 0 && p_ < re->nprog && !fora[p_] && re->slot[p_] < 0) re->slot[p_] = re->nslot++; } while (0)
  SLOT(0);
  for (i = 0; i < re->nprog; i++) {
    if (fora[i]) continue;
    if (re->prog[i].op == O_STAR) SLOT(i);
    if (re->prog[i].op == O_SPLIT || re->prog[i].op == O_STAR) SLOT(re->prog[i].x);
    else if (re->prog[i].op == O_LOOK) SLOT(i + 1);
  }
#undef SLOT
  free(fora);
  return 1;
}

// Bytes que o programa, a partir de `pc`, pode consumir PRIMEIRO (para frente),
// em `c`. Devolve 1 quando pode chegar ao O_MATCH sem consumir nada ou consome
// para tras (corpo de lookbehind): ai nao ha filtro. Asserções, marcas e
// lookarounds nao consomem e sao atravessados (o que vem depois deles e que
// consome na mesma posicao). `visto` corta os lacos: um pc ja visitado ja somou
// os seus bytes, e um "1" de qualquer caminho anula o conjunto inteiro.
static int primeirosProg(const RegexJs *re, int pc, unsigned char *c, unsigned char *visto) {
  int b;
  for (;;) {
    const Ins *in;
    if (pc < 0 || pc >= re->nprog) return 1;
    if (visto[pc]) return 0;
    visto[pc] = 1;
    in = &re->prog[pc];
    switch (in->op) {
      case O_BYTE:
        if (in->neg) return 1;
        POE(c, in->x);
        if ((re->flags & REGEXJS_I) && isalpha(in->x)) { POE(c, tolower(in->x)); POE(c, toupper(in->x)); }
        return 0;
      case O_ANY:
        if (in->neg) return 1;
        for (b = 0; b < 256; b++) if ((re->flags & REGEXJS_S) || (b != '\n' && b != '\r')) POE(c, b);
        return 0;
      case O_CLASSE:
        if (in->neg) return 1;
        for (b = 0; b < 128; b++) if (noClasse(&re->cls[in->x], (unsigned)b, re->flags & REGEXJS_I)) POE(c, b);
        for (b = 128; b < 256; b++) POE(c, b);
        return 0;
      case O_STAR:   // o corpo (pc + 1) ou, com zero repeticoes, a continuacao
        if (primeirosProg(re, pc + 1, c, visto)) return 1;
        pc = in->x; break;
      case O_SPLIT:
        if (primeirosProg(re, in->x, c, visto)) return 1;
        pc = in->y; break;
      case O_JMP: case O_LOOK: pc = in->x; break;
      case O_MARK: case O_CHK: case O_BOL: case O_EOL: case O_WB: case O_NWB: pc++; break;
      default: return 1;   // O_MATCH
    }
  }
}

static int montaSeg(RegexJs *re) {
  int i, k = 0;
  unsigned char *visto, c[32];
  for (i = 0; i < re->nprog; i++) if (re->prog[i].op == O_STAR) k++;
  if (!k) return 1;
  if (!(re->seg = malloc((size_t)k * sizeof *re->seg))) return 0;
  if (!(visto = malloc((size_t)re->nprog + 1))) return 0;
  for (i = 0; i < re->nprog; i++) {
    Ins *in = &re->prog[i];
    if (in->op != O_STAR) continue;
    in->y = 0;
    memset(visto, 0, (size_t)re->nprog + 1);
    memset(c, 0, sizeof c);
    if (primeirosProg(re, in->x, c, visto)) continue;
    memcpy(re->seg[re->nseg], c, sizeof c);
    in->y = ++re->nseg;
  }
  free(visto);
  return 1;
}

RegexJs *regexjs_compilar(const char *padrao, int flags, char *erro, size_t errotam) {
  Pr pr; Em em; RegexJs *re; int raiz;
  if (erro && errotam) erro[0] = 0;
  if (!padrao) return NULL;
  memset(&pr, 0, sizeof pr);
  pr.p = padrao; pr.fim = padrao + strlen(padrao); pr.flags = flags;
  raiz = parseAlt(&pr, 0);
  if (raiz >= 0 && pr.p < pr.fim) { raiz = -1; falha(&pr, "unmatched )"); }
  if (raiz < 0) {
    int i;
    if (erro && errotam) snprintf(erro, errotam, "%s", pr.erro[0] ? pr.erro : "invalid pattern");
    for (i = 0; i < pr.ncls; i++) free(pr.cls[i].f);
    free(pr.cls); free(pr.no);
    return NULL;
  }
  re = calloc(1, sizeof *re);
  if (!re) { free(pr.no); free(pr.cls); return NULL; }
  re->flags = flags;
  memset(&em, 0, sizeof em);
  em.re = re; em.pr = &pr;
  emiteNo(&em, raiz);
  emite(&em, O_MATCH, 0, 0, 0);
  re->cls = pr.cls; re->ncls = pr.ncls; re->capcls = pr.capcls;
  { int i; unsigned cp;
    for (i = 0; i < re->ncls; i++) {
      Classe *c = &re->cls[i];
      memset(c->ascii, 0, sizeof c->ascii);
      c->pronta = 0;
      for (cp = 0; cp < 128; cp++)
        if (noClasse(c, cp, flags & REGEXJS_I)) c->ascii[cp >> 3] |= (unsigned char)(1u << (cp & 7));
      c->pronta = 1;
    } }
  if (!em.erro) {
    memset(re->primeiro, 0, sizeof re->primeiro);
    re->temPrimeiro = !primeiros(&pr, pr.cls, flags, raiz, re->primeiro);
  }
  free(pr.no);
  if (!em.erro && !montaSlots(re, &em)) em.erro = 1;
  if (!em.erro && !montaSeg(re)) em.erro = 1;
  if (!em.erro) re->ancorado = re->nprog > 0 && re->prog[0].op == O_BOL && !(flags & REGEXJS_M);
  free(em.tr);
  if (em.erro) {
    if (erro && errotam) snprintf(erro, errotam, "pattern too large");
    regexjs_liberar(re);
    return NULL;
  }
  return re;
}

RegexJs *regexjs_compilar_web(const char *padrao, char *erro, size_t errotam) {
  int flags = 0;
  const char *p = padrao;
  while (p && p[0] == '(' && p[1] == '?') {
    const char *q = p + 2;
    int f = 0, ok = 0;
    while (*q && strchr("imxsIMXS", *q)) {
      switch (*q | 32) { case 'i': f |= REGEXJS_I; break; case 'm': f |= REGEXJS_M; break; case 's': f |= REGEXJS_S; break; }
      q++; ok = 1;
    }
    if (!ok || *q != ')') break;
    flags |= f;
    p = q + 1;
  }
  return regexjs_compilar(p, flags, erro, errotam);
}

// ---- casamento -------------------------------------------------------------
// MEMORIA: 2 bits por (slot, posicao): 0 = ainda nao, 1 = falhou, 2 = casou.
// Sem ela, um padrao de varios "(?=.*...)" seguidos (o do dono, com "(?!.*(..|.*DTS))"
// dentro) refaz o mesmo trabalho a cada posicao de inicio e a cada volta do
// ".*" de fora: numa descricao do AIOStreams (600 bytes) passava de 4 milhoes de
// passos, estourava o teto e a fonte boa era descartada. Com ela cada estado e
// resolvido uma vez so: o custo fica em slots x tamanho do texto. Acima do teto
// de memoria o motor roda como antes (sem lembrar).
#define MEMO_MAX_ESTADOS (16u * 1024u * 1024u)   // 4 MB de tabela

// Um quadro da pilha de retrocesso (ver roda): so inteiros, 28 bytes.
enum { Q_ALT, Q_REG, Q_MARCA, Q_STAR };
typedef struct { int t, a, b, c, d, e, f; } Quadro;

typedef struct {
  const RegexJs *re;
  const unsigned char *s;
  int n;
  long passos;
  int prof;          // aninhamento de lookarounds (a unica recursao que sobrou)
  int estourou;
  int *reg;
  unsigned char *memo;
  Quadro *pil;       // a pilha de retrocesso, na heap
  size_t np, cap;
} Ctx;

#define PASSOS_MAX 2000000
// Lookaround dentro de lookaround: o padrao aninha no maximo 100 grupos (parseAlt).
#define PROF_MAX 128
// Quadros de retrocesso: ~7 MB no pior caso. Um texto de fonte tem no maximo 8192
// bytes, e cada volta de laco consome pelo menos um, entao isso sobra; estourar
// conta como "nao casou", como o teto de passos.
#define PILHA_MAX 262144

static int ehPalavra(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int ehQuebra(unsigned c) { return c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029; }

static int noClasse(const Classe *c, unsigned cp, int icase) {
  int i, ach = 0;
  if (cp < 128 && c->pronta) return (c->ascii[cp >> 3] >> (cp & 7)) & 1;
  for (i = 0; i < c->n && !ach; i++) if (cp >= c->f[i].lo && cp <= c->f[i].hi) ach = 1;
  if (!ach && icase && cp < 128 && isalpha((int)cp)) {
    unsigned o = (unsigned)(islower((int)cp) ? toupper((int)cp) : tolower((int)cp));
    for (i = 0; i < c->n && !ach; i++) if (o >= c->f[i].lo && o <= c->f[i].hi) ach = 1;
  }
  return c->neg ? !ach : ach;
}

// Onde comeca o caractere UTF-8 que termina em `fim` (exclusivo), para casar
// para tras. Byte solto (UTF-8 quebrado) conta sozinho, como para frente.
static int inicioChar(const Ctx *cx, int fim) {
  int k = fim - 1, l;
  while (k > 0 && fim - k < 4 && (cx->s[k] & 0xC0) == 0x80) k--;
  decodifica(cx->s + k, cx->s + cx->n, &l);
  return k + l == fim ? k : fim - 1;
}

static int memoLe(const Ctx *cx, int slot, int sp) {
  size_t mi = (size_t)slot * (size_t)(cx->n + 1) + (size_t)sp;
  return (cx->memo[mi >> 2] >> ((mi & 3) * 2)) & 3;
}
static void memoGrava(Ctx *cx, int slot, int sp, int v) {
  size_t mi = (size_t)slot * (size_t)(cx->n + 1) + (size_t)sp;
  unsigned sh = (unsigned)(mi & 3) * 2;
  cx->memo[mi >> 2] = (unsigned char)((cx->memo[mi >> 2] & ~(3u << sh)) | ((unsigned)v << sh));
}

// Um caractere (O_BYTE, O_ANY, O_CLASSE), para frente ou para tras: a nova
// posicao, ou -1 se nao casa.
static int consome(const Ctx *cx, const Ins *in, int sp) {
  const RegexJs *re = cx->re;
  int l, ini; unsigned c;
  if (in->op == O_BYTE) {
    int b;
    if (in->neg ? sp <= 0 : sp >= cx->n) return -1;
    b = cx->s[in->neg ? sp - 1 : sp];
    if (b != in->x && (!(re->flags & REGEXJS_I) || !isalpha(in->x) || tolower(b) != tolower(in->x))) return -1;
    return sp + (in->neg ? -1 : 1);
  }
  // ASCII para frente (quase todo o texto de uma fonte): sem decodificar UTF-8.
  if (!in->neg && sp < cx->n && cx->s[sp] < 0x80) {
    unsigned b = cx->s[sp];
    if (in->op == O_ANY) return (!(re->flags & REGEXJS_S) && (b == '\n' || b == '\r')) ? -1 : sp + 1;
    if (re->cls[in->x].pronta) return (re->cls[in->x].ascii[b >> 3] >> (b & 7)) & 1 ? sp + 1 : -1;
  }
  if (in->neg) {
    if (sp <= 0) return -1;
    ini = inicioChar(cx, sp);
    c = decodifica(cx->s + ini, cx->s + cx->n, &l);
    l = sp - ini;
  } else {
    if (sp >= cx->n) return -1;
    c = decodifica(cx->s + sp, cx->s + cx->n, &l);
  }
  if (in->op == O_ANY) { if (!(re->flags & REGEXJS_S) && ehQuebra(c)) return -1; }
  else if (!noClasse(&re->cls[in->x], c, re->flags & REGEXJS_I)) return -1;
  return in->neg ? sp - l : sp + l;
}

// Desfaz UM caractere que `consome` comeu terminando em `e` (O_STAR recuando).
static int volta(const Ctx *cx, const Ins *in, int e) {
  int l;
  if (in->op == O_BYTE) return e + (in->neg ? 1 : -1);
  if (!in->neg) return cx->s[e - 1] < 0x80 ? e - 1 : inicioChar(cx, e);
  decodifica(cx->s + e, cx->s + cx->n, &l);
  return e + l;
}

// RETROCESSO SEM RECURSAO. Antes, cada alternativa (O_SPLIT, O_MARK, a continuacao
// de um O_STAR) era uma chamada recursiva de roda(): "(?:ab |c)*" sobre o texto de
// uma fonte (ate 8 KB) abria milhares de niveis de 144 bytes e estourava a pilha de
// um fio de 64 KB (SIGBUS/SIGSEGV; o teto de profundidade de entao, 6000 niveis ~
// 860 KB, nao ajudava). Agora roda() e um laco e as alternativas pendentes sao
// quadros num vetor na heap. So o lookaround (O_LOOK) chama roda() de dentro de
// roda(), e isso so aninha o quanto o padrao aninha (PROF_MAX).
//
// Quadros, do mais velho para o mais novo; falhar desempilha o de cima:
//   Q_ALT    outra alternativa: continuar em (a = pc, b = posicao)
//   Q_REG    desfazer o registrador a (valor antigo b), como o O_MARK recursivo fazia
//   Q_MARCA  um estado memorizavel (a = slot, b = posicao) em exploracao: chegar
//            nele falhando e ter esgotado tudo (grava "falhou"); casar com ele na
//            pilha grava "casou". Os quadros acima dele sao o que ele ainda tenta.
//   Q_STAR   as continuacoes de um O_STAR ainda por tentar (ver estrelaProx):
//            a = pc do O_STAR, b = posicao inicial, c = k (quantos caracteres o
//            corpo come), d = candidato tentado por ultimo (0..k; guloso comeca
//            em k + 1, preguicoso em -1), e = a posicao dele, f = o que a memoria
//            sabia do estado no fim (0 ou 1)
static int empilha(Ctx *cx, int t, int a, int b, int c, int d, int e, int f) {
  Quadro *q;
  if (cx->np == cx->cap) {
    size_t nc = cx->cap ? cx->cap * 2 : 256;
    Quadro *n = NULL;
    if (nc > PILHA_MAX) nc = PILHA_MAX;
    if (cx->np >= nc || !(n = realloc(cx->pil, nc * sizeof *n))) { cx->estourou = 1; return 0; }
    cx->pil = n; cx->cap = nc;
  }
  q = &cx->pil[cx->np++];
  q->t = t; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e; q->f = f;
  return 1;
}

// Entrar em (pc, sp) como o antigo "roda(pc, sp)" entrava: 2 = a memoria ja sabe
// que casa, 1 = ja sabe que falha, 0 = explorar (com um Q_MARCA se for um estado
// memorizavel).
static int entra(Ctx *cx, int pc, int sp) {
  int sl = cx->memo ? cx->re->slot[pc] : -1, v;
  if (sl < 0) return 0;
  if ((v = memoLe(cx, sl, sp)) != 0) return v;
  if (!empilha(cx, Q_MARCA, sl, sp, 0, 0, 0, 0)) return 1;
  return 0;
}

// Proxima continuacao a tentar do Q_STAR em pil[qi]: guloso do mais longo para o
// mais curto, preguicoso do mais curto para o mais longo, pulando as posicoes onde
// a continuacao nao pode comecar (montaSeg) e o fim ja sabido como falho. 1 se
// achou (d e e atualizados), 0 se esgotou.
static int estrelaProx(Ctx *cx, size_t qi) {
  const RegexJs *re = cx->re;
  Quadro *q = &cx->pil[qi];
  const Ins *in = &re->prog[q->a], *b = in + 1;
  const unsigned char *f = in->y ? re->seg[in->y - 1] : NULL;
  for (;;) {
    if (!in->lazy) {
      if (q->d > q->c) q->d = q->c;            // o primeiro: o fim, em q->e
      else if (q->d == 0) return 0;
      else { q->d--; q->e = volta(cx, b, q->e); }
    } else {
      if (q->d < 0) { q->d = 0; q->e = q->b; }
      else if (q->d >= q->c) return 0;
      else { q->d++; q->e = consome(cx, b, q->e); }
    }
    if (q->d == q->c && q->f) continue;        // o fim ja se sabe falho
    if (f && !(q->e < cx->n && (f[cx->s[q->e] >> 3] & (1u << (cx->s[q->e] & 7))))) continue;
    return 1;
  }
}

// Grava `v` na memoria do estado do O_STAR nas posicoes 0..ate do quadro (as que
// o corpo alcanca a partir da inicial, uma a uma).
static void estrelaMemo(Ctx *cx, const Quadro *q, int ate, int v) {
  const RegexJs *re = cx->re;
  const Ins *b = &re->prog[q->a + 1];
  int sl = re->slot[q->a], i, e = q->b;
  if (!cx->memo || sl < 0) return;
  for (i = 0; i <= ate; i++) {
    memoGrava(cx, sl, e, v);
    if (i < ate) e = consome(cx, b, e);
  }
}

static int roda(Ctx *cx, int pc, int sp) {
  const RegexJs *re = cx->re;
  const size_t base = cx->np;
  size_t i;
  int r;
  if (++cx->prof > PROF_MAX) { cx->estourou = 1; cx->prof--; return 0; }
  if ((r = entra(cx, pc, sp)) != 0) { cx->prof--; return r == 2; }
segue:
  for (;;) {
    const Ins *in = &re->prog[pc];
    if (++cx->passos > PASSOS_MAX) { cx->estourou = 1; goto acabou; }
    if (cx->estourou) goto acabou;
    switch (in->op) {
      case O_MATCH: goto casou;
      case O_BYTE: case O_ANY: case O_CLASSE:
        if ((sp = consome(cx, in, sp)) < 0) goto falha;
        pc++; break;
      case O_STAR: {
        // Corpo = a instrucao seguinte (um caractere); continuacao = in->x.
        // Para "testar" so a resposta interessa: o estado (O_STAR, p) casa se a
        // continuacao casa em ALGUM fim entre p e o ultimo caractere que o corpo
        // aceita. A ordem (guloso: do mais longo; preguicoso: do mais curto) so
        // decide quem e achado primeiro. Cada posicao por onde o O_STAR passa
        // fica lembrada, entao o ".*" que a posicao de inicio seguinte reabre
        // para na hora em vez de reler a linha inteira.
        const Ins *b = &re->prog[pc + 1];
        int slS = cx->memo ? re->slot[pc] : -1;
        int k = 0, v = 0, e = sp, e2;
        for (;;) {   // anda ate o fim do que o corpo aceita, ou ate um estado ja resolvido
          if (slS >= 0 && (v = memoLe(cx, slS, e)) != 0) break;
          if ((e2 = consome(cx, b, e)) < 0) break;
          e = e2; k++;
          if (++cx->passos > PASSOS_MAX) { cx->estourou = 1; goto acabou; }
        }
        if (v == 2) {   // o estado no fim ja casa, entao todos os de tras tambem
          int ee = sp, j;
          if (slS >= 0) for (j = 0; j < k; j++) { memoGrava(cx, slS, ee, 2); ee = consome(cx, b, ee); }
          goto casou;
        }
        if (v && !k) goto falha;
        // As continuacoes ficam num quadro: falhar a de cima volta aqui.
        if (!empilha(cx, Q_STAR, pc, sp, k, in->lazy ? -1 : k + 1, in->lazy ? sp : e, v)) goto acabou;
        goto falha;
      }
      case O_JMP: pc = in->x; break;
      case O_SPLIT:
        // x primeiro; se falhar, y (o quadro Q_ALT, que fica por baixo).
        if (!empilha(cx, Q_ALT, in->y, sp, 0, 0, 0, 0)) goto acabou;
        r = entra(cx, in->x, sp);
        if (r == 2) goto casou;
        if (r == 1) goto falha;
        pc = in->x; break;
      case O_MARK:
        if (!empilha(cx, Q_REG, in->x, cx->reg[in->x], 0, 0, 0, 0)) goto acabou;
        cx->reg[in->x] = sp;
        r = entra(cx, pc + 1, sp);
        if (r == 2) goto casou;
        if (r == 1) goto falha;
        pc++; break;
      case O_CHK:
        if (cx->reg[in->x] == sp) goto falha;
        pc++; break;
      case O_BOL:
        if (sp == 0 || ((re->flags & REGEXJS_M) && (cx->s[sp - 1] == '\n' || cx->s[sp - 1] == '\r'))) { pc++; break; }
        goto falha;
      case O_EOL:
        if (sp == cx->n || ((re->flags & REGEXJS_M) && (cx->s[sp] == '\n' || cx->s[sp] == '\r'))) { pc++; break; }
        goto falha;
      case O_WB: case O_NWB: {
        int a = sp > 0 && ehPalavra(cx->s[sp - 1]), b = sp < cx->n && ehPalavra(cx->s[sp]);
        if ((a != b) == (in->op == O_WB)) { pc++; break; }
        goto falha; }
      case O_LOOK: {
        int dentro = roda(cx, pc + 1, sp);   // o corpo, numa execucao propria
        if (cx->estourou) goto acabou;
        if ((dentro != 0) == (in->neg == 0)) { pc = in->x; break; }
        goto falha; }
      default: goto falha;
    }
  }

falha:
  // O caminho atual falhou: desempilha ate achar o que ainda se pode tentar.
  while (cx->np > base) {
    size_t qi = cx->np - 1;
    Quadro *q = &cx->pil[qi];
    if (q->t == Q_ALT) { pc = q->a; sp = q->b; cx->np--; goto segue; }
    if (q->t == Q_REG) { cx->reg[q->a] = q->b; cx->np--; continue; }
    if (q->t == Q_MARCA) {
      // Resultado de estouro nao vale como resposta: a busca inteira para.
      if (!cx->estourou) memoGrava(cx, q->a, q->b, 1);
      cx->np--;
      continue;
    }
    for (;;) {   // Q_STAR: a proxima continuacao
      int x, e;
      if (cx->estourou) goto acabou;
      if (!estrelaProx(cx, qi)) {
        // Esgotou: de nenhuma das posicoes que o corpo alcanca a continuacao casa.
        estrelaMemo(cx, &cx->pil[qi], cx->pil[qi].c - (cx->pil[qi].f ? 1 : 0), 1);
        cx->np--;
        break;
      }
      x = re->prog[cx->pil[qi].a].x; e = cx->pil[qi].e;
      r = entra(cx, x, e);   // pode crescer a pilha: `q` nao vale mais
      if (r == 2) goto casou;
      if (r == 0) { pc = x; sp = e; goto segue; }
    }
  }
  goto acabou;   // nada pendente: nao casa

casou:
  // Todo estado em exploracao na pilha esta no caminho que casou: lembra "casou".
  // Os registradores ficam como estao (o O_MARK recursivo tambem nao os desfazia).
  for (i = base; i < cx->np; i++) {
    const Quadro *q = &cx->pil[i];
    if (q->t == Q_MARCA) memoGrava(cx, q->a, q->b, 2);
    else if (q->t == Q_STAR) estrelaMemo(cx, q, q->d, 2);
  }
  cx->np = base; cx->prof--;
  return 1;

acabou:
  cx->np = base; cx->prof--;
  return 0;
}

int regexjs_testar(const RegexJs *re, const char *texto, size_t n) {
  Ctx cx;
  int i, r = 0;
  int regFixo[16];
  size_t estados;
  if (!re || !texto) return 0;
  if (n > 8192) n = 8192;
  memset(&cx, 0, sizeof cx);
  cx.re = re; cx.s = (const unsigned char *)texto; cx.n = (int)n;
  // Registradores: um por lookbehind / repeticao que pode casar vazio. Antes
  // eram 64 fixos, e um padrao com mais lookbehinds nunca casava, em silencio.
  if (re->nreg <= (int)(sizeof regFixo / sizeof *regFixo)) { memset(regFixo, 0, sizeof regFixo); cx.reg = regFixo; }
  else if (!(cx.reg = calloc((size_t)re->nreg, sizeof *cx.reg))) return 0;
  estados = (size_t)re->nslot * (n + 1);
  if (re->slot && re->nslot && estados <= MEMO_MAX_ESTADOS) cx.memo = calloc((estados + 3) / 4, 1);
  for (i = 0; i <= cx.n; i++) {
    if (i > 0 && re->ancorado) break;
    if (i > 0 && i < cx.n && (cx.s[i] & 0xC0) == 0x80) continue;
    if (re->temPrimeiro && (i == cx.n || !(re->primeiro[cx.s[i] >> 3] & (1u << (cx.s[i] & 7))))) continue;
    if (roda(&cx, 0, i)) { r = 1; break; }
    if (cx.estourou) break;
  }
  free(cx.memo);
  free(cx.pil);
  if (cx.reg != regFixo) free(cx.reg);
  return r;
}
