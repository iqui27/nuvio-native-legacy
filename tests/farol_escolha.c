// tests/farol_escolha.c — assistente de escolha P5 sem LLM (lote sense, issue 50).
//
// Parte 1 (motor puro, src/farolescolha.c; com -DFESC_SO_MOTOR liga SO o
// motor, sem SDL): teto de 3 perguntas, contexto (23h40, infantil, sem nada
// em andamento), parada com < 3, "tanto" nao filtra, voltar() exato,
// pre_responder() reduz o que falta, cand_de e 1.000 perfis/catalogos
// sinteticos aleatorios (zero becos, zero lacos, zero OOB sob ASan+UBSan).
// Parte 2 (app inteiro menos main.c): o Farol real (resolver.c) com o 5o item
// da coluna, D-pad (←/→/OK com key.repeat ignorado, Voltar 3→2→1→saida), P4
// com 2-3, cartaz com 1, "Nada exato…" com 0, e a captura PNG 1920x1080.
#include "farolescolha.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gFalhas;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: FALHA: %s\n", __FILE__, __LINE__, #c); \
  gFalhas++; exit(1); } } while (0)

// ------------------------------------------------------------ util motor --
static unsigned rngS = 0x1234567u;
static unsigned rng(void) {
  rngS ^= rngS << 13; rngS ^= rngS >> 17; rngS ^= rngS << 5;
  return rngS;
}

static FarolEscCand C[FESC_CAND_MAX];
static FarolEsc E;

static void candVazio(FarolEscCand *c, int id, int pontos) {
  memset(c, 0, sizeof *c);
  c->id = id;
  c->pontos = pontos;
}

// Catalogo "rico": todas as perguntas dividem.
static int catRico(int n) {
  int i, p;
  for (i = 0; i < n; i++) {
    candVazio(&C[i], i, 1000 - i);
    for (p = 0; p < FESC_N; p++) {
      int nf = farolescolha_tanto(p);
      C[i].m[p] = (uint8_t)(1u << ((i / (p + 1)) % nf));
    }
  }
  return n;
}

static void ctxNormal(FarolEscCtx *x) { x->hora = 20; x->infantil = 0; x->emAndamento = 1; }

static void conferirInvariantes(const FarolEsc *e) {
  int i, n;
  CHECK(farolescolha_feitas(e) <= FESC_PERG_MAX);
  CHECK(farolescolha_de(e) <= FESC_PERG_MAX);
  if (farolescolha_terminou(e)) {
    CHECK(farolescolha_n_opcoes(e) == 0 && farolescolha_passo(e) == 0);
    return;
  }
  n = farolescolha_n_opcoes(e);
  CHECK(n >= 2 && n <= FESC_OPC_MAX);
  CHECK(farolescolha_passo(e) >= 1 && farolescolha_passo(e) <= farolescolha_de(e));
  CHECK(farolescolha_vivos(e) >= 3);
  CHECK(farolescolha_opcao(e, n - 1) == farolescolha_tanto(farolescolha_pergunta(e)));
  CHECK(farolescolha_opcao(e, n) == -1 && farolescolha_opcao(e, -1) == -1);
  for (i = 0; i < n - 1; i++) {   // toda opcao de filtro oferecida tem vivo: sem beco
    int c = farolescolha_conta(e, farolescolha_pergunta(e), farolescolha_opcao(e, i));
    CHECK(c > 0 && c <= farolescolha_vivos(e));
  }
  CHECK(farolescolha_resposta(e, farolescolha_pergunta(e)) == FESC_SEM_RESP);
}

static void conferirResultado(const FarolEsc *e, int nCand) {
  FarolEscResultado r;
  int i;
  farolescolha_resultado(e, &r);
  CHECK(r.n >= 0 && r.n <= FESC_RES_MAX);
  CHECK(r.n == (farolescolha_vivos(e) < 3 ? farolescolha_vivos(e) : 3));
  CHECK(r.vivos == farolescolha_vivos(e));
  if (r.n == 0) CHECK(r.saida == FESC_SAIDA_NADA);
  else if (r.n == 1) CHECK(r.saida == FESC_SAIDA_ACHADOS);
  else CHECK(r.saida == FESC_SAIDA_COMPARAR);
  for (i = 0; i < r.n; i++) {
    CHECK(r.id[i] >= 0 && r.id[i] < nCand);
    if (i) CHECK(r.pontos[i - 1] >= r.pontos[i]);
  }
  CHECK(r.tempoMin == 0 || r.tempoMin == 45 || r.tempoMin == 100 || r.tempoMin == 999);
}

// ------------------------------------------------------------ provas motor --
static void provaBanco(void) {
  int p, o;
  for (p = 0; p < FESC_N; p++) {
    int n = farolescolha_n_opcoes_banco(p);
    CHECK(n >= 3 && n <= FESC_OPC_MAX);
    CHECK(!strcmp(farolescolha_opcao_id(p, n - 1), "tanto"));
    CHECK(farolescolha_pergunta_por_id(farolescolha_pergunta_id(p)) == p);
    CHECK(farolescolha_pergunta_rotulo(p)[0] != '\0');
    for (o = 0; o < n; o++) {
      CHECK(farolescolha_opcao_por_id(p, farolescolha_opcao_id(p, o)) == o);
      CHECK(farolescolha_opcao_rotulo(p, o)[0] != '\0');
    }
    CHECK(farolescolha_opcao_id(p, n)[0] == '\0');
  }
  CHECK(farolescolha_n_opcoes_banco(FESC_N) == 0 && farolescolha_n_opcoes_banco(-1) == 0);
  CHECK(farolescolha_pergunta_por_id("nada") == -1 && farolescolha_pergunta_por_id(NULL) == -1);
  CHECK(farolescolha_opcao_por_id(FESC_TEMPO, "t30") == -1);
  CHECK(!strcmp(farolescolha_opcao_id(FESC_TEMPO, 0), "t45"));
  CHECK(!strcmp(farolescolha_opcao_id(FESC_TEMPO, 1), "t100"));
  CHECK(!strcmp(farolescolha_opcao_id(FESC_TEMPO, 2), "t999"));
  puts("  banco: 9 perguntas, ids e rotulos FT_ESC_*, \"tanto\" sempre a ultima");
}

