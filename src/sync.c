#include "sync.h"
#include "sessao.h"
#include "nuvem.h"
#include "perfis.h"
#include "dados.h"
#include "addons.h"
#include "debrid.h"
#include "colecoes.h"
#include "trakt.h"
#include "traktauth.h"
#include "catalogo.h"
#include "progresso.h"
#include "syncprog.h"
#include "ajustes.h"
#include "catordem.h"
#include "descoberta.h"
#include "extras.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define SY_ADD_MAX   16

static pthread_t fio;
static int fioVivo, fioPronto;
static SyncEstado estado = SYNC_PARADO;
static char resumo[220] = "sem sincronizar";
static unsigned ultimoOk;
static int sujoProgresso, sujoAddons;

// O fio NAO toca no app: ele so preenche estas caixas, e sync_passo aplica no
// laco principal. Sem essa separacao, uma resposta de rede reescreveria a lista
// de addons no meio de um quadro que ja estava lendo dela.
static AddonRemoto addonsRem[SY_ADD_MAX];
static int nAddonsRem, temAddonsRem;
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


// Contagens do que foi puxado mas o app ainda nao consome. Elas existem para o
// resumo poder dizer a verdade em vez de "sincronizado" sem qualificar.
static int cVistos, cBiblio, cSalvos, cColecoes, temAjustesPerfil, temCatHome;

// Blob de ajustes do perfil, cru, esperando ser aplicado no fio principal.
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

// A ordem das fileiras da home, crua, tambem esperando o fio principal. Nao e
// contada como as outras so-leitura: ela e a home da pessoa, e ate agora a
// resposta chegava, virava um numero no resumo e era jogada fora.
static char *catHomeBlob;
static int   temCatHomeBlob;
static char *colBlob;        // sync_pull_collections, lido por colecoes.c no fio principal
static int   temColBlob;

// ---------------------------------------------------------------- utilitarios

static int ok2xx(const char *r, int st) { return r && st >= 200 && st < 300; }

// ---------------------------------------------------------------- addons

static void puxarAddons(void) {
  char consulta[400], dono[80];
  char *r;
  int st = 0, k = 0;
  const char *p;

  if (!perfis_dono()[0]) return;
  nuvem_url_escapar(perfis_dono(), dono, sizeof dono);
  // MEDIDO: `sync_pull_addons` NAO EXISTE neste servidor (PGRST202), e a
  // tabela `tv_addons` tambem nao (PGRST205). O unico caminho que responde e a
  // tabela `addons`, que e exatamente o caminho feliz do app web.
  // perfis_ativo_addons(), nao perfis_ativo(): um perfil marcado com
  // uses_primary_plugins LE os addons do perfil 1. Pedindo pelo indice dele a
  // resposta vinha vazia e o perfil abria sem addon nenhum.
  snprintf(consulta, sizeof consulta,
           "user_id=eq.%s&profile_id=eq.%d&select=*&order=sort_order.asc",
           dono, perfis_ativo_addons());
  // Com a chave anonima o RLS responde 401 "permission denied for table
  // addons": ler as linhas de alguem exige o token de quem esta pedindo.
  r = sessao_tabela("addons", consulta, &st);
  if (!ok2xx(r, st)) {
    if (r && nuvem_erro_ausente(r)) printf("[sync] tabela addons ausente\n");
    else if (st) printf("[sync] leitura de addons: HTTP %d\n", st);
    free(r);
    return;
  }
  for (p = js_raiz_array(r); p && k < SY_ADD_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char b[16];
    memset(&addonsRem[k], 0, sizeof addonsRem[k]);
    if (!js_texto(p, f, "url", addonsRem[k].url, sizeof addonsRem[k].url)) continue;
    js_texto(p, f, "name", addonsRem[k].nome, sizeof addonsRem[k].nome);
    // Ausente conta como LIGADO: e assim que o web le, e um addon que some por
    // causa de um campo que o servidor nao mandou e pior que um a mais.
    addonsRem[k].ativo = js_bruto(p, f, "enabled", b, sizeof b)
                         ? (strcmp(b, "false") != 0) : 1;
    k++;
  }
  free(r);
  nAddonsRem = k;
  temAddonsRem = 1;
}

