void servidores_esquecer_todos(void);   // servidores.c: wipe every profile's token
#include "sync.h"
#include <time.h>
#include "sessao.h"
#include "nuvem.h"
#include "perfis.h"
#include "agenda.h"
#include "lembrete.h"
#include "dados.h"
#include "addons.h"
#include "plugins.h"
#include "debrid.h"
#include "stalker.h"
#include "xtream.h"
#include "colecoes.h"
#include "colfileiras.h"
#include "contalib.h"
#include "salvosorg.h"
#include "salvos.h"
#include "mapa.h"
#include "recomenda.h"
#include "fontepref.h"
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
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#define SY_ADD_MAX   32   // o mesmo teto de ADD_MAX (addons.c)
// URL lida da conta ANTES de caber em AddonRemoto: maior que o limite de
// proposito, para o log dizer o tamanho de verdade em vez de "nao coube".
#define SY_URL_LEITURA (NV_ADDON_URL_MAX * 4)

static pthread_t fio;
static int fioVivo, fioPronto;
static SyncEstado estado = SYNC_PARADO;
static char resumo[220] = "sem sincronizar";
static unsigned ultimoOk;
static int sujoProgresso, sujoAjustes;
// Capturado no fio principal: o worker nao le a lista enquanto ela muda.
static unsigned addonsRev, addonsRevCiclo;
static int addonsPerfilCiclo, addonsLocalCiclo, nAddonsEnv;
static char addonsEdicaoCiclo[37];
static AddonRemoto addonsEnv[SY_ADD_MAX];
static pthread_mutex_t addonsTrava = PTHREAD_MUTEX_INITIALIZER;
// Cada identidade guarda sua edicao, inclusive quando o disco falha e a pessoa
// troca de perfil. URLs ficam apenas na pasta de dados; conta-*.txt tambem sai
// dos pacotes caso essa pasta tenha caido no ultimo recurso (art/).
#define SY_ADD_PREFIXO "conta-addons-pend-"
typedef struct AddonsPendencia {
  struct AddonsPendencia *prox;
  char dono[80], edicao[37];
  int perfil, n, pendente, gravada;
  unsigned rev;
  AddonRemoto lista[SY_ADD_MAX];
} AddonsPendencia;
static AddonsPendencia *addonsFila;
static char addonsContexto[80];
static int addonsContextoPerfil;
static char addonsLeituraDono[80];
static int addonsLeituraPerfil;

// Sempre sob addonsTrava; o quadro so consulta a fila, nunca o disco.
static AddonsPendencia *addonsEdicao(const char *dono, int perfil) {
  AddonsPendencia *p;
  for (p = addonsFila; p; p = p->prox)
    if (p->perfil == perfil && !strcmp(p->dono, dono)) return p;
  return NULL;
}
// Lazy, no maximo ~175 KB. Entradas ja salvas podem sair da RAM porque voltam
// do arquivo na proxima troca; nunca descartar uma edicao que o disco recusou.
static AddonsPendencia *addonsNova(const char *dono, int perfil) {
  AddonsPendencia *p, **elo, **vitima = NULL;
  int n = 0;
  for (elo = &addonsFila; *elo; elo = &(*elo)->prox) {
    n++;
    if (!(*elo)->pendente || (*elo)->gravada) vitima = elo;
  }
  if (n >= 8 && !vitima) {
    printf("[sync] fila de addons cheia com edicoes sem disco: edicao atual nao guardada\n");
    return NULL;
  }
  p = calloc(1, sizeof *p);
  if (!p) { printf("[sync] sem memoria para guardar edicao de addons\n"); return NULL; }
  if (n >= 8) { AddonsPendencia *velha = *vitima; *vitima = velha->prox; free(velha); }
  snprintf(p->dono, sizeof p->dono, "%s", dono); p->perfil = perfil;
  p->prox = addonsFila; addonsFila = p;
  return p;
}
static int addonsNome(char *dst, unsigned tam, const char *dono, int perfil) {
  static const char hex[] = "0123456789abcdef";
  char chave[160];
  size_t i, n = dono ? strlen(dono) : 0;
  int escritos;
  if (!n || n >= 80 || perfil < 1 || perfil > 32) return 0;
  for (i = 0; i < n; i++) {
    chave[2*i] = hex[(unsigned char)dono[i] >> 4];
    chave[2*i+1] = hex[(unsigned char)dono[i] & 15];
  }
  chave[2*n] = 0;
  escritos = snprintf(dst, tam, SY_ADD_PREFIXO "%s-p%d.txt", chave, perfil);
  return escritos > 0 && (unsigned)escritos < tam;
}
static int addonsGuardar(AddonsPendencia *p) {
  char nome[220];
  Jsw w;
  const char *texto;
  int i;
  if (!addonsNome(nome, sizeof nome, p->dono, p->perfil)) return 0;
  jsw_iniciar(&w); jsw_obj_ini(&w);
  jsw_ci(&w, "version", 1); jsw_cs(&w, "owner", p->dono);
  jsw_ci(&w, "profile", p->perfil); jsw_cs(&w, "edit", p->edicao);
  jsw_chave(&w, "addons"); jsw_arr_ini(&w);
  for (i = 0; i < p->n; i++) {
    jsw_obj_ini(&w); jsw_cs(&w, "url", p->lista[i].url);
    jsw_cs(&w, "name", p->lista[i].nome);
    jsw_cb(&w, "enabled", p->lista[i].ativo); jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w); jsw_obj_fim(&w);
  texto = jsw_texto_final(&w);
  p->gravada = texto && dados_gravar(nome, texto);
  jsw_livre(&w);
  if (!p->gravada) {
    printf("[sync] nao foi possivel guardar edicao de addons: mantida em RAM, envio aguarda disco\n");
    fflush(stdout);
  }
  return p->gravada;
}