static void provaCandDe(void) {
  FarolEscFatos f;
  FarolEscCand c;
  memset(&f, 0, sizeof f);
  f.minutos = 40; f.ano = 2022; f.genero = "Filme · Comédia · Drama";
  farolescolha_cand_de(&c, &f);
  CHECK(c.m[FESC_TEMPO] == ((1u << FESC_T45) | (1u << FESC_T100) | (1u << FESC_T999)));
  CHECK(c.m[FESC_HUMOR] == ((1u << FESC_RIR) | (1u << FESC_EMOCIONAR)));
  CHECK(c.m[FESC_PESO] == ((1u << FESC_LEVE) | (1u << FESC_INTENSO)));
  CHECK(c.m[FESC_EPOCA] == (1u << FESC_RECENTE));
  CHECK(c.m[FESC_FORMATO] == (1u << FESC_FILME));
  CHECK(c.m[FESC_IDIOMA] == 0);                   // sem dado: nunca inventa
  f.minutos = 130; f.ano = 1994; f.serie = 1; f.andamento = 1;
  f.genero = "Warner · Terror · Animação";       // "Warner" nao e "war"
  farolescolha_cand_de(&c, &f);
  CHECK(c.m[FESC_TEMPO] == (1u << FESC_T999));
  CHECK(c.m[FESC_EPOCA] == (1u << FESC_ANTIGA));
  CHECK(!(c.m[FESC_COMPANHIA] & (1u << FESC_CRIANCA)));   // terror nao e com crianca
  CHECK(c.m[FESC_CONTINUAR] == (1u << FESC_CONT_SIM));
  CHECK(c.m[FESC_NOVIDADE] == (1u << FESC_CONHECIDO));
  f.minutos = 0; f.ano = 0; f.genero = NULL;
  farolescolha_cand_de(&c, &f);
  CHECK(c.m[FESC_TEMPO] == 0 && c.m[FESC_HUMOR] == 0 && c.m[FESC_EPOCA] == 0);
  farolescolha_cand_de(NULL, &f);
  farolescolha_cand_de(&c, NULL);
  CHECK(c.id == 0 && c.m[FESC_FORMATO] == 0);
  // #50 revisao B (P2-4): "war" so como PALAVRA inteira e maiuscula
  // acentuada dobra para a mesma base ascii da minuscula.
  memset(&f, 0, sizeof f);
  f.minutos = 95; f.ano = 2020;
  f.genero = "Warner · Comédia";
  farolescolha_cand_de(&c, &f);
  CHECK(!(c.m[FESC_PESO] & (1u << FESC_INTENSO)));        // "Warner" nao e guerra
  CHECK(c.m[FESC_PESO] & (1u << FESC_LEVE));
  CHECK(c.m[FESC_HUMOR] & (1u << FESC_RIR));
  f.genero = "War";
  farolescolha_cand_de(&c, &f);
  CHECK(c.m[FESC_PESO] & (1u << FESC_INTENSO));           // "war" solto vale
  f.genero = "Filme · Guerra dos Mundos";
  farolescolha_cand_de(&c, &f);
  CHECK(c.m[FESC_PESO] & (1u << FESC_INTENSO));
  f.genero = "AÇÃO · COMÉDIA";                          // maiuscula com acento
  farolescolha_cand_de(&c, &f);
  CHECK(c.m[FESC_PESO] & (1u << FESC_INTENSO));
  CHECK(c.m[FESC_PESO] & (1u << FESC_LEVE));
  CHECK(c.m[FESC_HUMOR] & (1u << FESC_RIR));
  f.genero = "ACAO · COMEDIA";                          // sem acento, maiuscula
  farolescolha_cand_de(&c, &f);
  CHECK(c.m[FESC_PESO] & (1u << FESC_INTENSO));
  CHECK(c.m[FESC_HUMOR] & (1u << FESC_RIR));
  puts("  cand_de: tempo aninhado, radicais PT/EN no comeco da palavra, sem dado = 0");
}

static void provaTeto(void) {
  FarolEscCtx x;
  int n = catRico(300), k;
  ctxNormal(&x);
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_de(&E) == 3 && farolescolha_passo(&E) == 1);
  for (k = 0; k < 10 && !farolescolha_terminou(&E); k++) {
    conferirInvariantes(&E);
    CHECK(farolescolha_passo(&E) == k + 1);
    CHECK(farolescolha_responder(&E, farolescolha_n_opcoes(&E) - 1));   // tanto
  }
  CHECK(farolescolha_terminou(&E) && farolescolha_feitas(&E) == 3);
  CHECK(!farolescolha_responder(&E, 0));         // nunca a 4a
  CHECK(farolescolha_vivos(&E) == n);            // tanto x3 nao filtrou
  conferirResultado(&E, n);
  puts("  teto: 3 perguntas e para (a 4a e recusada); tanto x3 mantem os 300");
}

static void provaContexto(void) {
  FarolEscCtx x;
  int i, n = 60, p, viuTempo;
  // So o tempo divide: a 1a pergunta e o tempo.
  for (i = 0; i < n; i++) {
    candVazio(&C[i], i, i);
    C[i].m[FESC_TEMPO] = (uint8_t)(1u << (i % 3));
  }
  ctxNormal(&x);
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_pergunta(&E) == FESC_TEMPO && farolescolha_n_opcoes(&E) == 4);
  CHECK(farolescolha_de(&E) == 1);               // nada mais divide
  // 23h40: "A noite toda" nao e oferecida.
  x.hora = 23;
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_pergunta(&E) == FESC_TEMPO && farolescolha_n_opcoes(&E) == 3);
  for (i = 0; i < farolescolha_n_opcoes(&E); i++) CHECK(farolescolha_opcao(&E, i) != FESC_T999);
  x.hora = 3;
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_n_opcoes(&E) == 3);
  x.hora = 5;
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_n_opcoes(&E) == 4);
  // Infantil pula a companhia; sem andamento pula "continuar".
  n = catRico(120);
  ctxNormal(&x);
  x.infantil = 1; x.emAndamento = 0;
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_origem(&E, FESC_COMPANHIA) == FESC_ORIG_CONTEXTO);
  CHECK(farolescolha_origem(&E, FESC_CONTINUAR) == FESC_ORIG_CONTEXTO);
  CHECK(farolescolha_vivos(&E) == n);            // o contexto nao filtra
  viuTempo = 0;
  while (!farolescolha_terminou(&E)) {
    p = farolescolha_pergunta(&E);
    CHECK(p != FESC_COMPANHIA && p != FESC_CONTINUAR);
    viuTempo |= p == FESC_TEMPO;
    CHECK(farolescolha_responder(&E, 0));
  }
  (void)viuTempo;
  // Com tudo igual menos o contexto, a companhia ou "continuar" aparecem.
  for (i = 0; i < n; i++) {
    int q;
    for (q = 0; q < FESC_N; q++) C[i].m[q] = (q == FESC_COMPANHIA || q == FESC_CONTINUAR)
                                             ? (uint8_t)(1u << (i % 2)) : 1u;
  }
  ctxNormal(&x);
  farolescolha_iniciar(&E, C, n, &x);
  p = farolescolha_pergunta(&E);
  CHECK(p == FESC_COMPANHIA || p == FESC_CONTINUAR);
  x.infantil = 1; x.emAndamento = 0;
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_terminou(&E) && farolescolha_feitas(&E) == 0);
  conferirResultado(&E, n);
  puts("  contexto: 23h40 e 3h sem \"A noite toda\"; infantil sem companhia; sem andamento sem continuar");
}

