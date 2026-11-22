// farolpeca.c — parser e validador do FarolCena v1 (lote sense, issue 39;
// PLANO-GOSTO secoes 4.1 e 4.2). O contrato esta em farolpeca.h.
//
// POR QUE UM SEGUNDO ANALISADOR SE JS.H JA EXISTE. O js.h da casa e
// TOLERANTE de proposito ("nunca travar com campo faltando ou tipo
// inesperado") — ele le o que acha e devolve o resto por conta de quem
// chama. Um validador e o contrario: precisa saber que o documento inteiro
// e bem-formado, que cada chave existe no schema e que cada valor tem o
// tipo certo. Por isso a camada de varredura estrita daqui nasce SOBRE o
// js.h (js_fim acha o fim de objeto/array respeitando aspas e escapes) e
// completa o que falta: iteracao de chaves por profundidade, string JSON
// estrita (escape fora do alfabeto, controle cru e \u torto reprovam),
// numero inteiro estrito e chave duplicada. Sem malloc, sem SDL, sem rede.
//
// Uma passada por elemento: cada chave e lida uma vez e despachada; o
// "t" da peca pode vir em qualquer posicao porque as chaves universais
// ja recusam o que nao existe em schema nenhum, e a conferencia por tipo
// fecha no fim da peca.

#include "farolpeca.h"
#include "faroltexto.h"
#include "js.h"
#include <limits.h>
#include <string.h>

// ------------------------------------------------------------ contexto
typedef struct {
  int        nCand;      // tamanho da lista de candidatos da cena
  FarolCena *c;          // para o pool de "x"
} Fp;

// Retorno interno das leitoras de peca: a estrutura JSON quebrou — string
// sem fecho, escape ruim, controle cru, virgula/dois-pontos fora do lugar,
// token que nao e valor. Diferente de FPE_JSON (chave duplicada, elemento
// que nao e objeto: o documento e JSON valido), FP_TORTO derruba a cena
// inteira com -FCE_JSON.
#define FP_TORTO (-1)

// ------------------------------------------------------------ varredura
// Branco JSON e so espaco, tab, LF e CR: outro controle entre tokens torna
// o documento invalido (o js.h tolerante deixaria passar).
static const char *fp_pula(const char *p, const char *fim) {
  while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
    p++;
  return p;
}

static int fp_hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Aspa de fechamento de uma string comecando em *p=='"', ou NULL. Estrita:
// escape fora de "\"\\/bfnrt e \uXXXX sem hexa, controle cru (< 0x20) e
// string sem fecho reprovam — o js.h tolerante deixaria passar.
static const char *fp_str_fim(const char *p, const char *fim) {
  for (p++; p < fim; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"') return p;
    if (c == '\\') {
      if (++p >= fim) return NULL;
      if (*p == 'u') {
        if (fim - p < 5) return NULL;
        for (int i = 1; i <= 4; i++)
          if (fp_hexVal(p[i]) < 0) return NULL;
        p += 4;
        continue;
      }
      if (!strchr("\"\\/bfnrt", *p)) return NULL;
      continue;
    }
    if (c < 0x20) return NULL;
  }
  return NULL;
}

// Token cru de numero JSON: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?.
// Fracao e expoente passam aqui e reprovam depois, no inteiro estrito.
static int fp_num_json(const char *p, const char *q) {
  if (p < q && *p == '-') p++;
  if (p >= q) return 0;
  if (*p == '0') p++;
  else if (*p >= '1' && *p <= '9')
    while (p < q && *p >= '0' && *p <= '9') p++;
  else return 0;
  if (p < q && *p == '.') {
    if (++p >= q || *p < '0' || *p > '9') return 0;
    while (p < q && *p >= '0' && *p <= '9') p++;
  }
  if (p < q && (*p == 'e' || *p == 'E')) {
    if (++p < q && (*p == '+' || *p == '-')) p++;
    if (p >= q || *p < '0' || *p > '9') return 0;
    while (p < q && *p >= '0' && *p <= '9') p++;
  }
  return p == q;
}

// Fim de um valor qualquer comecando em p (ja sem branco a esquerda).
static const char *fp_val_fim(const char *p, const char *fim) {
  if (p >= fim) return NULL;
  if (*p == '"') {
    const char *q = fp_str_fim(p, fim);
    return q ? q + 1 : NULL;
  }
  if (*p == '{' || *p == '[') {
    // O documento ja foi conferido balanceado (js_fim na raiz), entao toda
    // chaveta aberta tem o fecho antes de fim; q > fim = estrutura torta.
    const char *q = js_fim(p);
    return (q && q <= fim) ? q : NULL;
  }
  const char *q = p;
  while (q < fim && *q != ',' && *q != '}' && *q != ']' &&
         (unsigned char)*q > ' ') q++;
  if (q <= p) return NULL;
  // Token cru so e JSON valido como numero ou true/false/null: "tru", "01",
  // "+1", "NaN" e cia tornam o documento inteiro invalido.
  if (*p == '-' || (*p >= '0' && *p <= '9'))
    return fp_num_json(p, q) ? q : NULL;
  if (q - p == 4 && (!memcmp(p, "true", 4) || !memcmp(p, "null", 4)))
    return q;
  if (q - p == 5 && !memcmp(p, "false", 5)) return q;
  return NULL;
}

// Iterador sobre o interior de um objeto/array: [p,fim) NAO inclui o
// delimitador de fecho (a faixa vai de depois do '{' ate antes do '}').
// precisa: depois de ',' um elemento e obrigatorio — ",}" e ",]" reprovam.
typedef struct { const char *p, *fim; int precisa; } FpIt;

// 1: devolve ki/kf (nome cru COM aspas) e vi/vf (valor cru); 0: acabou;
// -1: estrutura torta.
static int fp_obj_prox(FpIt *it, const char **ki, const char **kf,
                       const char **vi, const char **vf) {
  const char *p = fp_pula(it->p, it->fim);
  const char *k2, *v, *vi2, *vf2, *n;
  if (p >= it->fim) return it->precisa ? -1 : 0;
  if (*p != '"') return -1;
  if (!(k2 = fp_str_fim(p, it->fim))) return -1;
  v = fp_pula(k2 + 1, it->fim);
  if (v >= it->fim || *v != ':') return -1;
  vi2 = fp_pula(v + 1, it->fim);
  if (!(vf2 = fp_val_fim(vi2, it->fim))) return -1;
  n = fp_pula(vf2, it->fim);
  it->precisa = 0;
  if (n < it->fim) {
    if (*n != ',') return -1;
    it->precisa = 1;
    it->p = n + 1;
  } else it->p = n;
  *ki = p; *kf = k2 + 1; *vi = vi2; *vf = vf2;
  return 1;
}

// Mesmo contrato, para os elementos de um array.
static int fp_arr_prox(FpIt *it, const char **vi, const char **vf) {
  const char *p = fp_pula(it->p, it->fim);
  const char *vf2, *n;
  if (p >= it->fim) return it->precisa ? -1 : 0;
  if (!(vf2 = fp_val_fim(p, it->fim))) return -1;
  n = fp_pula(vf2, it->fim);
  it->precisa = 0;
  if (n < it->fim) {
    if (*n != ',') return -1;
    it->precisa = 1;
    it->p = n + 1;
  } else it->p = n;
  *vi = p; *vf = vf2;
  return 1;
}

