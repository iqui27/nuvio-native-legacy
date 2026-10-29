// Respostas tardias e paginas incompletas: usa os mesmos limites dublados do
// teste offline, com mudancas deterministicas durante a resposta HTTP.
#define main contaoffline_roteiro
#define perfis_ativo roteiro_perfis_ativo
#define perfis_ativo_addons roteiro_perfis_ativo_addons
#define sessao_tabela roteiro_sessao_tabela
#define sessao_rpc roteiro_sessao_rpc
#define contalib_ler_biblioteca roteiro_contalib_ler_biblioteca
#include "contaoffline.c"
#undef main
#undef perfis_ativo
#undef perfis_ativo_addons
#undef sessao_tabela
#undef sessao_rpc
#undef contalib_ler_biblioteca

static int perfilTeste = 1, trocaResposta, paginaStatus, paginaN;
static int vistosTeste, repeatPage;
static char *paginaTeste;
static int leiturasBibTeste, bibAtivaTeste = 900;

int contalib_ler_biblioteca(const char *j) {
  leiturasBibTeste++;
  bibAtivaTeste = 500;
  return roteiro_contalib_ler_biblioteca(j);
}

int perfis_ativo(void) { return perfilTeste; }
int perfis_ativo_addons(void) { return perfilTeste; }
char *sessao_tabela(const char *t, const char *q, int *st) {
  char *r = roteiro_sessao_tabela(t, q, st);
  if (trocaResposta == 1) perfilTeste = 2;
  if (trocaResposta == 2) usuario = "conta-b";
  if (trocaResposta == 3) contacache_esquecer();
  trocaResposta = 0;
  return r;
}
char *sessao_rpc(const char *funcao, const char *corpo, int *st) {
  if (paginaTeste && !strcmp(funcao, vistosTeste ? "sync_pull_watched_items" : "sync_pull_library")) {
    if (paginaN++ && !repeatPage) { *st = paginaStatus; return strdup("falha de pagina"); }
    *st = 200;
    return strdup(paginaTeste);
  }
  return roteiro_sessao_rpc(funcao, corpo, st);
}

#define NV_PERFIL_SESSAO_FORTE
#include "../src/debrid.h"

// Cycle RPCs now carry the captured token. This fixture never logs in;
// the empty token still has to reach the existing sessao_* fakes.
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t geracao) { return geracao == perfis_sessao_geracao(); }
int perfis_sessao_travar(uint64_t geracao) { return perfis_sessao_atual(geracao); }
void perfis_sessao_destravar(void) {}
int sessao_identidade_local_copiar(char *usuarioOut, size_t nUsuario, char *jwt, size_t nJwt) {
  (void)nUsuario; (void)nJwt;
  usuarioOut[0] = jwt[0] = 0;
  return 0;
}
void sessao_token_recusado(const char *tok) { (void)tok; }
char *nuvem_tabela(const char *t, const char *q, const char *bearer, int *st) {
  (void)bearer;
  return sessao_tabela(t, q, st);
}
char *nuvem_rpc_com(const char *funcao, const char *corpo, const char *bearer, int *st) {
  (void)bearer;
  return sessao_rpc(funcao, corpo, st);
}
int perfis_puxar_sessao(uint64_t sessao, const char *usuarioIn, const char *jwt) {
  (void)sessao; (void)usuarioIn; (void)jwt; return 0;
}
int syncprog_puxar_sessao(int perfil, uint64_t sessao) { (void)perfil; (void)sessao; return 0; }
int syncprog_empurrar_sessao(int perfil, uint64_t sessao) { (void)perfil; (void)sessao; return 0; }
void debrid_conta_iniciar(DebridConta *conta) { if (conta) memset(conta, 0, sizeof *conta); }
void debrid_conta_definir(DebridConta *conta, const char *servico, const char *chave) {
  (void)conta; (void)servico; (void)chave;
}
int debrid_aplicar_chaves_conta(const DebridConta *conta, uint64_t sessao) {
  (void)conta; (void)sessao; return 0;
}
void desc_sessao_preparada(void) {}
void desc_sessao_passo(void) {}
void desc_abrir_sessao(void) {}
void extras_abrir_sessao(void) {}
void perfis_invalidar_sessao(void) {}
void recomenda_esquecer_conta(void) {}
int sessao_token_copiar(char *dst, size_t n) { if (dst && n) dst[0] = 0; return 0; }
#include "../src/contalib.h"
void ajustes_perfil_padrao(void) {}
int ajustes_fonte_biblioteca(void) { return 0; }
int contalib_n(void) { return 0; }
const ContaLibItem *contalib_item(int i) { (void)i; return NULL; }
void iptv_contas_esquecer_todas(void) {}
void m3u_fontes_invalidar(void) {}
void stalker_fontes_invalidar(void) {}
void xtream_fontes_invalidar(void) {}