static void provaCedo(void) {
  FarolEscCtx x;
  int i, n;
  ctxNormal(&x);
  for (n = 0; n <= 2; n++) {
    FarolEscResultado r;
    catRico(n);
    farolescolha_iniciar(&E, C, n, &x);
    CHECK(farolescolha_terminou(&E) && farolescolha_feitas(&E) == 0 && farolescolha_de(&E) == 0);
    farolescolha_resultado(&E, &r);
    CHECK(r.n == n);
    CHECK(r.saida == (n == 0 ? FESC_SAIDA_NADA : n == 1 ? FESC_SAIDA_ACHADOS : FESC_SAIDA_COMPARAR));
    CHECK(!farolescolha_voltar(&E));             // nada a desfazer: sair
  }
  farolescolha_iniciar(&E, NULL, 5, &x);
  CHECK(farolescolha_terminou(&E) && farolescolha_vivos(&E) == 0);
  // 1a resposta deixa 2: encerra antes da 2a.
  n = 40;
  for (i = 0; i < n; i++) {
    candVazio(&C[i], i, i);
    C[i].m[FESC_FORMATO] = (uint8_t)(1u << (i < 2 ? FESC_SERIE : FESC_FILME));
    C[i].m[FESC_EPOCA] = (uint8_t)(1u << (i % 2));
    C[i].m[FESC_HUMOR] = (uint8_t)(1u << (i % 3));
  }
  farolescolha_iniciar(&E, C, n, &x);
  while (farolescolha_pergunta(&E) != FESC_FORMATO && !farolescolha_terminou(&E)) {
    // HUMOR (3 vias) divide mais que FORMATO; quando aparecer, responde tanto.
    CHECK(farolescolha_responder(&E, farolescolha_n_opcoes(&E) - 1));
  }
  if (!farolescolha_terminou(&E)) {
    int k, achou = -1;
    for (k = 0; k < farolescolha_n_opcoes(&E); k++)
      if (farolescolha_opcao(&E, k) == FESC_SERIE) achou = k;
    CHECK(achou >= 0);
    CHECK(farolescolha_responder(&E, achou));
    CHECK(farolescolha_terminou(&E) && farolescolha_vivos(&E) == 2);
    conferirResultado(&E, n);
  }
  // Resposta direta: 1a pergunta que deixa 2.
  for (i = 0; i < n; i++) { candVazio(&C[i], i, i); C[i].m[FESC_FORMATO] = (uint8_t)(1u << (i < 2 ? FESC_SERIE : FESC_FILME)); }
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_pergunta(&E) == FESC_FORMATO && farolescolha_opcao(&E, 1) == FESC_SERIE);
  CHECK(farolescolha_responder(&E, 1));
  CHECK(farolescolha_terminou(&E) && farolescolha_feitas(&E) == 1 && farolescolha_vivos(&E) == 2);
  puts("  cedo: 0/1/2 candidatos encerram sem perguntar; resposta que deixa 2 encerra na hora");
}

static void provaTanto(void) {
  FarolEscCtx x;
  int n = catRico(200), v, p;
  ctxNormal(&x);
  farolescolha_iniciar(&E, C, n, &x);
  v = farolescolha_vivos(&E);
  p = farolescolha_pergunta(&E);
  CHECK(farolescolha_conta(&E, p, farolescolha_tanto(p)) == v);
  CHECK(farolescolha_responder(&E, farolescolha_n_opcoes(&E) - 1));
  CHECK(farolescolha_vivos(&E) == v);
  CHECK(farolescolha_resposta(&E, p) == farolescolha_tanto(p));
  CHECK(farolescolha_origem(&E, p) == FESC_ORIG_PESSOA);
  CHECK(farolescolha_pergunta(&E) != p);         // respondida: nao volta a perguntar
  puts("  tanto: nao filtra, marca respondida");
}

typedef struct {
  int perg, nOpc, opc[FESC_OPC_MAX], de, passo, vivos, feitas;
  int8_t resp[FESC_N];
  uint8_t vivo[FESC_CAND_MAX];
} Foto;

static void fotografar(const FarolEsc *e, Foto *f) {
  int i;
  memset(f, 0, sizeof *f);
  f->perg = farolescolha_pergunta(e);
  f->nOpc = farolescolha_n_opcoes(e);
  for (i = 0; i < f->nOpc; i++) f->opc[i] = farolescolha_opcao(e, i);
  f->de = farolescolha_de(e);
  f->passo = farolescolha_passo(e);
  f->vivos = farolescolha_vivos(e);
  f->feitas = farolescolha_feitas(e);
  for (i = 0; i < FESC_N; i++) f->resp[i] = (int8_t)farolescolha_resposta(e, i);
  memcpy(f->vivo, e->vivo, sizeof f->vivo);
}