static void empurrarAddons(void) {
  AddonRemoto atuais[SY_ADD_MAX];
  Jsw w;
  char *r;
  int st = 0, n, i;

  n = addons_exportar(atuais, SY_ADD_MAX);
  // Lista local vazia NAO vira push. Um push vazio apaga os addons da pessoa em
  // todos os aparelhos dela, e "ainda nao carreguei nada" e indistinguivel de
  // "o usuario removeu tudo" deste lado.
  if (n <= 0) return;

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  // O MESMO perfil da leitura. Ler do perfil 1 e escrever no indice do perfil
  // atual criaria uma copia divergente a cada sync; escrever no 1 sem ler dele
  // sobrescreveria os addons de quem compartilha.
  jsw_ci(&w, "p_profile_id", perfis_ativo_addons());
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
  if (!ok2xx(r, st)) printf("[sync] push de addons falhou (HTTP %d)\n", st);
  else sujoAddons = 0;
  free(r);
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

// Conta os itens de uma RPC que devolve array. Estas superficies sao puxadas
// mas ainda nao consumidas: o app nativo nao tem tela propria para elas, e
// EMPURRAR sem ter a tela mandaria lista vazia — que apaga o dado nos outros
// aparelhos da pessoa. Contar e dizer no resumo e o comportamento honesto ate
// a tela existir.
// RPC que o servidor nao tem NAO e perguntada de novo. MEDIDO:
// `sync_pull_saved_library` nao existe neste servidor, e sem esta lista o app
// gastaria uma viagem por ciclo, para sempre, contra um 404 que nunca muda.
#define SY_AUSENTES 8
static const char *ausentes[SY_AUSENTES];
static int nAusentes;

static int jaAusente(const char *funcao) {
  int i;
  for (i = 0; i < nAusentes; i++)
    if (!strcmp(ausentes[i], funcao)) return 1;
  return 0;
}

static int contarRpc(const char *funcao, const char *corpo) {
  char *r;
  int st = 0, k = 0;
  const char *p;
  if (jaAusente(funcao)) return -1;
  r = sessao_rpc(funcao, corpo, &st);
  if (!ok2xx(r, st)) {
    if (r && nuvem_erro_ausente(r)) {
      printf("[sync] %s nao existe neste servidor\n", funcao);
      if (nAusentes < SY_AUSENTES) ausentes[nAusentes++] = funcao;
    }
    free(r);
    return -1;
  }
  for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) k++;
  free(r);
  return k;
}

// O blob de ajustes NAO e contado, e lido: ele e o layout da pessoa. Ate agora
// esta RPC so alimentava um numero no resumo, e as ~40 preferencias vinham dos
// padroes transcritos a mao do perfil de quem montou o pacote.
static int puxarAjustesPerfil(const char *corpo) {
  char *r;
  int st = 0, ok = 0;
  const char *p;
  if (!aplicarAjustes) return temAjustesPerfil;   // nada a fazer nesta volta
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
            temAjustesBlob = 1;
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
  cBiblio   = contarRpc("sync_pull_library", corpo);
  cColecoes = contarRpc("sync_pull_collections", corpo);
  if (cColecoes > 0 && !jaAusente("sync_pull_collections")) {
    int st = 0; char *r = sessao_rpc("sync_pull_collections", corpo, &st);
    if (ok2xx(r, st)) { free(colBlob); colBlob = r; temColBlob = 1; } else free(r);
  }

  // MEDIDO: `p_page` comeca em 1. Com 0 o servidor responde 400 "OFFSET must
  // not be negative" — a conta dele e (p_page - 1) * p_page_size.
  snprintf(corpo, sizeof corpo,
           "{\"p_profile_id\":%d,\"p_page\":1,\"p_page_size\":200}", perfil);
  cVistos = contarRpc("sync_pull_watched_items", corpo);

  snprintf(corpo, sizeof corpo,
           "{\"p_profile_id\":%d,\"p_limit\":200,\"p_offset\":0}", perfil);
  cSalvos = contarRpc("sync_pull_saved_library", corpo);

  snprintf(corpo, sizeof corpo,
           "{\"p_profile_id\":%d,\"p_platform\":\"tv\"}", perfil);
  temAjustesPerfil = puxarAjustesPerfil(corpo);

  snprintf(corpo, sizeof corpo,
           "{\"p_profile_id\":%d,\"p_platform\":\"home_catalog_shared\"}", perfil);
  temCatHome = puxarCatHome(corpo);
}

