void servidores_esquecer_todos(void);   // servidores.c: wipe every profile's token
#include "sync.h"
#include <time.h>
#include "sessao.h"
#include "nuvem.h"
#include "perfis.h"
#include "perfisessao.h"
#include "perfilinfantil.h"
#include "agenda.h"
#include "lembrete.h"
#include "dados.h"
#include "addons.h"
#include "plugins.h"
#include "debrid.h"
#include "stalker.h"
#include "xtream.h"
#include "iptvfontes.h"
#include "m3u.h"
#include "colecoes.h"
#include "colfileiras.h"
#include "contalib.h"
#include "visto.h"
#include "salvosorg.h"
#include "salvos.h"
#include "mapa.h"
#include "recomenda.h"
#include "fontepref.h"
#include "fontevolta.h"
#include "legmemoria.h"
#include "audiomemoria.h"
#include "buscasrec.h"
#include "trakt.h"
#include "traktauth.h"
#include "catalogo.h"
#include "vistoep.h"
#include "progresso.h"
#include "perfilcont.h"
#include "psparede.h"
#include "syncprog.h"
#include "contapend.h"
#include "ajustes.h"
#include "idioma.h"
#include "catordem.h"
#include "catordemcache.h"
#include "contacache.h"
#include "descoberta.h"
#include "simkl.h"
#include "homeestado.h"
#include "cachearte.h"
#include "extras.h"
#include "arteescolha.h"
#include "guia.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#define SY_ADD_MAX   128   // o mesmo teto de ADD_MAX (addons.c)
// URL lida da conta ANTES de caber em AddonRemoto: maior que o limite de
// proposito, para o log dizer o tamanho de verdade em vez de "nao coube".
#define SY_URL_LEITURA (NV_ADDON_URL_MAX * 8)   // 16 KB: URL grande vira apelido (addonurl.h, #203)

static pthread_t fio;
static int fioVivo, fioPronto;
// A ENTREGA DO FIO DE SYNC PARA O PRINCIPAL (#203). `fioPronto` e
// `addonsCedo` sao porteiras: o fio enche buffers (catHomeBlob, colBlob,
// addonsRem...) e SO DEPOIS liga a porteira; o principal ve a porteira e
// consome/libera os buffers. Escrita e leitura simples nao garantem essa ordem
// no ARM da TV (nem para o compilador): o principal podia ver a porteira antes
// dos ponteiros e dar free() num valor velho. Release na escrita, acquire na
// leitura. O TSAN apontava a porteira (sync.c:1249 x 1485) e, por ela, os
// campos que ela protege (cicloInterrompido em 1499).
//
// O que continua sem barreira, de proposito, e documentado: `estado`,
// `fioVivo` e `perfilAplicado` lidos pelo fio da descoberta
// (esperaAddonsDaConta -> sync_estado/sync_perfil_pronto) — inteiros de
// sondagem, relidos a cada volta de espera; nenhum ponteiro sai deles.
#define PRONTO_PUBLICAR(v, x) __atomic_store_n(&(v), (x), __ATOMIC_RELEASE)
#define PRONTO_LER(v)         __atomic_load_n(&(v), __ATOMIC_ACQUIRE)
static SyncEstado estado = SYNC_PARADO;
// Resumos com numero NAO guardam o texto pronto: o idioma pode mudar depois do
// ciclo (Ajustes) e a linha ficava no idioma de quando sincronizou (#312, em
// portugues com o app em ingles). Guardam os dados; sync_resumo() monta e
// traduz a cada leitura. tipo RES_FIXO = frase fixa em `fixo`.
//
// ESCRITO PELO FIO DO CICLO, LIDO NO QUADRO (tela de ajustes): tudo sob
// resumoTrava, e sync_resumo() tira uma copia inteira antes de montar a linha.
// Sem isso o quadro lia o tipo novo com os numeros velhos, ou a frase no meio
// de um snprintf. tests/sync_resumo_corrida.sh.
enum { RES_FIXO, RES_FORA_COPIA, RES_FORA_SEM, RES_CICLO };
typedef struct {
  int  tipo, http, trakt, pend, n[5];
  char data[32];
  char fixo[220];
} SyncResumo;
static SyncResumo resumoDados = { .tipo = RES_FIXO, .fixo = "sem sincronizar" };
static pthread_mutex_t resumoTrava = PTHREAD_MUTEX_INITIALIZER;
static void resumoPublicar(const SyncResumo *r) {
  pthread_mutex_lock(&resumoTrava); resumoDados = *r; pthread_mutex_unlock(&resumoTrava);
}
static void resumoFixo(const char *frase) {
  SyncResumo r;
  memset(&r, 0, sizeof r);
  r.tipo = RES_FIXO;
  snprintf(r.fixo, sizeof r.fixo, "%s", frase);
  resumoPublicar(&r);
}
static unsigned ultimoOk;
static int sujoProgresso, sujoAjustes;
static int addonsPerfilCiclo;
#define SY_ADD_PREFIXO "conta-addons-pend-"

static AddonRemoto addonsRem[SY_ADD_MAX];
static int nAddonsRem, temAddonsRem;
// Addons prontos ANTES do fim do ciclo. Ver a publicacao antecipada em
// sync_passo. `volatile` porque quem escreve e o fio de sync e quem le e o fio
// principal, e a barreira aqui e a mesma que `fioPronto` sempre foi: o valor so
// e ligado DEPOIS de o buffer estar cheio.
static int addonsCedo;   // PRONTO_PUBLICAR/PRONTO_LER, ver fioPronto
// Progresso pronto nao espera biblioteca, colecoes nem credenciais. A caixa
// de syncprog pertence ao principal depois desta publicacao; rodar guarda a
// contagem no proprio ciclo e nao volta a le-la enquanto ela e consumida.
static int progressoCedo;
static int progressoAplicadoCedo; // so o principal: reavaliar se os ajustes da conta mudarem
// Ordem e colecoes nao dependem do progresso nem das subidas deste ciclo.
// Os dois corpos deixam de pertencer ao fio depois desta porteira; nenhum
// push os consulta. A prontidao da Home e distinta do ciclo completo aplicado.
static int estruturaCedo, estruturaAplicada, estruturaPerfilAplicado;
static NvPerfilSessao estruturaSessaoAplicada;

static char traktTok[300];
static int  temTraktRem;

// Chaves de servico que a conta guarda e o app lia de arquivo do dono.
// MEDIDO na conta real: os provedores presentes sao animeskip, debrid:*,
// introdb, mdblist e tmdb — e NAO ha "trakt". O leitor de trakt continua aqui
// porque a RPC e a mesma e a linha aparece assim que o app web a escrever.
static char tmdbKey[120], mdbKey[120];
static int  temTmdb, temMdb;
static DebridConta debridDoCiclo;
static int temDebridRem;


// Contagens do ultimo ciclo, para o resumo da tela de ajustes poder dizer a
// verdade em vez de "sincronizado" sem qualificar. Vistos e biblioteca deixaram
// de ser SO contagem — ver a nota grande na secao "so leitura".
static int cVistos, cBiblio, cColecoes, temAjustesPerfil, temCatHome;

// Blob de ajustes do perfil, cru, esperando ser aplicado no fio principal — e,
// desde o push de ajustes (#85), GUARDADO depois disso: ele e a BASE da costura
// que sobe. Ver ajustes_mesclar_blob.
// `aplicarAjustes` comeca ligado: no arranque nao ha mudanca local para
// preservar, e e ai que a conta tem de mandar.
//
// ALOCADO, e nao um vetor fixo. MEDIDO na TV com uma conta de verdade: o blob
// nao coube em 4096 bytes e o app recusou aplicar — a recusa estava certa
// (aplicar metade das opcoes traria metade da conta e metade do padrao), mas o
// efeito era o recurso simplesmente nao funcionar. O app web guarda no mesmo
// objeto muito mais chaves do que este app conhece, e escolher um teto aqui e
// escolher uma conta que nao vai funcionar.
static char *ajustesBlob;
// Sobe a cada blob novo em ajustesBlob. "Blob novo" era o PONTEIRO mudar, e o
// do perfil seguinte, do mesmo tamanho, cai no endereco que
// sync_reaplicar_ajustes acabou de soltar (#378, revisao).
static unsigned ajustesBlobGeracao;
// DE QUAL PERFIL e o blob (#378, revisao): o ciclo do perfil anterior pode
// entregar o dele DEPOIS da troca (sync_reaplicar_ajustes ja soltou o velho).
static int ajustesBlobPerfil = -1;
static int  temAjustesBlob;
static int  aplicarAjustes = 1;
// No local/default upload until the main thread has restored this login.
static int ajustesRestaurando = 1;
// Detect file-backed edits without building the full account JSON on the UI.
static unsigned long long ajustesUltimoLocal;
static int ajustesTemLocal;
static void ajustesDetectarMudancas(void) {
  if (__atomic_load_n(&ajustesRestaurando, __ATOMIC_ACQUIRE)) return;
  unsigned long long hash = ajustes_sync_hash();
  if (ajustesTemLocal && hash != ajustesUltimoLocal) sync_proteger_ajustes_locais();
  ajustesUltimoLocal = hash; ajustesTemLocal = 1;
}
// Flag em disco: a pessoa ja mudou ajustes nesta TV depois do ultimo
// sync_reaplicar_ajustes. Sem ela, CADA arranque comecava com aplicarAjustes=1
// e o blob da conta (ainda sem as mudancas locais pendentes de envio)
// sobrescrevia ajustes.txt. Ver sync_proteger_ajustes_locais.
#define SY_AJUSTES_LOCAIS "ajustes-locais.txt"

static void carregarProtecaoAjustes(void) {
  static int ja = 0;
  char *t;
  if (ja) return;
  ja = 1;
  t = dados_ler(SY_AJUSTES_LOCAIS);
  if (t && t[0] == '1') {
    aplicarAjustes = 0;
    __atomic_store_n(&ajustesRestaurando, 0, __ATOMIC_RELEASE);
    sujoAjustes = 1;
    printf("[sync] ajustes locais protegidos: blob da conta nao reaplica neste arranque\n");
    fflush(stdout);
  }
  free(t);
}

// A ordem das fileiras da home, crua, tambem esperando o fio principal. Nao e
// contada como as outras so-leitura: ela e a home da pessoa, e ate agora a
// resposta chegava, virava um numero no resumo e era jogada fora.
static char *catHomeBlob;
static int   temCatHomeBlob;
// O cache da ordem e por perfil. Quando a pessoa troca de perfil, a ordem
// anterior precisa sair antes de a nova entrar; quando o app atualiza, ela
// entra antes da primeira resposta de rede.
static int catordemCachePerfil = -1;
// De qual perfil e o ciclo no ar, e o que ele deixou pendente. Ver
// sync_iniciar (pedido com o fio vivo) e sync_passo (repeticao ao terminar).
static int perfilDoCiclo, cicloInterrompido, pedidoComFioVivo;
// De qual perfil e o ultimo ciclo COMPLETO aplicado em sync_passo (0 = nenhum
// desde a ultima troca). Ver sync_perfil_pronto.
static int perfilAplicado;
static char *colBlob;       // sync_pull_collections, lido por colecoes.c no fio principal
static int   temColBlob;
// sync_pull_library e sync_pull_watched_items, crus, lidos por contalib.c no
// fio principal. Guardar o corpo em vez de contar e o conserto deste issue: as
// duas respostas eram liberadas depois de contadas.
static char *bibBlob;
static int   temBibBlob;
static char *vistosBlob;
static int   temVistosBlob;

// SERVIDOR DA CONTA FORA DO AR (#215, 02/10/2026: api.nuvio.tv em 504/502).
// O fio escreve as variaveis "Ciclo"; sync_passo publica nas outras quando o
// ciclo e aplicado, pela mesma porteira de `fioPronto` que o resto usa.
//   foraCiclo: HTTP da primeira falha TRANSITORIA deste ciclo (-1 = sem
//              resposta, 0 = o servidor respondeu);
//   copiaCiclo: quantas superficies sairam da copia local (contacache.c);
//   copiaQuandoCiclo: de quando e a copia (a dos addons, se usada).
static int  foraCiclo, copiaCiclo;
// Os ADDONS deste ciclo: 0 = vieram da conta, 1 = da copia, 2 = falharam sem
// copia. Separado de foraCiclo porque um 429 so na ordem de catalogos nao
// deixa a home sem addon nenhum, e o aviso da ilha e sobre os addons.
static int  addonsCiclo, addonsFora;
// O que puxarAddons devolveu NESTE ciclo (1 = a conta respondeu). Um 4xx nao
// derruba o ciclo, entao `estado` nao diz se a lista chegou; o botao do #360
// precisa saber para nao dizer "em dia" sobre uma leitura que falhou.
static int  addonsLidosCiclo;
static long copiaQuandoCiclo;
static int  foraHttp, usandoCopia;
static long copiaQuando;
// Status HTTP da ultima puxarBlob (-2 quando nem perguntou: RPC ausente).
static int  ultimoSt;
static char usuarioDoCiclo[80];
static char tokenDoCiclo[3000];
static unsigned copiaGeracaoDoCiclo;
static NvPerfilSessao sessaoDoCiclo;

static int contaDoCicloAtual(void) {
  return sessao_logada() && !strcmp(usuarioDoCiclo, sessao_usuario()) &&
         copiaGeracaoDoCiclo == contacache_geracao();
}
static int perfilDoCicloAtual(void) {
  return contaDoCicloAtual() && perfilDoCiclo == perfis_ativo() &&
         nv_perfil_sessao_atual(sessaoDoCiclo);
}

// The worker keeps the identity captured before dispatch. Resolving a live
// JWT here could send an old profile's settings under a newly logged-in user.
static char *pedidoDoCiclo(const char *nome, const char *corpo, int tabela, int *st) {
  char *r;
  char usuario[80], token[3000];
  int status = 0;
  if (st) *st = 0;
  // Empty token still reaches the transport: offline fixtures inject HTTP
  // status there, and a real server answers 401 instead of looking offline.
  if (!perfilDoCicloAtual()) return NULL;
  r = tabela ? nuvem_tabela(nome, corpo, tokenDoCiclo, &status)
             : nuvem_rpc_com(nome, corpo, tokenDoCiclo, &status);
  // One renewal remains possible, but only for this unchanged session. The
  // renewal API itself refuses a token belonging to a superseded login.
  if (status == 401 && perfilDoCicloAtual()) {
    sessao_token_recusado(tokenDoCiclo);
    if (sessao_identidade_local_copiar(usuario, sizeof usuario, token, sizeof token) &&
        !strcmp(usuario, usuarioDoCiclo) && perfilDoCicloAtual()) {
      snprintf(tokenDoCiclo, sizeof tokenDoCiclo, "%s", token);
      free(r);
      r = tabela ? nuvem_tabela(nome, corpo, tokenDoCiclo, &status)
                 : nuvem_rpc_com(nome, corpo, tokenDoCiclo, &status);
    }
    memset(token, 0, sizeof token);
  }
  if (!perfilDoCicloAtual()) { free(r); return NULL; }
  if (st) *st = status;
  return r;
}

static char *rpcDoCiclo(const char *nome, const char *corpo, int *st) {
  return pedidoDoCiclo(nome, corpo, 0, st);
}

static char *tabelaDoCiclo(const char *nome, const char *consulta, int *st) {
  return pedidoDoCiclo(nome, consulta, 1, st);
}

// #360 MAO UNICA: a pendencia de edicao local de addons deixou de existir —
// nada nesta versao grava nem envia conta-addons-pend-*.txt. Os arquivos
// gravados pela versao anterior (de TODAS as contas/perfis, inclusive os
// temporarios interrompidos) sao apagados na primeira execucao, sem nunca
// serem enviados. Uma varredura por processo basta: ninguem mais os recria.
static void addonsEsquecer(void) {
  static int migrado;
  DIR *d;
  struct dirent *e;
  const char *pasta;
  if (migrado) return;
  pasta = dados_dir();
  if (!pasta || !*pasta) return;   // sem diretorio ainda: tenta na proxima chamada
  migrado = 1;
  if ((d = opendir(pasta)) != NULL) {
    while ((e = readdir(d)) != NULL) {
      const char *nome = e->d_name, *s;
      if (strncmp(nome, SY_ADD_PREFIXO, sizeof SY_ADD_PREFIXO - 1)) continue;
      s = nome + sizeof SY_ADD_PREFIXO - 1;
      while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f')) s++;
      if (s == nome + sizeof SY_ADD_PREFIXO - 1 || strncmp(s, "-p", 2)) continue;
      s += 2;
      char *fim;
      long perfil = strtol(s, &fim, 10);
      if (perfil < 1 || perfil > 32 ||
          (strcmp(fim, ".txt") && strcmp(fim, ".txt.tmp"))) continue;
      if (!dados_apagar(nome)) printf("[sync] nao foi possivel apagar pendencia de addons na migracao\n");
    }
    closedir(d);
  } else {
    printf("[sync] nao foi possivel conferir pendencias de addons na migracao\n");
  }
}