// ------------------------------------------------- pre-passada estrutural
// Confere a SINTAXE JSON do documento inteiro antes de validar peca a
// peca. Os iteradores estritos so descem onde o esquema le: um trecho
// torto dentro de peca ja recusada por outro motivo — ou alem da 6a —
// passava despercebido. Um parser JSON de verdade (o do servidor)
// reprova o documento inteiro nesses casos; a TV faz o mesmo: qualquer
// torto, em qualquer nivel ou posicao, derruba a cena com -FCE_JSON.
// Iterativa, sem recursao: 8 KB aninham milhares de niveis e a pilha do
// fio pode ser curta — um BIT por nivel guarda o delimitador aberto
// (0 = '{', 1 = '['), entao 1 KB de contexto bastam para o pior caso.
// Mesmas regras de fp_val_fim + fp_obj_prox/fp_arr_prox: branco so
// " \t\n\r", string no alfabeto de escapes e sem controle cru, numero na
// gramatica, true/false/null exatos, "," e ":" no lugar e todo par
// fechado com o delimitador certo. A semantica (chaves, tipos,
// duplicatas) fica para a leitura por esquema.
static int fp_json_ok(const char *p, const char *fim) {
  unsigned char ctx[FAROL_CENA_MAX / 8];   // um bit por nivel aberto
  int dep = 0;
  int est = 0;   // 0 valor; 1 chave ou '}'; 2 elemento ou ']';
                 // 3 depois de valor; 4 chave depois de ','
  for (;;) {
    p = fp_pula(p, fim);
    if (est == 3) {
      int obj;
      if (dep == 0) return p == fim;        // fim: so branco pode restar
      obj = !(ctx[(dep - 1) >> 3] & (1u << ((dep - 1) & 7)));
      if (p < fim && *p == ',') { p++; est = obj ? 4 : 0; continue; }
      if (p < fim && *p == (obj ? '}' : ']')) { dep--; p++; continue; }
      return 0;
    }
    if (p >= fim) return 0;
    if (est == 1 && *p == '}') { dep--; p++; est = 3; continue; }
    if (est == 1 || est == 4) {
      const char *k2;
      if (*p != '"') return 0;    // fp_str_fim parte do pressuposto
      k2 = fp_str_fim(p, fim);
      if (!k2) return 0;
      p = fp_pula(k2 + 1, fim);
      if (p >= fim || *p != ':') return 0;
      p++;
      est = 0;
      continue;
    }
    if (est == 2 && *p == ']') { dep--; p++; est = 3; continue; }
    if (*p == '{') {
      if (dep == FAROL_CENA_MAX) return 0;  // 8 KB nao fecha 8 mil niveis
      ctx[dep >> 3] &= ~(1u << (dep & 7)); dep++;
      p++;
      est = 1;
      continue;
    }
    if (*p == '[') {
      if (dep == FAROL_CENA_MAX) return 0;
      ctx[dep >> 3] |= 1u << (dep & 7); dep++;
      p++;
      est = 2;
      continue;
    }
    if (*p == '"') {
      const char *q = fp_str_fim(p, fim);
      if (!q) return 0;
      p = q + 1;
    } else {
      const char *q = p;
      while (q < fim && *q != ',' && *q != '}' && *q != ']' &&
             (unsigned char)*q > ' ') q++;
      if (q == p) return 0;
      if (*p == '-' || (*p >= '0' && *p <= '9')) {
        if (!fp_num_json(p, q)) return 0;
      } else if (!((q - p == 4 &&
                    (!memcmp(p, "true", 4) || !memcmp(p, "null", 4))) ||
                   (q - p == 5 && !memcmp(p, "false", 5))))
        return 0;
      p = q;
    }
    est = 3;
  }
}

// ----------------------------------------------------------- decodifica
static int fp_hex4(const char *p, const char *fim, unsigned *v) {
  int h;
  *v = 0;
  if (fim - p < 4) return 0;
  for (int i = 0; i < 4; i++) {
    if ((h = fp_hexVal(p[i])) < 0) return 0;
    *v = (*v << 4) | (unsigned)h;
  }
  return 1;
}

// Decodifica a string [ki,kf) (com aspas, ja validada) para dst. As mesmas
// regras do decodificador do js.c — \uXXXX vira UTF-8 e par de substitutos
// vira um codepoint so; substituto solto vira espaco — mas com fidelidade
// que o validador precisa: \n, \r e \t saem como os caracteres reais, para
// o faroltexto_aceita enxergar o texto como sera exibido. Devolve o
// comprimento ou -1 quando nao coube.
static int fp_str_dec(const char *ki, const char *kf, char *dst, size_t tam) {
  const char *fimStr = kf - 1;
  size_t k = 0;
  for (const char *p = ki + 1; p < fimStr; p++) {
    unsigned char c = (unsigned char)*p;
    if (c != '\\') {
      if (k + 1 >= tam) return -1;
      dst[k++] = (char)c;
      continue;
    }
    c = (unsigned char)*++p;
    if (c == 'u') {
      unsigned cp, lo;
      if (!fp_hex4(p + 1, fimStr, &cp)) return -1;   // nao acontece: validada
      p += 4;
      if (cp >= 0xD800 && cp <= 0xDBFF && fimStr - p >= 6 &&
          p[1] == '\\' && p[2] == 'u' && fp_hex4(p + 3, fimStr, &lo) &&
          lo >= 0xDC00 && lo <= 0xDFFF) {
        cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
        p += 6;
      }
      if (cp >= 0xD800 && cp <= 0xDFFF) cp = ' ';
      if (cp == 0) return -1;   // \u0000 truncaria a string C na validacao
      if (cp < 0x80) {
        if (k + 1 >= tam) return -1;
        dst[k++] = (char)cp;
      } else if (cp < 0x800) {
        if (k + 2 >= tam) return -1;
        dst[k++] = (char)(0xC0 | (cp >> 6));
        dst[k++] = (char)(0x80 | (cp & 0x3F));
      } else if (cp < 0x10000) {
        if (k + 3 >= tam) return -1;
        dst[k++] = (char)(0xE0 | (cp >> 12));
        dst[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        dst[k++] = (char)(0x80 | (cp & 0x3F));
      } else {
        if (k + 4 >= tam) return -1;
        dst[k++] = (char)(0xF0 | (cp >> 18));
        dst[k++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        dst[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        dst[k++] = (char)(0x80 | (cp & 0x3F));
      }
      continue;
    }
    // \" \\ \/ \b \f \n \r \t — o caractere real.
    c = (c == 'n') ? '\n' : (c == 'r') ? '\r' : (c == 't') ? '\t' :
        (c == 'b') ? '\b' : (c == 'f') ? '\f' : c;
    if (k + 1 >= tam) return -1;
    dst[k++] = (char)c;
  }
  dst[k] = 0;
  // O resto do buffer zerado: quem compara a FarolCena crua (memcmp, hash
  // de conteudo, gravacao em disco) nao herda lixo de pilha depois do NUL.
  memset(dst + k + 1, 0, tam - k - 1);
  return (int)k;
}

// Inteiro JSON estrito: -?(0|[1-9][0-9]*). Fracao, expoente, "+" e "01"
// reprovam — nos campos numericos da cena so inteiro existe.
static int fp_int64(const char *vi, const char *vf, int64_t *v) {
  const char *p = vi;
  int64_t x = 0;
  int neg = 0;
  if (p < vf && *p == '-') { neg = 1; p++; }
  if (p >= vf || *p < '0' || *p > '9') return 0;
  if (*p == '0' && p + 1 < vf) return 0;
  for (; p < vf; p++) {
    if (*p < '0' || *p > '9') return 0;
    if (x > (LLONG_MAX - 9) / 10) return 0;
    x = x * 10 + (*p - '0');
  }
  *v = neg ? -x : x;
  return 1;
}

// A chave [ki,kf) (com aspas, sem escapes — as nossas sao ASCII puras; uma
// chave com escape nunca casa e cai em "desconhecida") e literalmente `nome`.
static int fp_key_eh(const char *ki, const char *kf, const char *nome) {
  size_t n = strlen(nome);
  return (size_t)(kf - ki - 2) == n && !memcmp(ki + 1, nome, n);
}

// UTF-8 valido? (argumentos de modelo: os demais campos de texto da cena
// sao ASCII por charset; "x" tem a checagem completa do faroltexto_aceita).
static int fp_utf8_ok(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned char c = *p;
    int extra = 0;
    if (c < 0x80) { p++; continue; }
    if (c >= 0xC2 && c <= 0xDF)      extra = 1;
    else if (c >= 0xE0 && c <= 0xEF) extra = 2;
    else if (c >= 0xF0 && c <= 0xF4) extra = 3;
    else return 0;
    for (int i = 1; i <= extra; i++)
      if ((p[i] & 0xC0) != 0x80) return 0;
    p += extra + 1;
  }
  return 1;
}

// Argumento decodificado pode aparecer na tela: sem controle (inclusive o
// que veio de escape \u000X) e UTF-8 valido.
static int fp_arg_ok(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    if (*p < 0x20 || *p == 0x7F) return 0;
  return fp_utf8_ok(s);
}

// Charset dos identificadores da cena.
static int fp_id_ok(const char *s, size_t max, int hifen) {
  size_t n = strlen(s);
  if (n == 0 || n > max) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
        (hifen && c == '-') || (c >= 'A' && c <= 'Z' && hifen)) continue;
    return 0;
  }
  return 1;
}

