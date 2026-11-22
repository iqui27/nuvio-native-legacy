// farolcena.c — compositor deterministico do FarolCena v1 (lote sense,
// issue 44). O contrato esta em farolcena.h; as regras de verdade dos
// motivos moram no farolrec (41), o filtro de texto livre no faroltexto
// (35) e os fatos da P6 na farolinfo (47). Este arquivo nao recomenda:
// ele escolhe o que o motor ja provou e serializa no schema que o
// validador da 39 reconhece.
//
// A saida e construida por emissao (nunca por varredura), entao virgula
// final, chave fora do schema e texto sem modelo nao tem como existir —
// a ressalva aberta da revisao FAROL39 r3 ("fp_json_ok aceita virgula
// final") nao se aplica a um documento que nasce sem ela. A FarolCena de
// 25,9 KB nao e criada aqui: o chamador valida/consome/descarta (a r3
// pede fora da pilha do fio da UI; o mesmo vale para os ~42 KB que a
// farolrec_escolher usa dentro desta chamada).

#include "farolcena.h"
#include "faroltexto.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

// ------------------------------------------------------------- emissor
// Buffer de escrita com estouro pegajoso: quando uma emissao nao cabe, p
// vai a fim e fica la — a peca que estourou e descartada inteira, nunca
// sai pela metade.
typedef struct { char *ini, *p, *fim; } Fc;

static void fc_raw(Fc *b, const char *s) {
  size_t n = strlen(s);
  if ((size_t)(b->fim - b->p) < n) { b->p = b->fim; return; }
  memcpy(b->p, s, n);
  b->p += n;
}
static void fc_ch(Fc *b, char c) { if (b->p < b->fim) *b->p++ = c; }
static void fc_i64(Fc *b, int64_t v) {
  char t[24];
  snprintf(t, sizeof t, "%lld", (long long)v);
  fc_raw(b, t);
}
// String JSON com aspas; escapa \" \\ e controle -> \u00XX por guarda
// (os argumentos ja foram conferidos sem controle antes de chegar aqui).
static void fc_str(Fc *b, const char *s) {
  fc_ch(b, '"');
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { fc_ch(b, '\\'); fc_ch(b, (char)c); }
    else if (c < 0x20) {
      char u[8];
      snprintf(u, sizeof u, "\\u%04x", c);
      fc_raw(b, u);
    } else fc_ch(b, (char)c);
  }
  fc_ch(b, '"');
}
// {"m":"<nome>"} ou {"m":"<nome>","a":["x","y"]} — sem "a" quando o
// modelo nao tem argumentos (a 39 exige aridade exata).
static void fc_texto(Fc *b, const char *m, const char *const *a, int na) {
  int i;
  fc_raw(b, "{\"m\":");
  fc_str(b, m);
  if (na > 0) {
    fc_raw(b, ",\"a\":[");
    for (i = 0; i < na; i++) {
      if (i) fc_ch(b, ',');
      fc_str(b, a[i]);
    }
    fc_ch(b, ']');
  }
  fc_ch(b, '}');
}

// ------------------------------------------------- modelos FT_* da casa
// Nome -> literal, da MESMA tabela que o validador e o i18n conferem:
// um modelo que a 35 nao tem nao pode entrar na cena (FPE_TEXTO) — a
// consulta aqui e a porta que deixa a linha de fora.
static const struct { const char *nome; const char *lit; } FC_MODS[] = {
#define FC_MOD_X(nome) { "FT_" #nome, FT_##nome },
  FAROL_FRASES(FC_MOD_X)
#undef FC_MOD_X
};
#define FC_MODS_N (sizeof FC_MODS / sizeof *FC_MODS)

// Aridade do modelo, ou -1 quando nao existe (FALTA_TEXTO da 35 — a
// linha e omitida e anotada no relatorio).
static int fc_modelo_args(const char *nome) {
  size_t i;
  for (i = 0; i < FC_MODS_N; i++)
    if (!strcmp(FC_MODS[i].nome, nome)) {
      const char *p;
      int n = 0;
      for (p = FC_MODS[i].lit; *p; p++) {
        if (p[0] == '%' && p[1] == '%') { p++; continue; }
        if (p[0] == '%') n++;
      }
      return n;
    }
  return -1;
}

// --------------------------------------------------------- validacoes
// Espelho minimo das checagens do validador para o que este modulo
// RECEBE (refs, args, ids): quem monta so emite o que passaria — a
// cena nao depende da boa vontade do chamador.
static int fc_utf8_ok(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned char c = *p;
    int extra = 0, i;
    if (c < 0x80) { p++; continue; }
    if (c >= 0xC2 && c <= 0xDF)      extra = 1;
    else if (c >= 0xE0 && c <= 0xEF) extra = 2;
    else if (c >= 0xF0 && c <= 0xF4) extra = 3;
    else return 0;
    for (i = 1; i <= extra; i++)
      if ((p[i] & 0xC0) != 0x80) return 0;
    p += extra + 1;
  }
  return 1;
}
static int fc_arg_ok(const char *s) {
  const unsigned char *p;
  if (!s) return 0;
  for (p = (const unsigned char *)s; *p; p++)
    if (*p < 0x20 || *p == 0x7F) return 0;
  return fc_utf8_ok(s);
}
// "c<digitos>" resolve contra nCand; "tt<digitos>" e id IMDb.
static int fc_ref_ok(const char *s, int nCand) {
  size_t n, i;
  int64_t idx = 0;
  if (!s || (n = strlen(s)) == 0 || n >= FAROL_REF_TAM) return 0;
  if (s[0] == 'c' && n >= 2) {
    for (i = 1; i < n; i++) {
      if (s[i] < '0' || s[i] > '9') return 0;
      idx = idx * 10 + (s[i] - '0');
      if (idx > INT16_MAX) return 0;
    }
    return idx < nCand;
  }
  if (n >= 3 && s[0] == 't' && s[1] == 't') {
    for (i = 2; i < n; i++)
      if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
  }
  return 0;
}
// Charset dos identificadores (id de peca/dica, icone): [a-z0-9_-A-Z].
static int fc_id_ok(const char *s, size_t max) {
  size_t n;
  const unsigned char *p;
  if (!s || !(n = strlen(s)) || n > max) return 0;
  for (p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
        c == '-' || (c >= 'A' && c <= 'Z')) continue;
    return 0;
  }
  return 1;
}