static void falhaServidor(int st) { if (!foraCiclo) foraCiclo = st ? st : -1; }

static void avisoCopia(const char *sup, int st, long quando) {
  char d[32];
  contacache_data(quando, d, sizeof d);
  if (st) printf("[sync] servidor da conta indisponivel (HTTP %d): usando a copia de %s (%s)\n",
                 st, d, sup);
  else    printf("[sync] servidor da conta indisponivel (sem resposta): usando a copia de %s (%s)\n",
                 d, sup);
  fflush(stdout);
}

static void avisoSemCopia(const char *sup, int st) {
  if (st) printf("[sync] servidor da conta indisponivel (HTTP %d) e nenhuma copia de %s neste aparelho\n",
                 st, sup);
  else    printf("[sync] servidor da conta indisponivel (sem resposta) e nenhuma copia de %s neste aparelho\n",
                 sup);
  fflush(stdout);
}

// ---------------------------------------------------------------- utilitarios

static int ok2xx(const char *r, int st) { return r && st >= 200 && st < 300; }

// ---------------------------------------------------------------- addons

// Destination and long-URL buffer belong to the caller. The drawing thread
// can restore B's cache while A's account worker still owns its staged reply.
static int lerAddonsPara(const char *r, AddonRemoto *saida, char *url) {
  const char *p;
  int k = 0, fora = 0;
  for (p = js_raiz_array(r); p && k < SY_ADD_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char b[16];
    memset(&saida[k], 0, sizeof saida[k]);
    if (!js_texto(p, f, "url", url, SY_URL_LEITURA)) continue;
    js_texto(p, f, "name", saida[k].nome, sizeof saida[k].nome);
    // A URL QUE NAO CABE NAO ENTRA CORTADA (#201). js_texto corta em silencio
    // no tamanho do destino, e era aqui que a URL do Comet (870) virava 599: o
    // addon recebia meia configuracao e respondia como se ela fosse valida.
    // #203: a que passa de NV_ADDON_URL_MAX (AIOLists tem 2 a 8 KB) vira
    // apelido; so some quando passa dos 16 KB da leitura ou a tabela enche.
    if (strlen(url) >= SY_URL_LEITURA - 1 ||
        !nv_addon_url_guardar(saida[k].nome, url, saida[k].url, sizeof saida[k].url)) {
      fora++; continue;
    }
    // #360: a conta pode trazer o MESMO addon duas vezes (heranca do tempo em
    // que o push da TV duplicava a lista dela). Dedupe pela chave do addon
    // (nv_addon_chave, addonurl.h; B4: URL grande e host sem caixa): fica a primeira ocorrencia,
    // com a URL exata que a conta mandou.
    { int i; char base[NV_ADDON_URL_MAX], bi[NV_ADDON_URL_MAX];
      nv_addon_chave(saida[k].url, base, sizeof base);
      if (!base[0]) continue;
      for (i = 0; i < k; i++) {
        nv_addon_chave(saida[i].url, bi, sizeof bi);
        if (!strcmp(bi, base)) break;
      }
      if (i < k) continue;
    }
    // Ausente conta como LIGADO: e assim que o web le, e um addon que some por
    // causa de um campo que o servidor nao mandou e pior que um a mais.
    saida[k].ativo = js_bruto(p, f, "enabled", b, sizeof b)
                         ? (strcmp(b, "false") != 0) : 1;
    k++;
  }
  // O TETO CORTAVA EM SILENCIO (#203): o laco para em SY_ADD_MAX sem contar o
  // resto. Contar aqui deixa no log que a TV ficou com MENOS addons que a
  // conta — a diferenca entre "a conta mudou" e "nao coube".
  for (; p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if (js_texto(p, f, "url", url, SY_URL_LEITURA)) fora++;
  }
  if (fora) printf("[sync] addons: %d da conta ficaram de fora (maximo %d, ou URL grande demais)\n",
                   fora, SY_ADD_MAX);
  return k;
}

static int lerAddons(const char *r) {
  static char url[SY_URL_LEITURA]; // worker-only staging
  return lerAddonsPara(r, addonsRem, url);
}

// A lista aplicada fica separada da resposta recebida: so aplicar confirma
// o snapshot. No boot, restaura antes da rede; pulls iguais preservam sondas.
static int addonsListaIgual(const AddonRemoto *a, int na, const AddonRemoto *b, int nb) {
  int i;
  if (na != nb) return 0;
  for (i = 0; i < na; i++) {
    char ua[NV_ADDON_URL_MAX], ub[NV_ADDON_URL_MAX];
    nv_addon_base(a[i].url, ua, sizeof ua);
    nv_addon_base(b[i].url, ub, sizeof ub);
    if (strcmp(ua, ub) || !!a[i].ativo != !!b[i].ativo) return 0;
  }
  return 1;
}
static pthread_mutex_t addonsToggleTrava = PTHREAD_MUTEX_INITIALIZER;

static int addonsAchar(const AddonRemoto *lista, int n, const char *url) {
  char key[NV_ADDON_URL_MAX], other[NV_ADDON_URL_MAX];
  nv_addon_chave(url, key, sizeof key);
  for (int i = 0; i < n; i++) {
    nv_addon_chave(lista[i].url, other, sizeof other);
    if (!strcmp(key, other)) return i;
  }
  return -1;
}

static void addonsGuardar(const char *superficie, const AddonRemoto *lista, int n,
                          int perfil, const char *usuario, unsigned geracao) {
  Jsw w;
  jsw_iniciar(&w); jsw_arr_ini(&w);
  for (int i = 0; i < n; i++) {
    const char *longa = nv_longa_bruto(lista[i].url);
    jsw_obj_ini(&w);
    jsw_cs(&w, "url", longa ? longa : lista[i].url);
    jsw_cs(&w, "name", lista[i].nome);
    jsw_cb(&w, "enabled", lista[i].ativo);
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  const char *txt = jsw_texto_final(&w);
  if (txt) contacache_gravar_geracao(superficie, perfil, usuario, txt, geracao);
  jsw_livre(&w);
}

// Pending flags are durable, account/profile scoped, and never change membership.
static void addonsEnviarToggles(const AddonRemoto *remotos, int nr) {
  AddonRemoto *pend = calloc(SY_ADD_MAX, sizeof *pend);
  char *url = malloc(SY_URL_LEITURA);
  char *saved;
  struct timespec agora;
  clock_gettime(CLOCK_MONOTONIC, &agora);
  long long fim = (long long)agora.tv_sec * 1000 + agora.tv_nsec / 1000000 + 2000;
  if (!pend || !url) { free(pend); free(url); return; }
  pthread_mutex_lock(&addonsToggleTrava);
  saved = contacache_ler(CC_ADDONS_TOGGLES, perfilDoCiclo, usuarioDoCiclo, NULL);
  pthread_mutex_unlock(&addonsToggleTrava);
  int n = saved ? lerAddonsPara(saved, pend, url) : 0;
  for (int i = 0; i < n && perfilDoCicloAtual(); i++) {
    clock_gettime(CLOCK_MONOTONIC, &agora);
    long long restante = fim - ((long long)agora.tv_sec * 1000 + agora.tv_nsec / 1000000);
    // Whole seconds also bound the WGT adapter, whose timeout uses seconds.
    if (restante < 1000) break;
    // A row the account no longer has needs no write: the pull removes it.
    int j = addonsAchar(remotos, nr, pend[i].url);
    if (j < 0) { pend[i].url[0] = 0; continue; }
    // Use the exact remote URL, quoted for PostgREST and URL escaped.
    Jsw w;
    char escaped[SY_URL_LEITURA * 3], dono[240], query[SY_URL_LEITURA * 3 + 400];
    const char *longa = nv_longa_bruto(remotos[j].url);
    jsw_iniciar(&w); jsw_str(&w, longa ? longa : remotos[j].url);
    nuvem_url_escapar(jsw_texto_final(&w), escaped, sizeof escaped);
    jsw_livre(&w);
    nuvem_url_escapar(perfis_dono(), dono, sizeof dono);
    snprintf(query, sizeof query, "user_id=eq.%s&profile_id=eq.%d&url=eq.%s",
             dono, addonsPerfilCiclo, escaped);
    int st = 0;
    char *r = nuvem_addon_alternar(query, pend[i].ativo, tokenDoCiclo,
                                   (unsigned)(restante / 1000) * 1000, &st);
    if (ok2xx(r, st) && perfilDoCicloAtual() && js_raiz_array(r) &&
        strchr(js_raiz_array(r), '{')) pend[i].url[0] = 0;
    free(r);
  }
  int restantes = 0;
  for (int i = 0; i < n; i++)
    if (pend[i].url[0]) pend[restantes++] = pend[i];
  pthread_mutex_lock(&addonsToggleTrava);
  char *current = contacache_ler(CC_ADDONS_TOGGLES, perfilDoCiclo, usuarioDoCiclo, NULL);
  if (saved && current && !strcmp(saved, current) && perfilDoCicloAtual())
    addonsGuardar(CC_ADDONS_TOGGLES, pend, restantes, perfilDoCiclo,
                  usuarioDoCiclo, copiaGeracaoDoCiclo);
  free(current);
  pthread_mutex_unlock(&addonsToggleTrava);
  free(saved); free(pend); free(url);
}

static AddonRemoto addonsTogglesRem[SY_ADD_MAX];
static int nAddonsTogglesRem;

static int addonsAplicar(void) {
  static AddonRemoto local[SY_ADD_MAX];
  int n = addons_exportar(local, SY_ADD_MAX), mudou = 0, i;
  char *anterior;
  const char *txt;
  Jsw w;
  // Lista vazia da conta nao substitui a local (addons_definir_lista recusa)
  // nem vira snapshot: gravar "[]" apagaria a ultima lista boa do arranque.
  if (!nAddonsRem) return 0;
  // Flag per remote addon, first match wins: unacknowledged toggle of this
  // profile; local flag when the list in memory is this profile's (or the
  // package list before any account list); else the remote value. Other
  // clients write default/unsupported enabled values, so existing addon flags
  // belong to this app; only membership/order come from the pull.
  static AddonRemoto pend[SY_ADD_MAX];
  static char buf[SY_URL_LEITURA];
  pthread_mutex_lock(&addonsToggleTrava);
  char *c = contacache_ler(CC_ADDONS_TOGGLES, perfilDoCiclo, usuarioDoCiclo, NULL);
  pthread_mutex_unlock(&addonsToggleTrava);
  int np = c ? lerAddonsPara(c, pend, buf) : 0;
  free(c);
  int donoLista = addons_perfil_da_lista();
  int preservarFlags = !donoLista || donoLista == perfis_ativo();
  for (i = 0; i < nAddonsRem; i++) {
    int j = addonsAchar(pend, np, addonsRem[i].url);
    if (j >= 0) addonsRem[i].ativo = pend[j].ativo;
    else if (preservarFlags && (j = addonsAchar(local, n, addonsRem[i].url)) >= 0)
      addonsRem[i].ativo = local[j].ativo;
  }
  if (!addonsListaIgual(local, n, addonsRem, nAddonsRem))
    mudou = addons_definir_lista(addonsRem, nAddonsRem);
  jsw_iniciar(&w); jsw_arr_ini(&w);
  for (i = 0; i < nAddonsRem; i++) {
    const char *longa = nv_longa_bruto(addonsRem[i].url);
    jsw_obj_ini(&w);
    jsw_cs(&w, "url", longa ? longa : addonsRem[i].url);
    jsw_cs(&w, "name", addonsRem[i].nome);
    jsw_cb(&w, "enabled", addonsRem[i].ativo);
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  txt = jsw_texto_final(&w);
  anterior = contacache_ler(CC_ADDONS_APLICADOS, addonsPerfilCiclo, usuarioDoCiclo, NULL);
  if (txt && (!anterior || strcmp(anterior, txt)))
    contacache_gravar_geracao(CC_ADDONS_APLICADOS, addonsPerfilCiclo, usuarioDoCiclo,
                              txt, copiaGeracaoDoCiclo);
  free(anterior); jsw_livre(&w);
  return mudou;
}
static char addonsRestauradosUsuario[80];
static int addonsRestauradosPerfil;
static void addonsRestaurar(void) {
  int perfil = perfis_ativo_addons();
  char *c;
  if (addonsRestauradosPerfil == perfil && !strcmp(addonsRestauradosUsuario, sessao_usuario())) return;
  c = contacache_ler(CC_ADDONS_APLICADOS, perfil, sessao_usuario(), NULL);
  if (c) {
    static AddonRemoto local[SY_ADD_MAX], guardados[SY_ADD_MAX];
    static char url[SY_URL_LEITURA]; // drawing-thread cache restoration
    int n = addons_exportar(local, SY_ADD_MAX), k = lerAddonsPara(c, guardados, url);
    if (!addonsListaIgual(local, n, guardados, k)) addons_definir_lista(guardados, k);
    addons_marcar_da_conta(perfis_ativo());
    free(c);
  }
  snprintf(addonsRestauradosUsuario, sizeof addonsRestauradosUsuario, "%s", sessao_usuario());
  addonsRestauradosPerfil = perfil;
}

// 1 = a conta respondeu; 0 = falhou e nao adianta repetir (4xx, tabela
// ausente); -1 = o SERVIDOR falhou (sem resposta, 429, 5xx) — o ciclo inteiro
// passa a usar as copias locais (ver rodar).
static int puxarAddons(void) {
  char consulta[400], dono[80];
  char *r;
  int st = 0, k, perfilAddons = addonsPerfilCiclo;

  if (!perfis_dono()[0]) return 0;
  nuvem_url_escapar(perfis_dono(), dono, sizeof dono);
  // MEDIDO: `sync_pull_addons` NAO EXISTE neste servidor (PGRST202), e a
  // tabela `tv_addons` tambem nao (PGRST205). O unico caminho que responde e a
  // tabela `addons`, que e exatamente o caminho feliz do app web.
  // perfis_ativo_addons(), nao perfis_ativo(): um perfil marcado com
  // uses_primary_plugins LE os addons do perfil 1. Pedindo pelo indice dele a
  // resposta vinha vazia e o perfil abria sem addon nenhum.
  snprintf(consulta, sizeof consulta,
           "user_id=eq.%s&profile_id=eq.%d&select=*&order=sort_order.asc",
           dono, perfilAddons);
  // Com a chave anonima o RLS responde 401 "permission denied for table
  // addons": ler as linhas de alguem exige o token de quem esta pedindo.
  r = tabelaDoCiclo("addons", consulta, &st);
  // O pedido pertence ao perfil/conta de antes da rede. Uma troca durante a
  // resposta nao pode guardar nem aplicar a lista antiga no novo perfil.
  if (!perfilDoCicloAtual() || perfilAddons != perfis_ativo_addons()) {
    free(r);
    return 0;
  }
  if (!ok2xx(r, st)) {
    int ausente = r && nuvem_erro_ausente(r);
    if (ausente) printf("[sync] tabela addons ausente\n");
    else if (st) printf("[sync] leitura de addons: HTTP %d\n", st);
    free(r);
    if (ausente || !contacache_falha_transitoria(st)) return 0;
    // A COPIA DA ULTIMA RESPOSTA BOA (#215). Sem ela, um servidor fora do ar
    // deixava o app sem addon nenhum — e sem addon nao ha catalogo, a home
    // abria vazia em toda TV ao mesmo tempo.
    falhaServidor(st);
    { long q = 0;
      char *c = contacache_ler(CC_ADDONS, perfilAddons, usuarioDoCiclo, &q);
      k = c ? lerAddons(c) : 0;
      free(c);
      if (k > 0) {
        nAddonsRem = k;
        temAddonsRem = 1;
        copiaCiclo++;
        copiaQuandoCiclo = q;
        avisoCopia(CC_ADDONS, st, q);
        addonsCiclo = 1;
      } else if (k <= 0) { avisoSemCopia(CC_ADDONS, st); addonsCiclo = 2; }
    }
    return -1;
  }
  k = lerAddons(r);
  // So uma lista NAO VAZIA vira copia: um 200 com array vazio por um perfil
  // errado (o caso do Mane155, abaixo) nao pode apagar a copia boa.
  if (k > 0) contacache_gravar_geracao(CC_ADDONS, perfilAddons, usuarioDoCiclo,
                                      r, copiaGeracaoDoCiclo);
  free(r);
  nAddonsRem = k;
  // The main thread may merge/consume addonsRem as soon as it is published.
  // PATCHes use a worker-owned snapshot and never mutate that mailbox.
  nAddonsTogglesRem = k;
  memcpy(addonsTogglesRem, addonsRem, (size_t)k * sizeof *addonsTogglesRem);
  temAddonsRem = 1;
  PRONTO_PUBLICAR(addonsCedo, 1);
  // REGISTRA O PERFIL PEDIDO, e nao so o resultado. O defeito que levou a esta
  // linha (perfil secundario com "0 addons", relato do Mane155) era um pedido
  // BEM FORMADO para o perfil errado: o PostgREST responde 200 com array vazio,
  // sem erro nenhum, e nao havia no log uma unica pista de qual profile_id
  // tinha sido consultado. `perfil != ativo` com `0 linha(s)` e a assinatura
  // exata dessa classe de erro.
  printf("[sync] addons: perfil %d (ativo %d) -> %d linha(s)\n",
         perfis_ativo_addons(), perfis_ativo(), k);
  return 1;
}

// ---------------------------------------------------------------- plugins
// Repositorios de plugins Nuvio (F09): tabela `plugins`, o mesmo caminho do web
// (select por user_id/profile_id, ordem por sort_order) e o mesmo perfil dos
// addons (uses_primary_plugins le o do perfil 1). ACK por revisao: ver
// plugins.h. Pendencia local primeiro sobe (inclusive lista VAZIA, quando a
// pessoa removeu o ultimo repositorio) e so com 2xx e confirmada; push falho
// mantem a pendencia e nao deixa a leitura passar por cima.
static void sincronizarPlugins(void) {
  static PlugRetrato s;            // fio do sync e unico; 24 KB fora da pilha
  static PlugRepo l[PLUG_REPOS_MAX];
  char consulta[400], dono[80];
  char *r;
  int st = 0, k, perfil = addonsPerfilCiclo;
  if (!perfis_dono()[0]) return;
  plugins_retrato(&s);
  if (s.pendente) {
    Jsw w;
    int i;
    jsw_iniciar(&w);
    jsw_obj_ini(&w);
    jsw_ci(&w, "p_profile_id", perfil);
    jsw_chave(&w, "p_plugins");
    jsw_arr_ini(&w);
    for (i = 0; i < s.n; i++) {
      jsw_obj_ini(&w);
      jsw_cs(&w, "url", s.lista[i].url);
      jsw_cs(&w, "name", s.lista[i].nome);
      jsw_cb(&w, "enabled", s.lista[i].ativo);
      jsw_ci(&w, "sort_order", i);
      // O tipo que veio da conta volta igual: o repositorio DEX do Android
      // nao roda aqui, mas nao pode sumir da conta por isso.
      jsw_cs(&w, "repo_type", s.lista[i].tipo[0] ? s.lista[i].tipo : "NUVIO_JS");
      jsw_obj_fim(&w);
    }
    jsw_arr_fim(&w);
    jsw_obj_fim(&w);
    r = rpcDoCiclo("sync_push_plugins", jsw_texto_final(&w), &st);
    jsw_livre(&w);
    if (ok2xx(r, st) && perfilDoCicloAtual()) {
      int ok = plugins_confirmar(s.rev, s.geracao);
      printf("[sync] plugins: %d repositorio(s) enviados%s\n", s.n,
             ok ? "" : " (edicao nova ou perfil trocado: pendencia mantida)");
    } else {
      printf("[sync] push de plugins falhou (HTTP %d): edicao local mantida\n", st);
      free(r);
      return;
    }
    free(r);
    plugins_retrato(&s);
    if (s.pendente) return;        // editou de novo durante o push: proximo ciclo
  }
  nuvem_url_escapar(perfis_dono(), dono, sizeof dono);
  snprintf(consulta, sizeof consulta,
           "user_id=eq.%s&profile_id=eq.%d&select=*&order=sort_order.asc", dono, perfil);
  r = tabelaDoCiclo("plugins", consulta, &st);
  if (!ok2xx(r, st) || !perfilDoCicloAtual() || perfil != perfis_ativo_addons()) {
    if (r && nuvem_erro_ausente(r)) printf("[sync] tabela plugins ausente\n");
    else if (st && !ok2xx(r, st)) printf("[sync] leitura de plugins: HTTP %d\n", st);
    free(r);
    return;
  }
  k = plugins_ler_conta(r, l, PLUG_REPOS_MAX);
  free(r);
  if (k < 0) { printf("[sync] plugins: resposta nao e lista, ignorada\n"); return; }
  printf("[sync] plugins: perfil %d -> %d repositorio(s)\n", perfil, k);
  plugins_definir_da_conta(l, k, s.geracao);
}

// ---------------------------------------------------------------- credenciais

static void puxarCredenciais(void) {
  Jsw w;
  char *r;
  int st = 0;
  const char *p;

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfilDoCiclo);
  jsw_obj_fim(&w);
  r = rpcDoCiclo("sync_pull_provider_credentials", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (!ok2xx(r, st) || !perfilDoCicloAtual()) { free(r); return; }
  debrid_conta_iniciar(&debridDoCiclo);
  temDebridRem = 1;

  // Trakt, debrid e mdblist compartilham as MESMAS RPC, separados so pelo campo
  // `provider`. Uma leitura serve para os tres.
  for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char prov[48], cred[900];
    if (!js_texto(p, f, "provider", prov, sizeof prov)) continue;
    if (!js_bruto(p, f, "credential_json", cred, sizeof cred)) continue;
    if (!strcmp(prov, "trakt")) {
      // O credential_json pode vir como OBJETO ou como string JSON — o web
      // trata os dois. Aqui basta procurar a chave dentro do texto cru.
      char tk[300];
      if (js_texto(cred, cred + strlen(cred), "access_token", tk, sizeof tk)) {
        snprintf(traktTok, sizeof traktTok, "%s", tk);
        temTraktRem = 1;
      }
    }
    else if (!strcmp(prov, "tmdb")) {
      if (js_texto(cred, cred + strlen(cred), "api_key", tmdbKey, sizeof tmdbKey))
        temTmdb = 1;
    }
    else if (!strcmp(prov, "mdblist")) {
      if (js_texto(cred, cred + strlen(cred), "api_key", mdbKey, sizeof mdbKey))
        temMdb = 1;
    }
    else if (!strncmp(prov, "debrid:", 7)) {
      // A chave solta serve para resolver torrent sem url (debrid.c). Quem tem
      // a chave embutida na URL do addon nao e afetado: esses ja vem com url.
      char k[200];
      if (js_texto(cred, cred + strlen(cred), "api_key", k, sizeof k))
        debrid_conta_definir(&debridDoCiclo, prov + 7, k);
    }
  }
  free(r);
}

// ---------------------------------------------------------------- progresso
//
// Vive em syncprog.c (pull/push/aplicar) sobre progresso.c (o registro local).
// Saiu daqui por dois motivos: para ter teste sem subir o ciclo inteiro, e
// porque o formato que este arquivo mandava divergia do web em tres pontos
// (chave, tipo de serie, hora) — PLANO-PROGRESSO.md, Parte 1.

// ---------------------------------------------------------------- so leitura

// ESTAS SUPERFICIES ERAM CONTADAS E JOGADAS FORA, e era o defeito relatado.
//
// O que estava escrito aqui antes — "o app nativo nao tem tela propria para
// elas" — era verdade para os SALVOS e para os VISTOS quando este arquivo
// nasceu, e deixou de ser: a tela de Biblioteca existe (src/biblioteca.c) e o
// historico de "assistido" existe (cat_historico_definir_id). O codigo nao
// acompanhou. O efeito medido pelo relator (@Haylefal, webOS 4) foi uma
// Biblioteca vazia com o selo "LOCAL" numa conta que tinha itens: a resposta
// chegava, virava um numero no resumo dos ajustes e o corpo era liberado.
//
// Continua valendo o outro lado da regra: o app PUXA e nao EMPURRA nenhuma
// destas. Empurrar uma lista que este app nao edita mandaria lista vazia, e
// lista vazia apaga o dado nos outros aparelhos da pessoa (secao 1.6, regra 2).
//
// RPC que o servidor nao tem NAO e perguntada de novo — uma viagem por ciclo,
// para sempre, contra um erro que nunca muda.
#define SY_AUSENTES 8
static const char *ausentes[SY_AUSENTES];
static int nAusentes;

static int jaAusente(const char *funcao) {
  int i;
  for (i = 0; i < nAusentes; i++)
    if (!strcmp(ausentes[i], funcao)) return 1;
  return 0;
}

// Puxa uma RPC de array, CONTA as linhas e GUARDA o corpo em *destino, para o
// fio principal interpretar. Devolve a contagem, ou -1 quando nao houve
// resposta util (e ai *destino fica como estava).
//
// Substituiu um `contarRpc` que so devolvia o numero. As colecoes ja mostravam
// por que ele nao servia: para ficar com o corpo, elas chamavam a MESMA RPC
// DUAS VEZES por ciclo — uma para contar, outra para guardar. O ciclo tinha
// oito requisicoes e uma delas era uma copia exata da anterior.
//
// O corpo NAO e interpretado aqui. Este e o fio de rede; mexer no catalogo, nas
// colecoes ou nas fileiras daqui e mexer num vetor que o desenho esta lendo no
// mesmo instante — a mesma razao pela qual os addons esperam sync_passo.
static int puxarBlob(const char *funcao, const char *corpo, char **destino) {
  char *r;
  int st = 0, k = 0;
  const char *p;
  ultimoSt = -2;
  if (jaAusente(funcao)) return -1;
  r = rpcDoCiclo(funcao, corpo, &st);
  ultimoSt = st;
  if (!ok2xx(r, st)) {
    if (r && nuvem_erro_ausente(r)) {
      ultimoSt = -2;
      printf("[sync] %s nao existe neste servidor\n", funcao);
      if (nAusentes < SY_AUSENTES) ausentes[nAusentes++] = funcao;
    } else if (st) {
      printf("[sync] %s: HTTP %d\n", funcao, st);
    }
    free(r);
    return -1;
  }
  for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) k++;
  if (destino) { free(*destino); *destino = r; }
  else free(r);
  return k;
}