// ------------------------------------------------- modelos FT_* da casa
// Nome -> literal: a tabela sai de FAROL_FRASES, entao "m" desconhecido e
// rejeitado contra a MESMA lista que o i18n confere nas 29 linguas.
static const struct { const char *nome; const char *lit; } FP_MODELOS[] = {
#define FP_MOD_X(nome) { "FT_" #nome, FT_##nome },
  FAROL_FRASES(FP_MOD_X)
#undef FP_MOD_X
};
#define FP_MODELOS_N (sizeof FP_MODELOS / sizeof *FP_MODELOS)

static const char *fp_modelo(const char *nome) {
  for (size_t i = 0; i < FP_MODELOS_N; i++)
    if (!strcmp(FP_MODELOS[i].nome, nome)) return FP_MODELOS[i].lit;
  return NULL;
}

// Quantos argumentos o modelo espera: um %d/%s por marcador ("%%" nao conta).
static int fp_modelo_args(const char *lit) {
  int n = 0;
  for (const char *p = lit; *p; p++) {
    if (p[0] == '%' && p[1] == '%') { p++; continue; }   // %% e '%' literal
    if (p[0] == '%') n++;
  }
  return n;
}

// --------------------------------------------------------------- enums
static const char *const FP_TIPOS[] = {
  "fileira", "dica", "porque", "comparar", "escolha", "info", "achados"
};
static const char *const FP_FORMAS[] = { "cartaz", "larga", "ficha", "palco" };
static const char *const FP_ACOES[] = {
  "abrir", "tocar", "comparar", "escolha", "salvar", "nada"
};
static const char *const FP_EIXOS[] = {
  "duracao", "nota", "clima", "porque", "qualidade", "termina",
  "onde", "amigos"
};
static const char *const FP_SELOS[] = { "pessoa", "tema", "genero" };
static const char *const FP_INFO_K[] = {
  "qualidade", "continuar", "tempo", "onde", "amigos", "saga",
  "rever", "retomada"
};

static int fp_enum(const char *s, const char *const *tab, int n) {
  for (int i = 0; i < n; i++)
    if (!strcmp(s, tab[i])) return i;
  return -1;
}

// ----------------------------------------------------------- referencias
// "c<digitos>" resolve contra a lista de candidatos; "tt<digitos>" e id
// IMDb. rc 1 ok, 0 malformada/fora da lista.
static int fp_ref(const char *s, int nCand, FarolRef *r) {
  size_t n = strlen(s);
  int64_t idx = 0;
  size_t i;
  if (n == 0 || n >= FAROL_REF_TAM) return 0;
  if (s[0] == 'c' && n >= 2) {
    for (i = 1; i < n; i++) {
      if (s[i] < '0' || s[i] > '9') return 0;
      idx = idx * 10 + (s[i] - '0');
      if (idx > INT16_MAX) return 0;
    }
    if (idx >= nCand) return 0;
    r->c = (int16_t)idx;
  } else if (s[0] == 't' && n >= 3 && s[1] == 't') {
    for (i = 2; i < n; i++)
      if (s[i] < '0' || s[i] > '9') return 0;
    r->c = -1;
  } else return 0;
  memset(r->s, 0, sizeof r->s);
  memcpy(r->s, s, n + 1);
  return 1;
}

// O argumento "c<digitos>" e uma referencia e tem de resolver; qualquer
// outra forma e texto livre de modelo (valor curto, nome, hora, icone...).
// "c<digitos><letras>" nao e referencia (secao 4.2): e texto livre — a
// faixa so decide quando a forma inteira e "c<digitos>"; checar o indice
// antes de ver a letra reprovava "c1x" com 0 ou 1 candidato.
static int fp_arg_ref_ok(const char *s, int nCand) {
  size_t n = strlen(s);
  int64_t idx = 0;
  if (n < 2 || s[0] != 'c') return 1;
  for (size_t i = 1; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return 1;
    if (idx <= nCand)                       // para de crescer sem estourar
      idx = idx * 10 + (s[i] - '0');
  }
  return idx < nCand;
}

// Procura ref entre os itens ja lidos; devolve o indice ou -1.
static int fp_ref_em(const FarolRef *itens, int n, const FarolRef *r) {
  for (int i = 0; i < n; i++)
    if ((itens[i].c >= 0 && itens[i].c == r->c) ||
        (itens[i].c < 0 && r->c < 0 && !strcmp(itens[i].s, r->s)))
      return i;
  return -1;
}

// ------------------------------------------------------------ o texto
// {"m":"FT_*","a":[...]} e/ou {"x":"..."}. Devolve FPE_*.
static int fp_texto(Fp *fp, const char *vi, const char *vf, FarolTexto *t) {
  FpIt it = { vi + 1, vf - 1, 0 };
  const char *ki, *kf, *v2, *vf2;
  const char *lit = NULL;
  char x[FAROL_X_TAM];
  int r, temM = 0, temA = 0, temX = 0, xOk = 0;
  memset(t, 0, sizeof *t);
  t->xi = -1;
  if (*vi != '{') return FPE_TIPO_VALOR;
  while ((r = fp_obj_prox(&it, &ki, &kf, &v2, &vf2)) == 1) {
    if (fp_key_eh(ki, kf, "m")) {
      if (temM) return FPE_JSON;
      if (*v2 != '"') return FPE_TIPO_VALOR;
      if (fp_str_dec(v2, vf2, t->m, sizeof t->m) < 0 ||
          !(lit = fp_modelo(t->m))) return FPE_TEXTO;
      temM = 1;
    } else if (fp_key_eh(ki, kf, "a")) {
      FpIt ait;
      if (temA) return FPE_JSON;
      if (*v2 != '[') return FPE_TIPO_VALOR;
      ait.p = v2 + 1; ait.fim = vf2 - 1; ait.precisa = 0;
      temA = 1;
      while ((r = fp_arr_prox(&ait, &v2, &vf2)) == 1) {
        if (t->nArgs >= FAROL_ARGS) return FPE_FAIXA;
        if (*v2 != '"') return FPE_TIPO_VALOR;
        if (fp_str_dec(v2, vf2, t->a[t->nArgs], FAROL_ARG_TAM) < 0)
          return FPE_TEXTO;
        if (!fp_arg_ok(t->a[t->nArgs])) return FPE_TEXTO;
        if (!fp_arg_ref_ok(t->a[t->nArgs], fp->nCand)) return FPE_REF;
        t->nArgs++;
      }
      if (r < 0) return FP_TORTO;
    } else if (fp_key_eh(ki, kf, "x")) {
      if (temX) return FPE_JSON;
      if (*v2 != '"') return FPE_TIPO_VALOR;
      temX = 1;
      // Nao coube em 512 = texto longo demais: reprovado como qualquer x ruim.
      xOk = fp_str_dec(v2, vf2, x, sizeof x) >= 0 && faroltexto_aceita(x);
    } else return FPE_CHAVE;
  }
  if (r < 0) return FP_TORTO;
  if (temA && !temM) return FPE_TEXTO;         // argumento sem modelo
  if (!temM && !temX) return FPE_TEXTO;        // texto vazio
  if (temM && t->nArgs != fp_modelo_args(lit)) return FPE_TEXTO;
  if (temX) {
    if (xOk && fp->c->nXs < FAROL_XS) {
      t->xi = (int8_t)fp->c->nXs;
      memcpy(fp->c->xs[fp->c->nXs++], x, strlen(x) + 1);
      t->usaX = 1;
    } else if (!temM) {
      return xOk ? FPE_LIMITE : FPE_X;          // pool cheio / x reprovado
    } else {
      t->xReprovada = 1;                        // cai no modelo de reserva
    }
  }
  return FPE_OK;
}

