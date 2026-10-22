#include "regexjs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Ver regexjs.h. Compila para um programa de VM com retrocesso (alternativas por
// recursao), como o regexp.c do mujs mas escrito aqui: pequeno e sem licenca de
// terceiros.

typedef struct { unsigned lo, hi; } Faixa;
typedef struct { Faixa *f; int n, cap; int neg; } Classe;

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
       O_LOOK, O_LOOKB, O_ENDAT, O_MATCH };
typedef struct { unsigned char op; unsigned char neg; int x, y, maxlen; } Ins;

struct RegexJs {
  Ins *prog; int nprog, capprog;
  Classe *cls; int ncls, capcls;
  int nreg;
  int flags;
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
  if (pr->no[at].tipo == N_ASSERT) return falha(pr, "nothing to repeat");
  { int n = novoNo(pr, N_REP);
    if (n < 0) return -1;
    pr->no[n].a = mn; pr->no[n].b = mx; pr->no[n].f1 = at;
    if (pr->p < pr->fim && *pr->p == '?') { pr->no[n].lazy = 1; pr->p++; }
    return n; }
}

static int parseSeq(Pr *pr, int prof) {
  int ult = -1;
  while (pr->p < pr->fim && *pr->p != '|' && *pr->p != ')') {
    int t = parseTermo(pr, prof);
    if (t < 0) return -1;
    if (ult < 0) ult = t;
    else { int s = novoNo(pr, N_SEQ); if (s < 0) return -1; pr->no[s].f1 = ult; pr->no[s].f2 = t; ult = s; }
  }
  if (ult < 0) return novoNo(pr, N_VAZIO);
  return ult;
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

typedef struct { RegexJs *re; Pr *pr; int erro; } Em;

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

// Limite em bytes para o lookbehind; -1 se pode consumir sem limite.
// Satura no teto de texto da VM, evitando overflow em repeticoes aninhadas.
static int maxlen(const Pr *pr, int n) {
  const No *o = &pr->no[n];
  int a, b;
  switch (o->tipo) {
    case N_BYTE: return 1;
    case N_ANY: case N_CLASSE: return 4; // um codepoint UTF-8
    case N_SEQ: case N_ALT:
      a = maxlen(pr, o->f1); b = maxlen(pr, o->f2);
      if (a < 0 || b < 0) return -1;
      a = o->tipo == N_SEQ ? a + b : (a > b ? a : b);
      break;
    case N_REP:
      if (o->b == 0) return 0;
      a = maxlen(pr, o->f1);
      if (a == 0) return 0;
      if (a < 0 || o->b < 0) return -1;
      a *= o->b;
      break;
    default: return 0; // assercoes nao consomem texto
  }
  return a > 8192 ? 8192 : a;
}

static void emiteNo(Em *em, int n);

static void emiteRep(Em *em, const No *o) {
  RegexJs *re = em->re;
  int i, mn = o->a, mx = o->b, vazio = minlen(em->pr, o->f1) == 0;
  int fins[512], nf = 0;
  if (mn > 1000 || mx > 1000) { em->erro = 1; return; }
  for (i = 0; i < mn && !em->erro; i++) emiteNo(em, o->f1);
  if (mx < 0) {
    int l1 = emite(em, O_SPLIT, 0, 0, 0), reg = -1, corpo = re->nprog;
    if (em->erro) return;
    if (vazio) { reg = re->nreg++; emite(em, O_MARK, 0, reg, 0); }
    emiteNo(em, o->f1);
    if (vazio) emite(em, O_CHK, 0, reg, 0);
    emite(em, O_JMP, 0, l1, 0);
    if (em->erro) return;
    // x = proximo da preferencia, y = o outro
    if (!o->lazy) { re->prog[l1].x = corpo; re->prog[l1].y = re->nprog; }
    else { re->prog[l1].x = re->nprog; re->prog[l1].y = corpo; }
    return;
  }
  for (i = mn; i < mx && !em->erro; i++) {
    int sp = emite(em, O_SPLIT, 0, 0, 0);
    if (em->erro) return;
    if (nf >= 512) { em->erro = 1; return; }
    fins[nf++] = sp;
    if (!o->lazy) re->prog[sp].x = re->nprog; else re->prog[sp].y = re->nprog;
    emiteNo(em, o->f1);
  }
  for (i = 0; i < nf; i++) {
    if (!o->lazy) re->prog[fins[i]].y = re->nprog; else re->prog[fins[i]].x = re->nprog;
  }
}

static void emiteNo(Em *em, int n) {
  RegexJs *re = em->re;
  const No *o = &em->pr->no[n];
  int a, b;
  if (em->erro) return;
  switch (o->tipo) {
    case N_VAZIO: break;
    case N_BYTE: emite(em, O_BYTE, 0, o->a, 0); break;
    case N_ANY: emite(em, O_ANY, 0, 0, 0); break;
    case N_CLASSE: emite(em, O_CLASSE, 0, o->classe, 0); break;
    case N_SEQ: emiteNo(em, o->f1); emiteNo(em, o->f2); break;
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
    case N_LOOK:
      if (!o->a) {
        a = emite(em, O_LOOK, o->b, 0, 0);
        if (em->erro) return;
        emiteNo(em, o->f1);
        emite(em, O_MATCH, 0, 0, 0);
        re->prog[a].x = re->nprog;
      } else {
        int reg = re->nreg++;
        a = emite(em, O_LOOKB, o->b, 0, reg);
        if (em->erro) return;
        re->prog[a].maxlen = maxlen(em->pr, o->f1);
        emiteNo(em, o->f1);
        emite(em, O_ENDAT, 0, reg, 0);
        emite(em, O_MATCH, 0, 0, 0);
        if (!em->erro) re->prog[a].x = re->nprog;
      }
      break;
  }
}

void regexjs_liberar(RegexJs *re) {
  int i;
  if (!re) return;
  for (i = 0; i < re->ncls; i++) free(re->cls[i].f);
  free(re->cls); free(re->prog); free(re);
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
  em.re = re; em.pr = &pr; em.erro = 0;
  emiteNo(&em, raiz);
  emite(&em, O_MATCH, 0, 0, 0);
  re->cls = pr.cls; re->ncls = pr.ncls; re->capcls = pr.capcls;
  free(pr.no);
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
typedef struct {
  const RegexJs *re;
  const unsigned char *s;
  int n;
  long passos;
  int prof;
  int estourou;
  int reg[64];
} Ctx;

#define PASSOS_MAX 400000
#define PROF_MAX 6000

static int ehPalavra(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int ehQuebra(unsigned c) { return c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029; }

static int noClasse(const Classe *c, unsigned cp, int icase) {
  int i, ach = 0;
  for (i = 0; i < c->n && !ach; i++) if (cp >= c->f[i].lo && cp <= c->f[i].hi) ach = 1;
  if (!ach && icase && cp < 128 && isalpha((int)cp)) {
    unsigned o = (unsigned)(islower((int)cp) ? toupper((int)cp) : tolower((int)cp));
    for (i = 0; i < c->n && !ach; i++) if (o >= c->f[i].lo && o <= c->f[i].hi) ach = 1;
  }
  return c->neg ? !ach : ach;
}

static int roda(Ctx *cx, int pc, int sp) {
  const RegexJs *re = cx->re;
  int resultado = 0;
  if (++cx->prof > PROF_MAX) { cx->estourou = 1; cx->prof--; return 0; }
  for (;;) {
    const Ins *in = &re->prog[pc];
    if (++cx->passos > PASSOS_MAX) { cx->estourou = 1; break; }
    if (cx->estourou) break;
    switch (in->op) {
      case O_MATCH: resultado = 1; goto fim;
      case O_BYTE:
        if (sp >= cx->n) goto fim;
        if (cx->s[sp] != in->x) {
          if (!(re->flags & REGEXJS_I) || !isalpha(in->x) || tolower(cx->s[sp]) != tolower(in->x)) goto fim;
        }
        sp++; pc++; break;
      case O_ANY: {
        int l; unsigned c;
        if (sp >= cx->n) goto fim;
        c = decodifica(cx->s + sp, cx->s + cx->n, &l);
        if (!(re->flags & REGEXJS_S) && ehQuebra(c)) goto fim;
        sp += l; pc++; break; }
      case O_CLASSE: {
        int l; unsigned c;
        if (sp >= cx->n) goto fim;
        c = decodifica(cx->s + sp, cx->s + cx->n, &l);
        if (!noClasse(&re->cls[in->x], c, re->flags & REGEXJS_I)) goto fim;
        sp += l; pc++; break; }
      case O_JMP: pc = in->x; break;
      case O_SPLIT:
        if (roda(cx, in->x, sp)) { resultado = 1; goto fim; }
        pc = in->y; break;
      case O_MARK: {
        int antigo = cx->reg[in->x];
        cx->reg[in->x] = sp;
        if (roda(cx, pc + 1, sp)) { resultado = 1; goto fim; }
        cx->reg[in->x] = antigo;
        goto fim; }
      case O_CHK:
        if (cx->reg[in->x] == sp) goto fim;
        pc++; break;
      case O_BOL:
        if (sp == 0 || ((re->flags & REGEXJS_M) && (cx->s[sp - 1] == '\n' || cx->s[sp - 1] == '\r'))) { pc++; break; }
        goto fim;
      case O_EOL:
        if (sp == cx->n || ((re->flags & REGEXJS_M) && (cx->s[sp] == '\n' || cx->s[sp] == '\r'))) { pc++; break; }
        goto fim;
      case O_WB: case O_NWB: {
        int a = sp > 0 && ehPalavra(cx->s[sp - 1]), b = sp < cx->n && ehPalavra(cx->s[sp]);
        if ((a != b) == (in->op == O_WB)) { pc++; break; }
        goto fim; }
      case O_LOOK: {
        int r = roda(cx, pc + 1, sp);
        if (cx->estourou) goto fim;
        if ((r != 0) == (in->neg == 0)) { pc = in->x; break; }
        goto fim; }
      case O_LOOKB: {
        int j, r = 0, antigo = cx->reg[in->y];
        int inicio = in->maxlen >= 0 && sp > in->maxlen ? sp - in->maxlen : 0;
        cx->reg[in->y] = sp;
        for (j = sp; j >= inicio && !r && !cx->estourou; j--) {
          if (j > 0 && j < cx->n && (cx->s[j] & 0xC0) == 0x80) continue;
          r = roda(cx, pc + 1, j);
        }
        cx->reg[in->y] = antigo;
        if (cx->estourou) goto fim;
        if ((r != 0) == (in->neg == 0)) { pc = in->x; break; }
        goto fim; }
      case O_ENDAT:
        if (sp != cx->reg[in->x]) goto fim;
        pc++; break;
      default: goto fim;
    }
  }
fim:
  cx->prof--;
  return resultado;
}

int regexjs_testar(const RegexJs *re, const char *texto, size_t n) {
  Ctx cx;
  int i;
  if (!re || !texto) return 0;
  if (re->nreg > 64) return 0;
  if (n > 8192) n = 8192;
  memset(&cx, 0, sizeof cx);
  cx.re = re; cx.s = (const unsigned char *)texto; cx.n = (int)n;
  for (i = 0; i <= cx.n; i++) {
    if (i > 0 && i < cx.n && (cx.s[i] & 0xC0) == 0x80) continue;
    if (roda(&cx, 0, i)) return 1;
    if (cx.estourou) return 0;
  }
  return 0;
}