// Cola o array JSON `pagina` no fim do array `*acum` (os dois `[...]`, como
// toda RPC do Supabase responde). Devolve 1 se colou. Nao valida o JSON: quem
// le e o contalib, com js.c, que ja tolera o que vier.
static int colarArray(char **acum, const char *pagina) {
  const char *a, *b, *fa;
  size_t na, nb;
  char *novo;
  if (!*acum) return 0;
  a = strchr(pagina, '[');
  b = strrchr(pagina, ']');
  if (!a || !b || b <= a) return 0;
  a++;
  while (a < b && (unsigned char)*a <= ' ') a++;
  if (a >= b) return 1;                      // pagina vazia: nada a colar
  fa = strrchr(*acum, ']');
  if (!fa) return 0;
  na = (size_t)(fa - *acum);
  nb = (size_t)(b - a);
  novo = (char *)malloc(na + nb + 3);
  if (!novo) return 0;
  memcpy(novo, *acum, na);
  // Array acumulado vazio ("[]") nao ganha virgula antes do primeiro item.
  { size_t z = na;
    while (z > 0 && (unsigned char)novo[z - 1] <= ' ') z--;
    na = z;
    if (na > 0 && novo[na - 1] != '[') novo[na++] = ','; }
  memcpy(novo + na, a, nb);
  novo[na + nb] = ']';
  novo[na + nb + 1] = 0;
  free(*acum);
  *acum = novo;
  return 1;
}

// `sync_pull_library` inteira, em paginas de CONTALIB_PAGINA, ate uma vir
// incompleta ou CONTALIB_PAGINAS paginas. Devolve o total de linhas (-1 quando
// any page fails). An incomplete snapshot must never replace membership.
static int puxarBiblioteca(int perfil, char **destino) {
  char corpo[160];
  char *acum = NULL, *pag = NULL;
  int pagina, total = 0, c;
  for (pagina = 0; pagina < CONTALIB_PAGINAS; pagina++) {
    snprintf(corpo, sizeof corpo,
             "{\"p_profile_id\":%d,\"p_limit\":%d,\"p_offset\":%d}",
             perfil, CONTALIB_PAGINA, pagina * CONTALIB_PAGINA);
    c = puxarBlob("sync_pull_library", corpo, pagina ? &pag : &acum);
    if (c < 0) {
      printf("[sync] biblioteca: pagina %d failed; partial snapshot discarded\n", pagina + 1);
      free(acum); free(pag);
      return -1;
    }
    if (pagina && pag) {
      if (!colarArray(&acum, pag)) {
        printf("[sync] biblioteca: page %d assembly failed; partial snapshot discarded\n", pagina + 1);
        free(acum); free(pag);
        return -1;
      }
      free(pag); pag = NULL;
    }
    total += c;
    if (c < CONTALIB_PAGINA) break;
    if (pagina + 1 == CONTALIB_PAGINAS) {
      printf("[sync] biblioteca: page cap reached; partial snapshot discarded\n");
      free(acum);
      return -1;
    }
  }
  free(pag);
  if (destino) { free(*destino); *destino = acum; }
  else free(acum);
  return total;
}

// OS VISTOS DA CONTA, todas as paginas (ate CONTALIB_VISTO_PAGINAS), coladas
// num array so — o mesmo caminho de puxarBiblioteca. Pagina 2 em diante que
// falha nao derruba a primeira: fica o que veio, e o log diz.
static int puxarVistos(int perfil, char **destino) {
  char corpo[160];
  char *acum = NULL, *pag = NULL;
  int pagina, total = 0, c;
  for (pagina = 1; pagina <= CONTALIB_VISTO_PAGINAS; pagina++) {
    snprintf(corpo, sizeof corpo,
             "{\"p_profile_id\":%d,\"p_page\":%d,\"p_page_size\":%d}",
             perfil, pagina, CONTALIB_VISTO_PAGINA);
    c = puxarBlob("sync_pull_watched_items", corpo, pagina > 1 ? &pag : &acum);
    if (c < 0) {
      if (pagina == 1) return -1;
      printf("[sync] vistos: pagina %d falhou; ficando com %d linhas\n", pagina, total);
      break;
    }
    if (pagina > 1 && pag) {
      if (!colarArray(&acum, pag)) {
        printf("[sync] vistos: pagina %d nao colou; ficando com %d linhas\n", pagina, total);
        free(pag); pag = NULL;
        break;
      }
      free(pag); pag = NULL;
    }
    total += c;
    if (c < CONTALIB_VISTO_PAGINA) break;
    if (pagina == CONTALIB_VISTO_PAGINAS)
      printf("[sync] vistos: %d linhas baixadas e a conta tem mais; o resto nao "
             "foi pedido\n", total);
  }
  free(pag);
  if (destino) { free(*destino); *destino = acum; }
  else free(acum);
  return total;
}

// A copia local de uma superficie so-leitura, no lugar da resposta que o
// servidor nao deu (#215). Mesmo contrato de puxarBlob: devolve a contagem (e o
// corpo em *destino) ou -1 sem copia.
static int copiaBlob(const char *sup, int perfil, int st, char **destino) {
  long q = 0;
  char *c;
  const char *p;
  int k = 0;
  if (!perfilDoCicloAtual()) return -1;
  c = contacache_ler(sup, perfil, usuarioDoCiclo, &q);
  if (!c) { avisoSemCopia(sup, st); return -1; }
  for (p = js_raiz_array(c); p; p = js_prox(js_fim(p))) k++;
  free(*destino);
  *destino = c;
  copiaCiclo++;
  if (!copiaQuandoCiclo) copiaQuandoCiclo = q;
  avisoCopia(sup, st, q);
  return k;
}