// ----------------------------------------------------- chaves das pecas
// Uniao de todas as chaves conhecidas: qualquer outra ja recusa na hora,
// sem esperar o "t". O indice e o bit de `seen`/`allow`/`req`.
enum {
  FK_T, FK_ID, FK_TITULO, FK_SUB, FK_FORMA, FK_ITENS, FK_MOTIVO,
  FK_L1, FK_L2, FK_ACAO, FK_ICONE, FK_ALVO, FK_CONFIANCA, FK_LINHAS,
  FK_SELOS, FK_EIXOS, FK_VEREDITO, FK_ESCOLHA, FK_PASSO, FK_DE,
  FK_PERGUNTA, FK_OPCOES, FK_N
};
static const char *const FP_CHAVES[FK_N] = {
  "t", "id", "titulo", "sub", "forma", "itens", "motivo",
  "l1", "l2", "acao", "icone", "alvo", "confianca", "linhas",
  "selos", "eixos", "veredito", "escolha", "passo", "de",
  "pergunta", "opcoes"
};

// Chaves permitidas e obrigatorias por tipo de peca (secao 4.2).
#define B(k) (1u << (k))
static const uint32_t FP_ALLOW[FP_ACHADOS + 1] = {
  /* fileira  */ B(FK_T)|B(FK_ID)|B(FK_TITULO)|B(FK_SUB)|B(FK_FORMA)|B(FK_ITENS)|B(FK_MOTIVO),
  /* dica     */ B(FK_T)|B(FK_ID)|B(FK_L1)|B(FK_L2)|B(FK_ACAO)|B(FK_ICONE),
  /* porque   */ B(FK_T)|B(FK_ALVO)|B(FK_CONFIANCA)|B(FK_LINHAS)|B(FK_SELOS),
  /* comparar */ B(FK_T)|B(FK_ITENS)|B(FK_EIXOS)|B(FK_VEREDITO)|B(FK_ESCOLHA),
  /* escolha  */ B(FK_T)|B(FK_PASSO)|B(FK_DE)|B(FK_PERGUNTA)|B(FK_OPCOES),
  /* info     */ B(FK_T)|B(FK_ALVO)|B(FK_ITENS),
  /* achados  */ B(FK_T)|B(FK_L1)|B(FK_L2)|B(FK_ITENS)|B(FK_MOTIVO),
};
static const uint32_t FP_REQ[FP_ACHADOS + 1] = {
  /* fileira  */ B(FK_ID)|B(FK_TITULO)|B(FK_FORMA)|B(FK_ITENS),
  /* dica     */ B(FK_ID)|B(FK_L1)|B(FK_ACAO),
  /* porque   */ B(FK_ALVO)|B(FK_CONFIANCA)|B(FK_LINHAS),
  /* comparar */ B(FK_ITENS)|B(FK_EIXOS),
  /* escolha  */ B(FK_PASSO)|B(FK_DE)|B(FK_PERGUNTA)|B(FK_OPCOES),
  /* info     */ B(FK_ALVO)|B(FK_ITENS),
  /* achados  */ B(FK_L1)|B(FK_ITENS),
};

typedef struct { const char *vi, *vf; } FpSlot;

// Le um campo de string curta; devolve FPE_* e o texto em dst.
static int fp_campo_str(const FpSlot *s, char *dst, size_t tam, int erroLen) {
  if (*s->vi != '"') return FPE_TIPO_VALOR;
  return fp_str_dec(s->vi, s->vf, dst, tam) < 0 ? erroLen : FPE_OK;
}

// Lista de referencias "c<n>"/"tt..." com checagem de duplicata.
static int fp_itens(Fp *fp, const FpSlot *s, FarolRef *itens, int max,
                    int min, int *nItens) {
  FpIt ait = { s->vi + 1, s->vf - 1, 0 };
  const char *e, *ef;
  char buf[FAROL_REF_TAM];
  int r, n = 0;
  if (*s->vi != '[') return FPE_TIPO_VALOR;
  while ((r = fp_arr_prox(&ait, &e, &ef)) == 1) {
    FarolRef ref = { -1, "" };
    if (n >= max) return FPE_FAIXA;
    if (*e != '"') return FPE_TIPO_VALOR;
    if (fp_str_dec(e, ef, buf, sizeof buf) < 0 ||
        !fp_ref(buf, fp->nCand, &ref)) return FPE_REF;
    if (fp_ref_em(itens, n, &ref) >= 0) return FPE_REF;   // duplicado
    itens[n++] = ref;
  }
  if (r < 0) return FP_TORTO;
  if (n < min) return FPE_FAIXA;
  *nItens = n;
  return FPE_OK;
}

// Objeto "motivo" de fileira/achados: chaves sao referencias a itens ja
// lidos; valores sao textos.
static int fp_motivo(Fp *fp, const FpSlot *s, FarolMotivo *mot, int *nMot,
                     const FarolRef *itens, int nItens, int max) {
  FpIt oit = { s->vi + 1, s->vf - 1, 0 };
  const char *ki, *kf, *v2, *vf2;
  char buf[FAROL_REF_TAM];
  int r, n = 0;
  if (*s->vi != '{') return FPE_TIPO_VALOR;
  while ((r = fp_obj_prox(&oit, &ki, &kf, &v2, &vf2)) == 1) {
    FarolRef ref = { -1, "" };
    int item;
    if (n >= max) return FPE_FAIXA;
    if (fp_str_dec(ki, kf, buf, sizeof buf) < 0 ||
        !fp_ref(buf, fp->nCand, &ref)) return FPE_REF;
    if ((item = fp_ref_em(itens, nItens, &ref)) < 0) return FPE_REF;
    // Chave duplicada, literal ou apelido ("c0" e "c00" resolvem igual).
    for (int i = 0; i < n; i++)
      if (mot[i].item == (int8_t)item) return FPE_JSON;
    if (*v2 != '{') return FPE_TIPO_VALOR;
    if ((r = fp_texto(fp, v2, vf2, &mot[n].t)) != FPE_OK) return r;
    mot[n].item = (int8_t)item;
    n++;
  }
  if (r < 0) return FP_TORTO;
  *nMot = n;
  return FPE_OK;
}