static void provaVoltar(void) {
  FarolEscCtx x;
  Foto f[4], g;
  int n = catRico(250), k;
  ctxNormal(&x);
  farolescolha_iniciar(&E, C, n, &x);
  for (k = 0; k < 3 && !farolescolha_terminou(&E); k++) {
    fotografar(&E, &f[k]);
    CHECK(farolescolha_responder(&E, 0));
  }
  CHECK(k == 3 && farolescolha_terminou(&E));
  for (k = 2; k >= 0; k--) {
    CHECK(farolescolha_voltar(&E));
    fotografar(&E, &g);
    CHECK(!memcmp(&g, &f[k], sizeof g));         // exatamente o passo anterior
    conferirInvariantes(&E);
  }
  CHECK(!farolescolha_voltar(&E));               // Voltar na 1a = sair
  fotografar(&E, &g);
  CHECK(!memcmp(&g, &f[0], sizeof g));
  puts("  voltar: 3->2->1 restaura vivos, pergunta, opcoes e \"de\" bit a bit; na 1a pede saida");
}

static void provaPre(void) {
  FarolEscCtx x;
  int n = catRico(300), de0;
  ctxNormal(&x);
  farolescolha_iniciar(&E, C, n, &x);
  de0 = farolescolha_de(&E);
  CHECK(de0 == 3);
  CHECK(farolescolha_pre_responder(&E, FESC_FORMATO, "filme"));
  CHECK(farolescolha_origem(&E, FESC_FORMATO) == FESC_ORIG_PRE);
  CHECK(farolescolha_de(&E) <= 2);
  CHECK(farolescolha_pergunta(&E) != FESC_FORMATO);
  CHECK(!farolescolha_pre_responder(&E, FESC_FORMATO, "serie"));   // ja respondida
  CHECK(!farolescolha_pre_responder(&E, FESC_TEMPO, "t30"));       // id invalido
  CHECK(!farolescolha_pre_responder(&E, 99, "t45"));
  CHECK(farolescolha_pre_responder(&E, FESC_HUMOR, "tanto"));
  CHECK(farolescolha_de(&E) <= 1);
  CHECK(farolescolha_pre_responder(&E, FESC_EPOCA, "nova"));
  CHECK(farolescolha_terminou(&E) && farolescolha_feitas(&E) == 0);   // 3 pre: encerra direto
  // #50 revisao B (P2-1): encerrado ou no teto, a 4a pre e recusada sem
  // tocar nos vivos.
  {
    int vAntes = farolescolha_vivos(&E);
    CHECK(!farolescolha_pre_responder(&E, FESC_IDIOMA, "dublado"));
    CHECK(!farolescolha_pre_responder(&E, FESC_PESO, "leve"));
    CHECK(farolescolha_vivos(&E) == vAntes);
  }
  conferirResultado(&E, n);
  // Depois da 1a resposta da pessoa o #54 nao entra mais.
  farolescolha_iniciar(&E, C, n, &x);
  CHECK(farolescolha_responder(&E, 0));
  CHECK(!farolescolha_pre_responder(&E, FESC_FORMATO, "filme"));
  // Pre-resposta que nao bate: NADA honesto, sem pergunta e sem beco.
  {
    int i;
    for (i = 0; i < 10; i++) { candVazio(&C[i], i, i); C[i].m[FESC_FORMATO] = 1u << FESC_FILME; }
    farolescolha_iniciar(&E, C, 10, &x);
    CHECK(farolescolha_pre_responder(&E, FESC_FORMATO, "serie"));
    CHECK(farolescolha_terminou(&E) && farolescolha_vivos(&E) == 0);
    conferirResultado(&E, 10);
  }
  puts("  pre_responder (#54): reduz o que falta, 3 encerram direto, so antes da pessoa");
}

static void provaAleatoria(void) {
  int perfil, total = 0, encerrados = 0;
  for (perfil = 0; perfil < 1000; perfil++) {
    FarolEscCtx x;
    int n = (int)(rng() % (FESC_CAND_MAX + 40)), i, p, passos = 0, respostas = 0;
    int nn = n > FESC_CAND_MAX ? FESC_CAND_MAX : n;
    for (i = 0; i < nn; i++) {
      candVazio(&C[i], i, (int32_t)(rng() % 2000) - 500);
      for (p = 0; p < FESC_N; p++) {
        unsigned q = rng() % 10;
        // ~20% sem dado, o resto com 1 ou mais opcoes (mascara aleatoria).
        C[i].m[p] = q < 2 ? 0 : (uint8_t)(rng() & ((1u << farolescolha_tanto(p)) - 1u));
      }
    }
    x.hora = (int)(rng() % 30) - 3;   // inclui fora da faixa
    x.infantil = (int)(rng() % 2);
    x.emAndamento = (int)(rng() % 2);
    farolescolha_iniciar(&E, C, n, &x);
    for (i = (int)(rng() % 4); i > 0; i--) {   // #54 aleatorio (valido ou nao)
      int pp = (int)(rng() % (FESC_N + 1));
      int oo = (int)(rng() % 5);
      farolescolha_pre_responder(&E, pp, oo == 4 ? "xx" : farolescolha_opcao_id(pp, oo));
    }
    conferirInvariantes(&E);
    // Caminhada aleatoria: responde ou volta; nunca mais de 3 de pe.
    while (!farolescolha_terminou(&E)) {
      int nOpc = farolescolha_n_opcoes(&E);
      CHECK(++passos < 60);                       // zero lacos
      if (farolescolha_feitas(&E) > 0 && rng() % 5 == 0) {
        CHECK(farolescolha_voltar(&E));
      } else {
        int vAntes = farolescolha_vivos(&E);
        CHECK(farolescolha_responder(&E, (int)(rng() % (unsigned)nOpc)));
        respostas++;
        CHECK(farolescolha_vivos(&E) > 0 && farolescolha_vivos(&E) <= vAntes);   // zero becos
      }
      conferirInvariantes(&E);
    }
    CHECK(!farolescolha_responder(&E, 0));
    conferirResultado(&E, nn);
    total += respostas;
    encerrados++;
  }
  CHECK(encerrados == 1000);
  printf("  1000 perfis aleatorios: zero becos, zero lacos (%d respostas)\n", total);
}