// ------------------------------------------------------ afinidades
// Slot da afinidade (dim,id) no modelo — para o "n >= 2 e peso > 0,5"
// da 3.5 aplicado aos selos da P3 e ao tema da fileira 2.
static const FarolAfin *fc_afin(const FarolGosto *g, int dim, int32_t id) {
  const FarolAfin *v;
  int cap, i;
  switch (dim) {
    case FG_DIM_GENERO:  v = g->genero;  cap = FG_GENERO_N;  break;
    case FG_DIM_PESSOA:  v = g->pessoa;  cap = FG_PESSOA_N;  break;
    case FG_DIM_TEMA:    v = g->tema;    cap = FG_TEMA_N;    break;
    case FG_DIM_DECADA:  v = g->decada;  cap = FG_DECADA_N;  break;
    case FG_DIM_IDIOMA:  v = g->idioma;  cap = FG_IDIOMA_N;  break;
    default:             v = g->duracao; cap = FG_DURACAO_N; break;
  }
  for (i = 0; i < cap; i++)
    if (v[i].id == id) return &v[i];
  return NULL;
}
// 1 = afinidade sustenta explicacao (n >= 2 e peso projetado > 0,5, 3.5).
static int fc_afin_prova(const FarolGosto *g, int dim, int32_t id,
                         uint16_t dia) {
  const FarolAfin *a = fc_afin(g, dim, id);
  return a && a->n >= 2 && farolgosto_peso(g, dim, id, dia) > 128;
}
// O id esta na lista de temas/generos da ficha? (fc_tema_em tambem serve
// a fileira 2 abaixo.)
static int fc_tema_em(const FarolFicha *f, int32_t t) {
  int j;
  if (!f) return 0;
  for (j = 0; j < FAROL_FICHA_TEMA && f->tema[j]; j++)
    if (f->tema[j] == t) return 1;
  return 0;
}
static int fc_gen_em(const FarolFicha *f, int32_t g) {
  int j;
  if (!f) return 0;
  for (j = 0; j < FAROL_FICHA_GEN && f->genero[j]; j++)
    if (f->genero[j] == g) return 1;
  return 0;
}
// Mesma tabela de faixas do farolgosto/farolrec (duplicada de proposito:
// o modulo nao importa estatico de irmao).
static int fc_durFaixa(int min) {
  if (min <= 0) return -1;
  if (min < 30) return 0;
  if (min < 60) return 1;
  if (min < 100) return 2;
  if (min < 140) return 3;
  return 4;
}
// Tempo decorrido codificado como farolinfo_retomada: "<n><u>" com u em
// {d,w,M,y} na entrada em DIAS (salvo nao tem hora). Mesma tabela de
// cortes: <14d "d", <63d "w" (d/7), <730d "M" (d/30), depois "y" (d/365).
static void fc_tempo_dias(char *dst, size_t cap, long dd) {
  if (dd < 14)        snprintf(dst, cap, "%ldd", dd > 0 ? dd : 1);
  else if (dd < 63)   snprintf(dst, cap, "%ldw", dd / 7);
  else if (dd < 730)  snprintf(dst, cap, "%ldM", dd / 30);
  else                snprintf(dst, cap, "%ldy", dd / 365);
}

// -------------------------------------------------------- o motivo
// FarolRecMotivo (estruturado, do motor) -> {modelo, args} renderizavel.
// Devolve 0 quando o argumento nao pode ser produzido com dado real —
// e a linha nao sai (3.5: sem motivo verdadeiro, sem linha de motivo).
typedef struct { const char *m; int na; char a[2][FAROL_ARG_TAM]; } FcMot;