// 0 somente quando o arquivo certamente nao existe; erro de disco nao e
// ausencia. Esse stat so roda quando a leitura de um novo ciclo falhou.
static int addonsArquivoPodeExistir(const char *nome) {
  char caminho[600];
  struct stat st;
  const char *pasta = dados_dir();
  if (!pasta || !*pasta ||
      snprintf(caminho, sizeof caminho, "%s/%s", pasta, nome) >= (int)sizeof caminho) return 1;
  return !stat(caminho, &st) || errno != ENOENT;
}

static int addonsLer(AddonsPendencia *p, const char *texto) {
  const char *fim, *a, *f, *s;
  char dono[80], habilitado[8];
  int n = 0;
  if (!texto || strlen(texto) > 140000 || *texto != '{') return 0;
  fim = js_fim(texto);
  if (fim <= texto || fim[-1] != '}') return 0;
  for (s = fim; *s && (unsigned char)*s <= ' '; s++) {}
  if (*s || js_num(texto, fim, "version", 0) != 1 ||
      js_num(texto, fim, "profile", 0) != p->perfil ||
      !js_texto_raiz(texto, "owner", dono, sizeof dono) || strcmp(dono, p->dono) ||
      !js_texto_raiz(texto, "edit", p->edicao, sizeof p->edicao) ||
      strlen(p->edicao) != 36) return 0;
  for (a = js_array(texto, fim, "addons"); a; a = js_prox(f)) {
    if (*a != '{' || n == SY_ADD_MAX) return 0;
    f = js_fim(a);
    if (f <= a || f > fim || f[-1] != '}') return 0;
    // Mesma regra de lerAddons: URL que nao cabe invalida a edicao inteira em
    // vez de entrar cortada — esta lista e a que vai ser EMPURRADA para a conta.
    { static char url[SY_URL_LEITURA];   // sob addonsTrava (addonsRestaurar)
      if (!js_texto_raiz_em(a, f, "url", url, sizeof url) ||
          !nv_addon_url_cabe("edicao local", strlen(url))) return 0;
      snprintf(p->lista[n].url, sizeof p->lista[n].url, "%s", url); }
    if (!p->lista[n].url[0] ||
        !js_bruto(a, f, "enabled", habilitado, sizeof habilitado) ||
        (strcmp(habilitado, "true") && strcmp(habilitado, "false"))) return 0;
    // O servidor aceita addon sem nome. O texto vazio nao invalida sua URL
    // nem pode impedir que um liga/desliga sobreviva ao proximo arranque.
    p->lista[n].nome[0] = 0;
    js_texto_raiz_em(a, f, "name", p->lista[n].nome, sizeof p->lista[n].nome);
    p->lista[n++].ativo = !strcmp(habilitado, "true");
  }
  if (!n) return 0; // nunca interpretar arquivo incompleto como remocao total
  p->n = n; p->pendente = p->gravada = 1; p->rev = ++addonsRev;
  return 1;
}

// Chamado no fio principal ANTES da rede, inclusive com um ciclo antigo no ar.
// Reentrar na mesma identidade nao relê nem refaz descoberta.
static void addonsRestaurar(void) {
  AddonsPendencia *p;
  // static: 32 x ~2 KB (NV_ADDON_URL_MAX) nao pertence a pilha. So o fio
  // principal entra aqui.
  static AddonRemoto lista[SY_ADD_MAX];
  char nome[220], *texto;
  const char *dono = sessao_usuario();
  int perfil = perfis_ativo_addons(), n = 0, leituraFalhou = 0;
  if (!sessao_logada() || !addonsNome(nome, sizeof nome, dono, perfil)) return;
  pthread_mutex_lock(&addonsTrava);
  if (addonsContextoPerfil == perfil && !strcmp(addonsContexto, dono)) {
    pthread_mutex_unlock(&addonsTrava); return;
  }
  addonsLeituraDono[0] = 0; addonsLeituraPerfil = 0;
  p = addonsEdicao(dono, perfil);
  if (!p) {
    texto = dados_ler(nome);
    if (texto) {
      p = addonsNova(dono, perfil);
      if (!p) leituraFalhou = 1;
      if (p) {
        if (!addonsLer(p, texto)) {
          addonsFila = p->prox; free(p); p = NULL;
          printf("[sync] edicao de addons invalida: nao aplicada\n");
        }
      }
    } else leituraFalhou = addonsArquivoPodeExistir(nome);
    free(texto);
  }
  if (leituraFalhou) {
    // A lista salva nao chegou a memoria: bloquear o pull antigo e tentar
    // ler de novo no proximo ciclo. Nunca fazer essa retentativa por quadro.
    snprintf(addonsLeituraDono, sizeof addonsLeituraDono, "%s", dono);
    addonsLeituraPerfil = perfil;
    printf("[sync] leitura de edicao de addons falhou: pull aguarda copia local\n");
    pthread_mutex_unlock(&addonsTrava); return;
  }
  snprintf(addonsContexto, sizeof addonsContexto, "%s", dono);
  addonsContextoPerfil = perfil;
  if (p && p->pendente) { n = p->n; memcpy(lista, p->lista, (size_t)n * sizeof *lista); }
  pthread_mutex_unlock(&addonsTrava);
  if (n) {
    int mudou = addons_definir_lista(lista, n);
    addons_marcar_da_conta(perfis_ativo());
    if (mudou) desc_repetir_addons();
    printf("[sync] edicao local de addons restaurada antes da rede\n");
  }
}
static int addonsPendentes(void) {
  int pendente;
  AddonsPendencia *p;
  pthread_mutex_lock(&addonsTrava);
  p = addonsEdicao(sessao_usuario(), perfis_ativo_addons());
  pendente = (p && p->pendente) ||
             (addonsLeituraPerfil == perfis_ativo_addons() &&
              !strcmp(addonsLeituraDono, sessao_usuario()));
  pthread_mutex_unlock(&addonsTrava);
  return pendente;
}