// Determinismo estrito: duas corridas identicas do mesmo roteiro.
static void provaDeterminismo(void) {
  static FarolEsc a, b;
  FarolEscCtx x;
  FarolEscResultado ra, rb;
  int n = catRico(333), k;
  ctxNormal(&x);
  farolescolha_iniciar(&a, C, n, &x);
  farolescolha_iniciar(&b, C, n, &x);
  CHECK(!memcmp(&a, &b, sizeof a));
  for (k = 0; k < 3 && !farolescolha_terminou(&a); k++) {
    CHECK(farolescolha_responder(&a, k % farolescolha_n_opcoes(&a)));
    CHECK(farolescolha_responder(&b, k % farolescolha_n_opcoes(&b)));
    CHECK(!memcmp(&a, &b, sizeof a));
  }
  farolescolha_resultado(&a, &ra);
  farolescolha_resultado(&b, &rb);
  CHECK(!memcmp(&ra, &rb, sizeof ra));
  puts("  determinismo: mesma entrada = mesmo estado bit a bit");
}

static void provasMotor(void) {
  provaBanco();
  provaCandDe();
  provaTeto();
  provaContexto();
  provaCedo();
  provaTanto();
  provaVoltar();
  provaPre();
  provaDeterminismo();
  provaAleatoria();
}

#ifdef FESC_SO_MOTOR
int main(void) {
  provasMotor();
  CHECK(!gFalhas);
  puts("farol_escolha (motor puro): ok");
  return 0;
}
#else
// ======================================================================
// Parte 2: a casca no app inteiro.
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "farolacha.h"
#include "farolcompara.h"
#include "farolescolhaui.h"
#include "gfx.h"
#include "perfilinfantil.h"
#include "perfis.h"
#include "resolver.h"
#include "tex_cache.h"
#include "text.h"

#ifdef NV_LINUX_DESKTOP
#include <GLES2/gl2.h>
#endif

static int gPlayer;
static int playerTeste(void) { return gPlayer; }

static void teclaR(SDL_Keycode k, int repeat) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.repeat = (Uint8)repeat;
  resolver_evento(&e);
  resolver_atualizar(0.016f, 1000);
}
static void tecla(SDL_Keycode k) { teclaR(k, 0); }

// Tecla DIRETO na camada da escolha (sem passar pelo resolver): devolve se o
// evento foi consumido; sair/abrir saem pelos ponteiros.
static int teclaUi(SDL_Keycode k, int *sair, int *abrir) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return farolescolhaui_evento(&e, sair, abrir);
}

static const char *const GENEROS[] = {
  "Filme · Comédia", "Filme · Drama · Romance", "Série · Suspense · Crime",
  "Filme · Terror", "Filme · Animação · Família", "Série · Comédia · Sitcom",
  "Filme · Ação · Aventura", "Série · Drama", "Filme · Documentário",
};

static CatItem CAT[48];

static void montarCatItem(CatItem *c, int i) {
  int serie = i % 3 == 2;
  int min = serie ? 25 + (i % 4) * 10 : 40 + (i * 17) % 120;
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "tt50%05d", i + 1);
  snprintf(c->tipo, sizeof c->tipo, "%s", serie ? "series" : "movie");
  snprintf(c->titulo, sizeof c->titulo, "Obra %02d", i + 1);
  snprintf(c->genero, sizeof c->genero, "%s", GENEROS[i % 9]);
  snprintf(c->meta, sizeof c->meta, "%d · %d min", (i % 2) ? 1990 + i % 9 : 2016 + i % 8, min);
  c->nota = 50 + (i * 7) % 50;
}

static int semearCatalogo(int n) {
  int i;
  for (i = 0; i < n; i++) montarCatItem(&CAT[i], i);
  cat_definir(CAT, n);
  return n;
}

static void irParaEscolha(void) {
  int i;
  for (i = 0; i < 40 && resolver_foco() != RESOLVER_B_ACHAR_ESCOLHA; i++) {
    int f = resolver_foco();
    if (f >= RESOLVER_B_ACHAR_CURTO && f <= RESOLVER_B_ACHAR_SURPRESA) tecla(SDLK_DOWN);
    else tecla(SDLK_RIGHT);
  }
  CHECK(resolver_foco() == RESOLVER_B_ACHAR_ESCOLHA);
}

static void abrirFarolEscolha(void) {
  resolver_abrir();
  resolver_atualizar(0.016f, 1000);
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
  irParaEscolha();
  teclaR(SDLK_RETURN, 1);                        // OK segurado nao abre
  CHECK(resolver_fase() == RESOLVER_FASE_INICIO);
  tecla(SDLK_RETURN);
  CHECK(resolver_fase() == RESOLVER_FASE_ESCOLHA || !resolver_aberto());
}

static void provaDpad(void) {
  const FarolEsc *m;
  int n = semearCatalogo(48), passo, nOpc;
  farolescolhaui_teste_hora(20);
  farolescolhaui_teste_infantil(0);
  abrirFarolEscolha();
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ESCOLHA);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(farolescolhaui_n_candidatos() == n);
  m = farolescolhaui_motor();
  CHECK(farolescolha_passo(m) == 1 && farolescolha_de(m) == 3);
  nOpc = farolescolha_n_opcoes(m);
  CHECK(nOpc >= 2 && farolescolhaui_foco() == 0);
  tecla(SDLK_LEFT);  CHECK(farolescolhaui_foco() == 0);        // borda: fica
  tecla(SDLK_RIGHT); CHECK(farolescolhaui_foco() == 1);
  for (passo = 0; passo < 6; passo++) tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == nOpc - 1);                    // "Tanto faz", ultima
  tecla(SDLK_DOWN);  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  tecla(SDLK_UP);    CHECK(farolescolhaui_foco() == nOpc - 1);
  teclaR(SDLK_RETURN, 1);                                      // repeat: ignorado
  CHECK(farolescolha_feitas(m) == 0 && farolescolha_passo(m) == 1);
  tecla(SDLK_RETURN);                                          // tanto
  CHECK(farolescolha_feitas(m) == 1 && farolescolha_passo(m) == 2);
  CHECK(farolescolha_vivos(m) == n);
  CHECK(farolescolhaui_foco() == 0);
  tecla(SDLK_RIGHT);
  teclaR(SDLK_RETURN, 1);
  CHECK(farolescolha_passo(m) == 2);
  tecla(SDLK_RETURN);                                          // opcao 1 no passo 2
  CHECK(farolescolha_passo(m) == 3 && farolescolha_feitas(m) == 2);
  // Voltar 3 -> 2 (foco na opcao escolhida) -> 1 -> saida.
  tecla(SDLK_ESCAPE);
  CHECK(resolver_fase() == RESOLVER_FASE_ESCOLHA && farolescolha_passo(m) == 2);
  CHECK(farolescolhaui_foco() == 1);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolha_passo(m) == 1 && farolescolhaui_foco() == nOpc - 1);
  CHECK(farolescolha_vivos(m) == n);
  tecla(SDLK_ESCAPE);
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
  CHECK(resolver_foco() == RESOLVER_B_ACHAR_ESCOLHA);          // foco restaurado
  // O botao Voltar na tela tambem desfaz.
  tecla(SDLK_RETURN);
  CHECK(resolver_fase() == RESOLVER_FASE_ESCOLHA);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  CHECK(resolver_fase() == RESOLVER_FASE_INICIO);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);
  puts("  dpad: <- -> v ^, OK com repeat ignorado, Voltar 3->2->1->Inicio com o foco de volta");
}