// Depois de puxar uma superficie: resposta boa vira a copia; falha do servidor
// troca pela copia. `n` e o que puxarBlob/puxarBiblioteca/puxarVistos
// devolveu; o retorno e o que fica valendo.
static int guardarOuCopia(const char *sup, int perfil, int n, char **blob) {
  if (!perfilDoCicloAtual()) return -1;
  if (n < 0) { free(*blob); *blob = NULL; }
  // Uma pagina posterior pode falhar depois de n linhas chegarem. Essa lista
  // parcial nao substitui a ultima copia boa nem esconde a queda do servidor.
  if (contacache_falha_transitoria(ultimoSt)) {
    int st = ultimoSt;
    int copia;
    falhaServidor(st);
    copia = copiaBlob(sup, perfil, st, blob);
    if (copia < 0) {
      // Sem snapshot inteiro, conserva a superficie que ja esta em memoria.
      // Publicar so a primeira pagina faria os demais titulos desaparecerem.
      free(*blob);
      *blob = NULL;
    }
    return copia;
  }
  // Collections can be a collections_json object rather than a root array.
  // Preserve that successful response for offline use, including count zero.
  if ((n > 0 || (n == 0 && !strcmp(sup, CC_COLECOES))) &&
      *blob && ultimoSt >= 200 && ultimoSt < 300 &&
      (strcmp(sup, CC_COLECOES) || col_resposta_valida(*blob)))
    contacache_gravar_geracao(sup, perfil, usuarioDoCiclo, *blob,
                              copiaGeracaoDoCiclo);
  return n;
}

// O blob de ajustes NAO e contado, e lido: ele e o layout da pessoa. Ate agora
// esta RPC so alimentava um numero no resumo, e as ~40 preferencias vinham dos
// padroes transcritos a mao do perfil de quem montou o pacote.
static int puxarAjustesPerfil(const char *corpo) {
  char *r;
  int st = 0, ok = 0;
  const char *p;
  carregarProtecaoAjustes();
  // PUXA SEMPRE, inclusive com a protecao local ligada — e a mudanca desta
  // versao. O blob e a BASE da costura de subida: sem ele, empurrarAjustes nao
  // tem o que reescrever e ficaria calado para sempre justamente na TV que
  // acabou de proteger os ajustes locais. O que a protecao decide agora e so se
  // o blob e APLICADO (temAjustesBlob abaixo), nao se ele e buscado.
  r = rpcDoCiclo("sync_pull_profile_settings_blob", corpo, &st);
  if (!ok2xx(r, st) || !perfilDoCicloAtual()) { free(r); return 0; }
  // A resposta e [{ "settings_json": { ... } }]; o que interessa e o objeto de
  // dentro, cru e INTEIRO.
  p = js_raiz_array(r);
  if (p) {
    const char *fimObj = js_fim(p);
    const char *k = strstr(p, "\"settings_json\"");
    if (k && k < fimObj) {
      const char *v = strchr(k, ':');
      if (v) {
        v++;
        while (*v && (unsigned char)*v <= ' ') v++;
        if (*v == '{') {
          const char *f = js_fim(v);
          size_t n = (size_t)(f - v);
          char *novo = (char *)malloc(n + 1);
          if (novo) {
            memcpy(novo, v, n);
            novo[n] = 0;
            if (nv_perfil_sessao_travar(sessaoDoCiclo)) {
              free(ajustesBlob);
              ajustesBlob = novo;
              ajustesBlobGeracao++;
              ajustesBlobPerfil = perfilDoCiclo;
              temAjustesBlob = aplicarAjustes;
              ok = 1;
              nv_perfil_sessao_destravar();
              printf("[sync] blob de ajustes: %d bytes\n", (int)n);
            } else free(novo);
          }
        } else {
          // O web aceita o blob tambem como STRING JSON serializada. Este
          // servidor devolve objeto; se um dia devolver string, o certo e
          // dizer, nao aplicar um pedaco.
          printf("[sync] blob de ajustes nao veio como objeto; nao aplicado\n");
        }
      }
    }
  }
  // A successful empty result is a new account, not a failed pull. It is
  // safe to seed an empty base only after the server confirmed its absence.
  const char *empty = r;
  char raw[16];
  while (*empty && (unsigned char)*empty <= ' ') empty++;
  int noSettings = *empty == '[';
  if (noSettings) {
    empty++; while (*empty && (unsigned char)*empty <= ' ') empty++;
    noSettings = *empty == ']';
  }
  if (!ok && (noSettings || (js_bruto(r, NULL, "settings_json", raw, sizeof raw) && !strcmp(raw, "null")))) {
    if (nv_perfil_sessao_travar(sessaoDoCiclo)) {
      free(ajustesBlob);
      ajustesBlob = strdup("{}");
      ajustesBlobPerfil = perfilDoCiclo;
      ajustesBlobGeracao++;
      temAjustesBlob = aplicarAjustes;
      ok = ajustesBlob != NULL;
      nv_perfil_sessao_destravar();
    }
  }
  free(r);
  return ok;
}

// AJUSTES DESTA TV -> CONTA (#85). Empurra o blob COSTURADO: o que veio da
// conta, com os valores locais escritos por cima das chaves que este app
// conhece. Ver ajustes_mesclar_blob para a regra do que sobe.
//
// TRES TRAVAS, todas contra a mesma familia de defeito — o aparelho apagar dado
// da conta:
//   1. SEM BASE, SEM PUSH. Se nenhum pull deste perfil trouxe blob (rede fora,
//      primeiro arranque, perfil sem ajustes salvos), nao ha o que costurar e
//      nao se manda nada. Montar um blob do zero aqui mandaria as ~40 chaves
//      deste app e apagaria da conta todas as outras — a "lista vazia apaga
//      tudo" da secao 1.6, na versao de ajustes.
//   2. DEPOIS DO PULL DESTE MESMO CICLO. A base e sempre a mais nova que o
//      servidor deu, entao o que so o web mudou nao e sobrescrito por um blob
//      velho guardado em memoria.
//   3. Existing web keys keep their types; portable C preferences use
//      c_settings_v1, including accounts with a confirmed empty blob.
//
// A base guardada e trocada pelo que foi ACEITO: sem isso, o ciclo seguinte
// costuraria sobre um blob que o servidor ja nao tem mais e mandaria a mesma
// diferenca de novo.
static void empurrarAjustes(void) {
  char *mesclado = NULL, *r;
  Jsw w;
  int st = 0;
  if (!sujoAjustes || !temAjustesPerfil || temAjustesBlob ||
      __atomic_load_n(&ajustesRestaurando, __ATOMIC_ACQUIRE)) return;
  if (ajustes_edicao_perfil()) return;
  if (jaAusente("sync_push_profile_settings_blob")) return;
  if (!nv_perfil_sessao_travar(sessaoDoCiclo)) return;
  if (!ajustesBlob) {
    nv_perfil_sessao_destravar();
    printf("[sync] ajustes locais pendentes, mas sem blob da conta para costurar: nada enviado\n");
    return;
  }
  if (ajustes_mesclar_blob(ajustesBlob, &mesclado) <= 0 || !mesclado) {
    // Nothing differs from the freshly pulled account blob: skip the upload.
    free(mesclado);
    sujoAjustes = 0;
    nv_perfil_sessao_destravar();
    return;
  }
  nv_perfil_sessao_destravar();
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfilDoCiclo);
  jsw_cs(&w, "p_platform", "tv");
  jsw_chave(&w, "p_settings_json");
  jsw_bruto(&w, mesclado);
  jsw_obj_fim(&w);
  r = rpcDoCiclo("sync_push_profile_settings_blob", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (!nv_perfil_sessao_travar(sessaoDoCiclo)) { free(r); free(mesclado); return; }
  if (ok2xx(r, st)) {
    sujoAjustes = 0;
    free(ajustesBlob);
    ajustesBlob = mesclado;
    ajustesBlobGeracao++;
    // NAO liga temAjustesBlob: o blob agora E o estado local: aplica-lo seria
    // trabalho para nao mudar nada.
    printf("[sync] ajustes desta TV guardados na conta\n");
  } else {
    free(mesclado);
    if (r && nuvem_erro_ausente(r)) {
      // O SERVIDOR NAO TEM A FUNCAO. E o caso que mantem `ajustes-locais.txt`
      // existindo: sem push, a unica defesa contra o proximo arranque desfazer
      // a mudanca local continua sendo nao aplicar o blob.
      printf("[sync] sync_push_profile_settings_blob nao existe neste servidor: "
             "ajustes ficam locais (ajustes-locais.txt)\n");
      if (nAusentes < SY_AUSENTES)
        ausentes[nAusentes++] = "sync_push_profile_settings_blob";
    } else {
      // `sujoAjustes` FICA LIGADO: 5xx e rede fora sao para tentar de novo no
      // proximo ciclo. Um 4xx tambem fica — e barato, e o ciclo e de 5 minutos.
      printf("[sync] push de ajustes falhou (HTTP %d)\n", st);
    }
  }
  free(r);
  nv_perfil_sessao_destravar();
}

// A resposta inteira e guardada, nao interpretada aqui: quem le e catordem.c,
// no fio principal. Interpretar neste fio e mexer na ordem das fileiras no meio
// de um quadro que ja esta desenhando a home — a mesma razao que faz os addons
// esperarem sync_passo.
static int puxarCatHome(const char *corpo) {
  char *r;
  int st = 0;
  if (jaAusente("sync_pull_home_catalog_settings")) return 0;
  r = rpcDoCiclo("sync_pull_home_catalog_settings", corpo, &st);
  if (!ok2xx(r, st)) {
    // IMPRIME, como o contarRpc que estava aqui antes ja fazia. Sem esta linha
    // a RPC ausente e a resposta vazia ficam com o MESMO sintoma no log —
    // silencio — e foi exatamente o que aconteceu no primeiro deploy: nao dava
    // para saber se o servidor nao tem a funcao ou se a conta nao configurou
    // ordem nenhuma. Um caso e limitacao do servidor, o outro e o app
    // funcionando; confundi-los custa uma sessao de investigacao.
    printf("[sync] ordem de catalogos: HTTP %d%s\n", st,
           (r && nuvem_erro_ausente(r)) ? " (funcao nao existe neste servidor)" : "");
    if (r && nuvem_erro_ausente(r) && nAusentes < SY_AUSENTES)
      ausentes[nAusentes++] = "sync_pull_home_catalog_settings";
    // A ordem ja tem copia propria (catordemcache.c), restaurada em
    // sync_iniciar antes de qualquer rede: aqui basta nao mexer nela.
    if (!(r && nuvem_erro_ausente(r)) && contacache_falha_transitoria(st)) {
      falhaServidor(st);
      if (catordem_tem_ordem())
        printf("[sync] servidor da conta indisponivel (HTTP %d): fica a ordem de catalogos guardada\n", st);
    }
    free(r);
    return 0;
  }
  free(catHomeBlob);
  catHomeBlob = r;          // o fio principal libera depois de ler
  temCatHomeBlob = 1;
  return 1;
}

static void puxarSoLeitura(void) {
  char corpo[160];
  int perfil = perfis_ativo();

  snprintf(corpo, sizeof corpo, "{\"p_profile_id\":%d}", perfil);
  cColecoes = puxarBlob("sync_pull_collections", corpo, &colBlob);
  cColecoes = guardarOuCopia(CC_COLECOES, perfil, cColecoes, &colBlob);
  // Collections RPC may return an object: a zero array count is still a successful blob.
  if (cColecoes >= 0 && colBlob) temColBlob = 1;

  // A BIBLIOTECA E PAGINADA, e o codigo antigo nao mandava a pagina.
  //
  // LIDO no app web (NuvioWeb-0.3.38-beta,
  // js/core/profile/savedLibrarySyncService.js:7): o servico que ele chama de
  // "saved library" usa a RPC `sync_pull_library` com
  // { p_profile_id, p_limit, p_offset } e soma paginas de 500 ate uma vir
  // incompleta.
  //
  // A CHAMADA MORTA QUE SAIU DAQUI: havia um segundo pedido a
  // `sync_pull_saved_library`, com estes mesmos parametros. Essa funcao nao
  // existe — nem neste servidor (PGRST202, medido e registrado na secao 1.4 do
  // PLANO-CONTA-SYNC.md, linha 114) nem em lugar nenhum: o proprio web,
  // no arquivo acima, chama `sync_pull_library`. A tabela da secao 1.4 lista
  // "Biblioteca" e "Biblioteca salva" como duas superficies; sao a MESMA, e o
  // codigo antigo tinha as duas metades trocadas — chamava `sync_pull_library`
  // sem os parametros de pagina e `sync_pull_saved_library` com eles.
  //
  // PAGINAS ATE UMA VIR INCOMPLETA, como o web. Era UMA pagina do tamanho do
  // teto (200), e isso escondia o titulo recem-salvo de quem tem mais que isso:
  // o servidor nao promete ordem, entao a primeira pagina nao e a dos mais
  // novos (issue do Owlphibia29, "o contador nunca passou de 205"). As paginas
  // sao COLADAS num array so e o contalib guarda as CONTALIB_MAX mais recentes.
  cBiblio = puxarBiblioteca(perfil, &bibBlob);
  cBiblio = guardarOuCopia(CC_BIBLIOTECA, perfil, cBiblio, &bibBlob);
  if (cBiblio >= 0) temBibBlob = 1;

  // MEDIDO: `p_page` comeca em 1. Com 0 o servidor responde 400 "OFFSET must
  // not be negative" — a conta dele e (p_page - 1) * p_page_size.
  // 900 e o tamanho de pagina do web (WATCHED_ITEMS_PAGE_SIZE), nao um numero
  // escolhido aqui. Todas as paginas, como o web (puxarVistos, issue #199).
  cVistos = puxarVistos(perfil, &vistosBlob);
  cVistos = guardarOuCopia(CC_VISTOS, perfil, cVistos, &vistosBlob);
  if (cVistos >= 0) temVistosBlob = 1;

  snprintf(corpo, sizeof corpo,
           "{\"p_profile_id\":%d,\"p_platform\":\"tv\"}", perfil);
  temAjustesPerfil = puxarAjustesPerfil(corpo);

  snprintf(corpo, sizeof corpo,
           "{\"p_profile_id\":%d,\"p_platform\":\"home_catalog_shared\"}", perfil);
  temCatHome = puxarCatHome(corpo);
}

// O SERVIDOR DA CONTA CAIU NO MEIO DO CICLO (os addons falharam com 5xx/429/
// sem resposta): o resto do ciclo NAO vai a rede. Cada RPC esperaria o mesmo
// 504 (ate 25 s cada, ~10 RPCs: minutos de home vazia) e martelaria um
// servidor que ja esta caido. As superficies so-leitura saem da copia; nada
// sobe — subir exige a base que o servidor nao deu.
static void soLeituraDaCopia(int st) {
  int perfil = perfis_ativo();
  cColecoes = copiaBlob(CC_COLECOES, perfil, st, &colBlob);
  // Collections RPC may return an object: a zero array count is still a successful blob.
  if (cColecoes >= 0 && colBlob) temColBlob = 1;
  cBiblio = copiaBlob(CC_BIBLIOTECA, perfil, st, &bibBlob);
  if (cBiblio >= 0) temBibBlob = 1;
  cVistos = copiaBlob(CC_VISTOS, perfil, st, &vistosBlob);
  if (cVistos >= 0) temVistosBlob = 1;
  if (catordem_tem_ordem())
    printf("[sync] ordem de catalogos: fica a guardada neste aparelho\n");
}

// ---------------------------------------------------------------- ciclo

// Inicio do ciclo no relogio de parede: o que o jornal da conta confirmou
// ANTES disto ja esta refletido no pull deste ciclo (contapend_podar).
static long long cicloInicioMs;

typedef struct {
  pthread_t fio;
  int perfil, n;
  NvPerfilSessao sessao;
} ProgressoSync;

static int progressoPuxar(int perfil, NvPerfilSessao sessao) {
  int n = syncprog_puxar_sessao(perfil, sessao);
  if (n >= 0 && nv_perfil_sessao_travar(sessao)) {
    if (perfilDoCicloAtual()) PRONTO_PUBLICAR(progressoCedo, 1);
    nv_perfil_sessao_destravar();
  }
  return n;
}
static void *fioProgresso(void *u) {
  ProgressoSync *p = u;
  p->n = progressoPuxar(p->perfil, p->sessao);
  return NULL;
}