static int fp_texto_campo(Fp *fp, const FpSlot *s, FarolTexto *t) {
  return *s->vi == '{' ? fp_texto(fp, s->vi, s->vf, t) : FPE_TIPO_VALOR;
}

static const FpSlot *fp_slot(const FpSlot *slots, const int *idx, int n, int k) {
  for (int i = 0; i < n; i++)
    if (idx[i] == k) return &slots[i];
  return NULL;
}

// ------------------------------------------------------------ as pecas
static int fp_fileira(Fp *fp, const FpSlot *sl, const int *idx, int n,
                      FarolPeca *p) {
  const FpSlot *s;
  int e, nIt = 0;
  if ((e = fp_campo_str(fp_slot(sl, idx, n, FK_ID), p->u.fileira.id,
                        sizeof p->u.fileira.id, FPE_TEXTO)) != FPE_OK) return e;
  if (!fp_id_ok(p->u.fileira.id, FAROL_ID_TAM - 1, 1)) return FPE_TEXTO;
  if ((e = fp_texto_campo(fp, fp_slot(sl, idx, n, FK_TITULO),
                          &p->u.fileira.titulo)) != FPE_OK) return e;
  if ((s = fp_slot(sl, idx, n, FK_SUB)) &&
      (e = fp_texto_campo(fp, s, &p->u.fileira.sub)) != FPE_OK) return e;
  {
    char buf[FAROL_ID_TAM];
    if ((e = fp_campo_str(fp_slot(sl, idx, n, FK_FORMA), buf, sizeof buf,
                          FPE_TEXTO)) != FPE_OK) return e;
    if ((e = fp_enum(buf, FP_FORMAS, 4)) < 0) return FPE_ENUM;
    p->u.fileira.forma = (uint8_t)e;
  }
  if ((e = fp_itens(fp, fp_slot(sl, idx, n, FK_ITENS), p->u.fileira.itens,
                    FAROL_ITENS_MAX, 6, &nIt)) != FPE_OK) return e;
  p->u.fileira.nItens = (uint8_t)nIt;
  nIt = 0;
  if ((s = fp_slot(sl, idx, n, FK_MOTIVO)) &&
      (e = fp_motivo(fp, s, p->u.fileira.motivo, &nIt,
                     p->u.fileira.itens, p->u.fileira.nItens,
                     FAROL_ITENS_MAX)) != FPE_OK) return e;
  p->u.fileira.nMotivo = s ? (uint8_t)nIt : 0;
  return FPE_OK;
}

static int fp_dica(Fp *fp, const FpSlot *sl, const int *idx, int n,
                   FarolPeca *p) {
  const FpSlot *s;
  int e;
  if ((e = fp_campo_str(fp_slot(sl, idx, n, FK_ID), p->u.dica.id,
                        sizeof p->u.dica.id, FPE_TEXTO)) != FPE_OK) return e;
  if (!fp_id_ok(p->u.dica.id, FAROL_ID_TAM - 1, 1)) return FPE_TEXTO;
  if ((e = fp_texto_campo(fp, fp_slot(sl, idx, n, FK_L1),
                          &p->u.dica.l1)) != FPE_OK) return e;
  if ((s = fp_slot(sl, idx, n, FK_L2)) &&
      (e = fp_texto_campo(fp, s, &p->u.dica.l2)) != FPE_OK) return e;
  { // acao: {tipo: enum, alvo?: ref} — alvo so com abrir/tocar/comparar/salvar
    const FpSlot *sa = fp_slot(sl, idx, n, FK_ACAO);
    FpIt ait;
    const char *ki, *kf, *v2, *vf2;
    char buf[FAROL_ID_TAM];
    int r, temTipo = 0, temAlvo = 0;
    if (*sa->vi != '{') return FPE_TIPO_VALOR;
    ait.p = sa->vi + 1; ait.fim = sa->vf - 1; ait.precisa = 0;
    while ((r = fp_obj_prox(&ait, &ki, &kf, &v2, &vf2)) == 1) {
      if (fp_key_eh(ki, kf, "tipo")) {
        if (temTipo) return FPE_JSON;
        if (*v2 != '"') return FPE_TIPO_VALOR;
        if (fp_str_dec(v2, vf2, buf, sizeof buf) < 0) return FPE_TEXTO;
        if ((e = fp_enum(buf, FP_ACOES, 6)) < 0) return FPE_ENUM;
        p->u.dica.acao = (uint8_t)e;
        temTipo = 1;
      } else if (fp_key_eh(ki, kf, "alvo")) {
        if (temAlvo) return FPE_JSON;
        if (*v2 != '"') return FPE_TIPO_VALOR;
        if (fp_str_dec(v2, vf2, buf, sizeof buf) < 0) return FPE_TEXTO;
        if (!fp_ref(buf, fp->nCand, &p->u.dica.alvo)) return FPE_REF;
        temAlvo = 1;
      } else return FPE_CHAVE;
    }
    if (r < 0) return FP_TORTO;
    if (!temTipo) return FPE_FALTA;
    if (p->u.dica.acao == FP_ACAO_NADA || p->u.dica.acao == FP_ACAO_ESCOLHA) {
      if (temAlvo) return FPE_FAIXA;              // alvo onde nao cabe
    } else if (!temAlvo) return FPE_FALTA;
    p->u.dica.temAlvo = (uint8_t)temAlvo;
  }
  p->u.dica.icone[0] = 0;
  if ((s = fp_slot(sl, idx, n, FK_ICONE)) &&
      ((e = fp_campo_str(s, p->u.dica.icone, sizeof p->u.dica.icone,
                         FPE_TEXTO)) != FPE_OK ||
       !fp_id_ok(p->u.dica.icone, FAROL_ICONE_TAM - 1, 0)))
    return e ? e : FPE_TEXTO;
  return FPE_OK;
}