static void provaComparar(void) {
  const FarolEsc *m;
  int guard = 0;
  semearCatalogo(48);
  gPlayer = 0;
  farolcompara_teste_player(playerTeste);
  farolcompara_teste_reset();
  abrirFarolEscolha();
  m = farolescolhaui_motor();
  while (resolver_aberto() && resolver_fase() == RESOLVER_FASE_ESCOLHA &&
         farolescolhaui_modo() == FESCUI_PERGUNTA) {
    CHECK(++guard <= 3);
    tecla(SDLK_RETURN);                          // sempre a 1a opcao
  }
  CHECK(farolescolha_feitas(m) <= 3);
  CHECK(farolescolhaui_saida() == FESC_SAIDA_COMPARAR);
  CHECK(farolescolhaui_modo() == FESCUI_COMPARAR);
  CHECK(!resolver_aberto());                     // o Farol fechou
  CHECK(farolcompara_aberto());
  CHECK(farolcompara_n_colunas() >= 2 && farolcompara_n_colunas() <= 3);
  farolcompara_fechar();
  farolcompara_teste_reset();
  resolver_atualizar(1.0f, 2000);

  // P4 recusado (player aberto): cartazes aqui, sem beco.
  gPlayer = 1;
  abrirFarolEscolha();
  guard = 0;
  while (farolescolhaui_modo() == FESCUI_PERGUNTA) { CHECK(++guard <= 3); tecla(SDLK_RETURN); }
  CHECK(resolver_aberto() && farolescolhaui_modo() == FESCUI_RESULTADO);
  CHECK(farolescolhaui_n_resultados() >= 2 && !farolcompara_aberto());
  tecla(SDLK_RIGHT); CHECK(farolescolhaui_foco() == 1);
  tecla(SDLK_RETURN);
  {
    FarolAchaPedido p;
    int idx;
    CHECK(resolver_pediu_abrir(&p));
    idx = resolver_resolver_pedido(&p, NULL);
    CHECK(idx == farolescolhaui_resultado_indice(1));
    CHECK(!strncmp(p.imdb, "tt50", 4));
    CHECK(!resolver_pediu_abrir(&p));            // uma vez so
  }
  tecla(SDLK_ESCAPE);                            // volta a ultima pergunta
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);
  gPlayer = 0;
  puts("  fim com 2-3: farolcompara_abrir_titulos e o Farol fecha; P4 recusado = cartazes + abrir");
}

static void provaUmEZero(void) {
  CatItem c1;
  FarolAchaPedido p;
  // 1 titulo: cartaz + OK abre; Voltar sai (nada a desfazer).
  montarCatItem(&c1, 7);
  cat_definir(&c1, 1);
  abrirFarolEscolha();
  CHECK(resolver_fase() == RESOLVER_FASE_ESCOLHA);
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO && farolescolhaui_n_resultados() == 1);
  CHECK(farolescolhaui_saida() == FESC_SAIDA_ACHADOS && farolescolhaui_foco() == 0);
  teclaR(SDLK_RETURN, 1);
  CHECK(!resolver_pediu_abrir(&p));
  tecla(SDLK_RETURN);
  CHECK(resolver_pediu_abrir(&p) && !strcmp(p.imdb, "tt5000008"));
  tecla(SDLK_ESCAPE);
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO &&
        resolver_foco() == RESOLVER_B_ACHAR_ESCOLHA);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 0 titulos: "Nada exato…" com o foco no Voltar.
  cat_definir(NULL, 0);
  abrirFarolEscolha();
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO && farolescolhaui_n_resultados() == 0);
  CHECK(farolescolhaui_saida() == FESC_SAIDA_NADA);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  tecla(SDLK_LEFT); tecla(SDLK_UP); tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);          // sem beco nem salto
  tecla(SDLK_RETURN);
  CHECK(resolver_fase() == RESOLVER_FASE_INICIO);
  resolver_fechar();
  resolver_atualizar(1.0f, 2000);

  // 0 por pre-resposta que nao bate (#54): honesto e Voltar sai.
  semearCatalogo(48);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(farolescolhaui_pre_responder("formato", "serie"));
  CHECK(farolescolhaui_pre_responder("tempo", "t45"));
  CHECK(farolescolhaui_pre_responder("epoca", "antiga"));
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO || farolescolhaui_modo() == FESCUI_COMPARAR);
  if (farolcompara_aberto()) farolcompara_fechar();
  farolescolhaui_pediu_fechar();
  farolescolhaui_reset();
  puts("  fim com 1: cartaz e OK abre; com 0: \"Nada exato…\" e Voltar; #54 pre-responde");
}