// O fio NAO toca no app: ele so preenche estas caixas, e sync_passo aplica no
// laco principal. Sem essa separacao, uma resposta de rede reescreveria a lista
// de addons no meio de um quadro que ja estava lendo dela.
static AddonRemoto addonsRem[SY_ADD_MAX];
static int nAddonsRem, temAddonsRem;
// QUANTOS ADDONS DA ULTIMA LEITURA DA CONTA FICARAM DE FORA por terem URL maior
// que NV_ADDON_URL_MAX (addonurl.h). Com isto acima de zero a lista desta TV e
// MENOR que a da conta, e o push manda a lista INTEIRA: empurrar apagaria da
// conta, em todos os aparelhos, justamente o addon que nao coube aqui. Ver
// sync_sujar_addons.
static volatile int addonsDeFora;
// Addons prontos ANTES do fim do ciclo. Ver a publicacao antecipada em
// sync_passo. `volatile` porque quem escreve e o fio de sync e quem le e o fio
// principal, e a barreira aqui e a mesma que `fioPronto` sempre foi: o valor so
// e ligado DEPOIS de o buffer estar cheio.
static volatile int addonsCedo;

static char traktTok[300];
static int  temTraktRem;

// Chaves de servico que a conta guarda e o app lia de arquivo do dono.
// MEDIDO na conta real: os provedores presentes sao animeskip, debrid:*,
// introdb, mdblist e tmdb — e NAO ha "trakt". O leitor de trakt continua aqui
// porque a RPC e a mesma e a linha aparece assim que o app web a escrever.
static char tmdbKey[120], mdbKey[120];
static int  temTmdb, temMdb;


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
static int  temAjustesBlob;
static int  aplicarAjustes = 1;
// Flag em disco: a pessoa ja mudou ajustes nesta TV depois do ultimo
// sync_reaplicar_ajustes. Sem ela, CADA arranque comecava com aplicarAjustes=1
// e o blob da conta (ainda sem as mudancas locais — a TV nao empurra layout)
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
static long copiaQuandoCiclo;
static int  foraHttp, usandoCopia;
static long copiaQuando;
// Status HTTP da ultima puxarBlob (-2 quando nem perguntou: RPC ausente).
static int  ultimoSt;
static char usuarioDoCiclo[80];
static unsigned copiaGeracaoDoCiclo;

static int contaDoCicloAtual(void) {
  return sessao_logada() && !strcmp(usuarioDoCiclo, sessao_usuario()) &&
         copiaGeracaoDoCiclo == contacache_geracao();
}
static int perfilDoCicloAtual(void) {
  return contaDoCicloAtual() && perfilDoCiclo == perfis_ativo();
}

static void addonsConfirmar(void) {
  AddonsPendencia *p;
  char nome[220], edicao[37], dono[80], *texto;
  int confere;
  pthread_mutex_lock(&addonsTrava);
  p = addonsEdicao(usuarioDoCiclo, addonsPerfilCiclo);
  if (!perfilDoCicloAtual() || addonsPerfilCiclo != perfis_ativo_addons() ||
      !p || !p->pendente || !p->gravada || p->rev != addonsRevCiclo ||
      strcmp(p->edicao, addonsEdicaoCiclo) ||
      !addonsNome(nome, sizeof nome, p->dono, p->perfil)) {
    pthread_mutex_unlock(&addonsTrava); return;
  }
  // Nao apagar sequer uma edicao diferente que apareca no disco entre ciclos.
  // O mesmo mutex serializa a leitura/remocao com a gravacao de uma edicao nova.
  texto = dados_ler(nome);
  confere = texto && js_texto_raiz(texto, "edit", edicao, sizeof edicao) &&
            !strcmp(edicao, addonsEdicaoCiclo) &&
            js_texto_raiz(texto, "owner", dono, sizeof dono) && !strcmp(dono, p->dono) &&
            js_num(texto, NULL, "profile", 0) == p->perfil;
  if (confere && dados_apagar(nome)) {
    char tmp[224];
    p->pendente = 0;
    snprintf(tmp, sizeof tmp, "%s.tmp", nome); dados_apagar(tmp);
  } else {
    // NULL tambem pode ser falha de leitura/alocacao: nao e prova de que o
    // arquivo sumiu. Regravar antes da retentativa recupera ambos os casos.
    if (!texto) p->gravada = 0;
    printf("[sync] ACK de addons sem remover edicao local: retentativa preservada\n");
    fflush(stdout);
  }
  free(texto);
  pthread_mutex_unlock(&addonsTrava);
}

static void addonsEsquecer(void) {
  AddonsPendencia *p;
  DIR *d;
  struct dirent *e;
  const char *pasta = dados_dir();
  pthread_mutex_lock(&addonsTrava);
  // Inclui arquivos de outras identidades e os temporarios interrompidos; o
  // logout deste aparelho limpa todas as pendencias, nao so o perfil aberto.
  if (pasta && *pasta && (d = opendir(pasta)) != NULL) {
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
      if (!dados_apagar(nome)) printf("[sync] nao foi possivel apagar pendencia de addons no logout\n");
    }
    closedir(d);
  } else if (pasta && *pasta) {
    printf("[sync] nao foi possivel conferir pendencias de addons no logout\n");
  }
  while ((p = addonsFila) != NULL) { addonsFila = p->prox; free(p); }
  addonsContexto[0] = 0; addonsContextoPerfil = 0;
  addonsLeituraDono[0] = 0; addonsLeituraPerfil = 0;
  addonsDeFora = 0;
  pthread_mutex_unlock(&addonsTrava);
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

// Le a resposta da tabela `addons` (ou a copia dela) para addonsRem.
static int lerAddons(const char *r) {
  // static: so o fio do sync le a conta, e 8 KB nao pertencem a pilha dele.
  static char url[SY_URL_LEITURA];
  const char *p;
  int k = 0, fora = 0;
  for (p = js_raiz_array(r); p && k < SY_ADD_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char b[16];
    memset(&addonsRem[k], 0, sizeof addonsRem[k]);
    if (!js_texto(p, f, "url", url, sizeof url)) continue;
    js_texto(p, f, "name", addonsRem[k].nome, sizeof addonsRem[k].nome);
    // A URL QUE NAO CABE NAO ENTRA CORTADA (#201). js_texto corta em silencio
    // no tamanho do destino, e era aqui que a URL do Comet (870) virava 599: o
    // addon recebia meia configuracao e respondia como se ela fosse valida.
    if (!nv_addon_url_cabe(addonsRem[k].nome, strlen(url))) { fora++; continue; }
    snprintf(addonsRem[k].url, sizeof addonsRem[k].url, "%s", url);
    // Ausente conta como LIGADO: e assim que o web le, e um addon que some por
    // causa de um campo que o servidor nao mandou e pior que um a mais.
    addonsRem[k].ativo = js_bruto(p, f, "enabled", b, sizeof b)
                         ? (strcmp(b, "false") != 0) : 1;
    k++;
  }
  addonsDeFora = fora;
  return k;
}

