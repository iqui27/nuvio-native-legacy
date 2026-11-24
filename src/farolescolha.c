// Assistente de escolha P5 sem LLM (issue 50). Contrato em farolescolha.h.
// Puro: sem SDL, sem rede, sem dados.h, sem malloc, sem log, sem estado
// estatico mutavel (as tabelas abaixo sao so leitura).
#include "farolescolha.h"
#include "faroltexto.h"

#include <stddef.h>
#include <string.h>

// ------------------------------------------------------------------ banco --
typedef struct {
  const char *id;
  const char *rotulo;
  int         nOpc;                       // com "Tanto faz" (sempre a ultima)
  const char *opcId[FESC_OPC_MAX];
  const char *opcRot[FESC_OPC_MAX];
} Pergunta;

static const Pergunta BANCO[FESC_N] = {
  { "tempo", FT_ESC_TEMPO, 4,
    { "t45", "t100", "t999", "tanto" },
    { FT_ESC_TEMPO_45, FT_ESC_TEMPO_FILME, FT_ESC_TEMPO_NOITE, FT_ESC_TANTO_FAZ } },
  { "peso", FT_ESC_PESO, 3,
    { "leve", "intenso", "tanto", NULL },
    { FT_ESC_PESO_LEVE, FT_ESC_PESO_INTENSO, FT_ESC_TANTO_FAZ, NULL } },
  { "companhia", FT_ESC_COMPANHIA, 4,
    { "so", "dois", "crianca", "tanto" },
    { FT_ESC_SO_EU, FT_ESC_A_DOIS, FT_ESC_CRIANCA, FT_ESC_TANTO_FAZ } },
  { "novidade", FT_ESC_NOVIDADE, 3,
    { "novo", "conhecido", "tanto", NULL },
    { FT_ESC_NOVO, FT_ESC_CONHECIDO, FT_ESC_TANTO_FAZ, NULL } },
  { "formato", FT_ESC_FORMATO, 3,
    { "filme", "serie", "tanto", NULL },
    { FT_ESC_FILME, FT_ESC_SERIE, FT_ESC_TANTO_FAZ, NULL } },
  { "idioma", FT_ESC_IDIOMA, 3,
    { "dublado", "legendado", "tanto", NULL },
    { FT_ESC_DUBLADO, FT_ESC_LEGENDADO, FT_ESC_TANTO_FAZ, NULL } },
  { "epoca", FT_ESC_EPOCA, 3,
    { "nova", "antiga", "tanto", NULL },
    { FT_ESC_EPOCA_NOVA, FT_ESC_EPOCA_ANTIGA, FT_ESC_TANTO_FAZ, NULL } },
  { "humor", FT_ESC_HUMOR, 4,
    { "rir", "emocionar", "tensao", "tanto" },
    { FT_ESC_HUMOR_RIR, FT_ESC_HUMOR_EMOCIONAR, FT_ESC_HUMOR_TENSAO, FT_ESC_TANTO_FAZ } },
  { "continuar", FT_ESC_CONTINUAR, 3,
    { "sim", "comecar", "tanto", NULL },
    { FT_ESC_CONT_SIM, FT_ESC_COMECAR, FT_ESC_TANTO_FAZ, NULL } },
};

static int pergValida(int p) { return p >= 0 && p < FESC_N; }

int farolescolha_n_opcoes_banco(int perg) { return pergValida(perg) ? BANCO[perg].nOpc : 0; }
int farolescolha_tanto(int perg) { return pergValida(perg) ? BANCO[perg].nOpc - 1 : -1; }

const char *farolescolha_pergunta_id(int perg) { return pergValida(perg) ? BANCO[perg].id : ""; }
const char *farolescolha_pergunta_rotulo(int perg) {
  return pergValida(perg) ? BANCO[perg].rotulo : "";
}
const char *farolescolha_opcao_id(int perg, int o) {
  if (!pergValida(perg) || o < 0 || o >= BANCO[perg].nOpc) return "";
  return BANCO[perg].opcId[o];
}
const char *farolescolha_opcao_rotulo(int perg, int o) {
  if (!pergValida(perg) || o < 0 || o >= BANCO[perg].nOpc) return "";
  return BANCO[perg].opcRot[o];
}

int farolescolha_pergunta_por_id(const char *id) {
  int p;
  if (!id) return -1;
  for (p = 0; p < FESC_N; p++) if (!strcmp(BANCO[p].id, id)) return p;
  return -1;
}