// ======================= revisao FAROL50-B (P1/P2 da r1) =======================
// P1-1: o perfil infantil filtra a coleta (sem classificacao conhecida fica
// fora tambem) — tanto pelo gancho de teste quanto pelo perfil real.
static void provaInfantilFiltra(void) {
  CatItem it[10];
  static const char *const CLASS[10] = { "18", "R", "TV-MA", "", "16",
                                         "L", "10", "G", "TV-Y7", "PG" };
  int i;
  for (i = 0; i < 10; i++) {
    montarCatItem(&it[i], i);
    snprintf(it[i].classificacao, sizeof it[i].classificacao, "%s", CLASS[i]);
  }
  gPlayer = 1;                                   // o P4 recusa: nada abre aqui
  farolcompara_teste_player(playerTeste);
  cat_definir(it, 10);
  farolescolhaui_teste_infantil(1);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_n_candidatos() == 5);       // so L/10/G/TV-Y7/PG
  if (farolcompara_aberto()) farolcompara_fechar();
  farolescolhaui_reset();
  // Perfil real (testeInfantil = -1): a mesma guarda tem de valer.
  CHECK(perfil_infantil_definir(perfis_ativo(), 1));
  farolescolhaui_teste_infantil(-1);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_n_candidatos() == 5);
  if (farolcompara_aberto()) farolcompara_fechar();
  CHECK(perfil_infantil_definir(perfis_ativo(), 0));
  farolescolhaui_teste_infantil(0);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_n_candidatos() == 10);      // adulto ve tudo
  if (farolcompara_aberto()) farolcompara_fechar();
  farolescolhaui_reset();
  gPlayer = 0;
  semearCatalogo(48);
  puts("  infantil (P1-1): 18/R/TV-MA/vazio/16 barrados na coleta; L/10/G/TV-Y7/PG passam");
}

// P1-2: OK em RESULTADO vale para o cartaz EXIBIDO sob o foco. Se o catalogo
// trocou entre o quadro e a tecla e aquele titulo sumiu, o OK so atualiza a
// tela — nunca abre outra obra nem dispara Voltar na mesma tecla.
static void provaToctouOk(void) {
  CatItem dois[2];
  FarolAchaPedido p;
  int sair = 0, abrir = 0, s0, s1;
  gPlayer = 1;                                   // o P4 recusa: RESULTADO
  farolcompara_teste_player(playerTeste);

  montarCatItem(&dois[0], 0);
  montarCatItem(&dois[1], 1);

  // Caso A (1 cartaz): o unico some -> OK nao dispara Voltar nem abre nada.
  cat_definir(dois, 1);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO && farolescolhaui_n_resultados() == 1);
  cat_definir(NULL, 0);
  CHECK(teclaUi(SDLK_RETURN, &sair, &abrir));
  CHECK(!sair && !abrir && !farolescolhaui_pediu_abrir(&p));
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO && farolescolhaui_n_resultados() == 0);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  farolescolhaui_reset();

  cat_definir(dois, 2);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_n_resultados() == 2);
  s0 = farolescolhaui_resultado_indice(0);       // exibido no slot 0
  s1 = farolescolhaui_resultado_indice(1);       // exibido no slot 1
  CHECK(s0 >= 0 && s1 >= 0 && s0 != s1);

  // Caso B (foco no slot 1): ele some -> OK NAO abre o slot 0.
  CHECK(teclaUi(SDLK_RIGHT, &sair, &abrir));
  CHECK(farolescolhaui_foco() == 1);
  cat_definir(&dois[s0], 1);                     // so o exibido do slot 0 sobra
  CHECK(teclaUi(SDLK_RETURN, &sair, &abrir));
  CHECK(!sair && !abrir && !farolescolhaui_pediu_abrir(&p));
  CHECK(farolescolhaui_n_resultados() == 1 && farolescolhaui_foco() == 0);
  farolescolhaui_reset();

  // Caso C (foco no slot 0): ele some -> OK NAO abre o ex-slot 1 (agora slot 0).
  cat_definir(dois, 2);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_n_resultados() == 2 && farolescolhaui_foco() == 0);
  cat_definir(&dois[s1], 1);                     // so o exibido do slot 1 sobra
  CHECK(teclaUi(SDLK_RETURN, &sair, &abrir));
  CHECK(!sair && !abrir && !farolescolhaui_pediu_abrir(&p));
  CHECK(farolescolhaui_n_resultados() == 1 && farolescolhaui_foco() == 0);
  farolescolhaui_reset();

  // Controle positivo: o exibido permaneceu (a revisao mudou, a obra nao) ->
  // o OK abre exatamente o que a pessoa viu.
  cat_definir(dois, 2);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_n_resultados() == 2 && farolescolhaui_foco() == 0);
  cat_definir(dois, 2);                          // nova revisao, mesma obra
  CHECK(teclaUi(SDLK_RETURN, &sair, &abrir));
  CHECK(abrir && farolescolhaui_pediu_abrir(&p));
  CHECK(!strcmp(p.imdb, dois[s0].imdb));
  farolescolhaui_reset();

  gPlayer = 0;
  semearCatalogo(48);
  puts("  toctou (P1-2): OK no cartaz que sumiu so atualiza — nao abre outro, nao dispara Voltar");
}

// P2-2: vencedor so-TMDB nao some em silencio. Com 3 vencedores o P4 (que
// exige imdb) engoliria so os 2 com imdb — a correcao segura o conjunto aqui,
// nos cartazes, e o #1 abre pela identidade TMDB.
static void provaSoTmdb(void) {
  CatItem it[3];
  FarolAchaPedido p;
  int sair = 0, abrir = 0, i, slot = -1;
  gPlayer = 0;
  farolcompara_teste_player(playerTeste);
  farolcompara_teste_reset();
  for (i = 0; i < 3; i++) {                      // mascaras identicas: encerra
    montarCatItem(&it[i], i);                    // ja com os 3 vencedores
    snprintf(it[i].tipo, sizeof it[i].tipo, "movie");
    snprintf(it[i].genero, sizeof it[i].genero, "Ação");
    snprintf(it[i].meta, sizeof it[i].meta, "2020 · 90 min");
  }
  it[0].imdb[0] = '\0';                          // identidade so TMDB
  it[0].tmdb = 555111;
  it[0].nota = 99;                               // vencedor #1
  it[1].nota = 80;
  it[2].nota = 70;
  cat_definir(it, 3);
  farolescolhaui_abrir();
  CHECK(farolescolhaui_modo() == FESCUI_RESULTADO && farolescolhaui_n_resultados() == 3);
  CHECK(!farolcompara_aberto());                 // #1 sem imdb: nada de P4 com 2
  for (i = 0; i < 3; i++) if (farolescolhaui_resultado_indice(i) == 0) slot = i;
  CHECK(slot >= 0);                              // #1 nao foi descartado
  for (i = 0; i < slot; i++) CHECK(teclaUi(SDLK_RIGHT, &sair, &abrir));
  CHECK(teclaUi(SDLK_RETURN, &sair, &abrir));
  CHECK(abrir && farolescolhaui_pediu_abrir(&p));
  CHECK(p.tmdb == 555111 && !p.imdb[0]);
  farolescolhaui_reset();
  farolcompara_teste_reset();
  semearCatalogo(48);
  puts("  so-TMDB (P2-2): o #1 sem imdb fica no cartaz (P4 nao o engoliria) e abre por TMDB");
}

