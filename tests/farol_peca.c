// Cena e validador do Farol (issue 39, secoes 4.1-4.2 do PLANO-GOSTO). Sem
// SDL nem rede: linka src/farolpeca.c + src/faroltexto.c + src/js.c.
//
//   bash tests/farol_peca.sh                  (ASan+UBSan, o padrao)
//   SANITIZE=0 bash tests/farol_peca.sh       (so os asserts)
//
// Cobre: o corpus compartilhado tests/farol_cenas/ INTEIRO (61 boas +
// 268 ruins do manifesto casos.json, com codigo FPE_* por posicao),
// asserts campo a campo nas sete pecas e nos textos x/m, e 100 000
// mutacoes deterministicas sobre cenas reais — sob ASan/UBSan qualquer
// leitura fora do buffer aparece aqui.
#include "../src/farolpeca.h"
#include "../src/js.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T_CENA_DIR "tests/farol_cenas/"
#define T_FUZZ 100000

static char t_buf[FAROL_CENA_MAX * 3 + 256];   // cabe ruim-01 (>8 KB) + folga
static char t_manifesto[256 * 1024];
static int t_falhas;

#define CHECK(c) do { if (!(c)) { \
    printf("FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); \
    if (++t_falhas > 20) abort(); } } while (0)

static size_t t_le(const char *arq, char *dst, size_t tam) {
  char caminho[256];
  FILE *f;
  size_t n;
  snprintf(caminho, sizeof caminho, "%s%s", T_CENA_DIR, arq);
  f = fopen(caminho, "rb");
  if (!f) { printf("FALHA: nao abre %s\n", caminho); abort(); }
  n = fread(dst, 1, tam - 1, f);
  fclose(f);
  dst[n] = 0;
  return n;
}

// Nome -> codigo FCE_* (manifesto "rc":"FCE_...").
static int t_fce(const char *nome) {
  for (int i = 0; i < 8; i++)
    if (!strcmp(farolpeca_cena_erro_nome(i), nome)) return i;
  return -1;
}
static int t_fpe(const char *nome) {
  for (int i = 0; i < 16; i++)
    if (!strcmp(farolpeca_erro_nome(i), nome)) return i;
  return -1;
}

// Roda UM caso do manifesto: le a cena, valida rc/boas/rec/erro[] por pos.
static void t_caso(const char *ci, const char *cf) {
  FarolCena c;
  char arq[64], rcNome[32], erros[512], posNome[12];
  int cand, rc;
  size_t n;

  arq[0] = rcNome[0] = 0;
  CHECK(js_texto(ci, cf, "arq", arq, sizeof arq));
  cand = (int)js_num(ci, cf, "cand", -1);
  CHECK(js_texto(ci, cf, "rc", rcNome, sizeof rcNome));
  n = t_le(arq, t_buf, sizeof t_buf);
  (void)n;
  rc = farolpeca_ler(&c, t_buf, cand);

  if (strcmp(rcNome, "OK")) {                       // cena inteira fora
    int esperado = t_fce(rcNome);
    CHECK(esperado > 0);
    CHECK(rc == -esperado);
    return;
  }
  CHECK(rc >= 0);
  CHECK(rc == c.nPecas);
  CHECK(c.nPecas == (int)js_num(ci, cf, "boas", -1));
  CHECK(c.nRecusadas == (int)js_num(ci, cf, "rec", -1));

  // erros: {"0":"FPE_*",...} ou ausente (tudo FPE_OK ate a 6a posicao)
  erros[0] = 0;
  int temErros = js_bruto(ci, cf, "erros", erros, sizeof erros);
  for (int p = 0; p < FAROL_PECA_MAX; p++) {
    int esperado = 0;
    snprintf(posNome, sizeof posNome, "%d", p);
    if (temErros && js_tem(erros, erros + strlen(erros), posNome)) {
      char en[32] = "";
      CHECK(js_texto(erros, erros + strlen(erros), posNome, en, sizeof en));
      esperado = t_fpe(en);
      CHECK(esperado > 0);
    }
    CHECK(c.erro[p] == esperado);
  }
}

// ------------------------------------------------------- corpus inteiro
static void t_corpus(void) {
  const char *fim, *el, *ef;
  size_t n = t_le("casos.json", t_manifesto, sizeof t_manifesto);
  int nCasos = 0, nBoas = 0, nRuins = 0;
  fim = t_manifesto + n;
  el = js_array(t_manifesto, fim, "casos");
  CHECK(el != NULL);
  while (el && el < fim) {
    char rcNome[32] = "";
    ef = js_fim(el);
    CHECK(ef && ef <= fim);
    CHECK(js_texto(el, ef, "rc", rcNome, sizeof rcNome));
    if (!strcmp(rcNome, "OK")) {
      char arq[64] = "";
      CHECK(js_texto(el, ef, "arq", arq, sizeof arq));
      if (!strncmp(arq, "boa-", 4)) nBoas++; else nRuins++;
    } else nRuins++;
    t_caso(el, ef);
    nCasos++;
    el = js_prox(ef);
    while (el && el < fim && *el != '[' && *el != '{' && *el != '"')
      el++;
  }
  CHECK(nBoas >= 40);
  CHECK(nRuins >= 80);
  printf("corpus: %d casos (%d boas, %d ruins)\n", nCasos, nBoas, nRuins);
}

// ------------------------------------------- asserts campo a campo
static void t_campos(void) {
  FarolCena c;
  FarolPeca *p;
  int rc;

  // P1 fileira — exemplo da secao 4.2, verbatim.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,"
    "\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":["
    "{\"t\":\"fileira\",\"id\":\"gosto1\","
    "\"titulo\":{\"m\":\"FT_FIL_PORQUE_TERMINOU\",\"a\":[\"tt11280740\"]},"
    "\"sub\":{\"m\":\"FT_FIL_SUB_TEMA\",\"a\":[\"k9951\"]},"
    "\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c4\",\"c1\",\"c7\",\"c2\",\"c9\"],"
    "\"motivo\":{\"c0\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]}}}]}",
    12);
  CHECK(rc == 1);
  CHECK(!strcmp(c.tela, "home") && c.perfil == 2 && c.feita == 1795000000 &&
        c.vale_s == 86400 && !strcmp(c.origem, "regras"));
  CHECK(c.opt == (FP_O_PERFIL | FP_O_FEITA | FP_O_VALE | FP_O_ORIGEM));
  p = &c.pecas[0];
  CHECK(p->tipo == FP_FILEIRA);
  CHECK(!strcmp(p->u.fileira.id, "gosto1"));
  CHECK(!strcmp(p->u.fileira.titulo.m, "FT_FIL_PORQUE_TERMINOU"));
  CHECK(!strcmp(p->u.fileira.titulo.a[0], "tt11280740"));
  CHECK(p->u.fileira.titulo.nArgs == 1);
  CHECK(p->u.fileira.forma == FP_FORMA_CARTAZ);
  CHECK(p->u.fileira.nItens == 6);
  CHECK(p->u.fileira.itens[1].c == 4);
  CHECK(!strcmp(p->u.fileira.itens[1].s, "c4"));
  CHECK(p->u.fileira.nMotivo == 1 && p->u.fileira.motivo[0].item == 0);
  CHECK(!strcmp(p->u.fileira.motivo[0].t.m, "FT_MOT_PESSOA"));

  // P2 dica — acao abrir com alvo tt.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"home\",\"pecas\":["
    "{\"t\":\"dica\",\"id\":\"d_serie_parada\","
    "\"l1\":{\"m\":\"FT_DICA_PAROU_L1\",\"a\":[\"tt0903747\",\"2\",\"4\"]},"
    "\"l2\":{\"m\":\"FT_DICA_PAROU_L2\",\"a\":[\"5\"]},"
    "\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt0903747\"},"
    "\"icone\":\"aj_farol\"}]}",
    12);
  CHECK(rc == 1 && c.opt == 0);                     // nenhum opcional veio
  p = &c.pecas[0];
  CHECK(p->tipo == FP_DICA);
  CHECK(p->u.dica.acao == FP_ACAO_ABRIR && p->u.dica.temAlvo);
  CHECK(p->u.dica.alvo.c == -1 && !strcmp(p->u.dica.alvo.s, "tt0903747"));
  CHECK(!strcmp(p->u.dica.icone, "aj_farol"));

  // P3 porque — confianca 2, 1 linha, 3 selos na ordem.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"detalhe\",\"pecas\":["
    "{\"t\":\"porque\",\"alvo\":\"tt15239678\",\"confianca\":2,"
    "\"linhas\":[{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1160419\",\"k4565\"]}],"
    "\"selos\":[{\"k\":\"pessoa\",\"id\":137427},{\"k\":\"tema\",\"id\":4565},"
    "{\"k\":\"genero\",\"id\":878}]}]}",
    12);
  CHECK(rc == 1 && !strcmp(c.tela, "detalhe"));
  p = &c.pecas[0];
  CHECK(p->tipo == FP_PORQUE);
  CHECK(p->u.porque.alvo.c == -1);
  CHECK(p->u.porque.confianca == 2 && p->u.porque.nLinhas == 1);
  CHECK(p->u.porque.nSelos == 3);
  CHECK(p->u.porque.selos[0].k == FP_SELO_PESSOA && p->u.porque.selos[0].id == 137427);
  CHECK(p->u.porque.selos[2].k == FP_SELO_GENERO);

  // P4 comparar — eixos na ordem + escolha aponta o indice dentro de itens.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"comparar\",\"pecas\":["
    "{\"t\":\"comparar\",\"itens\":[\"c0\",\"c3\",\"c5\"],"
    "\"eixos\":[\"duracao\",\"nota\",\"clima\",\"porque\",\"qualidade\",\"termina\"],"
    "\"veredito\":{\"m\":\"FT_CMP_VEREDITO_CABE\",\"a\":[\"c3\"]},"
    "\"escolha\":\"c3\"}]}",
    12);
  CHECK(rc == 1);
  p = &c.pecas[0];
  CHECK(p->tipo == FP_COMPARAR);
  CHECK(p->u.comparar.nItens == 3 && p->u.comparar.nEixos == 6);
  CHECK(p->u.comparar.eixos[0] == FP_EIXO_DURACAO &&
        p->u.comparar.eixos[5] == FP_EIXO_TERMINA);
  CHECK(p->u.comparar.escolha == 1);               // c3 e itens[1]
  CHECK(!strcmp(p->u.comparar.veredito.m, "FT_CMP_VEREDITO_CABE"));

  // P5 escolha — 4 opcoes com id + r modelo.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"escolha\",\"pecas\":["
    "{\"t\":\"escolha\",\"passo\":1,\"de\":3,"
    "\"pergunta\":{\"m\":\"FT_ESC_TEMPO\"},"
    "\"opcoes\":[{\"id\":\"t45\",\"r\":{\"m\":\"FT_ESC_TEMPO_45\"}},"
    "{\"id\":\"t100\",\"r\":{\"m\":\"FT_ESC_TEMPO_FILME\"}},"
    "{\"id\":\"t999\",\"r\":{\"m\":\"FT_ESC_TEMPO_NOITE\"}},"
    "{\"id\":\"tanto\",\"r\":{\"m\":\"FT_ESC_TANTO_FAZ\"}}]}]}",
    12);
  CHECK(rc == 1);
  p = &c.pecas[0];
  CHECK(p->tipo == FP_ESCOLHA);
  CHECK(p->u.escolha.passo == 1 && p->u.escolha.de == 3);
  CHECK(p->u.escolha.nOpcoes == 4);
  CHECK(!strcmp(p->u.escolha.opcoes[3].id, "tanto"));
  CHECK(!strcmp(p->u.escolha.opcoes[3].r.m, "FT_ESC_TANTO_FAZ"));

  // P6 info — 3 linhas com k enum.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"detalhe\",\"pecas\":["
    "{\"t\":\"info\",\"alvo\":\"tt0944947\",\"itens\":["
    "{\"k\":\"qualidade\",\"l\":{\"m\":\"FT_INFO_QUALI\",\"a\":[\"4K\",\"Dolby Vision\",\"pt\"]}},"
    "{\"k\":\"continuar\",\"l\":{\"m\":\"FT_INFO_SOBE\",\"a\":[\"2\",\"6\",\"45\"]}},"
    "{\"k\":\"tempo\",\"l\":{\"m\":\"FT_INFO_TERMINA\",\"a\":[\"23:40\"]}}]}]}",
    12);
  CHECK(rc == 1);
  p = &c.pecas[0];
  CHECK(p->tipo == FP_INFO);
  CHECK(p->u.info.nItens == 3);
  CHECK(p->u.info.itens[0].k == FP_INFO_QUALIDADE);
  CHECK(p->u.info.itens[1].k == FP_INFO_CONTINUAR);
  CHECK(!strcmp(p->u.info.itens[2].l.m, "FT_INFO_TERMINA"));

  // P7 achados — motivo ancorado no item.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"home\",\"pecas\":["
    "{\"t\":\"achados\",\"l1\":{\"m\":\"FT_ACHA_CURTOS_L1\",\"a\":[\"3\",\"100\"]},"
    "\"l2\":{\"m\":\"FT_ACHA_NAO_VIU\"},\"itens\":[\"c1\",\"c2\",\"c8\"],"
    "\"motivo\":{\"c1\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g53\"]}}}]}",
    12);
  CHECK(rc == 1);
  p = &c.pecas[0];
  CHECK(p->tipo == FP_ACHADOS);
  CHECK(p->u.achados.nItens == 3 && p->u.achados.nMotivo == 1);
  CHECK(p->u.achados.motivo[0].item == 0);
  CHECK(p->u.achados.itens[2].c == 8);

  // x aprovado com reserva: usaX e o texto vem de cena.xs.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"home\",\"pecas\":["
    "{\"t\":\"dica\",\"id\":\"d\",\"l1\":{\"x\":\"Para esta sexta.\","
    "\"m\":\"FT_DICA_TRAKT\"},\"acao\":{\"tipo\":\"nada\"}}]}",
    12);
  CHECK(rc == 1 && c.nXs == 1);
  p = &c.pecas[0];
  CHECK(p->u.dica.l1.usaX && p->u.dica.l1.xi == 0);
  CHECK(!strcmp(farolpeca_x(&c, &p->u.dica.l1), "Para esta sexta."));
  CHECK(farolpeca_texto_tem(&p->u.dica.l1));

  // x sozinho: tambem vale; modelo fica vazio.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"home\",\"pecas\":["
    "{\"t\":\"dica\",\"id\":\"d\",\"l1\":{\"x\":\"Do mesmo tema.\"},"
    "\"acao\":{\"tipo\":\"nada\"}}]}",
    12);
  CHECK(rc == 1 && p != NULL);
  CHECK(c.pecas[0].u.dica.l1.usaX && !c.pecas[0].u.dica.l1.m[0]);

  // x reprovado com reserva: cai no modelo, marca xReprovada.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"home\",\"pecas\":["
    "{\"t\":\"dica\",\"id\":\"d\",\"l1\":{\"x\":\"Desculpe o inconveniente\","
    "\"m\":\"FT_DICA_TRAKT\"},\"acao\":{\"tipo\":\"nada\"}}]}",
    12);
  CHECK(rc == 1 && c.nXs == 0);
  p = &c.pecas[0];
  CHECK(!p->u.dica.l1.usaX && p->u.dica.l1.xReprovada);
  CHECK(!strcmp(p->u.dica.l1.m, "FT_DICA_TRAKT"));
  CHECK(!strcmp(farolpeca_x(&c, &p->u.dica.l1), ""));

  // pecas vazio vale: cena sem nada a mostrar.
  rc = farolpeca_ler(&c, "{\"v\":1,\"tela\":\"home\",\"pecas\":[]}", 0);
  CHECK(rc == 0 && c.nPecas == 0 && c.nRecusadas == 0);

  // nCandidatos 0: so "tt" resolve.
  rc = farolpeca_ler(&c,
    "{\"v\":1,\"tela\":\"home\",\"pecas\":["
    "{\"t\":\"dica\",\"id\":\"d\",\"l1\":{\"m\":\"FT_DICA_TRAKT\"},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt1\"}}]}",
    0);
  CHECK(rc == 1);
}