int farolescolha_opcao_por_id(int perg, const char *id) {
  int o;
  if (!pergValida(perg) || !id) return -1;
  for (o = 0; o < BANCO[perg].nOpc; o++) if (!strcmp(BANCO[perg].opcId[o], id)) return o;
  return -1;
}

int farolescolha_tempo_min(int opcao) {
  if (opcao == FESC_T45) return 45;
  if (opcao == FESC_T100) return 100;
  if (opcao == FESC_T999) return 999;
  return 0;
}

// ------------------------------------------------------------- generos --
// Radicais em minusculas (PT e EN; os bytes UTF-8 ficam como estao). Casam no
// COMECO de uma palavra do genero ("Comédia" -> "coméd", "Romantic" ->
// "romant"), nunca no meio ("Warner" nao e "war": so o comeco conta, e o
// genero nao traz nome de estudio).
static const char *const R_RIR[] = { "comed", "coméd", "comic", "cômic", "sitcom",
  "anima", "famil", "famíl", "infant", "kids", "crian", NULL };
static const char *const R_EMOC[] = { "drama", "romanc", "romant", "român", "music",
  "músic", "biogra", NULL };
static const char *const R_TENS[] = { "suspen", "thrill", "terror", "horror", "crime",
  "myster", "mistér", "polici", NULL };
static const char *const R_LEVE[] = { "comed", "coméd", "comic", "cômic", "sitcom",
  "anima", "famil", "famíl", "infant", "kids", "crian", "aventur", "advent", "fantas",
  "music", "músic", "romanc", "romant", "român", NULL };
static const char *const R_INTENSO[] = { "suspen", "thrill", "terror", "horror", "crime",
  "myster", "mistér", "polici", "drama", "guerra", "action", "ação", "acao",
  "faroeste", "western", NULL };
// Palavras INTEIRAS (sem radical): "war" nao pode casar "Warner" nem
// "Warcraft" — a fronteira tem de valer nos dois lados.
static const char *const R_INTENSO_PALAVRA[] = { "war", NULL };
static const char *const R_CRIANCA[] = { "anima", "famil", "famíl", "infant", "kids",
  "crian", NULL };
static const char *const R_ASSUSTA[] = { "terror", "horror", NULL };
static const char *const R_DOIS[] = { "romanc", "romant", "român", "comed", "coméd",
  "drama", "suspen", "thrill", NULL };

static int letra(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
}

// 1 se algum radical de `rad` comeca uma palavra de `g` (ja em minusculas).
static int temRadical(const char *g, const char *const *rad) {
  int r;
  for (r = 0; rad[r]; r++) {
    size_t n = strlen(rad[r]);
    const char *p = g;
    while ((p = strstr(p, rad[r])) != NULL) {
      if (p == g || !letra((unsigned char)p[-1])) return 1;
      p += n;
    }
  }
  return 0;
}

// 1 se alguma palavra de `pal` aparece INTEIRA em `g`: fronteira de palavra
// (inicio/fim ou nao-letra) dos dois lados do casamento.
static int temPalavra(const char *g, const char *const *pal) {
  int r;
  for (r = 0; pal[r]; r++) {
    size_t n = strlen(pal[r]);
    const char *p = g;
    while ((p = strstr(p, pal[r])) != NULL) {
      if ((p == g || !letra((unsigned char)p[-1])) &&
          !letra((unsigned char)p[n])) return 1;
      p += n;
    }
  }
  return 0;
}

// Base ascii do latim-1 U+00C0..U+00FF (À→a … Ç→c … ñ→n … ÿ→y); 0 quando o
// ponto nao tem base simples (Æ Ð × Ø Þ ß æ ð ÷ ø þ). A mesma dobra de
// ft_base_latim em src/faroltexto.c.
static char baseLatim(unsigned cp) {
  static const char T[0x40] = {
    'a','a','a','a','a','a', 0 ,'c',  // À Á Â Ã Ä Å Æ Ç
    'e','e','e','e','i','i','i','i',  // È É Ê Ë Ì Í Î Ï
     0 ,'n','o','o','o','o','o', 0 ,  // Ð Ñ Ò Ó Ô Õ Ö ×
     0 ,'u','u','u','u','y', 0 , 0 ,  // Ø Ù Ú Û Ü Ý Þ ß
    'a','a','a','a','a','a', 0 ,'c',  // à á â ã ä å æ ç
    'e','e','e','e','i','i','i','i',  // è é ê ë ì í î ï
     0 ,'n','o','o','o','o','o', 0 ,  // ð ñ ò ó ô õ ö ÷
     0 ,'u','u','u','u','y', 0 ,'y',  // ø ù ú û ü ý þ ÿ
  };
  return (cp >= 0xC0 && cp <= 0xFF) ? T[cp - 0xC0] : 0;
}