static int fc_motivo(const FarolCenaCtx *ctx, const FarolRecCand *c,
                     uint16_t salvoDia, const FarolRecMotivo *mo, FcMot *o) {
  memset(o, 0, sizeof *o);
  switch (mo->tipo) {
    case FMO_TERMINOU: {
      const char *ref;
      if (mo->a < 0 || mo->a >= ctx->nSementes || !ctx->sementeRef)
        return 0;
      ref = ctx->sementeRef[mo->a];
      if (!ref || !fc_ref_ok(ref, ctx->nCand)) return 0;
      if (mo->b) {
        // b = afinidade compartilhada forte. O motor devolve so o id e
        // escolhe tema antes de genero — a mesma ordem e reprovada aqui
        // (id na ficha do candidato E na da semente + afinidade que
        // sustenta explicacao): um id de tema que so o genero provou
        // sai "g", nunca "k" (ressalva 2 da r1 — colisao numerica).
        const FarolFicha *fs = ctx->sementes[mo->a].ficha;
        int k = c->ficha && fc_tema_em(c->ficha, mo->b) &&
                fc_tema_em(fs, mo->b) &&
                fc_afin_prova(ctx->g, FG_DIM_TEMA, mo->b, ctx->dia);
        int gv = !k && c->ficha && fc_gen_em(c->ficha, mo->b) &&
                 fc_gen_em(fs, mo->b) &&
                 fc_afin_prova(ctx->g, FG_DIM_GENERO, mo->b, ctx->dia);
        if (k || gv) {
          snprintf(o->a[1], FAROL_ARG_TAM, "%s%ld", k ? "k" : "g",
                   (long)mo->b);
          o->m = "FT_MOT_TERMINOU_TEMA";
          o->na = 2;
        } else {
          o->m = "FT_MOT_TERMINOU";
          o->na = 1;
        }
      } else {
        o->m = "FT_MOT_TERMINOU";
        o->na = 1;
      }
      snprintf(o->a[0], FAROL_ARG_TAM, "%s", ref);
      return 1;
    }
    case FMO_GENERO:
      o->m = "FT_MOT_GENERO"; o->na = 1;
      snprintf(o->a[0], FAROL_ARG_TAM, "g%ld", (long)mo->a);
      return 1;
    case FMO_PESSOA: {
      // "Do diretor de <semente>, que voce viu <n> vezes": b e o indice
      // da semente que tambem tem a pessoa; pessoa[0] da ficha e a
      // direcao/criacao. A prova e quadrupla: semente nomeavel, pessoa
      // como direcao nas duas fichas, afinidade com n>=2 e a semente
      // citada TERMINADA (Diario sem PARCIAL). O <n> conta so titulos
      // vistos da pessoa — sementes terminadas com ela na direcao —
      // nao todo sinal do modelo (ressalva 3 da r1: salvo/interesse
      // nao e "viu"). Sem tudo isso cai na forma comum — o b = 0 e
      // ambiguo com "sem semente", nao vale prova.
      const FarolRecSemente *s;
      const FarolAfin *af;
      const char *ref;
      if (mo->b > 0 && mo->b < ctx->nSementes && c->ficha &&
          c->ficha->pessoa[0] == mo->a) {
        s = &ctx->sementes[mo->b];
        af = fc_afin(ctx->g, FG_DIM_PESSOA, mo->a);
        ref = ctx->sementeRef ? ctx->sementeRef[mo->b] : NULL;
        if (s->ficha && s->ficha->pessoa[0] == mo->a && s->terminou &&
            af && af->n >= 2 && ref && fc_ref_ok(ref, ctx->nCand)) {
          int nv = 0, i;
          for (i = 0; i < ctx->nSementes; i++) {
            const FarolRecSemente *si = &ctx->sementes[i];
            if (si->ficha && si->terminou &&
                si->ficha->pessoa[0] == mo->a) nv++;
          }
          snprintf(o->a[0], FAROL_ARG_TAM, "%s", ref);
          snprintf(o->a[1], FAROL_ARG_TAM, "%d", nv);
          o->m = "FT_MOT_DIRETOR"; o->na = 2;
          return 1;
        }
      }
      o->m = "FT_MOT_PESSOA"; o->na = 1;
      snprintf(o->a[0], FAROL_ARG_TAM, "p%ld", (long)mo->a);
      return 1;
    }
    case FMO_TEMA:
      o->m = "FT_MOT_TEMA"; o->na = 1;
      snprintf(o->a[0], FAROL_ARG_TAM, "k%ld", (long)mo->a);
      return 1;
    case FMO_AMIGO:
      if (!mo->a) { o->m = "FT_MOT_AMIGO_ANON"; return 1; }
      o->m = "FT_MOT_AMIGO"; o->na = 2;
      snprintf(o->a[0], FAROL_ARG_TAM, "u%ld", (long)mo->a);
      {
        int d = (int)((mo->b + 5) / 10);        // 0..100 -> 0..10
        if (d > 10) d = 10;
        snprintf(o->a[1], FAROL_ARG_TAM, "%d", d);
      }
      return 1;
    case FMO_RECOMENDADO:
      if (!mo->a) { o->m = "FT_MOT_RECOM_ANON"; return 1; }
      o->m = "FT_MOT_RECOMENDADO"; o->na = 1;
      snprintf(o->a[0], FAROL_ARG_TAM, "u%ld", (long)mo->a);
      return 1;
    case FMO_SALVO:
      if (!salvoDia || ctx->dia <= salvoDia) return 0;
      o->m = "FT_MOT_SALVO"; o->na = 1;
      fc_tempo_dias(o->a[0], FAROL_ARG_TAM,
                    (long)ctx->dia - (long)salvoDia);
      return 1;
    case FMO_CABE_HOJE:
      o->m = "FT_MOT_CABE_HOJE"; o->na = 1;
      snprintf(o->a[0], FAROL_ARG_TAM, "%ld", (long)mo->a);
      return 1;
    case FMO_REVER:
      o->m = "FT_MOT_REVER";
      return 1;
    case FMO_FORA_CURVA:
      o->m = "FT_MOT_FORA_CURVA"; o->na = 1;
      snprintf(o->a[0], FAROL_ARG_TAM, "g%ld", (long)mo->a);
      return 1;
  }
  return 0;
}

// O argumento de afinidade ("g<"/"k<"/"p<") que a linha ja renderizada
// carrega, ou NULL quando ela nao afirma afinidade — serve para a
// deduplicacao das linhas do porque.
static const char *fc_argafin(const FcMot *o) {
  if (!o->na) return NULL;
  if (!strcmp(o->m, "FT_MOT_TERMINOU_TEMA")) return o->a[1];
  if (!strcmp(o->m, "FT_MOT_GENERO") || !strcmp(o->m, "FT_MOT_PESSOA") ||
      !strcmp(o->m, "FT_MOT_TEMA") || !strcmp(o->m, "FT_MOT_FORA_CURVA"))
    return o->a[0];
  return NULL;
}

// Primeiro motivo renderizavel do item (o motor ja ordena por forca).
// `salvoDia` aqui e o valor do candidato, nao o vetor. Devolve o indice
// em it->motivo ou -1.
static int fc_motivo1(const FarolCenaCtx *ctx, const FarolRecCand *c,
                      uint16_t salvoDia, const FarolRecItem *it, FcMot *o) {
  int i;
  for (i = 0; i < it->nMotivo; i++)
    if (fc_motivo(ctx, c, salvoDia, &it->motivo[i], o)) return i;
  return -1;
}

// ----------------------------------------------------------- envelope
// Teto dos inteiros do envelope: o fp_int64 do validador desiste um
// digito antes do INT64_MAX real (rejeita o prefixo acima de
// (LLONG_MAX-9)/10), entao o maior numero que a cena carrega e
// INT64_MAX-8. Acima disso satura — a cena nunca sai invalida por um
// relogio grande demais (ressalva 4a da r1).
#define FC_ENV_MAX (INT64_MAX - 8)
static void fc_env(Fc *b, const FarolCenaCtx *ctx, const char *tela) {
  fc_raw(b, "{\"v\":1,\"tela\":");
  fc_str(b, tela);
  if (ctx->perfil >= 0) {
    fc_raw(b, ",\"perfil\":");
    fc_i64(b, ctx->perfil);
  }
  if (ctx->feita > 0) {
    fc_raw(b, ",\"feita\":");
    fc_i64(b, ctx->feita > FC_ENV_MAX ? FC_ENV_MAX : ctx->feita);
  }
  if (ctx->valeS > 0) {
    fc_raw(b, ",\"vale_s\":");
    fc_i64(b, ctx->valeS > FC_ENV_MAX ? FC_ENV_MAX : ctx->valeS);
  }
  fc_raw(b, ",\"origem\":\"regras\",\"pecas\":[");
}