// 1 = a conta respondeu; 0 = falhou e nao adianta repetir (4xx, tabela
// ausente); -1 = o SERVIDOR falhou (sem resposta, 429, 5xx) — o ciclo inteiro
// passa a usar as copias locais (ver rodar).
static int puxarAddons(void) {
  char consulta[400], dono[80];
  char *r;
  int st = 0, k, perfilAddons = perfis_ativo_addons();

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
  r = sessao_tabela("addons", consulta, &st);
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
      // Com mudanca local pendente a lista DESTA TV e a certa (ela nao subiu
      // ainda): a copia nao passa por cima dela.
      if (k > 0 && !addonsPendentes()) {
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
  temAddonsRem = 1;
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

static void empurrarAddons(void) {
  const AddonRemoto *atuais = addonsEnv;
  Jsw w;
  char *r;
  int st = 0, n, i;

  n = nAddonsEnv;
  // Lista local vazia NAO vira push. Um push vazio apaga os addons da pessoa em
  // todos os aparelhos dela, e "ainda nao carreguei nada" e indistinguivel de
  // "o usuario removeu tudo" deste lado.
  if (n <= 0) return;

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  // O MESMO perfil da leitura. Ler do perfil 1 e escrever no indice do perfil
  // atual criaria uma copia divergente a cada sync; escrever no 1 sem ler dele
  // sobrescreveria os addons de quem compartilha.
  jsw_ci(&w, "p_profile_id", addonsPerfilCiclo);
  jsw_chave(&w, "p_addons");
  jsw_arr_ini(&w);
  for (i = 0; i < n; i++) {
    jsw_obj_ini(&w);
    jsw_cs(&w, "url", atuais[i].url);
    jsw_ci(&w, "sort_order", i);
    jsw_cb(&w, "enabled", atuais[i].ativo);
    if (atuais[i].nome[0]) jsw_cs(&w, "name", atuais[i].nome);
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  r = sessao_rpc("sync_push_addons", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (!ok2xx(r, st)) {
    printf("[sync] push de addons falhou (HTTP %d): edicao local mantida\n", st);
    if (contacache_falha_transitoria(st)) falhaServidor(st);
  } else addonsConfirmar();
  free(r);
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
  int st = 0, k, perfil = perfis_ativo_addons();
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
    r = sessao_rpc("sync_push_plugins", jsw_texto_final(&w), &st);
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
  r = sessao_tabela("plugins", consulta, &st);
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
  jsw_ci(&w, "p_profile_id", perfis_ativo());
  jsw_obj_fim(&w);
  r = sessao_rpc("sync_pull_provider_credentials", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (!ok2xx(r, st)) { free(r); return; }

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
        debrid_definir_chave(prov + 7, k);
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
  r = sessao_rpc(funcao, corpo, &st);
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
// a PRIMEIRA pagina falhou — as seguintes falhando so encurtam a lista, e a
// lista curta ainda e melhor que nenhuma: o contalib guarda as mais recentes do
// que chegou).
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
      if (!pagina) return -1;
      printf("[sync] biblioteca: pagina %d falhou; ficando com %d linhas\n",
             pagina + 1, total);
      break;
    }
    if (pagina && pag) {
      if (!colarArray(&acum, pag)) {
        printf("[sync] biblioteca: pagina %d nao colou; ficando com %d linhas\n",
               pagina + 1, total);
        free(pag); pag = NULL;
        break;
      }
      free(pag); pag = NULL;
    }
    total += c;
    if (c < CONTALIB_PAGINA) break;
    if (pagina + 1 == CONTALIB_PAGINAS)
      printf("[sync] biblioteca: %d linhas baixadas e a conta tem mais; o "
             "resto nao foi pedido\n", total);
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
  r = sessao_rpc("sync_pull_profile_settings_blob", corpo, &st);
  if (!ok2xx(r, st)) { free(r); return 0; }
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
            free(ajustesBlob);
            ajustesBlob = novo;
            temAjustesBlob = aplicarAjustes;
            ok = 1;
            printf("[sync] blob de ajustes: %d bytes\n", (int)n);
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
//   3. SO CHAVE QUE JA EXISTE, so o valor dela (ajustes_mesclar_blob).
//
// A base guardada e trocada pelo que foi ACEITO: sem isso, o ciclo seguinte
// costuraria sobre um blob que o servidor ja nao tem mais e mandaria a mesma
// diferenca de novo.
static void empurrarAjustes(void) {
  char *mesclado = NULL, *r;
  Jsw w;
  int st = 0;
  if (!sujoAjustes) return;
  if (jaAusente("sync_push_profile_settings_blob")) return;
  if (!ajustesBlob) {
    printf("[sync] ajustes locais pendentes, mas sem blob da conta para costurar: nada enviado\n");
    return;
  }
  if (ajustes_mesclar_blob(ajustesBlob, &mesclado) <= 0 || !mesclado) {
    // Nada diferente do que a conta ja tem: pendencia resolvida sem viagem.
    free(mesclado);
    sujoAjustes = 0;
    return;
  }
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfis_ativo());
  jsw_cs(&w, "p_platform", "tv");
  jsw_chave(&w, "p_settings_json");
  jsw_bruto(&w, mesclado);
  jsw_obj_fim(&w);
  r = sessao_rpc("sync_push_profile_settings_blob", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (ok2xx(r, st)) {
    sujoAjustes = 0;
    free(ajustesBlob);
    ajustesBlob = mesclado;
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
}

// A resposta inteira e guardada, nao interpretada aqui: quem le e catordem.c,
// no fio principal. Interpretar neste fio e mexer na ordem das fileiras no meio
// de um quadro que ja esta desenhando a home — a mesma razao que faz os addons
// esperarem sync_passo.
static int puxarCatHome(const char *corpo) {
  char *r;
  int st = 0;
  if (jaAusente("sync_pull_home_catalog_settings")) return 0;
  r = sessao_rpc("sync_pull_home_catalog_settings", corpo, &st);
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

static void *rodar(void *u) {
  (void)u;
  cicloInicioMs = contapend_agora_ms();
  foraCiclo = 0;
  copiaCiclo = 0;
  addonsCiclo = 0;
  copiaQuandoCiclo = 0;
  perfis_puxar();
  if (!contaDoCicloAtual()) {
    estado = SYNC_PRONTO;
    fioPronto = 1;
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
    snprintf(resumo, sizeof resumo, "aguardando escolha de perfil");
    cicloInterrompido = 1;
    estado = SYNC_PRONTO;
    fioPronto = 1;
    return NULL;
  }
  // Lido DEPOIS da porteira: quando a pessoa responde enquanto perfis_puxar
  // ainda esta no ar, este ciclo ja segue com o perfil escolhido.
  perfilDoCiclo = perfis_ativo();
  if (puxarAddons() < 0) {
    char d[32];
    if (temAddonsRem && !addonsPendentes()) addonsCedo = 1;
    soLeituraDaCopia(foraCiclo > 0 ? foraCiclo : 0);
    contacache_data(copiaQuandoCiclo, d, sizeof d);
    if (copiaCiclo)
      snprintf(resumo, sizeof resumo, i18n("servidor da conta fora do ar (HTTP %d) · usando a cópia de %s"),
               foraCiclo > 0 ? foraCiclo : 0, d);
    else
      snprintf(resumo, sizeof resumo, i18n("servidor da conta fora do ar (HTTP %d) · sem cópia salva"),
               foraCiclo > 0 ? foraCiclo : 0);
    estado = SYNC_PRONTO;
    fioPronto = 1;
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
  // SO QUANDO NAO HA MUDANCA LOCAL PENDENTE. Com `sujoAddons`, a ordem antiga e
  // que esta certa: `empurrarAddons` (abaixo) manda a lista DESTA TV, com o
  // addon que a pessoa acabou de ligar, e so depois a lista da conta e
  // aplicada. Antecipar ali sobrescreveria a escolha antes de ela ser enviada,
  // e a pessoa veria o proprio toque desaparecer.
  if (temAddonsRem && !addonsPendentes()) addonsCedo = 1;
  puxarCredenciais();
  syncprog_puxar();
  puxarSoLeitura();
  // Empurrar DEPOIS de puxar, como o startupSyncService do web: puxar depois
  // de empurrar faria o aparelho sobrescrever com o que ele mesmo mandou.
  // O puxado NAO e aplicado aqui, e sim em sync_passo, no fio principal — e
  // la a regra e "pendente local vence": o que se assistiu entre o pull e o
  // push nao volta atras.
  // A PESSOA TROCOU DE PERFIL NO MEIO DO CICLO: nada sobe. Cada push leva
  // p_profile_id = perfis_ativo() (agora o perfil NOVO), mas a base da costura
  // (o blob de ajustes, a lista de addons) veio do perfil ANTERIOR — subir
  // seria escrever o perfil 1 dentro do 2 na conta. sync_passo descarta o que
  // foi puxado e pede a volta certa.
  if (!perfilDoCicloAtual()) {
    printf("[sync] perfil trocado no meio do ciclo (%d -> %d): nada sobe\n",
           perfilDoCiclo, perfis_ativo());
    fflush(stdout);
    snprintf(resumo, sizeof resumo, "perfil trocado; sincronizando de novo");
    estado = SYNC_PRONTO;
    fioPronto = 1;
    return NULL;
  }
  if (addonsLocalCiclo) empurrarAddons();
  // DEPOIS de puxarSoLeitura, pelo mesmo motivo dos addons e com um agravante:
  // a base da costura e o blob que acabou de chegar. Ver empurrarAjustes.
  empurrarAjustes();
  sincronizarPlugins();
  // Sempre, nao so quando `sujoProgresso`: linhas migradas do formato antigo
  // nascem pendentes sem ninguem ter marcado nada.
  // "Enviar histórico para a conta Nuvio" desligado neste perfil: o progresso
  // nao sobe (o pull continua, a conta pode ter coisa de outra TV).
  if (!ajustes_hist_conta()) sujoProgresso = 0;
  else if (syncprog_empurrar() >= 0) sujoProgresso = 0;
  // O JORNAL DA CONTA (vistos e Salvos marcados nesta TV): DEPOIS do pull,
  // como o resto. E a repeticao de quem falhou offline — cada ciclo tenta de
  // novo o que ainda nao teve 2xx.
  contapend_enviar();

  { int pend = contapend_pendentes();
    char sufixo[48] = "";
    if (pend > 0) snprintf(sufixo, sizeof sufixo, i18n(" · %d pendentes"), pend);
  snprintf(resumo, sizeof resumo,
           i18n("%d addons · %d progressos · %d vistos · %d na lista · %d coleções%s%s"),
           nAddonsRem, syncprog_puxadas(), cVistos < 0 ? 0 : cVistos,
           cBiblio < 0 ? 0 : cBiblio, cColecoes < 0 ? 0 : cColecoes,
           temTraktRem ? " · Trakt" : "", sufixo); }
  estado = SYNC_PRONTO;
  fioPronto = 1;
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

void sync_iniciar(void) {
  plugins_perfil_mudou();   // conta/perfil novos: estado e geracao dos plugins
  colfileiras_contexto();
  cat_historico_contexto(sessao_logada() ? sessao_usuario() : "",
                         sessao_logada() ? perfis_ativo() : 0);
  restaurarOrdemLocal();
  if (!sessao_logada()) return;
  addonsRestaurar();
  // PEDIDO COM O FIO VIVO NAO SE PERDE. Voltar calado deixava um buraco: o
  // fio que estava no ar pode ser justamente o interrompido pela pergunta de
  // perfil, e ai o ciclo completo do perfil escolhido so partia com
  // sync_periodico, cinco minutos depois (o ciclo interrompido marca
  // SYNC_PRONTO e conta como `ultimoOk`). sync_passo decide, quando o fio
  // acabar, se precisa de outra volta.
  if (fioVivo) { pedidoComFioVivo = 1; return; }
  pedidoComFioVivo = 0;
  if (nuvem_freio_ativo()) return;
  addonsPerfilCiclo = perfis_ativo_addons();
  pthread_mutex_lock(&addonsTrava);
  AddonsPendencia *p = addonsEdicao(sessao_usuario(), addonsPerfilCiclo);
  addonsLocalCiclo = p && p->pendente;
  nAddonsEnv = 0; addonsEdicaoCiclo[0] = 0;
  if (addonsLocalCiclo && (p->gravada || addonsGuardar(p))) {
    addonsRevCiclo = p->rev; nAddonsEnv = p->n;
    snprintf(addonsEdicaoCiclo, sizeof addonsEdicaoCiclo, "%s", p->edicao);
    memcpy(addonsEnv, p->lista, (size_t)p->n * sizeof *addonsEnv);
  }
  pthread_mutex_unlock(&addonsTrava);
  cicloInterrompido = 0;
  perfilDoCiclo = perfis_ativo();
  snprintf(usuarioDoCiclo, sizeof usuarioDoCiclo, "%s", sessao_usuario());
  copiaGeracaoDoCiclo = contacache_geracao();
  estado = SYNC_RODANDO;
  fioPronto = 0;
  if (pthread_create(&fio, NULL, rodar, NULL) == 0) { pthread_detach(fio); fioVivo = 1; }
  else { estado = SYNC_FALHOU; snprintf(resumo, sizeof resumo, "sem fio para sincronizar"); }
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

void sync_passo(unsigned agoraMs) {
  // ANTES DA PORTEIRA de `fioPronto`: ver o comentario em rodar(). O resto do
  // ciclo continua sendo aplicado de uma vez, no fim — so os addons saem na
  // frente, porque so eles mudam O QUE a descoberta vai buscar.
  if (addonsCedo) {
    addonsCedo = 0;
    if (temAddonsRem && perfilDoCicloAtual() && !addonsPendentes()) {
      // _addons: a volta que ainda nao leu a lista (o caso do arranque e da
      // escolha de perfil) atende o pedido sozinha, sem ser jogada fora.
      // A lista vale para a poda de fileiras SO DEPOIS de marcada como deste
      // perfil — e antes de desc_repetir_addons, para a volta que ela dispara
      // ja poder podar. Ver addons_marcar_da_conta.
      { int mudou = addons_definir_lista(addonsRem, nAddonsRem);
        if (nAddonsRem > 0) addons_marcar_da_conta(perfilDoCiclo);
        if (mudou) desc_repetir_addons(); }
      temAddonsRem = 0;
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
  if (!fioVivo || !fioPronto) return;
  fioVivo = 0;
  fioPronto = 0;
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
  if (!contaDoCicloAtual() || (!cicloInterrompido && !perfilDoCicloAtual())) {
    printf("[sync] ciclo do perfil %d descartado: o perfil ativo agora e %d\n",
           perfilDoCiclo, perfis_ativo());
    fflush(stdout);
    temAddonsRem = 0;
    temTraktRem = 0; traktTok[0] = 0;
    temTmdb = temMdb = 0;
    free(catHomeBlob); catHomeBlob = NULL; temCatHomeBlob = 0;
    free(colBlob);     colBlob = NULL;     temColBlob = 0;
    free(bibBlob);     bibBlob = NULL;     temBibBlob = 0;
    free(vistosBlob);  vistosBlob = NULL;  temVistosBlob = 0;
    temAjustesBlob = 0;
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
    int atualizarColecoes = temColBlob || temCatHomeBlob;
  // SO REMONTA QUANDO A LISTA MUDOU DE VERDADE. Ligar `remontar` porque a
  // resposta chegou fazia um ciclo de descoberta completo a cada cinco minutos
  // com a lista identica — ver listaIgual em addons.c.
  if (temAddonsRem && !addonsPendentes() && !addonsLocalCiclo) {
    if (addons_definir_lista(addonsRem, nAddonsRem)) soAddons = 1;
    // O ciclo de outro perfil ja foi descartado acima: esta lista e do ativo.
    if (nAddonsRem > 0) addons_marcar_da_conta(perfilDoCiclo);
  }
  // O pull precede o push: depois dele a caixa antiga nao vale como confirmacao.
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
  if (temTraktRem)  { if (traktauth_estado() != TRA_LIGADO) {
                        if (!trakt_credencial_igual(traktTok, nuvem_trakt_cliente())) {
                          trakt_definir(traktTok, nuvem_trakt_cliente()); remontar = 1; } }
                      else printf("[sync] trakt: vinculo local mantido, credencial da conta ignorada\n");
                      temTraktRem = 0; }
  if (temTmdb)      { desc_tmdb_definir(tmdbKey);   temTmdb = 0; }
  if (temMdb)       { extras_definir_chave(mdbKey); temMdb = 0; }
  spMarcar(SP_CRED);
  // A ordem da home entra no MESMO remontar, e so quando MUDOU de verdade.
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
      soFileiras = 1;
    }
    free(catHomeBlob);
    catHomeBlob = NULL;
    temCatHomeBlob = 0;
  }
  spMarcar(SP_ORDEM);
  if (temColBlob && colBlob) {
    unsigned antes = col_revisao();
    colfileiras_receber(colBlob);
    if (antes != col_revisao()) soFileiras = 1;
    free(colBlob); colBlob = NULL; temColBlob = 0;
  }
  spMarcar(SP_COLECOES);
  // Publish account additions, removals and visibility while Settings is open;
  // relying on the next Home draw left the editor on an older snapshot (#233).
  if (atualizarColecoes) colfileiras_sincronizar();
  spMarcar(SP_SINCRONIZAR);
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
    if (contalib_ler_biblioteca(bibBlob) > 0) contalib_aplicar_catalogo();
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
  if (temAjustesBlob && ajustesBlob) {
    ajustes_aplicar_blob(ajustesBlob);
    // O BLOB NAO E LIBERADO AQUI (mudou em #85): ele e a base da costura que
    // sobe no proximo ciclo. Quem o libera e o pull seguinte, que o substitui,
    // e o logout.
    temAjustesBlob = 0;
    aplicarAjustes = 0;   // daqui para frente, o que a pessoa mudar na TV fica
  }
  // #187: uma linha por blob novo (aplicado ou protegido) dizendo qual idioma
  // a TV pede ao TMDB e o que a conta guarda. O ponteiro muda a cada pull.
  { static const char *relatado;
    if (ajustesBlob && ajustesBlob != relatado) {
      relatado = ajustesBlob;
      ajustes_tmdb_idioma_relatar(ajustesBlob);
    } }
  spMarcar(SP_AJUSTES);
  // Progresso da conta: progresso.c decide linha a linha (pendente local vence,
  // senao o mais novo), guarda ate o que nao tem titulo no catalogo ainda, e
  // o catalogo recebe so o que foi aceito.
  // Aceito novo refaz a fileira: sem isto o que o celular assistiu so aparecia
  // em "Continuar assistindo" no proximo ciclo de descoberta (issue #38).
  if (syncprog_aplicar(NULL) > 0) desc_refazer_continuar();
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
  if (!cicloInterrompido) perfilAplicado = perfilDoCiclo;
  // A VOLTA QUE FOI PEDIDA COM O FIO VIVO. So quando ela serve para algo: o
  // ciclo que acabou parou na pergunta de perfil (e a pergunta ja foi
  // respondida) ou puxou para um perfil que nao e mais o ativo. Quando a
  // pessoa responde durante perfis_puxar, o proprio ciclo segue inteiro com o
  // perfil escolhido — repetir ali seria o ciclo de rede duas vezes por nada.
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
// As frases fixas (resumo guarda o portugues) passam pela tabela aqui; as com
// numero ja foram montadas com o formato traduzido.
const char *sync_resumo(void)      { return i18n(resumo); }
unsigned    sync_ultimo_ok(void)   { return ultimoOk; }
void        sync_sujar_progresso(void) { sujoProgresso = 1; }
void        sync_sujar_addons(void) {
  AddonsPendencia *p;
  char nome[220];
  const char *dono = sessao_usuario();
  int perfil = perfis_ativo_addons();
  if (!sessao_logada() || !addonsNome(nome, sizeof nome, dono, perfil)) return;
  // A LISTA DESTA TV ESTA INCOMPLETA (ver addonsDeFora): o push substitui a
  // lista da conta pela daqui, e a daqui nao tem o addon que nao coube. A
  // mudanca vale nesta TV ate o proximo pull; para a conta ela nao sobe.
  if (addonsDeFora > 0) {
    printf("[sync] edicao de addons nao enviada: %d addon(s) da conta nao cabem nesta TV\n",
           addonsDeFora);
    fflush(stdout);
    return;
  }
  pthread_mutex_lock(&addonsTrava);
  p = addonsEdicao(dono, perfil);
  if (!p) p = addonsNova(dono, perfil);
  if (p) {
    p->n = addons_exportar(p->lista, SY_ADD_MAX);
    p->pendente = 1; p->gravada = 0; p->rev = ++addonsRev;
    dados_uuid(p->edicao, sizeof p->edicao);
    addonsGuardar(p);
    addonsLeituraDono[0] = 0; addonsLeituraPerfil = 0;
    // Essa edicao ja e a lista do fio principal: nao reaplicar ao iniciar.
    snprintf(addonsContexto, sizeof addonsContexto, "%s", dono);
    addonsContextoPerfil = perfil;
  }
  pthread_mutex_unlock(&addonsTrava);
}
// Provedor que o servidor recusou com "Unsupported provider credential": o
// servidor de hoje nao guarda trakt/simkl, e a resposta nao muda ate o app
// reiniciar. Perguntar de novo a cada renovacao do token era um 400 no log por
// ciclo, sempre igual. Anota o provedor e para de perguntar nesta sessao; o
// vinculo continua valendo nesta TV, guardado em disco.
#define SY_CRED_RECUSADAS 4
static char credRecusada[SY_CRED_RECUSADAS][16];
static int nCredRecusadas;

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
  int i;
  credLerDisco();
  for (i = 0; i < nCredRecusadas; i++)
    if (!strcmp(credRecusada[i], provider)) return 1;
  return 0;
}

int sync_empurrar_credencial(const char *provider, const char *credJson) {
  Jsw w;
  char *r;
  int st = 0, ok;
  if (!sessao_logada() || !provider || !*provider || !credJson || !*credJson) return 0;
  // -1, como qualquer recusa 4xx: quem chamou encerra a pendencia.
  if (credJaRecusada(provider)) return -1;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfis_ativo());
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
  r = sessao_rpc("sync_push_provider_credentials", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = ok2xx(r, st) ? 1 : (st >= 400 && st < 500 ? -1 : 0);
  if (!ok2xx(r, st)) {
    printf("[sync] push de credencial %s falhou (HTTP %d): %.200s\n", provider, st, r ? r : "");
    if (st == 400 && r && strstr(r, "Unsupported provider") && nCredRecusadas < SY_CRED_RECUSADAS) {
      snprintf(credRecusada[nCredRecusadas++], sizeof credRecusada[0], "%s", provider);
      credGravarDisco();
      printf("[sync] servidor nao aceita credencial %s: nao tento de novo por 7 dias\n", provider);
    }
  } else printf("[sync] credencial %s guardada na conta\n", provider);
  free(r);
  return ok;
}

void sync_reaplicar_ajustes(void) {
  cat_historico_contexto(sessao_usuario(), perfis_ativo());
  // A conta ou o perfil ativo mudou: solta pins dos dois grupos para que cada
  // superfície publique em seguida o conjunto pertencente ao novo contexto.
  cachearte_limpar_referencias();
  aplicarAjustes = 1;
  // Conta manda de novo: a protecao local deixa de valer ate a pessoa mexer.
  dados_apagar(SY_AJUSTES_LOCAIS);
  // E A PENDENCIA DE SUBIDA CAI COM ELA. Esta funcao e chamada ao entrar e ao
  // TROCAR DE PERFIL: uma pendencia do perfil anterior empurrada depois da troca
  // escreveria os valores de um perfil no blob do outro. Quem manda agora e a
  // conta; a pessoa mexer de novo marca de novo.
  sujoAjustes = 0;
  // A base velha e do perfil velho. Solta-la aqui tambem evita que um push do
  // primeiro ciclo costure sobre o blob de outro perfil.
  free(ajustesBlob);
  ajustesBlob = NULL;
  temAjustesBlob = 0;
}

// A pendencia local de um perfil que nao esta ativo. So o perfil ativo usa
// SY_AJUSTES_LOCAIS; os outros guardam a marca deles aqui ate voltarem.
static void nomeLocaisPerfil(char *dst, size_t tam, int perfil) {
  snprintf(dst, tam, "ajustes-locais-p%d.txt", perfil);
}

static int perfilPrincipal(void) {
  int i;
  for (i = 0; i < perfis_n(); i++) {
    const ContaPerfil *p = perfis_item(i);
    if (p && p->primario) return p->indice;
  }
  return 1;
}

void sync_trocar_perfil(int antes) {
  int depois = perfis_ativo();
  char nome[40], *t;
  int pendente;
  if (antes <= 0 || antes == depois) { sync_reaplicar_ajustes(); return; }
  // 1. O QUE SAI. Os valores na memoria ainda sao os dele: a troca acabou de
  //    acontecer e nada foi aplicado desde entao.
  ajustes_perfil_guardar(antes);
  nomeLocaisPerfil(nome, sizeof nome, antes);
  t = dados_ler(SY_AJUSTES_LOCAIS);
  pendente = (t && t[0] == '1') || sujoAjustes;
  free(t);
  if (pendente) dados_gravar(nome, "1\n");
  else dados_apagar(nome);
  // 2. O QUE ENTRA. Primeira visita nesta TV: parte do perfil principal. Quando
  //    o principal e quem acabou de sair, os valores na memoria ja sao os dele;
  //    quando ele nunca foi usado aqui, nao ha de onde partir e fica o que esta.
  if (!ajustes_perfil_restaurar(depois)) {
    int principal = perfilPrincipal();
    if (principal != depois && principal != antes) ajustes_perfil_restaurar(principal);
    ajustes_auto_creditos_restaurar(depois);
  }
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
    dados_gravar(SY_AJUSTES_LOCAIS, "1\n");
    sujoAjustes = 1;
    printf("[sync] perfil %d tinha ajustes desta TV sem subir: mantidos\n", depois);
    fflush(stdout);
  }
  free(t);
  dados_apagar(nome);
  perfilAplicado = 0;
}

int sync_perfil_pronto(void) {
  return !fioVivo && perfilAplicado == perfis_ativo();
}

void sync_proteger_ajustes_locais(void) {
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
  recomenda_esquecer();
  // E A FONTE LEMBRADA DE CADA TITULO. Ela nao chega a ser um segredo, mas diz
  // o que a pessoa assistiu e em que idioma — e, como a lista de salvos, ela
  // nao depende de conta nenhuma para existir, entao sem esta linha ela
  // sobreviveria ao logout em disco e passaria a mandar na reproducao da
  // proxima pessoa.
  fontepref_esquecer();
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
  stalker_esquecer();
  xtream_esquecer();   // mesma razao: usuario e senha sao a assinatura de quem saiu
  servidores_esquecer_todos();   // personal-server tokens of every profile on this TV
  trakt_esquecer();
  // Os ajustes por perfil guardados nesta TV (e as pendencias deles) sao da
  // conta que saiu: a proxima conta nao parte deles.
  ajustes_perfil_esquecer();
  { char nome[40]; int i;
    for (i = 1; i <= 32; i++) { nomeLocaisPerfil(nome, sizeof nome, i); dados_apagar(nome); } }
  perfilAplicado = 0;
  perfis_esquecer();
  prog_esquecer_tudo();
  perfilcont_esquecer();
  psparede_esquecer();   // a parede de cada perfil (fundo "Filmes") e da conta que saiu
  syncprog_esquecer();

  // As caixas que o fio preenche tambem: um ciclo que terminou logo antes do
  // logout aplicaria os addons da conta anterior no proximo sync_passo.
  memset(addonsRem, 0, sizeof addonsRem);
  nAddonsRem = 0; temAddonsRem = 0; addonsCedo = 0;
  traktTok[0] = 0; temTraktRem = 0;
  memset(tmdbKey, 0, sizeof tmdbKey); temTmdb = 0;
  memset(mdbKey, 0, sizeof mdbKey);   temMdb = 0;
  cVistos = cBiblio = cColecoes = 0;
  temAjustesPerfil = temCatHome = 0;
  estado = SYNC_PARADO;
  ultimoOk = 0;
  sujoProgresso = 0; sujoAjustes = 0;
  addonsEsquecer();
  free(ajustesBlob);
  ajustesBlob = NULL;
  temAjustesBlob = 0;
  aplicarAjustes = 1;
  dados_apagar(SY_AJUSTES_LOCAIS);
  snprintf(resumo, sizeof resumo, "sem conta");
  printf("[sync] dados do usuario apagados deste aparelho\n");
}

void        sync_encerrar(void)    { }