// g dobrada para ascii-minuscula: ASCII A-Z baixa, e o latim-1 de 2 bytes
// (U+00C0..U+00FF) troca pela base ascii — "AÇÃO" casa "ação", "COMÉDIA"
// casa "comédia" (antes só a forma minuscula acentuada dobrava).
static void dobrarGenero(const char *s, char *g, size_t cap) {
  size_t i = 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p && i + 1 < cap) {
    unsigned char c = *p;
    if (c < 0x80) {
      g[i++] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
      p++;
      continue;
    }
    if (c == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) {
      char b = baseLatim(0xC0u + (p[1] - 0x80));
      p += 2;
      if (b) { g[i++] = b; continue; }
      // Sem base simples: seguem os dois bytes crus (continua "letra").
      if (i + 2 >= cap) break;
      g[i++] = (char)0xC3; g[i++] = (char)p[-1];
      continue;
    }
    // UTF-8 mais longo: copia o lead e os seguidores como estao (sequencia
    // quebrada: so o byte malformado, sem engolir o que vem depois).
    {
      int n = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
      int j;
      for (j = 1; j < n; j++)
        if (!(p[j] >= 0x80 && p[j] <= 0xBF)) { n = j; break; }
      for (j = 0; j < n && p[j] && i + 1 < cap; j++) g[i++] = (char)p[j];
      p += j;
    }
  }
  g[i] = '\0';
}

void farolescolha_cand_de(FarolEscCand *out, const FarolEscFatos *f) {
  char g[256];
  if (!out) return;
  memset(out, 0, sizeof *out);
  if (!f) return;
  out->id = f->id;
  out->pontos = f->pontos;
  g[0] = '\0';
  if (f->genero) dobrarGenero(f->genero, g, sizeof g);
  // Tempo aninhado: "quanto tempo voce tem" — o que cabe em 45 cabe no filme.
  if (f->minutos > 0) {
    if (f->minutos <= 45) out->m[FESC_TEMPO] |= 1u << FESC_T45;
    if (f->minutos <= 100) out->m[FESC_TEMPO] |= 1u << FESC_T100;
    out->m[FESC_TEMPO] |= 1u << FESC_T999;
  }
  if (g[0]) {
    if (temRadical(g, R_LEVE)) out->m[FESC_PESO] |= 1u << FESC_LEVE;
    if (temRadical(g, R_INTENSO) || temPalavra(g, R_INTENSO_PALAVRA))
      out->m[FESC_PESO] |= 1u << FESC_INTENSO;
    out->m[FESC_COMPANHIA] |= 1u << FESC_SO;
    if (temRadical(g, R_DOIS)) out->m[FESC_COMPANHIA] |= 1u << FESC_DOIS;
    if (temRadical(g, R_CRIANCA) && !temRadical(g, R_ASSUSTA))
      out->m[FESC_COMPANHIA] |= 1u << FESC_CRIANCA;
    if (temRadical(g, R_RIR)) out->m[FESC_HUMOR] |= 1u << FESC_RIR;
    if (temRadical(g, R_EMOC)) out->m[FESC_HUMOR] |= 1u << FESC_EMOCIONAR;
    if (temRadical(g, R_TENS)) out->m[FESC_HUMOR] |= 1u << FESC_TENSAO;
  }
  out->m[FESC_NOVIDADE] = (uint8_t)(1u << ((f->visto || f->andamento) ? FESC_CONHECIDO : FESC_NOVO));
  out->m[FESC_FORMATO] = (uint8_t)(1u << (f->serie ? FESC_SERIE : FESC_FILME));
  out->m[FESC_IDIOMA] = (uint8_t)(f->idiomas & ((1 << FESC_DUBLADO) | (1 << FESC_LEGENDADO)));
  if (f->ano >= 2015) out->m[FESC_EPOCA] = 1u << FESC_RECENTE;
  else if (f->ano > 0 && f->ano < 2000) out->m[FESC_EPOCA] = 1u << FESC_ANTIGA;
  out->m[FESC_CONTINUAR] = (uint8_t)(1u << (f->andamento ? FESC_CONT_SIM : FESC_COMECAR));
}

// ------------------------------------------------------------------ motor --
static int tardeDaNoite(const FarolEscCtx *c) {
  return c->hora >= 0 && c->hora <= 23 && (c->hora >= 23 || c->hora < 5);
}

static int atende(const FarolEscCand *c, int p, int o) {
  return (c->m[p] >> o) & 1u;
}