// ------------------------------------------------------------ fileira
// Prioridade de desempate do motivo dominante: a ordem do aceite
// (terminou X, pessoa, tema, hora, amigos), com genero entre tema e hora
// (o titulo de genero tambem existe) e o resto do enum ao final.
static const int FC_TIPO_PRIO[] = {
  FMO_TERMINOU, FMO_PESSOA, FMO_TEMA, FMO_GENERO, FMO_CABE_HOJE,
  FMO_AMIGO, FMO_SALVO, FMO_RECOMENDADO, FMO_REVER, FMO_FORA_CURVA
};

// Titulo da fileira pelo motivo dominante. `forca`/`forcaArg` quando o
// chamador ja sabe o titulo (fileira 2 e sempre "Mais sobre <tema>" —
// nao: FT_FIL_TEMA com o tema do grupo). Devolve o modelo em `mt` e o
// argumento formatado em `arg` ("" quando o modelo nao pede).
static void fc_titulo(const FarolCenaCtx *ctx, const FarolRecItem *out,
                      const int *idx, const int *tipo, const int *midx,
                      int n, int nivel,
                      const char *forca, const char *forcaArg,
                      const char **mt, char *arg, size_t argCap) {
  int conta[FMO_RECOMENDADO + 1];
  int i, j, dom = -1;
  static const char *const T[FMO_RECOMENDADO + 1] = {
    "FT_FIL_PORQUE_TERMINOU", "FT_FIL_GENERO", "FT_FIL_PESSOA",
    "FT_FIL_TEMA", "FT_FIL_AMIGOS", "FT_FIL_SALVOS", "FT_FIL_HORA",
    "FT_FIL_REVER", "FT_FIL_FORA_CURVA", "FT_FIL_RECOMENDADOS"
  };
  arg[0] = 0;
  *mt = NULL;
  if (forca) {
    *mt = forca;
    snprintf(arg, argCap, "%s", forcaArg ? forcaArg : "");
    return;
  }
  memset(conta, 0, sizeof conta);
  for (i = 0; i < n; i++)
    if (tipo[i] >= 0) conta[tipo[i]]++;
  // O mais frequente; no empate, a prioridade da tabela do aceite.
  for (i = 0; i < (int)(sizeof FC_TIPO_PRIO / sizeof *FC_TIPO_PRIO); i++)
    if (dom < 0 || conta[FC_TIPO_PRIO[i]] > conta[dom])
      dom = FC_TIPO_PRIO[i];
  if (dom >= 0 && !conta[dom]) dom = -1;
  if (nivel == 1) { *mt = "FT_FIL_COMECAR"; return; }
  if (dom < 0) { *mt = "FT_FIL_RECOMENDADOS"; return; }
  *mt = T[dom];
  if (dom == FMO_CABE_HOJE && ctx->fimSemana) *mt = "FT_FIL_FIMSEMANA";
  if (fc_modelo_args(*mt) != 1) return;
  // Argumento do titulo: a fonte mais frequente entre os motivos do tipo
  // dominante (empate -> o primeiro). TERMINOU => a ref da semente;
  // PESSOA/TEMA/GENERO => "p<"/"k<"/"g<" id. O indice do motivo
  // renderizado e o de midx[], nao sempre o motivo[0].
  {
    int bestC = 0;
    for (i = 0; i < n; i++) {
      int32_t a;
      int f = 0;
      const char *ref;
      if (tipo[i] != dom) continue;
      a = out[idx[i]].motivo[midx[i]].a;
      ref = NULL;
      if (dom == FMO_TERMINOU) {
        if (a < 0 || a >= ctx->nSementes || !ctx->sementeRef) continue;
        ref = ctx->sementeRef[a];
        if (!ref || !fc_ref_ok(ref, ctx->nCand)) continue;
      }
      for (j = 0; j < n; j++)
        if (tipo[j] == dom && out[idx[j]].motivo[midx[j]].a == a) f++;
      if (f > bestC) {
        bestC = f;
        if (dom == FMO_TERMINOU)
          snprintf(arg, argCap, "%s", ref);
        else
          snprintf(arg, argCap, "%s%ld",
                   dom == FMO_PESSOA ? "p" :
                   dom == FMO_TEMA ? "k" : "g", (long)a);
      }
    }
    if (!arg[0]) *mt = "FT_FIL_RECOMENDADOS";   // sem arg -> titulo simples
  }
}