static void *rodar(void *u) {
  int puxouAddons, nProgresso = -1;
  ProgressoSync *progresso = NULL;
  (void)u;
  cicloInicioMs = contapend_agora_ms();
  foraCiclo = 0;
  copiaCiclo = 0;
  addonsCiclo = 0;
  copiaQuandoCiclo = 0;
  addonsLidosCiclo = 0;
  perfis_puxar_sessao(sessaoDoCiclo, usuarioDoCiclo, tokenDoCiclo);
  if (!perfilDoCicloAtual()) {
    estado = SYNC_PRONTO;
    PRONTO_PUBLICAR(fioPronto, 1);
    return NULL;
  }
  // ESCOLHA DE PERFIL PENDENTE: PARA AQUI, e nao adivinha o perfil 1.
  //
  // Issue #19, "Random Profile Data Appears Briefly Before My Trakt Profile
  // Loads". Todo o resto deste ciclo e POR PERFIL — addons, credenciais,
  // progresso, biblioteca, vistos, colecoes, ajustes, catalogos da home; cada
  // RPC leva p_profile_id. `ativo` nasce em 1 (perfis.c) e so vira o perfil de
  // verdade quando alguem escolhe. Numa conta com mais de um perfil e nenhuma
  // escolha salva NESTA TV, o primeiro ciclo puxava tudo do perfil 1, a home
  // montava com o dado dele, a pessoa escolhia o perfil dela e tudo trocava —
  // que e exatamente o que o relator descreve e filmou.
  //
  // Isto ERA CONHECIDO e estava escrito em app.c, no ponto que roda o segundo
  // ciclo: "traz os addons e o progresso DESTE perfil, e nao os do perfil 1 que
  // o primeiro ciclo pegou por falta de escolha". A segunda volta consertava o
  // dado; nao consertava o que a pessoa ja tinha visto na tela.
  //
  // Home vazia por alguns segundos e melhor que a home de outro perfil. E nao
  // custa nada a quem ja escolheu: com perfil salvo, perfis_precisa_escolher()
  // e falso e o ciclo segue inteiro, como sempre. app.c abre a tela de escolha
  // e chama sync_iniciar() de novo assim que houver escolha.
  if (perfis_precisa_escolher()) {
    printf("[sync] ciclo interrompido: %d perfis e nenhum escolhido nesta TV\n",
           perfis_n());
    fflush(stdout);
    resumoFixo("aguardando escolha de perfil");
    cicloInterrompido = 1;
    estado = SYNC_PRONTO;
    PRONTO_PUBLICAR(fioPronto, 1);
    return NULL;
  }
  // The profile pull may have refreshed primary-addon inheritance. Freeze
  // that effective owner only after the same session's profiles are loaded.
  addonsPerfilCiclo = perfis_ativo_addons();
  addonsLidosCiclo = puxouAddons = puxarAddons();
  if (puxouAddons < 0) {
    char d[32];
    if (temAddonsRem) PRONTO_PUBLICAR(addonsCedo, 1);
    soLeituraDaCopia(foraCiclo > 0 ? foraCiclo : 0);
    contacache_data(copiaQuandoCiclo, d, sizeof d);
    { SyncResumo r;
      memset(&r, 0, sizeof r);
      r.http = foraCiclo > 0 ? foraCiclo : 0;
      snprintf(r.data, sizeof r.data, "%s", d);
      r.tipo = copiaCiclo ? RES_FORA_COPIA : RES_FORA_SEM;
      resumoPublicar(&r); }
    estado = SYNC_PRONTO;
    PRONTO_PUBLICAR(fioPronto, 1);
    return NULL;
  }
  // OS ADDONS SAO A SEGUNDA RPC DO CICLO, E ERAM APLICADOS NA ULTIMA LINHA DELE.
  //
  // MEDIDO NA C9, no arranque com conta: os addons da conta chegam em ~2 s e
  // eram aplicados em t=28 s, porque sync_passo so olha o buffer quando
  // `fioPronto` — e `fioPronto` e o fim de rodar(), depois de puxarSoLeitura(),
  // que faz sete RPCs. O efeito nao era atraso, era TRABALHO REFEITO: a
  // descoberta comecava em t=0,8 s com os addons LOCAIS, gastava 24 s (14 s de
  // Trakt em serie + 7 s de manifestos) para publicar a home, e ai o
  // `remontar` deste ciclo jogava tudo fora e refazia com os 7 addons da conta,
  // terminando em t=40 s. Duas voltas completas, e a primeira nao servia para
  // nada — num pacote distribuido ela nem tem addon local para usar.
  //
  // Publicando aqui, o remontar acontece com ~1,5 s de trabalho jogado fora em
  // vez de 27 s.
  //
  // Retomar larga primeiro, mas nao e uma barreira para credenciais,
  // colecoes ou ordem. O pedido tem perfil/sessao proprios e a caixa continua
  // sendo aplicada no principal pelas regras de recencia de syncprog.
  progresso = calloc(1, sizeof *progresso);
  if (progresso) {
    progresso->perfil = perfilDoCiclo;
    progresso->sessao = sessaoDoCiclo;
    if (pthread_create(&progresso->fio, NULL, fioProgresso, progresso) != 0) {
      free(progresso); progresso = NULL;
    }
  }
  // Sem memoria/fio, a mesma operacao em serie preserva o resultado.
  if (!progresso) nProgresso = progressoPuxar(perfilDoCiclo, sessaoDoCiclo);
  puxarCredenciais();
  puxarSoLeitura();
  // Somente estes dois corpos passam ao principal. Os pushes nao os leem;
  // ajustesBlob, credenciais e as demais caixas conservam a porteira final.
  PRONTO_PUBLICAR(estruturaCedo, 1);
  // O dono do ciclo nao sai nem comeca push antes de o pull de progresso
  // terminar. Assim nenhum pedido sobrevive aos seus dados ou a um novo sync.
  if (progresso) {
    pthread_join(progresso->fio, NULL);
    nProgresso = progresso->n;
    free(progresso);
  }
  // Empurrar DEPOIS de puxar, como o startupSyncService do web: puxar depois
  // de empurrar faria o aparelho sobrescrever com o que ele mesmo mandou.
  // O puxado NAO e aplicado aqui, e sim em sync_passo, no fio principal — e
  // la a regra e "pendente local vence": o que se assistiu entre o pull e o
  // push nao volta atras.
  // A PESSOA ABRIU OUTRA SESSAO NO MEIO DO CICLO: nada mais sobe. O perfil
  // e o JWT foram capturados no despacho; a base da costura tambem pertence
  // a esse ciclo. sync_passo descarta o resultado e pede a volta atual,
  // inclusive quando a pessoa volta ao mesmo perfil.
  if (!perfilDoCicloAtual()) {
    printf("[sync] perfil trocado no meio do ciclo (%d -> %d): nada sobe\n",
           perfilDoCiclo, perfis_ativo());
    fflush(stdout);
    resumoFixo("perfil trocado; sincronizando de novo");
    estado = SYNC_PRONTO;
    PRONTO_PUBLICAR(fioPronto, 1);
    return NULL;
  }
  // DEPOIS de puxarSoLeitura, pelo mesmo motivo dos addons e com um agravante:
  // a base da costura e o blob que acabou de chegar. Ver empurrarAjustes.
  empurrarAjustes();
  sincronizarPlugins();
  // Sempre, nao so quando `sujoProgresso`: linhas migradas do formato antigo
  // nascem pendentes sem ninguem ter marcado nada.
  // "Enviar histórico para a conta Nuvio" desligado neste perfil: o progresso
  // nao sobe (o pull continua, a conta pode ter coisa de outra TV).
  if (!ajustes_hist_conta()) sujoProgresso = 0;
  else if (syncprog_empurrar_sessao(perfilDoCiclo, sessaoDoCiclo) >= 0) sujoProgresso = 0;
  // O JORNAL DA CONTA (vistos e Salvos marcados nesta TV): DEPOIS do pull,
  // como o resto. E a repeticao de quem falhou offline — cada ciclo tenta de
  // novo o que ainda nao teve 2xx.
  contapend_enviar();
  contapend_reenviar();
  // Pending addon writes run last, with a two-second budget for the whole batch.
  if (puxouAddons == 1) addonsEnviarToggles(addonsTogglesRem, nAddonsTogglesRem);

  { SyncResumo r;
    memset(&r, 0, sizeof r);
    r.pend = contapend_pendentes();
    r.n[0] = nAddonsRem; r.n[1] = nProgresso < 0 ? 0 : nProgresso;
    r.n[2] = cVistos < 0 ? 0 : cVistos; r.n[3] = cBiblio < 0 ? 0 : cBiblio;
    r.n[4] = cColecoes < 0 ? 0 : cColecoes;
    r.trakt = temTraktRem;
    r.tipo = RES_CICLO;
    resumoPublicar(&r); }
  estado = SYNC_PRONTO;
  PRONTO_PUBLICAR(fioPronto, 1);
  return NULL;
}

// A ORDEM LOCAL NAO DEPENDE DE REDE, E TAMBEM NAO DEPENDE DO FIO (#125).
//
// Isto morava DEPOIS de `if (fioVivo) return` em sync_iniciar. O arranque com
// conta de varios perfis chama sync_iniciar com o perfil salvo, o fio para em
// "ciclo interrompido" esperando a pergunta, e a pessoa responde. Se ela
// responde antes de sync_passo ver o fio acabar — a pergunta abre do cache de
// perfis no primeiro quadro, e perfis_puxar ainda esta na rede —, o
// sync_iniciar da escolha voltava na primeira linha: o cache do perfil
// escolhido nunca era lido, e a home ficava na ordem do perfil salvo (ou na
// padrao) ate o blob da conta chegar e reordenar tudo. MEDIDO em
// tests/syncordem.sh, sessao 5: salvo 1, escolhe 2 com o fio vivo -> nenhuma
// "ordem restaurada ... (perfil 2)", e depois "3 na ordem da conta" + "ordem
// guardada" — o mesmo par de linhas dos logs de campo.
//
// Restaurar tambem antes do freio, como antes: num boot apos update, sem
// internet ou com o servidor em pausa, a Home ainda precisa abrir com a
// escolha que ja estava no aparelho.
//
// REMONTA TAMBEM QUANDO O PERFIL NOVO NAO TEM CACHE, se havia ordem na
// memoria: catordem_cache_carregar esquece a ordem anterior antes de ler, e a
// home ficaria desenhada na ordem do OUTRO perfil ate a rede responder.
static void restaurarOrdemLocal(void) {
  int tinha, mudou;
  if (!sessao_logada() || catordemCachePerfil == perfis_ativo()) return;
  tinha = catordem_tem_ordem();
  mudou = catordem_cache_carregar(perfis_ativo(), sessao_usuario());
  catordemCachePerfil = perfis_ativo();
  if (mudou || tinha) desc_remontar_fileiras();
}

// SINCRONIZAR ADD-ONS A MAO (#360). `manualRodando` liga so quando o fio que
// leva o pedido foi criado: um clique com um ciclo ja no ar espera ele acabar e
// pede outro, porque o que estava no ar pode ter lido a conta ANTES do clique.
static int manualPedido, manualRodando, manualResultado, manualAdd, manualRem, manualMud;
static AddonRemoto manualAntes[SY_ADD_MAX];
static int manualAntesN;

void sync_iniciar(void) {
  plugins_perfil_mudou();   // conta/perfil novos: estado e geracao dos plugins
  colfileiras_contexto();
  cat_historico_contexto(sessao_logada() ? sessao_usuario() : "",
                         sessao_logada() ? perfis_ativo() : 0);
  restaurarOrdemLocal();
  // #360: pendencias de addons da versao anterior somem na primeira execucao,
  // com ou sem login — elas nunca mais seriam lidas nem enviadas.
  addonsEsquecer();
  if (!sessao_logada()) return;
  // Cached sources are private to this session and do not touch the worker's
  // staging buffers. Restore them even while an old sync drains or backs off.
  addonsRestaurar();
  desc_sessao_preparada();
  // PEDIDO COM O FIO VIVO NAO SE PERDE. Voltar calado deixava um buraco: o
  // fio que estava no ar pode ser justamente o interrompido pela pergunta de
  // perfil, e ai o ciclo completo do perfil escolhido so partia com
  // sync_periodico, cinco minutos depois (o ciclo interrompido marca
  // SYNC_PRONTO e conta como `ultimoOk`). sync_passo decide, quando o fio
  // acabar, se precisa de outra volta.
  if (fioVivo) { pedidoComFioVivo = 1; return; }
  pedidoComFioVivo = 0;
  if (nuvem_freio_ativo()) return;
  ajustesDetectarMudancas();
  addonsPerfilCiclo = perfis_ativo_addons();
  cicloInterrompido = 0;
  perfilDoCiclo = perfis_ativo();
  copiaGeracaoDoCiclo = contacache_geracao();
  sessaoDoCiclo = nv_perfil_sessao_ler();
  if (!sessao_identidade_local_copiar(usuarioDoCiclo, sizeof usuarioDoCiclo,
                                    tokenDoCiclo, sizeof tokenDoCiclo) ||
      !perfilDoCicloAtual()) {
    memset(tokenDoCiclo, 0, sizeof tokenDoCiclo);
    return;
  }
  estado = SYNC_RODANDO;
  __atomic_store_n(&fioPronto, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&progressoCedo, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&estruturaCedo, 0, __ATOMIC_RELAXED);
  progressoAplicadoCedo = 0;
  if (pthread_create(&fio, NULL, rodar, NULL) == 0) { pthread_detach(fio); fioVivo = 1; if (manualPedido) manualRodando = 1; }
  else { estado = SYNC_FALHOU; resumoFixo("sem fio para sincronizar"); }
}

// Um ciclo automatico, se ja passou o intervalo. Devolve 1 quando disparou.
// Separado de sync_passo porque quem chama sabe se a hora e boa: durante a
// reproducao NAO e — uma rajada de HTTP no meio do video disputa CPU e rede
// com o decodificador, e um engasgo de imagem custa mais que 5 minutos de
// atraso no progresso.
int sync_periodico(unsigned agoraMs) {
  if (!sessao_logada() || fioVivo) return 0;
  if (nuvem_freio_ativo()) return 0;
  // Sem nenhum ciclo bem-sucedido ainda, quem manda e quem chamou sync_iniciar
  // — nao adianta insistir por cima de uma falha que o freio ja esta segurando.
  if (!ultimoOk) return 0;
  // Com o servidor da conta fora do ar, a volta seguinte vem antes: e ela que
  // troca a copia pela resposta de verdade quando ele voltar. O freio de
  // nuvem.c (acima) continua mandando no ritmo das tentativas.
  if (agoraMs - ultimoOk < (foraHttp ? SYNC_RETENTA_MS : SYNC_INTERVALO_MS)) return 0;
  sync_iniciar();
  return 1;
}

// QUANTO O CICLO CUSTA NO FIO PRINCIPAL. Registros da TCL Smart TV Pro do
// dono (Android 14, Mali-G52, 04/10/2026): a cada cinco minutos um quadro com
// `upd=59..232 ms`, sempre no mesmo segundo de "[colecoes] 129 pastas vindas da
// conta". O log mostrava QUE era o ciclo, nao QUAL passo. A linha sai so
// quando o total passa de 8 ms (meio quadro), uma por ciclo.
static double syncRelogioMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
enum { SP_ADDONS, SP_CRED, SP_ORDEM, SP_COLECOES, SP_SINCRONIZAR, SP_BIBLIOTECA,
       SP_VISTOS, SP_REMONTAR, SP_AJUSTES, SP_PROGRESSO, SP_N };
static double spMs[SP_N], spMarca;
static void spMarcar(int fase) { double t = syncRelogioMs(); spMs[fase] += t - spMarca; spMarca = t; }
static void spRelatar(void) {
  static const char *const NOME[SP_N] = { "addons", "credenciais", "ordem", "colecoes",
    "sincronizar", "biblioteca", "vistos", "remontar", "ajustes", "progresso" };
  double total = 0;
  int i;
  for (i = 0; i < SP_N; i++) total += spMs[i];
  if (total >= 8.0) {
    printf("[sync] aplicado no fio principal em %.1f ms:", total);
    for (i = 0; i < SP_N; i++) if (spMs[i] >= 0.5) printf(" %s=%.1f", NOME[i], spMs[i]);
    printf("\n");
    fflush(stdout);
  }
}