// P2-3: progresso >= 90 ja e visto (faixa de farolacha.c): sem "continuar",
// o titulo e contado como conhecido e a pergunta nem se oferece.
static void provaAndamento90(void) {
  CatItem it[2];
  const FarolEsc *m;
  montarCatItem(&it[0], 0);
  it[0].progresso = 95;
  it[0].retomadoMs = 12345;
  cat_definir(it, 1);
  farolescolhaui_abrir();
  m = farolescolhaui_motor();
  CHECK(m->ctx.emAndamento == 0);                // 95% nao e "em andamento"
  CHECK(farolescolha_origem(m, FESC_CONTINUAR) == FESC_ORIG_CONTEXTO);
  CHECK(m->cand[0].m[FESC_CONTINUAR] == (1u << FESC_COMECAR));
  CHECK(m->cand[0].m[FESC_NOVIDADE] == (1u << FESC_CONHECIDO));
  farolescolhaui_reset();

  montarCatItem(&it[0], 0);
  it[0].progresso = 50;                          // faixa real de andamento
  cat_definir(it, 1);
  farolescolhaui_abrir();
  m = farolescolhaui_motor();
  CHECK(m->ctx.emAndamento == 1);
  CHECK(m->cand[0].m[FESC_CONTINUAR] == (1u << FESC_CONT_SIM));
  farolescolhaui_reset();

  montarCatItem(&it[0], 0);
  it[0].progresso = 0;
  it[0].retomadoMs = 777;                        // retomada com 0% ainda conta
  cat_definir(it, 1);
  farolescolhaui_abrir();
  m = farolescolhaui_motor();
  CHECK(m->ctx.emAndamento == 1);
  CHECK(m->cand[0].m[FESC_CONTINUAR] == (1u << FESC_CONT_SIM));
  farolescolhaui_reset();
  semearCatalogo(48);
  puts("  andamento (P2-3): >=90 nao conta como em andamento; 1..89 e 0+retomadoMs contam");
}

static void provaRevisao50b(void) {
  provaInfantilFiltra();
  provaToctouOk();
  provaSoTmdb();
  provaAndamento90();
}

#ifdef NV_LINUX_DESKTOP
static void salvarPng(const char *path, const unsigned char *rgba, int w, int h) {
  int y, pitch = w * 4;
  unsigned char *flip = malloc((size_t)pitch * (size_t)h);
  SDL_Surface *s;
  CHECK(flip != NULL);
  for (y = 0; y < h; y++)
    memcpy(flip + (size_t)y * (size_t)pitch, rgba + (size_t)(h - 1 - y) * (size_t)pitch,
           (size_t)pitch);
  s = SDL_CreateRGBSurfaceFrom(flip, w, h, 32, pitch, 0x000000ffu, 0x0000ff00u,
                               0x00ff0000u, 0xff000000u);
  CHECK(s != NULL);
  CHECK(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
  free(flip);
}

static void capturar(const char *dir, const char *nome, unsigned char *px, int W, int H) {
  char path[1024];
  SDL_Surface *lida;
  int i, claros = 0;
  size_t b;
  for (i = 0; i < 40; i++) {
    resolver_atualizar(0.016f, 2000u + (Uint32)i * 16u);
    txt_novo_quadro();
    gfx_novo_quadro();
    glViewport(0, 0, W, H);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    resolver_desenhar(2000u + (Uint32)i * 16u);
    glFinish();
  }
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (b = 0; b < (size_t)W * (size_t)H * 4u; b += 4)
    if (px[b] > 200 && px[b + 1] > 200 && px[b + 2] > 200) claros++;
  CHECK(claros > 2000);                          // texto e pilulas desenhados
  snprintf(path, sizeof path, "%s/%s.png", dir, nome);
  salvarPng(path, px, W, H);
  lida = IMG_Load(path);
  CHECK(lida && lida->w == W && lida->h == H);
  SDL_FreeSurface(lida);
  printf("  captura %s: %dx%d ok\n", nome, W, H);
}

static void provaGl(const char *dir) {
  const int W = 1920, H = 1080;
  SDL_Window *win;
  SDL_GLContext gl;
  unsigned char *px = malloc((size_t)W * (size_t)H * 4u);
  CHECK(px != NULL);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  win = SDL_CreateWindow("farol-50", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  CHECK(win != NULL);
  gl = SDL_GL_CreateContext(win);
  CHECK(gl != NULL);
  glViewport(0, 0, W, H);
  gfx_tamanho_alvo(W, H);
  CHECK(gfx_iniciar());
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  CHECK(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);

  semearCatalogo(48);
  resolver_abrir();
  irParaEscolha();
  capturar(dir, "escolha-inicio", px, W, H);     // o 5o item na coluna
  tecla(SDLK_RETURN);
  CHECK(resolver_fase() == RESOLVER_FASE_ESCOLHA && farolescolhaui_modo() == FESCUI_PERGUNTA);
  tecla(SDLK_RIGHT);
  capturar(dir, "escolha-pergunta", px, W, H);
  resolver_fechar();
  resolver_atualizar(1.0f, 3000);
  cat_definir(NULL, 0);
  resolver_abrir();
  irParaEscolha();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_n_resultados() == 0);
  capturar(dir, "escolha-nada", px, W, H);
  resolver_fechar();
  resolver_atualizar(1.0f, 4000);

  free(px);
  tex_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
}
#endif

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : NULL;
  provasMotor();
  CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  CHECK(farolacha_minutos_meta("2020 · 95 min") == 95);
  farolcompara_teste_player(playerTeste);
  provaDpad();
  provaComparar();
  provaUmEZero();
  provaRevisao50b();
#ifdef NV_LINUX_DESKTOP
  if (dir) provaGl(dir);
#endif
  (void)dir;
  CHECK(!gFalhas);
  CHECK(farolcompara_teste_esperar(3000));
  SDL_Quit();
  puts("farol_escolha: ok");
  return 0;
}
#endif