// Serializa {"t":"fileira",...} em dst (compacto). idx[] sao posicoes do
// vetor `out` do motor. Devolve o comprimento ou -1 se nao coube.
static int fc_fileira(char *dst, size_t cap, const FarolCenaCtx *ctx,
                      const FarolRecItem *out, const int *idx, int n,
                      int nivel, const char *id,
                      const char *mtForca, const char *argForca) {
  Fc b = { dst, dst, dst + cap - 1 };
  FcMot mot[FAROL_ITENS_MAX];
  int tipo[FAROL_ITENS_MAX], midx[FAROL_ITENS_MAX];
  int i, nMot = 0;
  const char *mt;
  const char *args[1];
  char arg[FAROL_ARG_TAM];

  for (i = 0; i < n; i++) {
    const FarolRecItem *it = &out[idx[i]];
    uint16_t sd = ctx->salvoDia ? ctx->salvoDia[it->cand] : 0;
    midx[i] = fc_motivo1(ctx, &ctx->cand[it->cand], sd, it, &mot[i]);
    tipo[i] = midx[i] < 0 ? -1 : (int)it->motivo[midx[i]].tipo;
    if (midx[i] >= 0) nMot++;
  }
  fc_titulo(ctx, out, idx, tipo, midx, n, nivel, mtForca, argForca,
            &mt, arg, sizeof arg);

  fc_raw(&b, "{\"t\":\"fileira\",\"id\":");
  fc_str(&b, id);
  fc_raw(&b, ",\"titulo\":");
  args[0] = arg;
  fc_texto(&b, mt, args, arg[0] ? 1 : 0);
  fc_raw(&b, ",\"forma\":\"cartaz\",\"itens\":[");
  for (i = 0; i < n; i++) {
    char r[16];
    if (i) fc_ch(&b, ',');
    snprintf(r, sizeof r, "c%ld", (long)out[idx[i]].cand);
    fc_str(&b, r);
  }
  fc_ch(&b, ']');
  if (nMot) {
    int j = 0;
    fc_raw(&b, ",\"motivo\":{");
    for (i = 0; i < n; i++) {
      char r[16];
      const char *a2[2] = { mot[i].a[0], mot[i].a[1] };
      if (tipo[i] < 0) continue;
      if (j++) fc_ch(&b, ',');
      snprintf(r, sizeof r, "c%ld", (long)out[idx[i]].cand);
      fc_str(&b, r);
      fc_ch(&b, ':');
      fc_texto(&b, mot[i].m, a2, mot[i].na);
    }
    fc_ch(&b, '}');
  }
  fc_ch(&b, '}');
  if (b.p == b.fim) return -1;
  *b.p = 0;
  return (int)(b.p - dst);
}

// Fileira 2 da 5.2: o grupo com a MAIOR afinidade de tema entre os
// escolhidos que ficaram de fora da fileira 1 (o sem-LLM da tabela:
// "fileira 1 = melhores; fileira 2 = maior afinidade de tema"). Cada
// tema tem de sustentar explicacao (n >= 2 e peso > 0,5 — a mesma regra
// de verdade dos motivos); eles concorrem pelo peso projetado, com o
// menor id desempatando, e so forma fileira quem junta >= 6 itens.
static int fc_tema_f2(const FarolCenaCtx *ctx, const FarolRecItem *out,
                      int ini, int n, int32_t *temaOut) {
  int32_t ids[96];
  int32_t pesos[96];
  int nt = 0, i, j, k;
  for (i = ini; i < n; i++) {
    const FarolFicha *f = ctx->cand[out[i].cand].ficha;
    if (!f) continue;
    for (j = 0; j < FAROL_FICHA_TEMA && f->tema[j]; j++) {
      int32_t t = f->tema[j];
      for (k = 0; k < nt && ids[k] != t; k++)
        ;
      if (k < nt) continue;
      if (nt >= 96) break;
      if (!fc_afin_prova(ctx->g, FG_DIM_TEMA, t, ctx->dia)) continue;
      ids[nt] = t;
      pesos[nt] = farolgosto_peso(ctx->g, FG_DIM_TEMA, t, ctx->dia);
      nt++;
    }
  }
  // ordena por peso desc, id asc — insercao, nt e pequeno e a ordem e
  // total (deterministica por construcao, nao pela semente).
  for (i = 1; i < nt; i++) {
    int32_t ti = ids[i], pi = pesos[i];
    for (j = i; j > 0 && (pesos[j - 1] < pi ||
         (pesos[j - 1] == pi && ids[j - 1] > ti)); j--) {
      pesos[j] = pesos[j - 1];
      ids[j] = ids[j - 1];
    }
    pesos[j] = pi;
    ids[j] = ti;
  }
  for (k = 0; k < nt; k++) {
    int conta = 0;
    for (i = ini; i < n; i++) {
      const FarolFicha *f = ctx->cand[out[i].cand].ficha;
      if (f)
        for (j = 0; j < FAROL_FICHA_TEMA && f->tema[j]; j++)
          if (f->tema[j] == ids[k]) { conta++; break; }
    }
    if (conta >= 6) { *temaOut = ids[k]; return 1; }
  }
  return 0;
}

// -------------------------------------------------------------- dica
static const char *const FC_DICA_L1[FC_DICA_N] = {
  "FT_DICA_PAROU_L1", "FT_DICA_FALTA_L1", "FT_DICA_ESTREIA",
  "FT_DICA_TEMPORADA", "FT_DICA_SALVO_L1", "FT_DICA_REVER",
  "FT_DICA_CURTO", "FT_DICA_PERGUNTAS", "FT_DICA_TRAKT"
};
static const char *const FC_DICA_L2[FC_DICA_N] = {
  "FT_DICA_PAROU_L2", "FT_DICA_FALTA_L2", 0, 0,
  "FT_DICA_SALVO_L2", 0, 0, 0, 0
};
static const char *const FC_ACAO_NOME[] = {
  "abrir", "tocar", "comparar", "escolha", "salvar", "nada"
};

// A dica vale? id no charset, acao no enum, alvo conforme a acao pede
// (abrir/tocar/comparar/salvar exigem ref; escolha/nada proibem),
// argumentos na aridade exata dos modelos e nivel de frio respeitado
// (frio 0: so PERGUNTAS e TRAKT — a porta e o assistente, 3.4).
static int fc_dica_ok(const FarolCenaCtx *ctx, const FarolDicaFato *d,
                      int nivel) {
  int i, precisa;
  if (d->tipo < 0 || d->tipo >= FC_DICA_N) return 0;
  if (nivel == 0 && d->tipo != FC_DICA_PERGUNTAS &&
      d->tipo != FC_DICA_TRAKT) return 0;
  if (!fc_id_ok(d->id, FAROL_ID_TAM - 1)) return 0;
  if (d->acao < 0 || d->acao > FP_ACAO_NADA) return 0;
  precisa = d->acao == FP_ACAO_ABRIR || d->acao == FP_ACAO_TOCAR ||
            d->acao == FP_ACAO_COMPARAR || d->acao == FP_ACAO_SALVAR;
  if (precisa) {
    if (!fc_ref_ok(d->alvo, ctx->nCand)) return 0;
  } else if (d->alvo && d->alvo[0]) return 0;
  if (d->l1na != fc_modelo_args(FC_DICA_L1[d->tipo])) return 0;
  for (i = 0; i < d->l1na; i++)
    if (!fc_arg_ok(d->l1a[i])) return 0;
  if (d->l2na > 0) {
    if (!FC_DICA_L2[d->tipo] ||
        d->l2na != fc_modelo_args(FC_DICA_L2[d->tipo])) return 0;
    for (i = 0; i < d->l2na; i++)
      if (!fc_arg_ok(d->l2a[i])) return 0;
  }
  return 1;
}