static int fp_porque(Fp *fp, const FpSlot *sl, const int *idx, int n,
                     FarolPeca *p) {
  const FpSlot *s;
  int64_t v;
  int e;
  s = fp_slot(sl, idx, n, FK_ALVO);
  {
    char buf[FAROL_REF_TAM];
    if ((e = fp_campo_str(s, buf, sizeof buf, FPE_REF)) != FPE_OK) return e;
    if (!fp_ref(buf, fp->nCand, &p->u.porque.alvo)) return FPE_REF;
  }
  s = fp_slot(sl, idx, n, FK_CONFIANCA);
  if (*s->vi != '-' && (*s->vi < '0' || *s->vi > '9')) return FPE_TIPO_VALOR;
  if (!fp_int64(s->vi, s->vf, &v) || v < 0 || v > 3) return FPE_FAIXA;
  p->u.porque.confianca = (uint8_t)v;
  { // linhas: 1..2 textos
    FpIt ait;
    const char *e2, *ef2;
    int r;
    s = fp_slot(sl, idx, n, FK_LINHAS);
    if (*s->vi != '[') return FPE_TIPO_VALOR;
    ait.p = s->vi + 1; ait.fim = s->vf - 1; ait.precisa = 0;
    p->u.porque.nLinhas = 0;
    while ((r = fp_arr_prox(&ait, &e2, &ef2)) == 1) {
      if (p->u.porque.nLinhas >= FAROL_LINHAS_MAX) return FPE_FAIXA;
      if (*e2 != '{') return FPE_TIPO_VALOR;
      if ((r = fp_texto(fp, e2, ef2,
                        &p->u.porque.linhas[p->u.porque.nLinhas])) != FPE_OK)
        return r;
      p->u.porque.nLinhas++;
    }
    if (r < 0) return FP_TORTO;
    if (p->u.porque.nLinhas == 0) return FPE_FAIXA;
  }
  p->u.porque.nSelos = 0;
  if ((s = fp_slot(sl, idx, n, FK_SELOS))) { // ate 3 {k: enum, id: >=0}
    FpIt ait;
    const char *e2, *ef2;
    int r;
    if (*s->vi != '[') return FPE_TIPO_VALOR;
    ait.p = s->vi + 1; ait.fim = s->vf - 1; ait.precisa = 0;
    while ((r = fp_arr_prox(&ait, &e2, &ef2)) == 1) {
      FpIt oit;
      const char *ki, *kf, *v2, *vf2;
      char buf[FAROL_ID_TAM];
      int temK = 0, temId = 0;
      FarolSelo selo;
      if (p->u.porque.nSelos >= FAROL_SELA_MAX) return FPE_FAIXA;
      if (*e2 != '{') return FPE_TIPO_VALOR;
      oit.p = e2 + 1; oit.fim = ef2 - 1; oit.precisa = 0;
      memset(&selo, 0, sizeof selo);
      while ((r = fp_obj_prox(&oit, &ki, &kf, &v2, &vf2)) == 1) {
        if (fp_key_eh(ki, kf, "k")) {
          if (temK) return FPE_JSON;
          if (*v2 != '"') return FPE_TIPO_VALOR;
          if (fp_str_dec(v2, vf2, buf, sizeof buf) < 0) return FPE_TEXTO;
          if ((e = fp_enum(buf, FP_SELOS, 3)) < 0) return FPE_ENUM;
          selo.k = (uint8_t)e;
          temK = 1;
        } else if (fp_key_eh(ki, kf, "id")) {
          if (temId) return FPE_JSON;
          if (*v2 != '-' && (*v2 < '0' || *v2 > '9')) return FPE_TIPO_VALOR;
          if (!fp_int64(v2, vf2, &v) || v < 0 || v > INT32_MAX)
            return FPE_FAIXA;
          selo.id = (int32_t)v;
          temId = 1;
        } else return FPE_CHAVE;
      }
      if (r < 0) return FP_TORTO;
      if (!temK || !temId) return FPE_FALTA;
      p->u.porque.selos[p->u.porque.nSelos++] = selo;
    }
    if (r < 0) return FP_TORTO;
  }
  return FPE_OK;
}

static int fp_comparar(Fp *fp, const FpSlot *sl, const int *idx, int n,
                       FarolPeca *p) {
  const FpSlot *s;
  int e, nIt = 0;
  if ((e = fp_itens(fp, fp_slot(sl, idx, n, FK_ITENS), p->u.comparar.itens,
                    3, 2, &nIt)) != FPE_OK) return e;
  p->u.comparar.nItens = (uint8_t)nIt;
  { // eixos: 1..8 da lista fechada, sem repetir
    FpIt ait;
    const char *e2, *ef2;
    char buf[FAROL_ID_TAM];
    int r;
    uint32_t vistos = 0;
    s = fp_slot(sl, idx, n, FK_EIXOS);
    if (*s->vi != '[') return FPE_TIPO_VALOR;
    ait.p = s->vi + 1; ait.fim = s->vf - 1; ait.precisa = 0;
    p->u.comparar.nEixos = 0;
    while ((r = fp_arr_prox(&ait, &e2, &ef2)) == 1) {
      if (p->u.comparar.nEixos >= FAROL_EIXO_MAX) return FPE_FAIXA;
      if (*e2 != '"') return FPE_TIPO_VALOR;
      if (fp_str_dec(e2, ef2, buf, sizeof buf) < 0) return FPE_TEXTO;
      if ((e = fp_enum(buf, FP_EIXOS, 8)) < 0) return FPE_ENUM;
      if (vistos & (1u << e)) return FPE_FAIXA;
      vistos |= 1u << e;
      p->u.comparar.eixos[p->u.comparar.nEixos++] = (uint8_t)e;
    }
    if (r < 0) return FP_TORTO;
    if (p->u.comparar.nEixos == 0) return FPE_FAIXA;
  }
  if ((s = fp_slot(sl, idx, n, FK_VEREDITO)) &&
      (e = fp_texto_campo(fp, s, &p->u.comparar.veredito)) != FPE_OK) return e;
  p->u.comparar.escolha = -1;
  if ((s = fp_slot(sl, idx, n, FK_ESCOLHA))) {
    char buf[FAROL_REF_TAM];
    FarolRef ref = { -1, "" };
    if ((e = fp_campo_str(s, buf, sizeof buf, FPE_REF)) != FPE_OK) return e;
    if (!fp_ref(buf, fp->nCand, &ref)) return FPE_REF;
    if ((e = fp_ref_em(p->u.comparar.itens, p->u.comparar.nItens, &ref)) < 0)
      return FPE_REF;                                   // fora dos itens
    p->u.comparar.escolha = (int8_t)e;
  }
  return FPE_OK;
}

static int fp_escolha(Fp *fp, const FpSlot *sl, const int *idx, int n,
                      FarolPeca *p) {
  const FpSlot *s;
  int64_t passo, de;
  int e;
  s = fp_slot(sl, idx, n, FK_PASSO);
  if (*s->vi != '-' && (*s->vi < '0' || *s->vi > '9')) return FPE_TIPO_VALOR;
  if (!fp_int64(s->vi, s->vf, &passo)) return FPE_FAIXA;
  s = fp_slot(sl, idx, n, FK_DE);
  if (*s->vi != '-' && (*s->vi < '0' || *s->vi > '9')) return FPE_TIPO_VALOR;
  if (!fp_int64(s->vi, s->vf, &de)) return FPE_FAIXA;
  if (de < 1 || de > 3 || passo < 1 || passo > de) return FPE_FAIXA;
  p->u.escolha.passo = (uint8_t)passo;
  p->u.escolha.de = (uint8_t)de;
  if ((e = fp_texto_campo(fp, fp_slot(sl, idx, n, FK_PERGUNTA),
                          &p->u.escolha.pergunta)) != FPE_OK) return e;
  { // opcoes: 2..4 de {id: identificador, r: texto}, id sem repetir
    FpIt ait;
    const char *e2, *ef2;
    int r;
    s = fp_slot(sl, idx, n, FK_OPCOES);
    if (*s->vi != '[') return FPE_TIPO_VALOR;
    ait.p = s->vi + 1; ait.fim = s->vf - 1; ait.precisa = 0;
    p->u.escolha.nOpcoes = 0;
    while ((r = fp_arr_prox(&ait, &e2, &ef2)) == 1) {
      FpIt oit;
      const char *ki, *kf, *v2, *vf2;
      int temId = 0, temR = 0;
      FarolOpcao op;
      if (p->u.escolha.nOpcoes >= FAROL_OPCOES_MAX) return FPE_FAIXA;
      if (*e2 != '{') return FPE_TIPO_VALOR;
      memset(&op, 0, sizeof op);
      oit.p = e2 + 1; oit.fim = ef2 - 1; oit.precisa = 0;
      while ((r = fp_obj_prox(&oit, &ki, &kf, &v2, &vf2)) == 1) {
        if (fp_key_eh(ki, kf, "id")) {
          if (temId) return FPE_JSON;
          if (*v2 != '"') return FPE_TIPO_VALOR;
          if (fp_str_dec(v2, vf2, op.id, sizeof op.id) < 0 ||
              !fp_id_ok(op.id, FAROL_OPCAO_TAM - 1, 1)) return FPE_TEXTO;
          temId = 1;
        } else if (fp_key_eh(ki, kf, "r")) {
          if (temR) return FPE_JSON;
          if (*v2 != '{') return FPE_TIPO_VALOR;
          if ((r = fp_texto(fp, v2, vf2, &op.r)) != FPE_OK) return r;
          temR = 1;
        } else return FPE_CHAVE;
      }
      if (r < 0) return FP_TORTO;
      if (!temId || !temR) return FPE_FALTA;
      for (int i = 0; i < p->u.escolha.nOpcoes; i++)
        if (!strcmp(p->u.escolha.opcoes[i].id, op.id)) return FPE_FAIXA;
      p->u.escolha.opcoes[p->u.escolha.nOpcoes++] = op;
    }
    if (r < 0) return FP_TORTO;
    if (p->u.escolha.nOpcoes < 2) return FPE_FAIXA;
  }
  return FPE_OK;
}

