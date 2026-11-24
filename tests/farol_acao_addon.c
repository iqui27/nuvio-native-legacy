// FAROL #17 — farolacao_addon.c com o anel de eventos de verdade.
//
// Reaproveita a fachada do Farol (tests/farol_fachada.c: farol.c, farolanel.c,
// farolregra.c reais e os dubles do resto do app) e acrescenta um "mundo de
// add-ons" falso, a ilha da acao e um dados_* que grava em arquivo de verdade
// (NUVIO_DADOS), para provar persistencia e recarga da quarentena.
// Relogio injetado: nenhuma espera real de 24 h.
#define main fachada_main
#include "farol_fachada.c"
#undef main
#include "addonstats.h"
#include "faroltexto.h"
#include "farolacao_addon.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "resolver.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

// ------------------------------------------------------------------ dados_*
static void caminho(char *dst, size_t n, const char *nome) {
  const char *d = getenv("NUVIO_DADOS");
  assert(d && *d);
  snprintf(dst, n, "%s/%s", d, nome);
}
char *dados_ler(const char *nome) {
  char p[512], *b;
  long n;
  FILE *f;
  caminho(p, sizeof p, nome);
  f = fopen(p, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  assert(b && fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f);
  return b;
}
static pthread_mutex_t dadosTrava = PTHREAD_MUTEX_INITIALIZER;
static int dadosFalha;   // > 0: as proximas gravacoes falham (disco cheio, flash ocupada)
int dados_gravar_leve(const char *nome, const char *conteudo) {
  char p[512];
  FILE *f;
  caminho(p, sizeof p, nome);
  pthread_mutex_lock(&dadosTrava);
  if (dadosFalha > 0) { dadosFalha--; pthread_mutex_unlock(&dadosTrava); return 0; }
  f = fopen(p, "wb");
  if (f) { fputs(conteudo, f); fclose(f); }
  pthread_mutex_unlock(&dadosTrava);
  return f != NULL;
}

// ------------------------------------------------- mundo de add-ons falso
#define FN 6
static char fkChave[FN][32];
static int  fkN, fkLento = -1;
int addons_lento_isolavel(void) { return fkLento; }
int addons_chave_estatistica(int i, char chave[32]) {
  if (i < 0 || i >= fkN) return 0;
  snprintf(chave, 32, "%s", fkChave[i]);
  return 1;
}
int addons_fontes_vivas_n(void) {
  int k = 0;
  for (int i = 0; i < fkN; i++) if (!addonstats_isolado_s(fkChave[i])) k++;
  return k;
}
static void barreira(void);
static int corridaNoMeio;   // secao 11: os dois fios ja leram a lista antes de qualquer um isolar
int addons_fontes_chaves(char (*chaves)[32], int max) {
  int k = 0;
  for (int i = 0; i < fkN && k < max; i++) memcpy(chaves[k++], fkChave[i], 32);
  if (corridaNoMeio) barreira();
  return k;
}
const char *addons_nome(int i) { return i == 1 ? "PenguPlay" : "Outro"; }
static IlhaAcao ilhaUlt;
static char ilhaCtx[64];
static int ilhaN;
void ilhaacao_feita(const IlhaAcao *a) {
  assert(a->ctxN <= sizeof ilhaCtx);
  ilhaUlt = *a;
  memcpy(ilhaCtx, a->ctx, a->ctxN);
  ilhaUlt.ctx = ilhaCtx;
  ilhaN++;
}

static time_t agora = 1800000000;
static time_t relogio(void) { return agora; }

static void mundo(int n) {
  fkN = n; fkLento = -1;
  for (int i = 0; i < n; i++) snprintf(fkChave[i], sizeof fkChave[i], "a-%016x", 0xA000 + i);
}
static void limpo(void) {
  char p[512];
  resetCenario();
  addonstats_isolar_zerar();
  caminho(p, sizeof p, "fonte-quarentena.txt");
  unlink(p);
  agora = 1800000000;
  mundo(4);
  ilhaN = 0;
}
static int isolaN(const char *c) { return farolacao_addon_isolar(c); }

static int ringN(FarolEvento *ev) { return farol_eventos(ev, FAROL_ANEL_N); }
static char *arquivo(void) { return dados_ler("fonte-quarentena.txt"); }

// Barreira de dois (pthread_barrier_t nao existe no macOS).
static pthread_mutex_t barTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t barSinal = PTHREAD_COND_INITIALIZER;
static int barChegou, barVolta;
static void barreira(void) {
  pthread_mutex_lock(&barTrava);
  int v = barVolta;
  if (++barChegou == 2) { barChegou = 0; barVolta++; pthread_cond_broadcast(&barSinal); }
  else while (v == barVolta) pthread_cond_wait(&barSinal, &barTrava);
  pthread_mutex_unlock(&barTrava);
}
typedef struct { int i, rc; } Corrida;
static void *corrida(void *u) {
  Corrida *c = u;
  barreira();
  c->rc = isolaN(fkChave[c->i]);
  return NULL;
}

typedef struct { int id; } Giro;
static void *giro(void *u) {
  int id = ((Giro *)u)->id;
  for (int k = 0; k < 300; k++) {
    char c[32];
    snprintf(c, sizeof c, "a-%016x", 0xB000 + (id * 7 + k) % 5);
    if (k % 3 == 2) farolacao_addon_desfazer(c);
    else isolaN(c);
    assert(farolacao_addon_isolados_n() <= FAAD_TETO_ISOLADOS);
    (void)farolacao_addon_isolado(c, NULL);
  }
  return NULL;
}

int main(void) {
  FarolEvento ev[FAROL_ANEL_N];
  char c[32], *t;
  int s, n;

  farolanel_relogio(relogioMock);
  addonstats_relogio(relogio);
  limpo();

  // --- 1. isola por 24 h, o prazo vence sozinho -----------------------------
  assert(isolaN(fkChave[0]) == FAAD_OK);
  assert(farolacao_addon_isolado(fkChave[0], &s) && s == 86400 && FAAD_ISOLA_S == 86400);
  agora += 43200;
  assert(farolacao_addon_isolado(fkChave[0], &s) && s == 43200);
  agora += 43199;
  assert(farolacao_addon_isolado(fkChave[0], &s) && s == 1);
  agora += 1;
  assert(!farolacao_addon_isolado(fkChave[0], &s) && s == 0);
  assert(farolacao_addon_isolados_n() == 0);
  puts("ok  24 h: de lado ate 86399 s, livre em 86400 s");

  // --- 2. anel: acao e resultado --------------------------------------------
  limpo();
  mundo(2);
  assert(isolaN(fkChave[0]) == FAAD_OK);
  n = ringN(ev);
  assert(n == 2);
  assert(ev[0].ev == FE_FAROL_ACAO && ev[0].a == FAROL_A_ISOLAR_ADDON && !strcmp(ev[0].tok, "isolar_addon"));
  assert(ev[1].ev == FE_FAROL_RESULTADO && ev[1].a == FAROL_A_ISOLAR_ADDON && ev[1].b == 1);
  assert(isolaN(fkChave[1]) == FAAD_UNICO);       // o outro ja esta de lado
  n = ringN(ev);
  assert(n == 4 && ev[3].ev == FE_FAROL_RESULTADO && ev[3].b == 0 && ev[3].sev == 1);
  puts("ok  anel: farol_registrar_acao + resultado (ok e recusa)");

  // --- 3. persistencia e recarga ---------------------------------------------
  limpo();
  assert(isolaN(fkChave[0]) == FAAD_OK);
  agora += 1000;
  assert(isolaN(fkChave[1]) == FAAD_OK);
  t = arquivo();
  assert(t && strstr(t, fkChave[0]) && strstr(t, fkChave[1]));
  free(t);
  addonstats_isolar_zerar();                           // "a TV reiniciou"
  assert(farolacao_addon_isolados_n() == 0);
  addonstats_ler();
  assert(farolacao_addon_isolado(fkChave[0], &s) && s == 86400 - 1000);
  assert(farolacao_addon_isolado(fkChave[1], &s) && s == 86400);
  agora += 86400 - 1000;                               // a primeira vence
  assert(!farolacao_addon_isolado(fkChave[0], NULL) && farolacao_addon_isolado(fkChave[1], &s) && s == 1000);
  addonstats_isolar_zerar();
  addonstats_ler();
  assert(!farolacao_addon_isolado(fkChave[0], NULL) && farolacao_addon_isolado(fkChave[1], NULL));
  puts("ok  quarentena persiste em fonte-quarentena.txt e recarrega com o prazo certo");

  // arquivo sujo: vencido, absurdo, lixo e duplicata nao entram.
  {
    char sujo[512];
    snprintf(sujo, sizeof sujo,
             "velho\t%lld\nfuturo\t%lld\nlixo sem tab\n\t5\nok-1\t%lld\nok-1\t%lld\nsem-data\tabc\n",
             (long long)agora - 5, (long long)agora + 86400LL * 400, (long long)agora + 60, (long long)agora + 90);
    addonstats_isolar_carregar(sujo);
    assert(farolacao_addon_isolados_n() == 1);
    assert(farolacao_addon_isolado("ok-1", &s) && s == 60);
    assert(!farolacao_addon_isolado("velho", NULL) && !farolacao_addon_isolado("futuro", NULL));
  }
  puts("ok  recarga descarta vencido, data absurda, lixo e duplicata");

  // --- 4. Desfazer imediato ----------------------------------------------------
  limpo();
  assert(isolaN(fkChave[2]) == FAAD_OK);
  assert(farolacao_addon_desfazer(fkChave[2]) == 1);
  assert(!farolacao_addon_isolado(fkChave[2], NULL) && farolacao_addon_isolados_n() == 0);
  assert(farolacao_addon_desfazer(fkChave[2]) == 0);   // nao ha o que desfazer
  assert(farolacao_addon_desfazer(NULL) == 0 && farolacao_addon_desfazer("") == 0);
  addonstats_isolar_zerar();
  addonstats_ler();
  assert(!farolacao_addon_isolado(fkChave[2], NULL));  // o arquivo tambem esqueceu
  puts("ok  Desfazer imediato, tambem no arquivo");

  // --- 5. teto de 3 -----------------------------------------------------------
  limpo();
  mundo(5);
  for (int i = 0; i < 3; i++) assert(isolaN(fkChave[i]) == FAAD_OK);
  assert(isolaN(fkChave[3]) == FAAD_TETO);
  assert(!farolacao_addon_isolado(fkChave[3], NULL) && farolacao_addon_isolados_n() == 3);
  assert(farolacao_addon_desfazer(fkChave[0]) == 1);
  assert(isolaN(fkChave[3]) == FAAD_OK);              // a vaga voltou com o Desfazer
  assert(isolaN(fkChave[4]) == FAAD_TETO);
  agora += FAAD_ISOLA_S;                               // tudo vence: vagas de novo
  assert(farolacao_addon_isolados_n() == 0);
  assert(isolaN(fkChave[4]) == FAAD_OK);
  puts("ok  no maximo 3 de lado; a quarta e recusada");

  // --- 6. nunca o unico de fontes ----------------------------------------------
  limpo();
  mundo(1);
  assert(isolaN(fkChave[0]) == FAAD_UNICO);
  mundo(0);
  assert(isolaN(fkChave[0]) == FAAD_UNICO);           // sem nenhuma fonte na lista
  assert(farolacao_addon_isolados_n() == 0);
  mundo(2);
  assert(isolaN(fkChave[0]) == FAAD_OK);
  assert(isolaN(fkChave[1]) == FAAD_UNICO);           // sobrou um: esse nunca sai
  assert(isolaN(fkChave[0]) == FAAD_JA_ISOLADO);
  {
    // a guarda pura: chave repetida na lista (dois add-ons com a mesma
    // configuracao) conta uma vez so; vale tambem para quem nao e de fontes.
    const char dup[3][ADDONSTATS_CHAVE_N] = { "d-a", "d-b", "d-b" };
    addonstats_isolar_zerar();
    assert(farolacao_addon_isolar_entre("d-a", dup, 3) == FAAD_OK);
    assert(farolacao_addon_isolar_entre("d-b", dup, 3) == FAAD_UNICO);
    assert(farolacao_addon_isolar_entre("so-catalogo", dup, 3) == FAAD_OK);   // d-b segue vivo
    assert(farolacao_addon_isolar_entre("x", dup, 0) == FAAD_UNICO);
  }
  puts("ok  recusa isolar o unico add-on de fontes");

  // --- 7. reisolar nao renova o prazo; chave invalida -----------------------------
  limpo();
  assert(isolaN(fkChave[0]) == FAAD_OK);
  agora += 1000;
  assert(isolaN(fkChave[0]) == FAAD_JA_ISOLADO);
  assert(farolacao_addon_isolado(fkChave[0], &s) && s == 86400 - 1000);
  assert(isolaN(NULL) == FAAD_INVALIDO && isolaN("") == FAAD_INVALIDO);
  {
    char longa[80];
    memset(longa, 'x', sizeof longa - 1); longa[sizeof longa - 1] = 0;
    assert(isolaN(longa) == FAAD_INVALIDO);
  }
  puts("ok  ja isolado nao renova; chave invalida recusada");

  // --- 8. isolar_lento: escolhe, isola e avisa na ilha com Desfazer -----------------
  limpo();
  assert(farolacao_addon_isolar_lento() == FAAD_NENHUM && ilhaN == 0);
  fkLento = 1;
  assert(farolacao_addon_isolar_lento() == FAAD_OK);
  assert(ilhaN == 1 && farolacao_addon_isolado(fkChave[1], NULL));
  assert(!strcmp(ilhaUlt.frase, FT_ADDON_ISOLADO) && !strcmp(ilhaUlt.titulo, "PenguPlay"));
  assert(!strcmp(ilhaUlt.onde, FT_ADDON_DE_LADO) && ilhaUlt.desfazer && ilhaUlt.tipo == ILHA_INFO);
  ilhaUlt.desfazer(ilhaUlt.ctx);                       // o botao "Desfazer" da ilha
  assert(!farolacao_addon_isolado(fkChave[1], NULL) && farolacao_addon_isolados_n() == 0);
  mundo(1);
  fkLento = 0;
  assert(farolacao_addon_isolar_lento() == FAAD_UNICO && ilhaN == 1);   // sem aviso quando recusa
  assert(farolacao_addon_isolados_n() == 0);
  puts("ok  isolar_lento: ilha com Desfazer; recusa nao avisa");

  // --- 9. despacho de debrid e fontes ------------------------------------------------
  limpo();
  _Static_assert(FAAD_PEDIU_NADA == RESOLVER_PEDIU_NADA, "contrato nada");
  _Static_assert(FAAD_PEDIU_FONTES == RESOLVER_PEDIU_FONTES, "contrato fontes");
  _Static_assert(FAAD_PEDIU_DEBRID == RESOLVER_PEDIU_DEBRID, "contrato debrid");
  assert(farolacao_addon_pedido(FAROL_A_ABRIR_FONTES) == RESOLVER_PEDIU_FONTES);
  assert(farolacao_addon_pedido(FAROL_A_ABRIR_DEBRID) == RESOLVER_PEDIU_DEBRID);
  assert(farolacao_addon_pedido(FAROL_A_RECONECTAR_DEBRID) == RESOLVER_PEDIU_DEBRID);
  n = ringN(ev);
  assert(n == 3);
  assert(ev[0].ev == FE_FAROL_ACAO && !strcmp(ev[0].tok, "login_addon") && ev[0].a == FAROL_A_ABRIR_FONTES);
  assert(ev[1].ev == FE_FAROL_ACAO && !strcmp(ev[1].tok, "login_debrid") && ev[1].a == FAROL_A_ABRIR_DEBRID);
  assert(ev[2].ev == FE_FAROL_ACAO && !strcmp(ev[2].tok, "login_debrid") && ev[2].a == FAROL_A_RECONECTAR_DEBRID);
  assert(farolacao_addon_pedido(FAROL_A_TENTAR_DE_NOVO) == RESOLVER_PEDIU_NADA);
  assert(farolacao_addon_pedido(FAROL_A_ISOLAR_ADDON) == RESOLVER_PEDIU_NADA);
  assert(farolacao_addon_pedido(FAROL_A_NENHUMA) == RESOLVER_PEDIU_NADA);
  assert(farolacao_addon_pedido(-1) == RESOLVER_PEDIU_NADA && farolacao_addon_pedido(9999) == RESOLVER_PEDIU_NADA);
  assert(ringN(ev) == 3);                              // quem nao e desta familia nao registra
  farolacao_addon_resultado(FAROL_A_ABRIR_DEBRID, 1);  // voltou logado e reabriu o video
  farolacao_addon_resultado(FAROL_A_RECONECTAR_DEBRID, 0);
  farolacao_addon_resultado(FAROL_A_ISOLAR_ADDON, 1);  // nao e desta familia: ignora
  n = ringN(ev);
  assert(n == 5 && ev[3].ev == FE_FAROL_RESULTADO && ev[3].b == 1 && ev[4].ev == FE_FAROL_RESULTADO && ev[4].b == 0);
  // As regras do Farol que apontam para estas acoes existem e batem com o pedido.
  {
    int nr, achou = 0;
    const FarolRegra *tab = farolregra_tabela(&nr);
    for (int i = 0; i < nr; i++) {
      if (tab[i].codigo == FAROL_C_ADDON_NENHUM) { assert(farolacao_addon_pedido(tab[i].acao) == RESOLVER_PEDIU_FONTES); achou |= 1; }
      if (tab[i].codigo == FAROL_C_DEBRID_SEM_PLANO) { assert(farolacao_addon_pedido(tab[i].acao) == RESOLVER_PEDIU_DEBRID); achou |= 2; }
      if (tab[i].codigo == FAROL_C_DEBRID_DESCONECTOU) { assert(farolacao_addon_pedido(tab[i].acao) == RESOLVER_PEDIU_DEBRID); achou |= 4; }
      if (tab[i].codigo == FAROL_C_ADDON_LENTO) assert(tab[i].acao == FAROL_A_ISOLAR_ADDON);
    }
    assert(achou == 7);
  }
  puts("ok  fontes/debrid: pedido ao app (resolver_pedido) e anel");

  // --- 10. varios fios (ASan/UBSan/TSan) ------------------------------------------------
  limpo();
  {
    pthread_t th[4];
    Giro g[4];
    for (int i = 0; i < 4; i++) { g[i].id = i; assert(pthread_create(&th[i], NULL, giro, &g[i]) == 0); }
    for (int i = 0; i < 4; i++) pthread_join(th[i], NULL);
    assert(farolacao_addon_isolados_n() <= FAAD_TETO_ISOLADOS);
  }
  (void)c;
  puts("ok  varios fios mantem o teto");

  // --- 11. dois fios, duas fontes: nunca as duas de lado (P1 da FAROL17-r1) --------
  for (int volta = 0; volta < 200; volta++) {
    pthread_t th[2];
    Corrida cr[2];
    limpo();
    mundo(2);
    // voltas impares: a pior intercalacao, forcada (os dois leem 2 fontes antes
    // de qualquer isolamento); pares: o escalonador decide.
    corridaNoMeio = volta & 1;
    for (int i = 0; i < 2; i++) { cr[i].i = i; assert(pthread_create(&th[i], NULL, corrida, &cr[i]) == 0); }
    for (int i = 0; i < 2; i++) pthread_join(th[i], NULL);
    corridaNoMeio = 0;
    assert((cr[0].rc == FAAD_OK && cr[1].rc == FAAD_UNICO) || (cr[0].rc == FAAD_UNICO && cr[1].rc == FAAD_OK));
    assert(addons_fontes_vivas_n() == 1 && farolacao_addon_isolados_n() == 1);
  }
  puts("ok  dois fios com 2 fontes: 1 FAAD_OK, 1 FAAD_UNICO, 1 fonte viva");

  // --- 12. gravacao que falha nao perde a quarentena nem o Desfazer ----------------
  limpo();
  dadosFalha = 1;
  assert(isolaN(fkChave[0]) == FAAD_OK);              // memoria ok, disco falhou
  t = arquivo();
  assert(!t);
  addonstats_isolar_salvar();                          // a proxima tentativa grava
  t = arquivo();
  assert(t && strstr(t, fkChave[0]));
  free(t);
  dadosFalha = 1;
  assert(farolacao_addon_desfazer(fkChave[0]) == 1);  // Desfazer com o disco falhando
  addonstats_isolar_salvar();
  t = arquivo();
  assert(t && !strstr(t, fkChave[0]));                 // o arquivo esqueceu tambem
  free(t);
  addonstats_isolar_zerar();
  addonstats_ler();
  assert(!farolacao_addon_isolado(fkChave[0], NULL));  // o reinicio nao ressuscita
  puts("ok  falha de escrita fica marcada e a proxima gravacao acerta o arquivo");

  puts("farol_acao_addon: tudo ok");
  return 0;
}