static int fc_dica(char *dst, size_t cap, const FarolDicaFato *d) {
  Fc b = { dst, dst, dst + cap - 1 };
  fc_raw(&b, "{\"t\":\"dica\",\"id\":");
  fc_str(&b, d->id);
  fc_raw(&b, ",\"l1\":");
  fc_texto(&b, FC_DICA_L1[d->tipo], d->l1a, d->l1na);
  if (d->l2na > 0) {
    fc_raw(&b, ",\"l2\":");
    fc_texto(&b, FC_DICA_L2[d->tipo], d->l2a, d->l2na);
  }
  fc_raw(&b, ",\"acao\":{\"tipo\":");
  fc_str(&b, FC_ACAO_NOME[d->acao]);
  if (d->acao != FP_ACAO_NADA && d->acao != FP_ACAO_ESCOLHA) {
    fc_raw(&b, ",\"alvo\":");
    fc_str(&b, d->alvo);
  }
  fc_raw(&b, "},\"icone\":\"aj_farol\"}");
  if (b.p == b.fim) return -1;
  *b.p = 0;
  return (int)(b.p - dst);
}

// ---------------------------------------------------------------- home
int farolcena_home(char *dst, size_t cap, const FarolCenaCtx *ctx,
                   const FarolDicaFato *dicas, int nDicas) {
  FarolRecItem out[24];
  int idx[24];
  char pc[4096];
  Fc b;
  int nivel, n = 0, n1 = 0, i, pos = 0, nPec = 0;

  if (!dst || !ctx || cap < 96) return -1;
  if (cap > FAROL_CENA_MAX + 1) cap = FAROL_CENA_MAX + 1;
  b.ini = dst; b.p = dst; b.fim = dst + cap - 1;

  nivel = ctx->g ? farolrec_nivel(ctx->g) : 0;
  fc_env(&b, ctx, "home");

  if (nivel > 0 && ctx->cand && ctx->nCand > 0) {
    FarolRecCtx rc;
    memset(&rc, 0, sizeof rc);
    rc.g = ctx->g;
    rc.sementes = ctx->sementes;
    rc.nSementes = ctx->nSementes;
    rc.dia = ctx->dia;
    rc.faixaHora = ctx->faixaHora;
    rc.naHome = 1;
    rc.semente = ctx->semente;
    n = farolrec_escolher(&rc, ctx->cand, ctx->nCand, out, 24);
    n1 = n < FAROL_ITENS_MAX ? n : FAROL_ITENS_MAX;
    for (i = 0; i < n1; i++) {
      idx[i] = i;
      if (out[i].pontos > 0) pos++;
    }
    // "Nao aparece": menos de 6 candidatos com pontos > 0 (4.2).
    if (n1 >= 6 && pos >= 6) {
      int f1 = 0;
      int l = fc_fileira(pc, sizeof pc, ctx, out, idx, n1, nivel,
                         "gosto1", NULL, NULL);
      if (l > 0 && (size_t)(b.fim - b.p) >= (size_t)l) {
        if (nPec) fc_ch(&b, ',');
        fc_raw(&b, pc);
        nPec++;
        f1 = 1;
      }
      // Fileira 2: o grupo de maior afinidade de tema na segunda pagina.
      // So existe atras de uma gosto1 emitida — com cap apertado a
      // segunda nunca sai sozinha (ressalva 4b da r1).
      if (f1 && nivel >= 2 && n > n1) {
        int32_t tema = 0;
        if (fc_tema_f2(ctx, out, n1, n, &tema)) {
          int m = 0;
          char arg2[FAROL_ARG_TAM];
          for (i = n1; i < n && m < FAROL_ITENS_MAX; i++)
            if (fc_tema_em(ctx->cand[out[i].cand].ficha, tema))
              idx[m++] = i;
          if (m >= 6) {
            snprintf(arg2, sizeof arg2, "k%ld", (long)tema);
            l = fc_fileira(pc, sizeof pc, ctx, out, idx, m, nivel,
                           "gosto2", "FT_FIL_TEMA", arg2);
            if (l > 0 && (size_t)(b.fim - b.p) >= (size_t)l) {
              if (nPec) fc_ch(&b, ',');
              fc_raw(&b, pc);
              nPec++;
            }
          }
        }
      }
    }
  }

  // Uma dica: a primeira valida na ordem de tipos (prioridade = o enum,
  // continuidade primeiro, generica por ultimo).
  if (dicas) {
    int t, melhor = -1;
    for (t = 0; t < FC_DICA_N && melhor < 0; t++)
      for (i = 0; i < nDicas; i++)
        if (dicas[i].tipo == t && fc_dica_ok(ctx, &dicas[i], nivel)) {
          melhor = i;
          break;
        }
    if (melhor >= 0 && fc_dica(pc, sizeof pc, &dicas[melhor]) > 0 &&
        (size_t)(b.fim - b.p) >= strlen(pc)) {
      if (nPec) fc_ch(&b, ',');
      fc_raw(&b, pc);
      nPec++;
    }
  }

  fc_raw(&b, "]}");
  if (b.p == b.fim) return -1;
  *b.p = 0;
  return (int)(b.p - b.ini);
}