// ---------------------------------------------------------------- ciclo

static void *rodar(void *u) {
  (void)u;
  perfis_puxar();
  puxarAddons();
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
  if (temAddonsRem && !sujoAddons) addonsCedo = 1;
  puxarCredenciais();
  syncprog_puxar();
  puxarSoLeitura();
  // Empurrar DEPOIS de puxar, como o startupSyncService do web: puxar depois
  // de empurrar faria o aparelho sobrescrever com o que ele mesmo mandou.
  // O puxado NAO e aplicado aqui, e sim em sync_passo, no fio principal — e
  // la a regra e "pendente local vence": o que se assistiu entre o pull e o
  // push nao volta atras.
  if (sujoAddons) empurrarAddons();
  // Sempre, nao so quando `sujoProgresso`: linhas migradas do formato antigo
  // nascem pendentes sem ninguem ter marcado nada.
  if (syncprog_empurrar() >= 0) sujoProgresso = 0;

  snprintf(resumo, sizeof resumo,
           "%d addons · %d progressos · %d vistos · %d na lista · %d coleções%s",
           nAddonsRem, syncprog_puxadas(), cVistos < 0 ? 0 : cVistos,
           cBiblio < 0 ? 0 : cBiblio, cColecoes < 0 ? 0 : cColecoes,
           temTraktRem ? " · Trakt" : "");
  estado = SYNC_PRONTO;
  fioPronto = 1;
  return NULL;
}

void sync_iniciar(void) {
  if (fioVivo || !sessao_logada()) return;
  if (nuvem_freio_ativo()) return;
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
  if (agoraMs - ultimoOk < SYNC_INTERVALO_MS) return 0;
  sync_iniciar();
  return 1;
}