// No fio principal: consome cada corpo uma vez, antes ou no fim do ciclo.
// Nenhum destes corpos participa de um push; o restante conserva fioPronto.
static int aplicarEstruturaDaConta(int medir) {
  int mudou = 0;
  int atualizarColecoes = temColBlob || temCatHomeBlob;
  // Uma remontagem por ciclo de sync custaria a home inteira a cada 5 minutos,
  // e o baseline de jank desta TV nao tem essa folga; duas remontagens no mesmo
  // quadro (addons e ordem) custariam o dobro por nada.
  if (temCatHomeBlob && catHomeBlob) {
    if (catordem_ler(catHomeBlob)) {
      catordem_cache_gravar(perfis_ativo(), sessao_usuario(), catHomeBlob);
      if (catordem_tem_ocultar_nao_lancados())
        ajustes_definir_ocultar_nao_lancados(catordem_ocultar_nao_lancados());
      // `hide_catalog_underline` e lido e NAO aplicado: nao ha sublinhado de
      // catalogo desenhado neste app. Ler ja evita confundir "a conta nao
      // mandou" com "a conta mandou false" quando a fileira ganhar rotulo.
      mudou = 1;
    }
    free(catHomeBlob);
    catHomeBlob = NULL;
    temCatHomeBlob = 0;
  }
  if (medir) spMarcar(SP_ORDEM);
  if (temColBlob && colBlob) {
    unsigned antes = col_revisao();
    colfileiras_receber(colBlob);
    if (antes != col_revisao()) mudou = 1;
    free(colBlob); colBlob = NULL; temColBlob = 0;
  }
  if (medir) spMarcar(SP_COLECOES);
  // Publish account additions, removals and visibility while Settings is open;
  // relying on the next Home draw left the editor on an older snapshot (#233).
  if (atualizarColecoes) colfileiras_sincronizar();
  if (medir) spMarcar(SP_SINCRONIZAR);
  return mudou;
}

void sync_passo(unsigned agoraMs) {
  if (sessao_logada()) desc_sessao_passo();
  // Consome apenas uma resposta da conta/perfil que a pediu. A rede segue no
  // mesmo fio de sempre; uma colecao lenta nao segura o progresso recebido.
  if (PRONTO_LER(progressoCedo)) {
    __atomic_store_n(&progressoCedo, 0, __ATOMIC_RELAXED);
    if (perfilDoCicloAtual()) {
      if (syncprog_aplicar(NULL) > 0) {
        progressoAplicadoCedo = 1;
        desc_refazer_continuar();
      }
    } else syncprog_esquecer();
  }
  // ANTES DA PORTEIRA de `fioPronto`: ver o comentario em rodar(). O resto do
  // ciclo continua sendo aplicado de uma vez, no fim — addons e progresso
  // saem na frente para a descoberta e o retorno ao que estava sendo visto.
  if (PRONTO_LER(addonsCedo)) {
    __atomic_store_n(&addonsCedo, 0, __ATOMIC_RELAXED);
    if (temAddonsRem && perfilDoCicloAtual()) {
      // _addons: a volta que ainda nao leu a lista (o caso do arranque e da
      // escolha de perfil) atende o pedido sozinha, sem ser jogada fora.
      // A lista vale para a poda de fileiras SO DEPOIS de marcada como deste
      // perfil — e antes de desc_repetir_addons, para a volta que ela dispara
      // ja poder podar. Ver addons_marcar_da_conta.
      { int mudou = addonsAplicar();
        int antes = addons_perfil_da_lista();
        if (nAddonsRem > 0) addons_marcar_da_conta(perfilDoCiclo);
        // #392: lista identica de outro perfil para este: refaz o registro.
        if (mudou || (nAddonsRem > 0 && antes > 0 && antes != perfilDoCiclo))
          desc_repetir_addons(); }
      temAddonsRem = 0;
    }
  }
  if (PRONTO_LER(estruturaCedo)) {
    __atomic_store_n(&estruturaCedo, 0, __ATOMIC_RELAXED);
    if (nv_perfil_sessao_travar(sessaoDoCiclo)) {
      if (perfilDoCicloAtual()) {
        if (aplicarEstruturaDaConta(0)) desc_remontar_fileiras();
        PRONTO_PUBLICAR(estruturaSessaoAplicada, sessaoDoCiclo);
        PRONTO_PUBLICAR(estruturaPerfilAplicado, perfilDoCiclo);
        PRONTO_PUBLICAR(estruturaAplicada, 1);
      }
      nv_perfil_sessao_destravar();
    }
  }
  // TAMBEM ANTES DA PORTEIRA, e por um motivo diferente do dos addons: a
  // descoberta republica o catalogo inteiro varias vezes por ciclo
  // (cat_definir_tudo), e cada republicacao apaga os itens que a conta
  // acrescentou. Sem esta linha a Biblioteca da conta funcionava do fim do sync
  // ate o fim da descoberta e depois esvaziava sozinha — um defeito
  // intermitente e mudo, que e o pior tipo que esta area produz.
  //
  // Custo no caso comum: uma comparacao de inteiro (cat_n) e um strcmp de 16
  // bytes. Nada e refeito enquanto o catalogo for o mesmo.
  contalib_reconciliar();
  if (!fioVivo || !PRONTO_LER(fioPronto)) return;
  fioVivo = 0;
  __atomic_store_n(&fioPronto, 0, __ATOMIC_RELAXED);
  memset(spMs, 0, sizeof spMs);
  spMarca = syncRelogioMs();

  // O CICLO INTEIRO E DE UM PERFIL SO. Trocar de perfil com o fio no ar (a
  // pessoa entra no 1 e volta ao 2 antes de o ciclo acabar) fazia o ciclo do 1
  // ser aplicado no 2: colecoes, biblioteca, vistos, credencial do Trakt e o
  // PROGRESSO — prog_aplicar_remoto grava com perfis_ativo(), entao as linhas
  // do 1 entravam no arquivo como se fossem do 2 e o "Continuar assistindo" do
  // 2 passava a ser o do 1 (medido na C9 do dono, 24/09: 103 linhas aceitas).
  // Descartar tudo e pedir a volta do perfil certo; o que ja estava na tela e
  // do perfil novo (invalidarPerfil em app.c).
  if (!contaDoCicloAtual() || !nv_perfil_sessao_atual(sessaoDoCiclo) ||
      (!cicloInterrompido && !perfilDoCicloAtual())) {
    printf("[sync] ciclo do perfil %d descartado: o perfil ativo agora e %d\n",
           perfilDoCiclo, perfis_ativo());
    fflush(stdout);
    temAddonsRem = 0;
    temTraktRem = 0; traktTok[0] = 0;
    temTmdb = temMdb = temDebridRem = 0;
    free(catHomeBlob); catHomeBlob = NULL; temCatHomeBlob = 0;
    free(colBlob);     colBlob = NULL;     temColBlob = 0;
    free(bibBlob);     bibBlob = NULL;     temBibBlob = 0;
    free(vistosBlob);  vistosBlob = NULL;  temVistosBlob = 0;
    temAjustesBlob = 0;
    // O blob que o ciclo descartado trouxe e do perfil anterior: nao fica
    // como base da costura nem como "Da conta" do perfil novo (#378).
    if (ajustesBlob && ajustesBlobPerfil != perfis_ativo()) {
      free(ajustesBlob);
      ajustesBlob = NULL;
    }
    syncprog_esquecer();
    pedidoComFioVivo = 0;
    sync_iniciar();
    return;
  }

  // Uma credencial que muda o CONTEUDO do catalogo obriga a remontar. Vale
  // para o Trakt (fileiras proprias) e para os addons (sao a fonte dos
  // catalogos). A chave do TMDB e do mdblist so enriquecem o que ja esta la.
  // DOIS SINAIS, e nao um. `remontar` juntava quatro causas com custos muito
  // diferentes: addons e Trakt mudam O QUE BUSCAR e exigem um ciclo de rede
  // novo; ordem de catalogos e colecoes mudam so QUAIS FILEIRAS EXISTEM e em que
  // sequencia — nenhum item muda. Disparar desc_repetir() por causa das duas
  // ultimas refazia Trakt e todos os manifestos por nada, e como o ciclo novo
  // publica um conjunto diferente do anterior, a home carregava um catalogo,
  // trocava por outro e so entao assentava na ordem final.
  { int remontar = 0, soFileiras = 0, soAddons = 0;
  // SO REMONTA QUANDO A LISTA MUDOU DE VERDADE. Ligar `remontar` porque a
  // resposta chegou fazia um ciclo de descoberta completo a cada cinco minutos
  // com a lista identica — ver listaIgual em addons.c.
  if (temAddonsRem) {
    int antes = addons_perfil_da_lista();
    if (addonsAplicar()) soAddons = 1;
    // O ciclo de outro perfil ja foi descartado acima: esta lista e do ativo.
    if (nAddonsRem > 0) {
      addons_marcar_da_conta(perfilDoCiclo);
      // #392: lista identica, mas marcada ate aqui de outro perfil.
      if (antes > 0 && antes != perfilDoCiclo) soAddons = 1;
    }
  }
  // A caixa deste ciclo ja foi aplicada (ou descartada): nao vale de novo.
  temAddonsRem = 0;
  spMarcar(SP_ADDONS);
  // Vinculo feito NESTA TV ganha do que a conta manda: o servidor nao aceita o
  // push de "trakt" (400 22023), entao a linha da conta pode ser um token
  // antigo e vencido — aplica-lo por cima do novo devolvia 401 em tudo logo
  // depois de a pessoa ter acabado de autorizar.
  // "NESTA TV" QUER DIZER "DESTE PERFIL NESTA TV": o vinculo local e por perfil
  // (trakt-p<N>.txt, traktauth.c) e traktauth_estado() e o do perfil ativo. A
  // credencial da conta deste ciclo tambem e do perfil ativo — um ciclo de outro
  // perfil ja foi descartado acima —, entao o perfil 2 sem vinculo local recebe
  // o Trakt da conta DELE, e nunca o vinculo local do 1.
  // A MESMA CREDENCIAL DE NOVO NAO REMONTA. A conta manda o token do Trakt em
  // TODO ciclo, e `remontar` ligava sempre: um desc_repetir por sync — a cada
  // cinco minutos um ciclo de rede inteiro, e no arranque a montagem em voo
  // descartada ("remontagem pedida no meio") por uma credencial que ela ja
  // estava usando.
  if (temTraktRem)  { if (traktauth_estado() != TRA_LIGADO && !traktauth_desconectado()) {
                        if (!trakt_credencial_igual(traktTok, nuvem_trakt_cliente())) {
                          trakt_definir(traktTok, nuvem_trakt_cliente()); remontar = 1; } }
                      else printf("[sync] trakt: vinculo local mantido, credencial da conta ignorada\n");
                      temTraktRem = 0; }
  if (temDebridRem) {
    // A refused publish keeps the batch. The next cycle of this profile
    // pulls it again; clearing here dropped keys that arrived mid-switch.
    if (debrid_aplicar_chaves_conta(&debridDoCiclo, sessaoDoCiclo))
      temDebridRem = 0;
  }
  if (temTmdb)      { desc_tmdb_definir(tmdbKey);   temTmdb = 0; }
  if (temMdb)       { extras_definir_chave(mdbKey); temMdb = 0; }
  spMarcar(SP_CRED);
  // A mesma aplicacao que pode ter ocorrido cedo: as caixas consumidas
  // estao vazias, e nao repetem o parse, a gravacao nem a remontagem.
  soFileiras |= aplicarEstruturaDaConta(1);
  // A BIBLIOTECA DA CONTA. Ler e aplicar sao passos separados de proposito:
  // contalib_ler_biblioteca pode RECUSAR a resposta (lista remota vazia com
  // lista guardada — secao 1.6, regra 1), e nesse caso o que ja esta no
  // catalogo continua onde esta.
  //
  // NAO liga `remontar` nem `soFileiras`: os itens da conta entram no fim do
  // vetor de itens e nao criam nem reordenam fileira nenhuma. Pedir uma
  // remontagem aqui custaria o ciclo de rede inteiro por nada, a cada cinco
  // minutos — foi o erro que a ordem de catalogos ja cometeu neste arquivo.
  //
  // O "+" E O TIRAR CHEGAM NA CONTA pelo jornal (contapend.c): ler a lista
  // inteira, aplicar so o gesto e subir com sync_push_library — nunca a lista
  // LOCAL (secao 1.6, regra 2). O que a pessoa tirou e ainda nao saiu da conta
  // fica fora daqui pelo filtro de contalib_aplicar_catalogo.
  if (temBibBlob && bibBlob) {
    int before = contalib_n(), kept = 0;
    for (int i = 0; i < before; i++) {
      const ContaLibItem *old = contalib_item(i);
      for (const char *p = js_raiz_array(bibBlob); p; p = js_prox(js_fim(p))) {
        char id[24];
        if (js_texto(p, js_fim(p), "content_id", id, sizeof id) && !strcmp(old->id, id)) {
          kept++; break;
        }
      }
    }
    int accepted = contalib_ler_biblioteca(bibBlob);
    if (accepted > 0) contalib_aplicar_catalogo();
    printf("[library-sync] source=account profile=%d effective=%d added=%d removed=%d kept=%d accepted=%d\n",
           perfilDoCiclo, ajustes_fonte_biblioteca(), accepted > 0 ? accepted - kept : 0,
           accepted > 0 ? before - kept : 0, accepted > 0 ? kept : before, accepted >= 0);
    free(bibBlob); bibBlob = NULL; temBibBlob = 0;
  }
  spMarcar(SP_BIBLIOTECA);
  // OS VISTOS DA CONTA, e so quando o Trakt NAO esta no ar.
  //
  // E o que o web faz: `shouldUseSupabaseWatchProgressSync` em
  // js/core/profile/watchedItemsSyncService.js pula esta sincronizacao inteira
  // quando ha provedor (Trakt ou Simkl) escolhido. O motivo aqui e concreto: o
  // historico do Trakt e escrito pelo proprio app quando a pessoa marca algo na
  // TV (trakt.c, cat_historico_definir_id depois do 2xx), e uma linha antiga da
  // conta aplicada por cima desfaria essa marca no ciclo seguinte — a pessoa
  // desmarcaria um titulo e ele voltaria marcado sozinho.
  //
  // Sem Trakt, esta e a UNICA fonte de "assistido" que o app tem, e ate agora
  // ela nao existia: era a mesma queixa do issue por outro angulo.
  if (temVistosBlob && vistosBlob) {
    unsigned rev = contalib_vistos_revisao();
    if (contalib_ler_vistos(vistosBlob) > 0 && !trakt_ativo())
      contalib_aplicar_vistos();
    free(vistosBlob); vistosBlob = NULL; temVistosBlob = 0;
    // O "a seguir" da conta (issue #199) sai destes vistos. Vistos novos e
    // sem Trakt/Simkl no ar: a fileira e refeita, senao so o ciclo seguinte
    // (10 min) os veria.
    if (rev != contalib_vistos_revisao() && !trakt_ativo() && !simkl_ativo())
      desc_refazer_continuar();
  }
  // O JORNAL DA CONTA POR CIMA do que veio: o que a pessoa marcou/desmarcou
  // aqui e ainda nao esta (ou acabou de entrar) na conta continua valendo na
  // tela, inclusive depois de reabrir o app sem rede. E o que ja foi
  // confirmado antes deste ciclo sai do jornal: o pull acima ja o reflete.
  contapend_podar(cicloInicioMs);
  contapend_aplicar_local();
  // A LISTA ANTIGA DESTA TV (salvos.txt de antes da lista por perfil) sobe UMA
  // vez para o perfil que a recebeu, item a item pelo jornal — que le a lista
  // da conta e so ACRESCENTA. Espera esse perfil estar ativo.
  { int p = 0;
    if (sessao_logada() && salvos_migracao_conta(&p) && p == perfis_ativo() &&
        salvos_perfil_atual() == p) {
      int i, k = 0;
      for (i = 0; i < salvos_n(); i++) {
        const SalvoItem *it = salvos_item(i);
        if (it) k += contapend_lista(it->id, it->tipo, it->titulo, it->poster, 1);
      }
      salvos_migracao_conta_feita();
      printf("[sync] lista antiga do perfil %d: %d titulos entregues a conta\n", p, k);
      fflush(stdout);
      if (k) contapend_chutar();
    } }
  spMarcar(SP_VISTOS);
  // Rede so quando muda o que buscar. Quando as duas coisas mudam no mesmo
  // ciclo, o ciclo de rede ja remonta as fileiras no fim — nao ha o que somar.
  // Credencial nova pede a volta inteira de novo (o Trakt ja lido e o velho);
  // so a lista de addons, nem sempre — ver desc_repetir_addons.
  if (remontar) desc_repetir_silencioso();   // sync: a pessoa nao pediu, sem alerta na ilha
  else if (soAddons) desc_repetir_addons();
  else if (soFileiras) desc_remontar_fileiras();
  spMarcar(SP_REMONTAR); }
  int blobAplicado = 0, ajustesMudaram = 0;
  if (temAjustesBlob && ajustesBlob && !ajustes_edicao_perfil()) {
    ajustesMudaram = ajustes_aplicar_blob(ajustesBlob);
    blobAplicado = 1;
    // O BLOB NAO E LIBERADO AQUI (mudou em #85): ele e a base da costura que
    // sobe no proximo ciclo. Quem o libera e o pull seguinte, que o substitui,
    // e o logout.
    temAjustesBlob = 0;
    aplicarAjustes = 0;   // daqui para frente, o que a pessoa mudar na TV fica
    __atomic_store_n(&ajustesRestaurando, 0, __ATOMIC_RELEASE);
    ajustesDetectarMudancas();
  }
  // #187: uma linha por blob novo (aplicado ou protegido) dizendo qual idioma
  // a TV pede ao TMDB e o que a conta guarda. A geracao muda a cada blob.
  { static unsigned relatado;
    if (ajustesBlob && ajustesBlobGeracao != relatado) {
      relatado = ajustesBlobGeracao;
      ajustes_tmdb_idioma_relatar(ajustesBlob);
      // #378: PROTEGIDO, o blob nao e aplicado — mas os idiomas de legenda e
      // audio da conta nao sao ajuste desta TV: sao o que "Da conta" quer
      // dizer, e linguas.c ja nao os deixa passar por cima da escolha local.
      // Sem isto, quem mexeu em QUALQUER ajuste ficava com "Da conta" = nada.
      if (!blobAplicado && ajustesBlobPerfil == perfis_ativo())
        ajustes_idiomas_da_conta(ajustesBlob);
    } }
  spMarcar(SP_AJUSTES);
  // Progresso da conta: progresso.c decide linha a linha (pendente local vence,
  // senao o mais novo), guarda ate o que nao tem titulo no catalogo ainda, e
  // o catalogo recebe so o que foi aceito.
  // Aceito novo refaz a fileira: sem isto o que o celular assistiu so aparecia
  // em "Continuar assistindo" no proximo ciclo de descoberta (issue #38).
  if (syncprog_aplicar(NULL) > 0 || (progressoAplicadoCedo && ajustesMudaram))
    desc_refazer_continuar();
  spMarcar(SP_PROGRESSO);
  spRelatar();
  if (estado == SYNC_PRONTO) ultimoOk = agoraMs;
  // O ciclo parado na pergunta de perfil nao chegou a perguntar nada: o estado
  // do servidor continua o do ciclo anterior.
  if (!cicloInterrompido) {
    foraHttp = foraCiclo;
    usandoCopia = copiaCiclo > 0;
    copiaQuando = copiaQuandoCiclo;
    addonsFora = addonsCiclo;
  }
  if (!cicloInterrompido) {
    perfilAplicado = perfilDoCiclo;
    PRONTO_PUBLICAR(estruturaSessaoAplicada, sessaoDoCiclo);
    PRONTO_PUBLICAR(estruturaPerfilAplicado, perfilDoCiclo);
    PRONTO_PUBLICAR(estruturaAplicada, 1);
  }
  // A VOLTA QUE FOI PEDIDA COM O FIO VIVO. So quando ela serve para algo: o
  // ciclo que acabou parou na pergunta de perfil (e a pergunta ja foi
  // respondida) ou puxou para um perfil que nao e mais o ativo. Quando a
  // pessoa responde durante perfis_puxar, o proprio ciclo segue inteiro com o
  // perfil escolhido — repetir ali seria o ciclo de rede duas vezes por nada.
  // O PEDIDO DO BOTAO "Sincronizar add-ons" (#360). Acabou o ciclo que o levava:
  // compara as listas e deixa o resultado para a tela. O ciclo que ja estava no ar
  // quando o botao foi apertado nao conta (manualRodando=0): pede outro.
  if (manualPedido && !cicloInterrompido) {
    if (!manualRodando) { sync_iniciar(); if (!manualRodando) { manualPedido = 0; manualResultado = 3; } }
    else {
      static AddonRemoto depois[SY_ADD_MAX];
      int nd = addons_exportar(depois, SY_ADD_MAX);
      addons_diferenca(manualAntes, manualAntesN, depois, nd, &manualAdd, &manualRem, &manualMud);
      manualPedido = manualRodando = 0;
      manualResultado = (estado == SYNC_FALHOU || foraHttp || addonsLidosCiclo != 1) ? 3
                      : 2;
      printf("[sync] add-ons a mao: +%d -%d ~%d (resultado %d)\n",
             manualAdd, manualRem, manualMud, manualResultado);
    }
  }
  if (pedidoComFioVivo) {
    pedidoComFioVivo = 0;
    if ((cicloInterrompido && !perfis_precisa_escolher()) ||
        (!cicloInterrompido && perfilDoCiclo != perfis_ativo()))
      sync_iniciar();
  }
}