// ------------------------------------------------------------- detalhe
int farolcena_detalhe(char *dst, size_t cap, const FarolCenaCtx *ctx,
                      const FarolCenaDet *det) {
  FarolInfoLinha li[10];
  char pc[4096];
  Fc b;
  int nivel, nPec = 0, nLi = 0, i;

  if (!dst || !ctx || !det || cap < 96) return -1;
  if (cap > FAROL_CENA_MAX + 1) cap = FAROL_CENA_MAX + 1;
  b.ini = dst; b.p = dst; b.fim = dst + cap - 1;
  nivel = ctx->g ? farolrec_nivel(ctx->g) : 0;
  fc_env(&b, ctx, "detalhe");

  // P3 "porque": frio >= 1, titulo nao visto e referencia valida. Os
  // motivos sao os do motor sobre o alvo como candidato unico — as
  // mesmas regras de verdade da 3.5, sem excecao no detalhe.
  if (nivel >= 1 && det->alvoRef &&
      !(det->alvo.flags & FRC_FL_VISTO) &&
      fc_ref_ok(det->alvoRef, ctx->nCand)) {
    FarolRecCtx rc;
    FarolRecItem it;
    FcMot mt[FAROL_LINHAS_MAX];
    FarolSelo selos[FAROL_SELA_MAX];
    int nL = 0, nSel = 0, conf;
    memset(&rc, 0, sizeof rc);
    rc.g = ctx->g;
    rc.sementes = ctx->sementes;
    rc.nSementes = ctx->nSementes;
    rc.dia = ctx->dia;
    rc.faixaHora = ctx->faixaHora;
    rc.semente = ctx->semente;
    if (farolrec_escolher(&rc, &det->alvo, 1, &it, 1) == 1)
      for (i = 0; i < it.nMotivo && nL < FAROL_LINHAS_MAX; i++) {
        const char *af;
        int k;
        if (!fc_motivo(ctx, &det->alvo, det->salvoDia, &it.motivo[i],
                       &mt[nL]))
          continue;
        // A mesma afinidade nao se repete em duas linhas do porque —
        // verdadeira mas redundante na tela (ressalva 4c da r1).
        af = fc_argafin(&mt[nL]);
        if (af) {
          for (k = 0; k < nL; k++)
            if (fc_argafin(&mt[k]) && !strcmp(fc_argafin(&mt[k]), af))
              break;
          if (k < nL) continue;
        }
        nL++;
      }
    // Sinal contrario forte (4.2): o habito de duracao do perfil pesa
    // CONTRA a duracao deste titulo (peso projetado < -0,5) — a P3
    // honesta avisa quando ainda ha o que dizer a favor.
    if (nL > 0 && nL < FAROL_LINHAS_MAX && det->alvo.ficha &&
        det->alvo.ficha->minutos > 0) {
      int fd = fc_durFaixa(det->alvo.ficha->minutos);
      if (fd >= 0) {
        int32_t id = fd + 1 + (det->alvo.ficha->tipo == FF_SERIE ? 5 : 0);
        if (farolgosto_peso(ctx->g, FG_DIM_DURACAO, id, ctx->dia) < -128) {
          mt[nL].m = "FT_MOT_CONTRA";
          mt[nL].na = 0;
          nL++;
        }
      }
    }
    // Selos: as afinidades comprovadas da ficha (3.5), pessoa/tema/genero.
    if (det->alvo.ficha) {
      static const int DIM[3] = { FG_DIM_PESSOA, FG_DIM_TEMA,
                                  FG_DIM_GENERO };
      static const int KND[3] = { FP_SELO_PESSOA, FP_SELO_TEMA,
                                  FP_SELO_GENERO };
      static const int CAP2[3] = { FAROL_FICHA_PESSOA, FAROL_FICHA_TEMA,
                                   FAROL_FICHA_GEN };
      int d2, j;
      for (d2 = 0; d2 < 3 && nSel < FAROL_SELA_MAX; d2++) {
        const int32_t *ids = d2 == 0 ? det->alvo.ficha->pessoa :
                             d2 == 1 ? det->alvo.ficha->tema :
                                       det->alvo.ficha->genero;
        for (j = 0; j < CAP2[d2] && ids[j] && nSel < FAROL_SELA_MAX; j++)
          if (fc_afin_prova(ctx->g, DIM[d2], ids[j], ctx->dia)) {
            selos[nSel].k = (uint8_t)KND[d2];
            selos[nSel].id = ids[j];
            nSel++;
          }
      }
    }
    conf = nL + (nSel > 0);
    if (conf > 3) conf = 3;
    // confianca 0 -> nao aparece (4.2); linhas e obrigatorio no schema,
    // entao peca sem linha renderizavel tambem nao sai (mesmo com selo).
    if (conf > 0 && nL > 0) {
      Fc b2 = { pc, pc, pc + sizeof pc - 1 };
      const char *a2[2];
      fc_raw(&b2, "{\"t\":\"porque\",\"alvo\":");
      fc_str(&b2, det->alvoRef);
      fc_raw(&b2, ",\"confianca\":");
      fc_i64(&b2, conf);
      fc_raw(&b2, ",\"linhas\":[");
      for (i = 0; i < nL; i++) {
        if (i) fc_ch(&b2, ',');
        a2[0] = mt[i].a[0];
        a2[1] = mt[i].a[1];
        fc_texto(&b2, mt[i].m, a2, mt[i].na);
      }
      fc_ch(&b2, ']');
      if (nSel) {
        static const char *const SN[] = { "pessoa", "tema", "genero" };
        fc_raw(&b2, ",\"selos\":[");
        for (i = 0; i < nSel; i++) {
          if (i) fc_ch(&b2, ',');
          fc_raw(&b2, "{\"k\":");
          fc_str(&b2, SN[selos[i].k]);
          fc_raw(&b2, ",\"id\":");
          fc_i64(&b2, selos[i].id);
          fc_ch(&b2, '}');
        }
        fc_ch(&b2, ']');
      }
      fc_ch(&b2, '}');
      if (b2.p < b2.fim && (size_t)(b.fim - b.p) >= (size_t)(b2.p - pc)) {
        *b2.p = 0;
        fc_raw(&b, pc);
        nPec++;
      }
    }
  }

  // P6 "info": as linhas da 47 por ordem de utilidade (a ordem do enum),
  // deduplicadas por k (o schema nao repete k), so as que tem modelo na
  // 35 com aridade certa — linha sem modelo e FALTA_TEXTO, nao entra.
  if (det->alvoRef && fc_ref_ok(det->alvoRef, ctx->nCand)) {
    uint32_t vistos = 0;
    nLi += farolinfo_qualidade(li + nLi, 2, det->fontes, det->nFontes,
                               &det->caps);
    nLi += farolinfo_continuar(li + nLi, det->temps, det->nTemps,
                               det->faltam, det->minEp);
    nLi += farolinfo_tempo(li + nLi, det->agora, det->h12, det->minutos,
                           det->minNoite);
    nLi += farolinfo_onde(li + nLi, det->onde);
    nLi += farolinfo_amigos(li + nLi, det->amigos, det->nAmigos);
    nLi += farolinfo_saga(li + nLi, det->sagaPos, det->sagaTotal,
                          det->sagaVistos);
    nLi += farolinfo_rever(li + nLi, det->reverAno, det->reverMes,
                           det->reverNota10);
    nLi += farolinfo_retomada(li + nLi, det->retT, det->retE,
                              (long long)ctx->feita, det->retQuando);
    {
      Fc b2 = { pc, pc, pc + sizeof pc - 1 };
      int nIt = 0, j;
      fc_raw(&b2, "{\"t\":\"info\",\"alvo\":");
      fc_str(&b2, det->alvoRef);
      fc_raw(&b2, ",\"itens\":[");
      for (i = 0; i < nLi && nIt < FAROL_INFO_MAX; i++) {
        int ar = fc_modelo_args(li[i].m);
        const char *a2[FI_ARGS];
        if (ar < 0 || ar != li[i].na) continue;        // FALTA_TEXTO
        if (vistos & (1u << li[i].k)) continue;        // k nao repete
        for (j = 0; j < li[i].na; j++)
          if (!fc_arg_ok(li[i].a[j])) break;
        if (j < li[i].na) continue;
        vistos |= 1u << li[i].k;
        if (nIt++) fc_ch(&b2, ',');
        fc_raw(&b2, "{\"k\":");
        fc_str(&b2, farolinfo_k_nome(li[i].k));
        fc_raw(&b2, ",\"l\":");
        for (j = 0; j < li[i].na; j++) a2[j] = li[i].a[j];
        fc_texto(&b2, li[i].m, a2, li[i].na);
        fc_ch(&b2, '}');
      }
      fc_raw(&b2, "]}");
      if (nIt > 0 && b2.p < b2.fim &&
          (size_t)(b.fim - b.p) >= (size_t)(b2.p - pc)) {
        *b2.p = 0;
        if (nPec) fc_ch(&b, ',');
        fc_raw(&b, pc);
        nPec++;
      }
    }
  }

  fc_raw(&b, "]}");
  if (b.p == b.fim) return -1;
  *b.p = 0;
  return (int)(b.p - b.ini);
}