void sync_passo(unsigned agoraMs) {
  // ANTES DA PORTEIRA de `fioPronto`: ver o comentario em rodar(). O resto do
  // ciclo continua sendo aplicado de uma vez, no fim — so os addons saem na
  // frente, porque so eles mudam O QUE a descoberta vai buscar.
  if (addonsCedo) {
    addonsCedo = 0;
    if (temAddonsRem) {
      addons_definir_lista(addonsRem, nAddonsRem);
      temAddonsRem = 0;
      desc_repetir();
    }
  }
  if (!fioVivo || !fioPronto) return;
  fioVivo = 0;
  fioPronto = 0;

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
  { int remontar = 0, soFileiras = 0;
  if (temAddonsRem) { addons_definir_lista(addonsRem, nAddonsRem); temAddonsRem = 0; remontar = 1; }
  // Vinculo feito NESTA TV ganha do que a conta manda: o servidor nao aceita o
  // push de "trakt" (400 22023), entao a linha da conta pode ser um token
  // antigo e vencido — aplica-lo por cima do novo devolvia 401 em tudo logo
  // depois de a pessoa ter acabado de autorizar.
  if (temTraktRem)  { if (traktauth_estado() != TRA_LIGADO) { trakt_definir(traktTok, nuvem_trakt_cliente()); remontar = 1; }
                      else printf("[sync] trakt: vinculo local mantido, credencial da conta ignorada\n");
                      temTraktRem = 0; }
  if (temTmdb)      { desc_tmdb_definir(tmdbKey);   temTmdb = 0; }
  if (temMdb)       { extras_definir_chave(mdbKey); temMdb = 0; }
  // A ordem da home entra no MESMO remontar, e so quando MUDOU de verdade.
  // Uma remontagem por ciclo de sync custaria a home inteira a cada 5 minutos,
  // e o baseline de jank desta TV nao tem essa folga; duas remontagens no mesmo
  // quadro (addons e ordem) custariam o dobro por nada.
  if (temCatHomeBlob && catHomeBlob) {
    if (catordem_ler(catHomeBlob)) {
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
  if (temColBlob && colBlob) {
    if (col_definir_json(colBlob) > 0) soFileiras = 1;
    free(colBlob); colBlob = NULL; temColBlob = 0;
  }
  // Rede so quando muda o que buscar. Quando as duas coisas mudam no mesmo
  // ciclo, o ciclo de rede ja remonta as fileiras no fim — nao ha o que somar.
  if (remontar) desc_repetir();
  else if (soFileiras) desc_remontar_fileiras(); }
  if (temAjustesBlob && ajustesBlob) {
    ajustes_aplicar_blob(ajustesBlob);
    free(ajustesBlob);
    ajustesBlob = NULL;
    temAjustesBlob = 0;
    aplicarAjustes = 0;   // daqui para frente, o que a pessoa mudar na TV fica
  }
  // Progresso da conta: progresso.c decide linha a linha (pendente local vence,
  // senao o mais novo), guarda ate o que nao tem titulo no catalogo ainda, e
  // o catalogo recebe so o que foi aceito.
  syncprog_aplicar(NULL);
  if (estado == SYNC_PRONTO) ultimoOk = agoraMs;
}

SyncEstado  sync_estado(void)      { return estado; }
const char *sync_resumo(void)      { return resumo; }
unsigned    sync_ultimo_ok(void)   { return ultimoOk; }
void        sync_sujar_progresso(void) { sujoProgresso = 1; }
void        sync_sujar_addons(void)    { sujoAddons = 1; }
int sync_empurrar_credencial(const char *provider, const char *credJson) {
  Jsw w;
  char *r;
  int st = 0, ok;
  if (!sessao_logada() || !provider || !*provider || !credJson || !*credJson) return 0;
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
  if (!ok2xx(r, st)) printf("[sync] push de credencial %s falhou (HTTP %d): %.200s\n", provider, st, r ? r : "");
  else printf("[sync] credencial %s guardada na conta\n", provider);
  free(r);
  return ok;
}

void sync_reaplicar_ajustes(void) { aplicarAjustes = 1; }

void sync_esquecer_usuario(void) {
  // A ordem importa pouco, mas o CONJUNTO nao: cada linha aqui corresponde a
  // uma coisa que sobrevivia ao logout.
  catordem_esquecer();
  free(catHomeBlob);
  catHomeBlob = NULL;
  temCatHomeBlob = 0;
  addons_esquecer();
  debrid_esquecer();
  trakt_esquecer();
  perfis_esquecer();
  prog_esquecer_tudo();
  syncprog_esquecer();

  // As caixas que o fio preenche tambem: um ciclo que terminou logo antes do
  // logout aplicaria os addons da conta anterior no proximo sync_passo.
  memset(addonsRem, 0, sizeof addonsRem);
  nAddonsRem = 0; temAddonsRem = 0; addonsCedo = 0;
  traktTok[0] = 0; temTraktRem = 0;
  memset(tmdbKey, 0, sizeof tmdbKey); temTmdb = 0;
  memset(mdbKey, 0, sizeof mdbKey);   temMdb = 0;
  cVistos = cBiblio = cSalvos = cColecoes = 0;
  temAjustesPerfil = temCatHome = 0;
  estado = SYNC_PARADO;
  ultimoOk = 0;
  sujoProgresso = 0; sujoAddons = 0;
  free(ajustesBlob);
  ajustesBlob = NULL;
  temAjustesBlob = 0;
  aplicarAjustes = 1;
  snprintf(resumo, sizeof resumo, "sem conta");
  printf("[sync] dados do usuario apagados deste aparelho\n");
}

void        sync_encerrar(void)    { }