static int fp_info(Fp *fp, const FpSlot *sl, const int *idx, int n,
                   FarolPeca *p) {
  const FpSlot *s;
  int e;
  {
    char buf[FAROL_REF_TAM];
    s = fp_slot(sl, idx, n, FK_ALVO);
    if ((e = fp_campo_str(s, buf, sizeof buf, FPE_REF)) != FPE_OK) return e;
    if (!fp_ref(buf, fp->nCand, &p->u.info.alvo)) return FPE_REF;
  }
  { // itens: 1..3 de {k: enum, l: texto}, k sem repetir
    FpIt ait;
    const char *e2, *ef2;
    int r;
    uint32_t vistos = 0;
    s = fp_slot(sl, idx, n, FK_ITENS);
    if (*s->vi != '[') return FPE_TIPO_VALOR;
    ait.p = s->vi + 1; ait.fim = s->vf - 1; ait.precisa = 0;
    p->u.info.nItens = 0;
    while ((r = fp_arr_prox(&ait, &e2, &ef2)) == 1) {
      FpIt oit;
      const char *ki, *kf, *v2, *vf2;
      char buf[FAROL_ID_TAM];
      int temK = 0, temL = 0;
      FarolInfoItem item;
      if (p->u.info.nItens >= FAROL_INFO_MAX) return FPE_FAIXA;
      if (*e2 != '{') return FPE_TIPO_VALOR;
      memset(&item, 0, sizeof item);
      oit.p = e2 + 1; oit.fim = ef2 - 1; oit.precisa = 0;
      while ((r = fp_obj_prox(&oit, &ki, &kf, &v2, &vf2)) == 1) {
        if (fp_key_eh(ki, kf, "k")) {
          if (temK) return FPE_JSON;
          if (*v2 != '"') return FPE_TIPO_VALOR;
          if (fp_str_dec(v2, vf2, buf, sizeof buf) < 0) return FPE_TEXTO;
          if ((e = fp_enum(buf, FP_INFO_K, 8)) < 0) return FPE_ENUM;
          if (vistos & (1u << e)) return FPE_FAIXA;
          vistos |= 1u << e;
          item.k = (uint8_t)e;
          temK = 1;
        } else if (fp_key_eh(ki, kf, "l")) {
          if (temL) return FPE_JSON;
          if (*v2 != '{') return FPE_TIPO_VALOR;
          if ((r = fp_texto(fp, v2, vf2, &item.l)) != FPE_OK) return r;
          temL = 1;
        } else return FPE_CHAVE;
      }
      if (r < 0) return FP_TORTO;
      if (!temK || !temL) return FPE_FALTA;
      p->u.info.itens[p->u.info.nItens++] = item;
    }
    if (r < 0) return FP_TORTO;
    if (p->u.info.nItens == 0) return FPE_FAIXA;
  }
  return FPE_OK;
}

static int fp_achados(Fp *fp, const FpSlot *sl, const int *idx, int n,
                      FarolPeca *p) {
  const FpSlot *s;
  int e, nIt = 0;
  if ((e = fp_texto_campo(fp, fp_slot(sl, idx, n, FK_L1),
                          &p->u.achados.l1)) != FPE_OK) return e;
  if ((s = fp_slot(sl, idx, n, FK_L2)) &&
      (e = fp_texto_campo(fp, s, &p->u.achados.l2)) != FPE_OK) return e;
  if ((e = fp_itens(fp, fp_slot(sl, idx, n, FK_ITENS), p->u.achados.itens,
                    3, 1, &nIt)) != FPE_OK) return e;
  p->u.achados.nItens = (uint8_t)nIt;
  nIt = 0;
  if ((s = fp_slot(sl, idx, n, FK_MOTIVO)) &&
      (e = fp_motivo(fp, s, p->u.achados.motivo, &nIt, p->u.achados.itens,
                     p->u.achados.nItens, 3)) != FPE_OK) return e;
  p->u.achados.nMotivo = s ? (uint8_t)nIt : 0;
  return FPE_OK;
}

// Valida UMA peca [vi,vf) dentro de c->pecas... o chamador escreve em slot.
static int fp_peca(Fp *fp, const char *vi, const char *vf, FarolPeca *p) {
  FpIt it;
  const char *ki, *kf, *v2, *vf2;
  FpSlot slots[FK_N];
  int idx[FK_N], nSl = 0, tSlot = -1, r, tipo = -1;
  uint32_t seen = 0;
  memset(p, 0, sizeof *p);
  if (vi >= vf || *vi != '{') return FPE_JSON;
  it.p = vi + 1; it.fim = vf - 1; it.precisa = 0;
  while ((r = fp_obj_prox(&it, &ki, &kf, &v2, &vf2)) == 1) {
    int k;
    for (k = 0; k < FK_N && !fp_key_eh(ki, kf, FP_CHAVES[k]); k++) {}
    if (k == FK_N) return FPE_CHAVE;
    if (seen & (1u << k)) return FPE_JSON;
    seen |= 1u << k;
    slots[nSl].vi = v2; slots[nSl].vf = vf2; idx[nSl] = k;
    if (k == FK_T) tSlot = nSl;
    nSl++;
  }
  if (r < 0) return FP_TORTO;
  if (tSlot < 0) return FPE_TIPO;
  {
    const FpSlot *ts = &slots[tSlot];
    char buf[FAROL_ID_TAM];
    if (*ts->vi != '"') return FPE_TIPO_VALOR;
    if (fp_str_dec(ts->vi, ts->vf, buf, sizeof buf) < 0 ||
        (tipo = fp_enum(buf, FP_TIPOS, 7)) < 0) return FPE_TIPO;
    p->tipo = (uint8_t)tipo;
    for (int i = 0; i < nSl; i++)
      if (!(FP_ALLOW[tipo] & (1u << idx[i]))) return FPE_CHAVE;
    if ((seen & FP_REQ[tipo]) != FP_REQ[tipo]) return FPE_FALTA;
  }
  switch (tipo) {
    case FP_FILEIRA:  return fp_fileira(fp, slots, idx, nSl, p);
    case FP_DICA:     return fp_dica(fp, slots, idx, nSl, p);
    case FP_PORQUE:   return fp_porque(fp, slots, idx, nSl, p);
    case FP_COMPARAR: return fp_comparar(fp, slots, idx, nSl, p);
    case FP_ESCOLHA:  return fp_escolha(fp, slots, idx, nSl, p);
    case FP_INFO:     return fp_info(fp, slots, idx, nSl, p);
    case FP_ACHADOS:  return fp_achados(fp, slots, idx, nSl, p);
  }
  return FPE_TIPO;
}

// -------------------------------------------------------------- envelope
// Chaves do envelope: a desconhecida recusa a cena inteira (nao ha peca
// para atribuir a recusa).
enum { FKE_V, FKE_TELA, FKE_PERFIL, FKE_FEITA, FKE_VALE, FKE_ORIGEM,
       FKE_PECAS, FKE_N };