int  sync_servidor_fora(void) { return foraHttp; }
int  sync_usando_copia(void)  { return foraHttp && usandoCopia; }
long sync_copia_quando(void)  { return copiaQuando; }
int  sync_addons_fora(void)   { return foraHttp ? addonsFora : 0; }
SyncEstado  sync_estado(void)      { return estado; }
int sync_manual_addons(void) {
  if (!sessao_logada()) return 0;
  if (manualPedido) return 1;   // ja ha um pedido no ar: o botao nao empilha outro
  if (nuvem_freio_ativo()) { manualResultado = 3; return 1; }
  manualAntesN = addons_exportar(manualAntes, SY_ADD_MAX);
  manualPedido = 1; manualRodando = 0; manualResultado = 0;
  sync_iniciar();
  if (!fioVivo) { manualPedido = 0; manualResultado = 3; }
  return 1;
}
int sync_manual_addons_rodando(void) { return manualPedido; }
int sync_manual_addons_estado(int *add, int *rem, int *mud) {
  int r;
  if (manualPedido) return 1;
  r = manualResultado;
  if (r >= 2) {
    if (add) *add = manualAdd;
    if (rem) *rem = manualRem;
    if (mud) *mud = manualMud;
    manualResultado = 0;
  }
  return r;
}
// As frases fixas (resumo guarda o portugues) passam pela tabela aqui; as com
// numero ja foram montadas com o formato traduzido.
const char *sync_resumo(void) {
  static char saida[260];
  char sufixo[64] = "";
  SyncResumo r;
  pthread_mutex_lock(&resumoTrava); r = resumoDados; pthread_mutex_unlock(&resumoTrava);
  switch (r.tipo) {
    case RES_FORA_COPIA:
      snprintf(saida, sizeof saida, i18n("servidor da conta fora do ar (HTTP %d) · usando a cópia de %s"),
               r.http, r.data);
      return saida;
    case RES_FORA_SEM:
      snprintf(saida, sizeof saida, i18n("servidor da conta fora do ar (HTTP %d) · sem cópia salva"), r.http);
      return saida;
    case RES_CICLO:
      if (r.pend > 0) snprintf(sufixo, sizeof sufixo, i18n(" · %d pendentes"), r.pend);
      snprintf(saida, sizeof saida,
               i18n("%d addons · %d progressos · %d vistos · %d na lista · %d coleções%s%s"),
               r.n[0], r.n[1], r.n[2], r.n[3], r.n[4],
               r.trakt ? " · Trakt" : "", sufixo);
      return saida;
    default:
      // i18n devolve a propria entrada quando nao ha traducao: copia para
      // `saida`, nunca um ponteiro para o resumo que o fio reescreve.
      snprintf(saida, sizeof saida, "%s", i18n(r.fixo));
      return saida;
  }
}
unsigned    sync_ultimo_ok(void)   { return ultimoOk; }
// Push only progress during playback; the full catalog sync stays deferred.
static int pontoEmVoo;
static struct { int perfil; uint64_t sessao; } pontoPedido;
static void *empurrarPonto(void *arg) {
  (void)arg;
  syncprog_empurrar_sessao(pontoPedido.perfil, pontoPedido.sessao);
  __atomic_store_n(&pontoEmVoo, 0, __ATOMIC_RELEASE);
  return NULL;
}
void sync_sujar_progresso(void) {
  sujoProgresso = 1;
  if (!sessao_logada() || !ajustes_hist_conta() || nuvem_freio_ativo() ||
      __atomic_exchange_n(&pontoEmVoo, 1, __ATOMIC_ACQUIRE)) return;
  pontoPedido.perfil = perfis_ativo();
  pontoPedido.sessao = nv_perfil_sessao_ler();
  pthread_t fioPonto;
  if (pthread_create(&fioPonto, NULL, empurrarPonto, NULL) == 0) pthread_detach(fioPonto);
  else __atomic_store_n(&pontoEmVoo, 0, __ATOMIC_RELEASE);
}
// Addon flag state, per account (see addonsAplicar for the merge order):
//  - CC_ADDONS_TOGGLES, keyed by the ACTIVE profile: each addon the user
//    toggled, until the account acknowledges that row (PATCH 2xx) or drops it.
//    It wins every merge and survives restart; nothing else is ever queued.
//  - CC_ADDONS_APLICADOS, keyed by the addon profile: last merged list. A
//    toggle updates only that flag, and only when the snapshot already exists,
//    so a package or foreign list never seeds membership.
void sync_sujar_addons(const char *url) {
  static AddonRemoto local[SY_ADD_MAX], lista[SY_ADD_MAX];
  static char buf[SY_URL_LEITURA];
  char usuario[80];
  int perfil = perfis_ativo(), perfilAddons = perfis_ativo_addons();
  unsigned geracao = contacache_geracao();
  if (!url || !sessao_logada()) return;
  // A list left in memory by another profile is not this profile's state.
  int donoLista = addons_perfil_da_lista();
  if (donoLista && donoLista != perfil) return;
  snprintf(usuario, sizeof usuario, "%s", sessao_usuario());
  pthread_mutex_lock(&addonsToggleTrava);
  int n = addons_exportar(local, SY_ADD_MAX);
  int i = addonsAchar(local, n, url);
  if (i >= 0) {
    char *c = contacache_ler(CC_ADDONS_TOGGLES, perfil, usuario, NULL);
    int np = c ? lerAddonsPara(c, lista, buf) : 0, k = addonsAchar(lista, np, url);
    free(c);
    if (k < 0 && np < SY_ADD_MAX) k = np++;
    if (k >= 0) lista[k] = local[i];
    addonsGuardar(CC_ADDONS_TOGGLES, lista, np, perfil, usuario, geracao);
    c = contacache_ler(CC_ADDONS_APLICADOS, perfilAddons, usuario, NULL);
    int nb = c ? lerAddonsPara(c, lista, buf) : 0;
    free(c);
    if ((k = addonsAchar(lista, nb, url)) >= 0) {
      lista[k].ativo = local[i].ativo;
      addonsGuardar(CC_ADDONS_APLICADOS, lista, nb, perfilAddons, usuario, geracao);
    }
  }
  pthread_mutex_unlock(&addonsToggleTrava);
}
// Provedor que o servidor recusou com "Unsupported provider credential": o
// servidor de hoje nao guarda trakt/simkl, e a resposta nao muda ate o app
// reiniciar. Perguntar de novo a cada renovacao do token era um 400 no log por
// ciclo, sempre igual. Anota o provedor e para de perguntar nesta sessao; o
// vinculo continua valendo nesta TV, guardado em disco.
#define SY_CRED_RECUSADAS 4
static char credRecusada[SY_CRED_RECUSADAS][16];
static int nCredRecusadas;
// sync_empurrar_credencial roda em fio proprio (credfio.c, #203), trakt e simkl
// podem estar no ar juntos: a lista e o arquivo de recusas pedem trava. Nunca
// segurada durante a rede.
static pthread_mutex_t credTrava = PTHREAD_MUTEX_INITIALIZER;

// A recusa tambem vai para DISCO (cred-recusada.txt, "provedor epoch"), por 7
// dias: so a memoria da sessao deixava cada arranque repetir o mesmo 400
// (D1 de 22/09 a 05/10: 18 pessoas). Passados 7 dias pergunta de novo, caso o
// servidor passe a aceitar.
#define SY_CRED_VALE_S (7L * 24L * 3600L)
static int credDiscoLido;

static void credLerDisco(void) {
  char *t, *p;
  if (credDiscoLido) return;
  credDiscoLido = 1;
  t = dados_ler("cred-recusada.txt");
  for (p = t; p && *p; ) {
    char prov[16]; long quando;
    if (sscanf(p, "%15s %ld", prov, &quando) == 2 && (long)time(NULL) - quando < SY_CRED_VALE_S &&
        (long)time(NULL) - quando >= 0 && nCredRecusadas < SY_CRED_RECUSADAS)
      snprintf(credRecusada[nCredRecusadas++], sizeof credRecusada[0], "%s", prov);
    p = strchr(p, '\n');
    if (p) p++;
  }
  free(t);
}

static void credGravarDisco(void) {
  char buf[SY_CRED_RECUSADAS * 40 + 1];
  int i, n = 0;
  for (i = 0; i < nCredRecusadas; i++)
    n += snprintf(buf + n, sizeof buf - (size_t)n, "%s %ld\n", credRecusada[i], (long)time(NULL));
  buf[n] = 0;
  dados_gravar("cred-recusada.txt", buf);
}

static int credJaRecusada(const char *provider) {
  int i, r = 0;
  pthread_mutex_lock(&credTrava);
  credLerDisco();
  for (i = 0; i < nCredRecusadas && !r; i++)
    if (!strcmp(credRecusada[i], provider)) r = 1;
  pthread_mutex_unlock(&credTrava);
  return r;
}

int sync_credencial_pedido(SyncCredencialPedido *pedido) {
  if (!pedido || !sessao_logada()) return 0;
  memset(pedido, 0, sizeof *pedido);
  pedido->sessao = nv_perfil_sessao_ler();
  pedido->perfil = perfis_ativo();
  snprintf(pedido->usuario, sizeof pedido->usuario, "%s", sessao_usuario());
  return pedido->usuario[0] && nv_perfil_sessao_atual(pedido->sessao);
}

static int credencialPedidoAtual(const SyncCredencialPedido *pedido) {
  return pedido && sessao_logada() && pedido->perfil == perfis_ativo() &&
         !strcmp(pedido->usuario, sessao_usuario()) &&
         nv_perfil_sessao_atual(pedido->sessao);
}

int sync_empurrar_credencial_pedido(const SyncCredencialPedido *pedido,
                                  const char *provider, const char *credJson) {
  Jsw w;
  char *r, token[3000];
  int st = 0, ok;
  if (!credencialPedidoAtual(pedido) || !provider || !*provider || !credJson || !*credJson) return 0;
  // -1, como qualquer recusa 4xx: quem chamou encerra a pendencia.
  if (credJaRecusada(provider)) return -1;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", pedido->perfil);
  jsw_cs(&w, "p_origin_client_id", dados_cliente_id());
  jsw_chave(&w, "p_credentials");
  jsw_arr_ini(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "provider", provider);
  jsw_chave(&w, "credential_json");
  jsw_bruto(&w, credJson);
  jsw_obj_fim(&w);
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  if (!sessao_token_copiar(token, sizeof token) || !credencialPedidoAtual(pedido)) {
    memset(token, 0, sizeof token);
    jsw_livre(&w);
    return 0;
  }
  r = nuvem_rpc_com("sync_push_provider_credentials", jsw_texto_final(&w), token, &st);
  memset(token, 0, sizeof token);
  jsw_livre(&w);
  if (!credencialPedidoAtual(pedido)) { free(r); return 0; }
  ok = ok2xx(r, st) ? 1 : (st >= 400 && st < 500 ? -1 : 0);
  if (!ok2xx(r, st)) {
    printf("[sync] push de credencial %s falhou (HTTP %d): %.200s\n", provider, st, r ? r : "");
    if (st == 400 && r && strstr(r, "Unsupported provider")) {
      int anotou = 0;
      pthread_mutex_lock(&credTrava);
      if (nCredRecusadas < SY_CRED_RECUSADAS) {
        snprintf(credRecusada[nCredRecusadas++], sizeof credRecusada[0], "%s", provider);
        credGravarDisco();
        anotou = 1;
      }
      pthread_mutex_unlock(&credTrava);
      if (anotou) printf("[sync] servidor nao aceita credencial %s: nao tento de novo por 7 dias\n", provider);
    }
  } else printf("[sync] credencial %s guardada na conta\n", provider);
  free(r);
  return ok;
}