static void refazerVivos(FarolEsc *e) {
  int i, p;
  e->nVivo = 0;
  for (i = 0; i < e->nCand; i++) {
    int ok = 1;
    for (p = 0; p < FESC_N && ok; p++) {
      int r = e->resp[p];
      if (r == FESC_SEM_RESP || r == farolescolha_tanto(p)) continue;   // tanto nao filtra
      if (!atende(&e->cand[i], p, r)) ok = 0;
    }
    e->vivo[i] = (uint8_t)ok;
    e->nVivo += ok;
  }
}

int farolescolha_conta(const FarolEsc *e, int perg, int opcao) {
  int i, n = 0;
  if (!e || !pergValida(perg) || opcao < 0 || opcao >= BANCO[perg].nOpc) return 0;
  if (opcao == farolescolha_tanto(perg)) return e->nVivo;
  for (i = 0; i < e->nCand; i++) n += e->vivo[i] && atende(&e->cand[i], perg, opcao);
  return n;
}

// Opcoes de filtro que a pergunta oferece agora (so as que tem vivo) e o
// ganho (Gini: pares de vivos que a resposta separa). Devolve quantas de
// filtro (sem contar "Tanto faz").
static int oferecer(const FarolEsc *e, int p, int *opc, long long *ganho) {
  int o, k = 0, tanto = farolescolha_tanto(p);
  long long gsum = 0;
  for (o = 0; o < tanto; o++) {
    int c;
    if (p == FESC_TEMPO && o == FESC_T999 && tardeDaNoite(&e->ctx)) continue;
    c = farolescolha_conta(e, p, o);
    if (c <= 0) continue;
    if (opc) opc[k] = o;
    k++;
    gsum += (long long)c * (long long)(e->nVivo - c);
  }
  if (ganho) *ganho = gsum;
  return k;
}

static int pulada(const FarolEsc *e, int p) {
  return e->resp[p] != FESC_SEM_RESP;
}

static void terminar(FarolEsc *e) {
  e->perg = -1;
  e->nOpc = 0;
  e->de = e->nPilha;
}

// Escolhe a proxima pergunta (ou termina). Depende so de resp/nPilha/nPre e
// dos vivos — por isso voltar() reencontra exatamente a pergunta anterior.
static void escolher(FarolEsc *e) {
  int p, melhor = -1, eleg = 0, teto;
  long long melhorG = 0;
  teto = FESC_PERG_MAX - e->nPre;
  if (teto < 0) teto = 0;
  if (e->nVivo < 3 || e->nPilha >= teto) { terminar(e); return; }
  for (p = 0; p < FESC_N; p++) {
    long long g;
    if (pulada(e, p)) continue;
    if (oferecer(e, p, NULL, &g) < 1 || g <= 0) continue;
    eleg++;
    if (g > melhorG) { melhorG = g; melhor = p; }
  }
  if (melhor < 0) { terminar(e); return; }
  e->perg = melhor;
  e->nOpc = oferecer(e, melhor, e->opc, NULL);
  e->opc[e->nOpc++] = farolescolha_tanto(melhor);
  e->de = e->nPilha + (teto - e->nPilha < eleg ? teto - e->nPilha : eleg);
}

void farolescolha_iniciar(FarolEsc *e, const FarolEscCand *cand, int n,
                          const FarolEscCtx *ctx) {
  int p;
  if (!e) return;
  memset(e, 0, sizeof *e);
  e->cand = cand;
  e->nCand = cand ? (n < 0 ? 0 : (n > FESC_CAND_MAX ? FESC_CAND_MAX : n)) : 0;
  e->ctx.hora = -1;
  e->ctx.emAndamento = 1;
  if (ctx) e->ctx = *ctx;
  for (p = 0; p < FESC_N; p++) e->resp[p] = FESC_SEM_RESP;
  // O que o contexto responde sai do banco (sem filtrar: quem monta a lista
  // — montarCandidatos no farolescolhaui — ja barrou o que o perfil infantil
  // nao pode ver; e sem nada em andamento "continuar" nao divide).
  if (e->ctx.infantil) {
    e->resp[FESC_COMPANHIA] = (int8_t)farolescolha_tanto(FESC_COMPANHIA);
    e->orig[FESC_COMPANHIA] = FESC_ORIG_CONTEXTO;
  }
  if (!e->ctx.emAndamento) {
    e->resp[FESC_CONTINUAR] = (int8_t)farolescolha_tanto(FESC_CONTINUAR);
    e->orig[FESC_CONTINUAR] = FESC_ORIG_CONTEXTO;
  }
  refazerVivos(e);
  escolher(e);
}