static const char *const FP_ENV[FKE_N] = {
  "v", "tela", "perfil", "feita", "vale_s", "origem", "pecas"
};

int farolpeca_ler(FarolCena *c, const char *json, int nCandidatos) {
  const char *fim, *ini, *fimDoc;
  FpIt it, ait;
  const char *ki, *kf, *vi, *vf, *e, *ef;
  FpSlot env[FKE_N];
  int envIdx[FKE_N], nEnv = 0;
  uint32_t seen = 0;
  Fp fp;
  size_t len;
  int r, pos = 0;
  int64_t v;

  memset(c, 0, sizeof *c);
  fp.c = c;
  fp.nCand = nCandidatos < 0 ? 0 : nCandidatos;
  if (!json) return -FCE_JSON;
  len = strlen(json);
  if (len > FAROL_CENA_MAX) return -FCE_TAMANHO;
  fim = json + len;
  ini = fp_pula(json, fim);
  fimDoc = fim;
  while (fimDoc > ini && (fimDoc[-1] == ' ' || fimDoc[-1] == '\t' ||
         fimDoc[-1] == '\n' || fimDoc[-1] == '\r')) fimDoc--;
  if (ini >= fimDoc || *ini != '{' || fimDoc[-1] != '}') return -FCE_JSON;
  // Pre-passada estrutural: o documento inteiro tem de ser JSON valido —
  // fecho da raiz, delimitadores, strings, numeros e literais — inclusive
  // onde a leitura por esquema nao desce (peca ja recusada, alem da 6a).
  if (!fp_json_ok(ini, fimDoc)) return -FCE_JSON;

  it.p = ini + 1; it.fim = fimDoc - 1; it.precisa = 0;
  while ((r = fp_obj_prox(&it, &ki, &kf, &vi, &vf)) == 1) {
    int k;
    for (k = 0; k < FKE_N && !fp_key_eh(ki, kf, FP_ENV[k]); k++) {}
    if (k == FKE_N) return -FCE_CHAVE;
    if (seen & (1u << k)) return -FCE_JSON;
    seen |= 1u << k;
    env[nEnv].vi = vi; env[nEnv].vf = vf; envIdx[nEnv] = k;
    nEnv++;
  }
  if (r < 0) return -FCE_JSON;
  if (!(seen & B(FKE_V)) || !(seen & B(FKE_TELA)) || !(seen & B(FKE_PECAS)))
    return -FCE_FALTA;
  {
    const FpSlot *sV = fp_slot(env, envIdx, nEnv, FKE_V);
    const FpSlot *sT = fp_slot(env, envIdx, nEnv, FKE_TELA);
    const FpSlot *sP = fp_slot(env, envIdx, nEnv, FKE_PECAS);
    const FpSlot *s;
    if (*sV->vi != '-' && (*sV->vi < '0' || *sV->vi > '9')) return -FCE_CAMPO;
    if (!fp_int64(sV->vi, sV->vf, &v)) return -FCE_CAMPO;
    if (v != 1) return -FCE_V;
    if (*sT->vi != '"') return -FCE_CAMPO;
    if (fp_str_dec(sT->vi, sT->vf, c->tela, sizeof c->tela) < 0 ||
        !fp_id_ok(c->tela, FAROL_ENV_TAM - 1, 0)) return -FCE_CAMPO;
    if (*sP->vi != '[') return -FCE_CAMPO;
    if ((s = fp_slot(env, envIdx, nEnv, FKE_PERFIL))) {
      if (*s->vi != '-' && (*s->vi < '0' || *s->vi > '9')) return -FCE_CAMPO;
      if (!fp_int64(s->vi, s->vf, &v) || v < 0 || v > INT32_MAX)
        return -FCE_CAMPO;
      c->perfil = (int32_t)v;
      c->opt |= FP_O_PERFIL;
    }
    if ((s = fp_slot(env, envIdx, nEnv, FKE_FEITA))) {
      if (*s->vi != '-' && (*s->vi < '0' || *s->vi > '9')) return -FCE_CAMPO;
      if (!fp_int64(s->vi, s->vf, &v)) return -FCE_CAMPO;
      c->feita = v;
      c->opt |= FP_O_FEITA;
    }
    if ((s = fp_slot(env, envIdx, nEnv, FKE_VALE))) {
      if (*s->vi != '-' && (*s->vi < '0' || *s->vi > '9')) return -FCE_CAMPO;
      if (!fp_int64(s->vi, s->vf, &v) || v < 0) return -FCE_CAMPO;
      c->vale_s = v;
      c->opt |= FP_O_VALE;
    }
    if ((s = fp_slot(env, envIdx, nEnv, FKE_ORIGEM))) {
      if (*s->vi != '"') return -FCE_CAMPO;
      if (fp_str_dec(s->vi, s->vf, c->origem, sizeof c->origem) < 0 ||
          !fp_id_ok(c->origem, FAROL_ENV_TAM - 1, 0)) return -FCE_CAMPO;
      c->opt |= FP_O_ORIGEM;
    }

    // As pecas: uma a uma; a torta cai sozinha e a 7a em diante conta
    // recusada sem chegar a ser lida. pecas[] fica compactada com as boas.
    ait.p = sP->vi + 1; ait.fim = sP->vf - 1; ait.precisa = 0;
    while ((r = fp_arr_prox(&ait, &e, &ef)) == 1) {
      if (pos >= FAROL_PECA_MAX) {
        c->nRecusadas++;                      // a 7a e alem: sozinhas fora
        pos++;
        continue;
      }
      {
        int xsAntes = c->nXs;
        int er = fp_peca(&fp, e, ef, &c->pecas[c->nPecas]);
        if (er == FP_TORTO) return -FCE_JSON;   // documento invalido
        c->erro[pos] = (uint8_t)er;
        if (er == FPE_OK) c->nPecas++;
        else { c->nRecusadas++; c->nXs = xsAntes; }
      }
      pos++;
    }
    // Elemento torto entre virgulas (vazio, virgula sobrando, colchete sem
    // fecho): o array nao e JSON valido — a cena inteira cai, nao uma peca.
    if (r < 0) return -FCE_JSON;
  }
  return c->nPecas;
}

// ------------------------------------------------------------------ nomes
static const char *const FP_ERROS[] = {
  "FPE_OK", "FPE_JSON", "FPE_TIPO", "FPE_CHAVE", "FPE_TIPO_VALOR",
  "FPE_FALTA", "FPE_ENUM", "FPE_REF", "FPE_TEXTO", "FPE_X", "FPE_FAIXA",
  "FPE_LIMITE"
};
static const char *const FP_CERROS[] = {
  "FCE_OK", "FCE_TAMANHO", "FCE_JSON", "FCE_V", "FCE_CHAVE", "FCE_CAMPO",
  "FCE_FALTA"
};

const char *farolpeca_erro_nome(int fpe) {
  return (fpe >= 0 && fpe < (int)(sizeof FP_ERROS / sizeof *FP_ERROS))
         ? FP_ERROS[fpe] : "FPE_?";
}
const char *farolpeca_cena_erro_nome(int fce) {
  return (fce >= 0 && fce < (int)(sizeof FP_CERROS / sizeof *FP_CERROS))
         ? FP_CERROS[fce] : "FCE_?";
}
const char *farolpeca_tipo_nome(int tipo) {
  return (tipo >= 0 && tipo < 7) ? FP_TIPOS[tipo] : "?";
}

int farolpeca_texto_tem(const FarolTexto *t) {
  return t->usaX || t->m[0];
}

const char *farolpeca_x(const FarolCena *c, const FarolTexto *t) {
  return (t->usaX && t->xi >= 0 && t->xi < c->nXs) ? c->xs[t->xi] : "";
}