// Invariantes que valem para QUALQUER saida do validador — o fuzz confere.
static void t_invariantes(const FarolCena *c, int rc) {
  CHECK(c->nPecas >= 0 && c->nPecas <= FAROL_PECA_MAX);
  CHECK(c->nRecusadas >= 0 && c->nRecusadas <= 64);
  CHECK(c->nXs >= 0 && c->nXs <= FAROL_XS);
  if (rc >= 0) CHECK(rc == c->nPecas);
  for (int i = 0; i < FAROL_PECA_MAX; i++)
    CHECK(c->erro[i] <= FPE_LIMITE);
  for (int i = 0; i < c->nPecas; i++) {
    const FarolPeca *p = &c->pecas[i];
    CHECK(p->tipo <= FP_ACHADOS);
  }
}

// ------------------------------------------------------------- fuzz
// PRNG deterministico (splitmix64): a mesma semente repete a mesma serie.
static uint64_t t_seed;
static uint64_t t_rnd(void) {
  uint64_t z = (t_seed += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// Cenas-base para mutar: uma por familia de peca + uma grande.
static const char *const T_BASE[] = {
  "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,"
  "\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":["
  "{\"t\":\"fileira\",\"id\":\"g1\","
  "\"titulo\":{\"m\":\"FT_FIL_PORQUE_TERMINOU\",\"a\":[\"tt11280740\"]},"
  "\"sub\":{\"m\":\"FT_FIL_SUB_TEMA\",\"a\":[\"k9951\"]},"
  "\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c4\",\"c1\",\"c7\",\"c2\",\"c9\"],"
  "\"motivo\":{\"c0\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]}}},"
  "{\"t\":\"dica\",\"id\":\"d1\","
  "\"l1\":{\"m\":\"FT_DICA_PAROU_L1\",\"a\":[\"tt0903747\",\"2\",\"4\"]},"
  "\"l2\":{\"x\":\"Livre valido.\",\"m\":\"FT_DICA_PAROU_L2\",\"a\":[\"5\"]},"
  "\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt0903747\"},\"icone\":\"aj_farol\"}]}",

  "{\"v\":1,\"tela\":\"detalhe\",\"pecas\":["
  "{\"t\":\"porque\",\"alvo\":\"tt15239678\",\"confianca\":2,"
  "\"linhas\":[{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1160419\",\"k4565\"]},"
  "{\"x\":\"Linha livre.\"}],"
  "\"selos\":[{\"k\":\"pessoa\",\"id\":137427},{\"k\":\"tema\",\"id\":4565}]},"
  "{\"t\":\"info\",\"alvo\":\"tt0944947\",\"itens\":["
  "{\"k\":\"qualidade\",\"l\":{\"m\":\"FT_INFO_QUALI\",\"a\":[\"4K\",\"DV\",\"pt\"]}},"
  "{\"k\":\"continuar\",\"l\":{\"m\":\"FT_INFO_SOBE\",\"a\":[\"2\",\"6\",\"45\"]}}]}]}",

  "{\"v\":1,\"tela\":\"comparar\",\"pecas\":["
  "{\"t\":\"comparar\",\"itens\":[\"c0\",\"c3\",\"c5\"],"
  "\"eixos\":[\"duracao\",\"nota\",\"clima\",\"porque\",\"qualidade\",\"termina\"],"
  "\"veredito\":{\"m\":\"FT_CMP_VEREDITO_CABE\",\"a\":[\"c3\"]},"
  "\"escolha\":\"c3\"},"
  "{\"t\":\"escolha\",\"passo\":1,\"de\":3,"
  "\"pergunta\":{\"m\":\"FT_ESC_TEMPO\"},"
  "\"opcoes\":[{\"id\":\"t45\",\"r\":{\"m\":\"FT_ESC_TEMPO_45\"}},"
  "{\"id\":\"tanto\",\"r\":{\"m\":\"FT_ESC_TANTO_FAZ\"}}]},"
  "{\"t\":\"achados\",\"l1\":{\"m\":\"FT_ACHA_CURTOS_L1\",\"a\":[\"3\",\"100\"]},"
  "\"itens\":[\"c1\",\"c2\",\"c8\"],"
  "\"motivo\":{\"c1\":{\"x\":\"Motivo livre.\",\"m\":\"FT_MOT_GENERO\",\"a\":[\"g53\"]}}}]}",

  "{\"v\":1,\"tela\":\"home\",\"pecas\":[]}",
};

static char t_fuzz_buf[FAROL_CENA_MAX + 512];

static void t_fuzz(void) {
  FarolCena c;
  t_seed = 0xF4A0A1ull;
  for (int i = 0; i < T_FUZZ; i++) {
    const char *base = T_BASE[t_rnd() % (sizeof T_BASE / sizeof T_BASE[0])];
    size_t n = strlen(base), cap = sizeof t_fuzz_buf - 1;
    int ops = 1 + (int)(t_rnd() % 4);
    if (n > cap) n = cap;
    memcpy(t_fuzz_buf, base, n);
    for (int o = 0; o < ops; o++) {
      size_t pos = (size_t)(t_rnd() % (n + 1));
      switch (t_rnd() % 6) {
        case 0:                                   // troca um byte
          if (n) t_fuzz_buf[t_rnd() % n] = (char)t_rnd();
          break;
        case 1:                                   // insere um byte
          if (n < cap) {
            memmove(t_fuzz_buf + pos + 1, t_fuzz_buf + pos, n - pos);
            t_fuzz_buf[pos] = (char)t_rnd();
            n++;
          }
          break;
        case 2:                                   // apaga um byte
          if (n) {
            pos = t_rnd() % n;
            memmove(t_fuzz_buf + pos, t_fuzz_buf + pos + 1, n - pos - 1);
            n--;
          }
          break;
        case 3:                                   // trunca
          n = t_rnd() % (n + 1);
          break;
        case 4:                                   // duplica um trecho
          if (n && n * 2 <= cap) {
            size_t trecho = 1 + t_rnd() % (n < 32 ? n : 32);
            pos = t_rnd() % n;
            memmove(t_fuzz_buf + pos + trecho, t_fuzz_buf + pos, n - pos);
            memcpy(t_fuzz_buf + pos, t_fuzz_buf + pos + trecho, trecho);
            n += trecho;
          }
          break;
        default:                                  // char JSON estrutural
          if (n < cap) {
            static const char JC[] = "{}[],:\"\\";
            memmove(t_fuzz_buf + pos + 1, t_fuzz_buf + pos, n - pos);
            t_fuzz_buf[pos] = JC[t_rnd() % (sizeof JC - 1)];
            n++;
          }
          break;
      }
    }
    t_fuzz_buf[n] = 0;
    {
      // Copia heap do tamanho exato: leitura alem do NUL vira erro do ASan
      // (o buffer estatico de 8,7 KB a esconderia).
      char *b = malloc(n + 1);
      int rc;
      memcpy(b, t_fuzz_buf, n + 1);
      rc = farolpeca_ler(&c, b, (int)(t_rnd() % 14));
      free(b);
      CHECK(rc >= -FCE_FALTA && rc <= FAROL_PECA_MAX);
      t_invariantes(&c, rc);
    }
  }
  printf("fuzz: %d mutacoes deterministicas\n", T_FUZZ);
}

int main(void) {
  printf("sizeof(FarolCena) = %u\n", (unsigned)sizeof(FarolCena));
  t_campos();
  t_corpus();
  t_fuzz();
  if (t_falhas) { printf("farol_peca: %d FALHAS\n", t_falhas); return 1; }
  printf("farol_peca: tudo ok\n");
  return 0;
}