// Os pontos de cache sao estaticos; incluir a implementacao permite controlar
// a resposta no instante preciso, sem sleeps nem corrida entre fios do teste.
#include "../src/sync.c"
#include "nvtmp.h"

static void cicloTeste(void) {
  perfilTeste = perfilDoCiclo = 1;
  usuario = "conta-a";
  snprintf(usuarioDoCiclo, sizeof usuarioDoCiclo, "%s", usuario);
  tokenDoCiclo[0] = 0;
  sessaoDoCiclo = perfis_sessao_geracao();
  copiaGeracaoDoCiclo = contacache_geracao();
  foraCiclo = copiaCiclo = 0;
  temAddonsRem = 0;
}
static int semCopia(const char *sup, int perfil, const char *dono) {
  char *c = contacache_ler(sup, perfil, dono, NULL);
  int vazio = c == NULL;
  free(c);
  return vazio;
}
static void paginaIncompleta(int vistos, int st, int temCopia) {
  const char *sup = vistos ? CC_VISTOS : CC_BIBLIOTECA;
  int i, n;
  char *b = NULL, *c;
  cicloTeste();
  paginaN = 0;
  vistosTeste = vistos;
  paginaStatus = st;
  if (temCopia) contacache_gravar(sup, 1, usuario, "[{\"ultima_boa\":true}]");
  else contacache_esquecer();
  copiaGeracaoDoCiclo = contacache_geracao();
  paginaTeste = malloc(12000);
  strcpy(paginaTeste, "[");
  for (i = 0; i < (vistos ? CONTALIB_VISTO_PAGINA : CONTALIB_PAGINA); i++)
    strcat(paginaTeste, i ? ",{\"x\":1}" : "{\"x\":1}");
  strcat(paginaTeste, "]");
  n = vistos ? puxarVistos(1, &b) : puxarBiblioteca(1, &b);
  n = guardarOuCopia(sup, 1, n, &b);
  c = contacache_ler(sup, 1, usuario, NULL);
  confere("pagina falhada nao sobrescreve a ultima copia boa",
          temCopia ? c && strstr(c, "ultima_boa") : c == NULL);
  if (contacache_falha_transitoria(st)) {
    confere("queda em pagina posterior aparece no estado do servidor", foraCiclo == st);
    confere("usa a copia inteira; sem copia descarta o parcial recebido",
            temCopia ? n == 1 && b && strstr(b, "ultima_boa")
                     : n == -1 && b == NULL);
    if (!temCopia && !vistos) {
      bibBlob = b;
      b = NULL;
      temBibBlob = n >= 0;
      bibAtivaTeste = 900;
      leiturasBibTeste = 0;
      fioVivo = fioPronto = 1;
      sync_passo(1000);
      confere("500 linhas seguidas de HTTP 503 preservam a biblioteca ativa sem cache",
              !leiturasBibTeste && bibAtivaTeste == 900 && !bibBlob);
    }
  } else {
    confere("recusa 400 nao se passa por indisponibilidade do servidor", foraCiclo == 0);
    confere("hard page failure never publishes a partial library", n == -1 && b == NULL);
  }
  free(c); free(b); free(paginaTeste); paginaTeste = NULL;
}

int main(void) {
  int t;
  unsigned anterior;
  char *c;
  setvbuf(stdout, NULL, _IOLBF, 0);
  for (t = 1; t <= 3; t++) {
    contacache_esquecer();
    cicloTeste();
    trocaResposta = t;
    puxarAddons();
    confere("resposta tardia nao aplica nem grava no perfil/conta novos",
            !temAddonsRem && semCopia(CC_ADDONS, 2, "conta-a") &&
            semCopia(CC_ADDONS, 1, "conta-b") && semCopia(CC_ADDONS, 1, "conta-a"));
  }
  anterior = contacache_geracao();
  contacache_esquecer();
  confere("gravacao iniciada antes do logout nao recria o arquivo",
          !contacache_gravar_geracao(CC_ADDONS, 1, "conta-a", "[]", anterior) &&
          semCopia(CC_ADDONS, 1, "conta-a"));
  contacache_gravar(CC_ADDONS, 1, "conta-a", "[{\"perfil\":1}]");
  contacache_gravar(CC_ADDONS, 2, "conta-a", "[{\"perfil\":2}]");
  c = contacache_ler(CC_ADDONS, 2, "conta-a", NULL);
  confere("perfis distintos conservam suas proprias listas", c && strstr(c, "\"perfil\":2"));
  free(c);
  paginaIncompleta(0, 503, 1);
  paginaIncompleta(1, 429, 1);
  paginaIncompleta(0, 503, 0);
  paginaIncompleta(0, 400, 1);
  repeatPage = 1;
  paginaIncompleta(0, 200, 1);
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