int sync_empurrar_credencial(const char *provider, const char *credJson) {
  SyncCredencialPedido pedido;
  return sync_credencial_pedido(&pedido)
       ? sync_empurrar_credencial_pedido(&pedido, provider, credJson) : 0;
}

void sync_reaplicar_ajustes(void) {
  ajustesTemLocal = 0;
  cat_historico_contexto(sessao_usuario(), perfis_ativo());
  // A conta ou o perfil ativo mudou: solta pins dos dois grupos para que cada
  // superfície publique em seguida o conjunto pertencente ao novo contexto.
  cachearte_limpar_referencias();
  aplicarAjustes = 1;
  __atomic_store_n(&ajustesRestaurando, 1, __ATOMIC_RELEASE);
  // Conta manda de novo: a protecao local deixa de valer ate a pessoa mexer.
  dados_apagar(SY_AJUSTES_LOCAIS);
  // E A PENDENCIA DE SUBIDA CAI COM ELA. Esta funcao e chamada ao entrar e ao
  // TROCAR DE PERFIL: uma pendencia do perfil anterior empurrada depois da troca
  // escreveria os valores de um perfil no blob do outro. Quem manda agora e a
  // conta; a pessoa mexer de novo marca de novo.
  sujoAjustes = 0;
  // A base velha e do perfil velho. Solta-la aqui tambem evita que um push do
  // primeiro ciclo costure sobre o blob de outro perfil.
  if (nv_perfil_sessao_travar(nv_perfil_sessao_ler())) {
    free(ajustesBlob);
    ajustesBlob = NULL;
    temAjustesBlob = 0;
    nv_perfil_sessao_destravar();
  }
  // Os idiomas da conta eram do perfil velho: ate o blob do novo chegar,
  // "Da conta" nao tem idioma (#378, revisao).
  ajustes_idiomas_da_conta(NULL);
}

// A pendencia local de um perfil que nao esta ativo. So o perfil ativo usa
// SY_AJUSTES_LOCAIS; os outros guardam a marca deles aqui ate voltarem.
static void nomeLocaisPerfil(char *dst, size_t tam, int perfil) {
  snprintf(dst, tam, "ajustes-locais-p%d.txt", perfil);
}

// 2.1: uma copia de ajustes feita por outro perfil (admin) deixa a pendencia
// escrita no perfil de destino. Nunca sobe agora: so quando ele for o ativo
// (sync_trocar_perfil) e a costura tiver o blob dele.
void sync_marcar_ajustes_pendentes_perfil(int perfil) {
  char nome[40];
  if (perfil <= 0) return;
  if (perfil == perfis_ativo()) { sync_proteger_ajustes_locais(); return; }
  nomeLocaisPerfil(nome, sizeof nome, perfil);
  dados_gravar(nome, "1\n");
}


void sync_trocar_perfil(int antes) {
  int depois = perfis_ativo();
  char nome[40], *t;
  int pendente;
  ajustes_edicao_encerrar();   // 2.1: nunca trocar de perfil com a edicao de outro em curso
  if (antes <= 0 || antes == depois) { sync_reaplicar_ajustes(); return; }
  ajustes_migrar_escopo();   // 2.1: no-op depois da primeira vez; antes de guardar o que sai
  // 1. O QUE SAI. Os valores na memoria ainda sao os dele: a troca acabou de
  //    acontecer e nada foi aplicado desde entao.
  ajustes_perfil_guardar(antes);
  nomeLocaisPerfil(nome, sizeof nome, antes);
  t = dados_ler(SY_AJUSTES_LOCAIS);
  pendente = (t && t[0] == '1') || sujoAjustes;
  free(t);
  if (pendente) dados_gravar(nome, "1\n");
  else dados_apagar(nome);
  // 2. Load this profile over independent defaults, including partial files.
  ajustes_perfil_padrao();
  ajustes_perfil_restaurar(depois);
  // 3. A conta manda de novo (blob do perfil novo), e a base e a pendencia do
  //    perfil anterior caem — ver sync_reaplicar_ajustes.
  sync_reaplicar_ajustes();
  // 4. ...A NAO SER QUE ESTE PERFIL TENHA MUDANCA LOCAL QUE NUNCA SUBIU. Ela
  //    volta a valer como se a pessoa nunca tivesse trocado: o blob da conta
  //    nao e aplicado por cima, e a mudanca sobe no proximo ciclo costurada no
  //    blob DESTE perfil (sem blob, nada sobe — empurrarAjustes).
  nomeLocaisPerfil(nome, sizeof nome, depois);
  t = dados_ler(nome);
  if (t && t[0] == '1') {
    aplicarAjustes = 0;
    __atomic_store_n(&ajustesRestaurando, 0, __ATOMIC_RELEASE);
    dados_gravar(SY_AJUSTES_LOCAIS, "1\n");
    sujoAjustes = 1;
    printf("[sync] perfil %d tinha ajustes desta TV sem subir: mantidos\n", depois);
    fflush(stdout);
  }
  free(t);
  dados_apagar(nome);
  perfilAplicado = 0;
  PRONTO_PUBLICAR(estruturaAplicada, 0);
  __atomic_store_n(&estruturaCedo, 0, __ATOMIC_RELAXED);
  syncprog_esquecer();
}

void sync_abrir_sessao(int antes) {
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  visto_iniciar();
  char *c;
  if (antes > 0 && antes != perfis_ativo()) sync_trocar_perfil(antes);
  // A fresh opening of the same profile also starts a new history map.
  if (nv_perfil_sessao_travar(sessao)) {
    contalib_esquecer();
    vistoep_esquecer();
    cat_historico_contexto("", 0);
    cat_historico_contexto(sessao_usuario(), perfis_ativo());
    nv_perfil_sessao_destravar();
  }
  perfilAplicado = 0;
  PRONTO_PUBLICAR(estruturaAplicada, 0);
  __atomic_store_n(&estruturaCedo, 0, __ATOMIC_RELAXED);
  syncprog_esquecer();
  catordemCachePerfil = -1;
  addonsRestauradosPerfil = 0;
  addonsRestauradosUsuario[0] = 0;
  extras_abrir_sessao();
  desc_abrir_sessao();
  // Validated account/profile envelopes restore only this profile's private
  // data. They remain useful while a cancelled older worker finishes HTTP.
  c = contacache_ler(CC_BIBLIOTECA, perfis_ativo(), sessao_usuario(), NULL);
  if (c) { contalib_ler_biblioteca(c); free(c); }
  c = contacache_ler(CC_VISTOS, perfis_ativo(), sessao_usuario(), NULL);
  if (c) { contalib_ler_vistos(c); if (!trakt_ativo()) contalib_aplicar_vistos(); free(c); }
  c = contacache_ler(CC_COLECOES, perfis_ativo(), sessao_usuario(), NULL);
  if (c) { colfileiras_receber(c); free(c); }
  contapend_aplicar_local();
}

int sync_perfil_pronto(void) {
  return !fioVivo && perfilAplicado == perfis_ativo();
}

int sync_home_fontes_prontas(void) {
  return PRONTO_LER(estruturaAplicada) &&
         PRONTO_LER(estruturaPerfilAplicado) == perfis_ativo() &&
         nv_perfil_sessao_atual(PRONTO_LER(estruturaSessaoAplicada));
}

void sync_proteger_ajustes_locais(void) {
  // Cloud wins while a login/profile restore is pending, including retries.
  if (sessao_logada() && __atomic_load_n(&ajustesRestaurando, __ATOMIC_ACQUIRE)) return;
  aplicarAjustes = 0;
  dados_gravar(SY_AJUSTES_LOCAIS, "1\n");
  // E MARCA PARA SUBIR (#85). As duas coisas andam juntas de proposito: a
  // protecao segura o blob da conta ATE o push acontecer, e o push e o que
  // resolve a divergencia na origem. Se o push funcionar, as duas dizem a mesma
  // coisa; se o servidor nao tiver a funcao ou a rede estiver fora, a protecao
  // continua sendo a unica defesa — e e por isso que ela nao foi removida.
  sujoAjustes = 1;
}

void sync_esquecer_usuario(void) {
  ajustesTemLocal = 0;
  // Reject old worker publications before any private cache is purged.
  perfis_invalidar_sessao();
  cat_historico_contexto("", 0);
  // A ordem importa pouco, mas o CONJUNTO nao: cada linha aqui corresponde a
  // uma coisa que sobrevivia ao logout.
  catordem_esquecer();
  catordem_cache_esquecer();
  catordemCachePerfil = -1;
  // As copias da conta (#215): addons com chave de debrid na URL, biblioteca,
  // vistos e colecoes de quem saiu.
  contacache_esquecer();
  foraHttp = usandoCopia = addonsFora = 0;
  copiaQuando = 0;
  pedidoComFioVivo = 0;
  homeestado_esquecer();
  cachearte_limpar_referencias();
  // O CACHE DO CATALOGO TAMBEM. Ele guarda o catalogo montado da conta que
  // saiu — watchlist, continuar assistindo, feed de amigos com nome e avatar —
  // e, pior, a `base` de cada fileira, que no Xperience carrega um JWT dentro
  // do caminho (catalogo.h:236). Sem esta linha, a proxima abertura mostrava a
  // home de quem saiu ate a rede substituir, com a credencial dele em disco.
  //
  // O cache tambem se recusa sozinho por nao bater o usuario no cabecalho, mas
  // recusar so serve a quem ABRE; apagar e o que tira o arquivo do aparelho.
  cat_apagar_cache();
  // O texto/arte localizados guardados (loc-texto.txt, #213) tambem.
  desc_loc_apagar();
  // O mapa de episodios vistos e da conta que saiu, como todo o resto.
  vistoep_esquecer();
  // O jornal da conta sai da MEMORIA; o arquivo (por usuario, so ids) fica
  // para quando ESTA pessoa voltar — e o que ainda nao subiu nao se perde.
  contapend_esquecer();
  free(catHomeBlob);
  catHomeBlob = NULL;
  temCatHomeBlob = 0;
  // A biblioteca e os vistos da conta anterior. Numa TV de sala isto nao e
  // detalhe: sem esta linha, a proxima pessoa a entrar veria a lista de filmes
  // salvos de quem saiu na tela de Biblioteca dela.
  contalib_esquecer();
  // A LISTA LOCAL DE SALVOS, pelo mesmo motivo da linha acima e com um agravante:
  // ela nao depende de conta nenhuma para existir, entao sem esta chamada ela
  // sobreviveria ao logout em disco e a proxima pessoa abriria o painel da tecla
  // AZUL com os filmes de quem saiu.
  salvos_esquecer();
  // As categorias e o jeito de ver os Salvos (salvosorg.h) vao junto.
  sorg_esquecer();
  // O mapa do gosto da Explorar e derivado do historico de quem saiu.
  mapa_esquecer();
  // E A AGENDA: o calendario e os lembretes sao a lista de series de quem
  // saiu, com o dia em que cada uma volta. Mesmo argumento dos salvos, e com o
  // agravante de a tela Agenda mostrar essa lista inteira de uma vez.
  agenda_esquecer();
  // E os lembretes de programa do guia, pelo mesmo motivo.
  lembrete_esquecer_todos();
  // E AS RECOMENDACOES, pela mesma razao com um agravante proprio: elas trazem
  // o NOME de quem mandou. Sem esta linha a proxima pessoa a entrar abriria a
  // aba Social com a lista de amigos de quem saiu.
  recomenda_esquecer_conta();
  // E A FONTE LEMBRADA DE CADA TITULO. Ela nao chega a ser um segredo, mas diz
  // o que a pessoa assistiu e em que idioma — e, como a lista de salvos, ela
  // nao depende de conta nenhuma para existir, entao sem esta linha ela
  // sobreviveria ao logout em disco e passaria a mandar na reproducao da
  // proxima pessoa.
  fontepref_esquecer();
  // E OS LINKS GUARDADOS ("Reusar o ultimo link", fontevolta.h): no disco, e
  // um link de debrid e credencial na pratica.
  fontevolta_esquecer_tudo();
  // E A LEGENDA ESCOLHIDA A MAO (2.0.3): o idioma e os titulos de quem saiu.
  legmem_esquecer();
  // E A FAIXA DE AUDIO ESCOLHIDA A MAO (audiomemoria.h), pela mesma razao.
  audiomem_esquecer();
  // E A ARTE ESCOLHIDA A MAO (#142): diz o que a pessoa abriu e guardou, e
  // sem esta linha a proxima conta herdaria as fotos de quem saiu.
  arteesc_esquecer();
  // E AS BUSCAS RECENTES. Nao sobem para a conta (sao deste aparelho), mas sao
  // o que a pessoa procurou — a proxima a entrar abriria a Busca com a lista
  // de quem saiu nas pilulas, a um OK de refazer cada uma.
  buscasrec_esquecer();
  free(bibBlob);    bibBlob = NULL;    temBibBlob = 0;
  free(vistosBlob); vistosBlob = NULL; temVistosBlob = 0;
  // colBlob estava de fora desta lista desde que foi criado, ao lado de um
  // catHomeBlob que ja era liberado. Um ciclo que terminou logo antes do logout
  // deixava as colecoes da conta anterior esperando o proximo sync_passo, que
  // as aplicaria na sessao seguinte.
  free(colBlob);    colBlob = NULL;    temColBlob = 0;
  addons_esquecer();
  plugins_esquecer();
  desc_esquecer();   // solta a cache de manifestos da conta que saiu
  debrid_esquecer();
  // O portal IPTV vai junto, e tem de ir: o MAC autentica a assinatura de
  // QUEM SAIU. Deixar o arquivo no aparelho entregaria o acesso pago dessa
  // pessoa para a proxima que logasse nesta TV.
  iptv_contas_esquecer_todas();
  stalker_fontes_invalidar(); m3u_fontes_invalidar(); xtream_fontes_invalidar();
  /* Credentials and caches of every native account/profile were removed. */   // mesma razao: usuario e senha sao a assinatura de quem saiu
  guia_fonte_esquecer("st"); guia_fonte_esquecer("xt");
  servidores_esquecer_todos();   // personal-server tokens of every profile on this TV
  trakt_esquecer();
  // Os ajustes por perfil guardados nesta TV (e as pendencias deles) sao da
  // conta que saiu: a proxima conta nao parte deles.
  ajustes_perfil_esquecer();
  { char nome[40]; int i;
    for (i = 1; i <= 32; i++) { nomeLocaisPerfil(nome, sizeof nome, i); dados_apagar(nome); } }
  perfilAplicado = 0;
  perfis_esquecer();
  perfil_infantil_esquecer();   // tambem apaga as marcas de ajustes travados
  prog_esquecer_tudo();
  perfilcont_esquecer();
  psparede_esquecer();   // a parede de cada perfil (fundo "Filmes") e da conta que saiu
  syncprog_esquecer();
  __atomic_store_n(&progressoCedo, 0, __ATOMIC_RELAXED);
  __atomic_store_n(&estruturaCedo, 0, __ATOMIC_RELAXED);
  PRONTO_PUBLICAR(estruturaAplicada, 0);
  progressoAplicadoCedo = 0;

  // As caixas que o fio preenche tambem: um ciclo que terminou logo antes do
  // logout aplicaria os addons da conta anterior no proximo sync_passo.
  memset(addonsRem, 0, sizeof addonsRem);
  nAddonsRem = 0; temAddonsRem = 0; __atomic_store_n(&addonsCedo, 0, __ATOMIC_RELAXED);
  traktTok[0] = 0; temTraktRem = 0;
  memset(tmdbKey, 0, sizeof tmdbKey); temTmdb = 0;
  memset(mdbKey, 0, sizeof mdbKey);   temMdb = 0;
  cVistos = cBiblio = cColecoes = 0;
  temAjustesPerfil = temCatHome = 0;
  estado = SYNC_PARADO;
  ultimoOk = 0;
  sujoProgresso = 0; sujoAjustes = 0;
  // Um pedido do botao de addons (#360) no ar e da conta que saiu.
  manualPedido = manualRodando = manualResultado = 0;
  addonsEsquecer();
  addonsRestauradosPerfil = 0; addonsRestauradosUsuario[0] = 0;
  free(ajustesBlob);
  ajustesBlob = NULL;
  temAjustesBlob = 0;
  aplicarAjustes = 1;
  __atomic_store_n(&ajustesRestaurando, 1, __ATOMIC_RELEASE);
  dados_apagar(SY_AJUSTES_LOCAIS);
  resumoFixo("sem conta");
  printf("[sync] dados do usuario apagados deste aparelho\n");
}

void        sync_encerrar(void)    { }