// ------------------------------------------------------------ comparar
int farolcena_comparar(char *dst, size_t cap, const FarolCenaCtx *ctx,
                       const FarolCenaCmp *cmp) {
  static const char *const EN[] = {
    "duracao", "nota", "clima", "porque", "qualidade", "termina",
    "onde", "amigos"
  };
  char pc[2048];
  Fc b, b2;
  int i, esc = -1, best = -1;
  uint32_t vistos = 0;

  if (!dst || !ctx || !cmp || cap < 96) return -1;
  if (cmp->nItens < 2 || cmp->nItens > 3 || cmp->nEixos < 1 ||
      cmp->nEixos > FAROL_EIXO_MAX) return -1;
  for (i = 0; i < cmp->nItens; i++) {
    int j;
    if (!fc_ref_ok(cmp->itens[i], ctx->nCand)) return -1;
    for (j = 0; j < i; j++)
      if (!strcmp(cmp->itens[i], cmp->itens[j])) return -1;
  }
  for (i = 0; i < cmp->nEixos; i++) {
    if (cmp->eixos[i] > FP_EIXO_AMIGOS) return -1;
    if (vistos & (1u << cmp->eixos[i])) return -1;
    vistos |= 1u << cmp->eixos[i];
  }
  if (cap > FAROL_CENA_MAX + 1) cap = FAROL_CENA_MAX + 1;
  b.ini = dst; b.p = dst; b.fim = dst + cap - 1;
  fc_env(&b, ctx, "comparar");

  // A coluna de maior pontuacao que cabe no tempo (5.2). Sem nenhuma
  // cabendo — ou sem dado de tempo — escolha = a de maior pontuacao e
  // o veredito nao sai (ele afirmaria o que o dado nao prova).
  if (cmp->tempoMin > 0)
    for (i = 0; i < cmp->nItens; i++)
      if (cmp->minutos[i] > 0 && cmp->minutos[i] <= cmp->tempoMin &&
          (best < 0 || cmp->pontos[i] > cmp->pontos[best]))
        best = i;
  esc = best;
  if (esc < 0)
    for (i = 0; i < cmp->nItens; i++)
      if (esc < 0 || cmp->pontos[i] > cmp->pontos[esc]) esc = i;

  b2.ini = pc; b2.p = pc; b2.fim = pc + sizeof pc - 1;
  fc_raw(&b2, "{\"t\":\"comparar\",\"itens\":[");
  for (i = 0; i < cmp->nItens; i++) {
    if (i) fc_ch(&b2, ',');
    fc_str(&b2, cmp->itens[i]);
  }
  fc_raw(&b2, "],\"eixos\":[");
  for (i = 0; i < cmp->nEixos; i++) {
    if (i) fc_ch(&b2, ',');
    fc_str(&b2, EN[cmp->eixos[i]]);
  }
  fc_ch(&b2, ']');
  if (best >= 0) {
    const char *a2[1];
    fc_raw(&b2, ",\"veredito\":");
    a2[0] = cmp->itens[best];
    fc_texto(&b2, "FT_CMP_VEREDITO_CABE", a2, 1);
  }
  if (esc >= 0) {
    fc_raw(&b2, ",\"escolha\":");
    fc_str(&b2, cmp->itens[esc]);
  }
  fc_ch(&b2, '}');
  if (b2.p < b2.fim && (size_t)(b.fim - b.p) >= (size_t)(b2.p - pc)) {
    *b2.p = 0;
    fc_raw(&b, pc);
  }

  fc_raw(&b, "]}");
  if (b.p == b.fim) return -1;
  *b.p = 0;
  return (int)(b.p - b.ini);
}