int farolescolha_pre_responder(FarolEsc *e, int perg, const char *opcaoId) {
  int o;
  // nPre < FESC_PERG_MAX: o teto total de perguntas e 3, somando pre (#54)
  // e feitas. Nao barra por terminou(): a pre sobre motor ja encerrado e o
  // fluxo legitimo do #54 ("pre-resposta que nao bate" -> NADA honesto).
  if (!e || e->nPre >= FESC_PERG_MAX || e->nPilha > 0 ||
      !pergValida(perg) || e->resp[perg] != FESC_SEM_RESP) return 0;
  o = farolescolha_opcao_por_id(perg, opcaoId);
  if (o < 0) return 0;
  e->resp[perg] = (int8_t)o;
  e->orig[perg] = FESC_ORIG_PRE;
  e->nPre++;
  refazerVivos(e);
  escolher(e);
  return 1;
}

int farolescolha_responder(FarolEsc *e, int i) {
  int p;
  if (!e || e->perg < 0 || i < 0 || i >= e->nOpc || e->nPilha >= FESC_PERG_MAX) return 0;
  p = e->perg;
  e->resp[p] = (int8_t)e->opc[i];
  e->orig[p] = FESC_ORIG_PESSOA;
  e->pilha[e->nPilha++] = (int8_t)p;
  refazerVivos(e);
  escolher(e);
  return 1;
}

int farolescolha_voltar(FarolEsc *e) {
  int p;
  if (!e || e->nPilha <= 0) return 0;
  p = e->pilha[--e->nPilha];
  e->pilha[e->nPilha] = 0;
  e->resp[p] = FESC_SEM_RESP;
  e->orig[p] = FESC_ORIG_NADA;
  refazerVivos(e);
  escolher(e);
  return 1;
}

// ---------------------------------------------------------------- leitura --
int farolescolha_terminou(const FarolEsc *e) { return !e || e->perg < 0; }
int farolescolha_pergunta(const FarolEsc *e) { return e ? e->perg : -1; }
int farolescolha_n_opcoes(const FarolEsc *e) { return e && e->perg >= 0 ? e->nOpc : 0; }
int farolescolha_opcao(const FarolEsc *e, int i) {
  if (!e || e->perg < 0 || i < 0 || i >= e->nOpc) return -1;
  return e->opc[i];
}
int farolescolha_passo(const FarolEsc *e) { return e && e->perg >= 0 ? e->nPilha + 1 : 0; }
int farolescolha_de(const FarolEsc *e) { return e ? e->de : 0; }
int farolescolha_feitas(const FarolEsc *e) { return e ? e->nPilha : 0; }
int farolescolha_vivos(const FarolEsc *e) { return e ? e->nVivo : 0; }
int farolescolha_resposta(const FarolEsc *e, int perg) {
  return e && pergValida(perg) ? e->resp[perg] : FESC_SEM_RESP;
}
int farolescolha_origem(const FarolEsc *e, int perg) {
  return e && pergValida(perg) ? e->orig[perg] : FESC_ORIG_NADA;
}

void farolescolha_resultado(const FarolEsc *e, FarolEscResultado *r) {
  int i, k;
  if (!r) return;
  memset(r, 0, sizeof *r);
  if (!e || e->perg >= 0) { r->saida = FESC_SAIDA_PERGUNTA; return; }
  r->vivos = e->nVivo;
  {
    int rt = e->resp[FESC_TEMPO];
    if (rt != FESC_SEM_RESP && rt != farolescolha_tanto(FESC_TEMPO))
      r->tempoMin = farolescolha_tempo_min(rt);
  }
  // Os 3 de mais pontos (empate = posicao no vetor), por insercao.
  for (i = 0; i < e->nCand; i++) {
    int32_t pt;
    if (!e->vivo[i]) continue;
    pt = e->cand[i].pontos;
    k = r->n < FESC_RES_MAX ? r->n++ : FESC_RES_MAX;
    if (k == FESC_RES_MAX) {
      if (pt <= r->pontos[FESC_RES_MAX - 1]) continue;
      k = FESC_RES_MAX - 1;
    }
    while (k > 0 && r->pontos[k - 1] < pt) {
      r->id[k] = r->id[k - 1];
      r->pontos[k] = r->pontos[k - 1];
      k--;
    }
    r->id[k] = e->cand[i].id;
    r->pontos[k] = pt;
  }
  r->saida = r->n == 0 ? FESC_SAIDA_NADA : (r->n == 1 ? FESC_SAIDA_ACHADOS : FESC_SAIDA_COMPARAR);
}
