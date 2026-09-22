#include "stalker.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "perfis.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define ST_ARQ_FMT     "stalker-p%d.txt"
#define ST_MAX_CANAL   1200
#define ST_PAGINA_MAX  64      // teto de paginas; portais grandes param antes
#define ST_PRAZO_S     15
// Piso de renovacao do token. O 401 e o sinal de verdade e e ele que manda;
// isto so cobre o caso em que o portal aceita o token velho para a LISTA e o
// recusa para o create_link — visto em portais que separam as duas validacoes.
// Renovar antes de tocar custa uma requisicao e evita o zap morrer na mao.
#define ST_TOKEN_VALIDO_S (55 * 60)

typedef struct { char id[80], cmd[192]; } StCmd;

static char portal[256];      // base ja normalizada, sem barra no fim
static char mac[32];
static char deviceId[80];
static char serial[64];
static char token[128];
static char caminho[64];      // endpoint que respondeu ("/server/load.php"...)
static time_t tokenEm;
static int   perfilLido = -1;
static int   lido;

static StCmd tabela[ST_MAX_CANAL];
static int   nTabela;

// UMA trava para a sessao (token/caminho) e outra para a tabela de cmd. Sao
// duas porque os donos sao diferentes: o fio do guia escreve a tabela enquanto
// o fio da reproducao pode estar renovando o token. Uma trava so faria o
// create_link esperar o fim de uma listagem de 1200 canais.
static pthread_mutex_t travaSessao = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t travaTabela = PTHREAD_MUTEX_INITIALIZER;

// ---------------------------------------------------------------- utilidades

// Percent-encode do que vai em query. O `cmd` do portal tem espacos e ":" e
// "/" — sem isto o create_link recebe um comando cortado no primeiro espaco e
// devolve link de outro canal, ou nenhum.
static void urlenc(const char *s, char *dst, unsigned tam) {
  static const char *HEX = "0123456789ABCDEF";
  unsigned k = 0;
  if (!tam) return;
  for (; s && *s && k + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      dst[k++] = (char)c;
    } else {
      dst[k++] = '%'; dst[k++] = HEX[c >> 4]; dst[k++] = HEX[c & 15];
    }
  }
  dst[k] = 0;
}

// Tira esquema, barra final e qualquer caminho que a pessoa tenha colado junto
// ("http://portal.tv:8080/c/" e "portal.tv:8080" tem de virar a mesma coisa).
static void normalizarPortal(const char *entrada, char *dst, unsigned tam) {
  const char *p = entrada ? entrada : "";
  const char *barra;
  char host[256];
  unsigned n;
  if (!strncmp(p, "http://", 7)) p += 7;
  else if (!strncmp(p, "https://", 8)) p += 8;
  barra = strchr(p, '/');
  n = barra ? (unsigned)(barra - p) : (unsigned)strlen(p);
  if (n >= sizeof host) n = sizeof host - 1;
  memcpy(host, p, n); host[n] = 0;
  while (n > 0 && (host[n - 1] == '/' || host[n - 1] == ' ')) host[--n] = 0;
  // O esquema volta como http: portal Stalker com TLS e raro, e quando existe
  // o host costuma vir com a porta 443 explicita — caso em que quem configurou
  // digitou https e a normalizacao acima o removeu. Preferir http aqui e o que
  // funciona nos portais reais; um https quebrado apareceria como "sem
  // resposta", que e o mesmo diagnostico de portal errado.
  snprintf(dst, tam, "http://%s", host);
}

// ----------------------------------------------------------------- cadastro

static const char *arquivo(void) {
  static char nome[64];
  snprintf(nome, sizeof nome, ST_ARQ_FMT, perfis_ativo());
  return nome;
}

static void limparSessao(void) {
  token[0] = 0; caminho[0] = 0; tokenEm = 0;
}

// Corpo da leitura, SEM travar: quem chama ja segura travaSessao. Existe
// separado porque os setters precisam ler-e-escrever numa seccao critica so.
static void carregarTravado(void) {
  int p = perfis_ativo();
  char *b, *linha, *fim;
  if (lido && p == perfilLido) return;
  perfilLido = p; lido = 1;
  portal[0] = mac[0] = deviceId[0] = serial[0] = 0;
  limparSessao();
  pthread_mutex_lock(&travaTabela); nTabela = 0; pthread_mutex_unlock(&travaTabela);
  b = dados_ler(arquivo());
  if (!b) return;
  for (linha = b; *linha; linha = fim) {
    char *sep;
    fim = strchr(linha, '\n');
    if (fim) *fim++ = 0; else fim = linha + strlen(linha);
    if (linha[0] == '#' || !linha[0]) continue;
    sep = strchr(linha, '\t');
    if (!sep) continue;
    *sep++ = 0;
    if (!strcmp(linha, "portal")) snprintf(portal, sizeof portal, "%s", sep);
    else if (!strcmp(linha, "mac")) snprintf(mac, sizeof mac, "%s", sep);
    else if (!strcmp(linha, "device")) snprintf(deviceId, sizeof deviceId, "%s", sep);
    else if (!strcmp(linha, "serial")) snprintf(serial, sizeof serial, "%s", sep);
  }
  free(b);
}

// O CADASTRO E LIDO POR DOIS FIOS: o do guia (que lista canais) e o de desenho
// (que mostra o portal mascarado em Ajustes). Sem a trava, uma troca de perfil
// zeraria `portal` no meio de um `chamar` que acabou de copia-lo.
void stalker_carregar(void) {
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  pthread_mutex_unlock(&travaSessao);
}

static void gravar(void) {
  char txt[720];
  // O COMENTARIO VAI NO ARQUIVO, como em listas.c: quem o encontrar primeiro
  // sera alguem depurando a TV de outra pessoa — e precisa saber na hora que
  // aquilo nao e configuracao, e credencial.
  snprintf(txt, sizeof txt,
           "# CREDENCIAL. O MAC autentica a assinatura de quem configurou este\n"
           "# portal, como uma senha. Nao versionar, nao empacotar, nao colar\n"
           "# em issue nem em log.\n"
           "portal\t%s\nmac\t%s\ndevice\t%s\nserial\t%s\n",
           portal, mac, deviceId, serial);
  dados_gravar(arquivo(), txt);
}

// DOIS CAMPOS, DOIS SETTERS, e nao um `definir` com tudo. A tela de Ajustes
// edita um de cada vez, e um setter unico a obrigaria a ler o MAC atual para
// reescreve-lo ao trocar so o endereco — ou seja, a devolver a credencial em
// claro para a camada de desenho so para poder grava-la de volta.
void stalker_definir_portal(const char *p) {
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  normalizarPortal(p, portal, sizeof portal);
  limparSessao();
  gravar();
  pthread_mutex_unlock(&travaSessao);
  pthread_mutex_lock(&travaTabela); nTabela = 0; pthread_mutex_unlock(&travaTabela);
}

void stalker_definir_mac(const char *m) {
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  snprintf(mac, sizeof mac, "%s", m ? m : "");
  limparSessao();
  gravar();
  pthread_mutex_unlock(&travaSessao);
  pthread_mutex_lock(&travaTabela); nTabela = 0; pthread_mutex_unlock(&travaTabela);
}

void stalker_definir_aparelho(const char *d, const char *ser) {
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  snprintf(deviceId, sizeof deviceId, "%s", d ? d : "");
  snprintf(serial, sizeof serial, "%s", ser ? ser : "");
  limparSessao();
  gravar();
  pthread_mutex_unlock(&travaSessao);
}

void stalker_esquecer(void) {
  pthread_mutex_lock(&travaSessao);
  dados_apagar(arquivo());
  portal[0] = mac[0] = deviceId[0] = serial[0] = 0;
  limparSessao();
  lido = 1; perfilLido = perfis_ativo();
  pthread_mutex_unlock(&travaSessao);
  pthread_mutex_lock(&travaTabela); nTabela = 0; pthread_mutex_unlock(&travaTabela);
}

int stalker_configurado(void) {
  int ok;
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  ok = portal[0] && mac[0];
  pthread_mutex_unlock(&travaSessao);
  return ok;
}

const char *stalker_portal_curto(void) {
  static char curto[128];
  const char *p;
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  p = portal;
  if (!strncmp(p, "http://", 7)) p += 7;
  snprintf(curto, sizeof curto, "%s", p[0] ? p : "-");
  pthread_mutex_unlock(&travaSessao);
  return curto;
}

const char *stalker_mac_mascarado(void) {
  static char m[24];
  size_t n;
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  n = strlen(mac);
  // Os dois ultimos octetos bastam para a pessoa reconhecer QUAL cadastro esta
  // ali sem que a tela (ou uma foto dela numa issue) entregue o valor inteiro.
  if (n < 5) snprintf(m, sizeof m, "%s", mac[0] ? "··" : "-");
  else       snprintf(m, sizeof m, "··:··:··:··:%s", mac + n - 5);
  pthread_mutex_unlock(&travaSessao);
  return m;
}

// ------------------------------------------------------------------ sessao

// Monta os cabecalhos de STB. Mantidos juntos porque so fazem sentido juntos:
// portal que confere um deles costuma conferir os quatro.
static void cabecalhos(const char *portalLocal, const char *macLocal,
                       const char *cab[7], const char *tok,
                       char *linhaCookie, unsigned tamCookie,
                       char *linhaAuth, unsigned tamAuth, char *linhaRef,
                       unsigned tamRef) {
  int k = 0;
  snprintf(linhaCookie, tamCookie,
           "Cookie: mac=%s; stb_lang=en; timezone=Europe/Kiev", macLocal);
  snprintf(linhaRef, tamRef, "Referer: %s/c/", portalLocal);
  cab[k++] = linhaCookie;
  cab[k++] = linhaRef;
  cab[k++] = "User-Agent: Mozilla/5.0 (QtEmbedded; U; Linux; C) "
             "AppleWebKit/533.3 (KHTML, like Gecko) MAG200 stbapp ver: 2 "
             "rev: 250 Safari/533.3";
  cab[k++] = "X-User-Agent: Model: MAG250; Link: WiFi";
  cab[k++] = "Accept: */*";
  if (tok && tok[0]) {
    snprintf(linhaAuth, tamAuth, "Authorization: Bearer %s", tok);
    cab[k++] = linhaAuth;
  }
  cab[k] = NULL;
}

// Uma requisicao crua, sem re-handshake. Recebe rota e token POR COPIA de
// proposito: o HTTP dura segundos e roda FORA da trava, entao ler `token` ou
// `caminho` globais aqui seria le-los enquanto outro fio os reescreve.
static char *pedir(const char *portalLocal, const char *macLocal,
                   const char *rota, const char *tok, const char *consulta) {
  const char *cab[7];
  char url[2048], c1[128], c2[192], c3[320];
  cabecalhos(portalLocal, macLocal, cab, tok, c1, sizeof c1, c2, sizeof c2, c3, sizeof c3);
  snprintf(url, sizeof url, "%s%s?%s&JsHttpRequest=1-xml", portalLocal, rota, consulta);
  return rede_baixar_com(url, ST_PRAZO_S, cab);
}

// Resposta util e a que traz o objeto `js`. Portal que recusou o token devolve
// corpo vazio, HTML de login, ou um `js` falso — e nenhum deles tem o campo
// pedido. Tratar "sem js" como "token morreu" e o que faz a renovacao disparar
// no caso real, em que o 401 nem sempre vem com codigo.
static int temJs(const char *corpo) {
  return corpo && strstr(corpo, "\"js\"") != NULL;
}

// Caminhos conhecidos de portal. A ordem e a frequencia com que aparecem.
static const char *ROTAS[] = {
  "/server/load.php", "/portal.php", "/stalker_portal/server/load.php", NULL
};

// Refaz o handshake e guarda o token. Chamada SEMPRE com travaSessao presa.
static int handshakeTravado(void) {
  int i;
  token[0] = 0;
  for (i = 0; ROTAS[i]; i++) {
    char *corpo;
    // Rota que ja provou funcionar entra primeiro nas vezes seguintes; na
    // primeira, `caminho` esta vazio e a varredura e a ordem acima.
    const char *rota = caminho[0] ? caminho : ROTAS[i];
    corpo = pedir(portal, mac, rota, "", "type=stb&action=handshake&token=&prehash=0");
    if (temJs(corpo)) {
      char t[128];
      if (js_texto_raiz(corpo, "token", t, sizeof t) && t[0]) {
        snprintf(token, sizeof token, "%s", t);
        snprintf(caminho, sizeof caminho, "%s", rota);
        tokenEm = time(NULL);
        free(corpo);
        // get_profile confirma o MAC. Portal que recusa a assinatura responde
        // aqui, e nao no handshake — sem esta chamada o app so descobriria no
        // create_link, com a pessoa ja olhando a tela de carregar.
        corpo = pedir(portal, mac, caminho, token, "type=stb&action=get_profile&hd=1&num_banks=2"
                                           "&stb_type=MAG250&image_version=218&auth_second_step=0");
        if (corpo) free(corpo);
        return 1;
      }
    }
    if (corpo) free(corpo);
    // A rota lembrada falhou: esquecer e varrer de novo a partir do inicio.
    if (caminho[0]) { caminho[0] = 0; i = -1; }
  }
  return 0;
}

// A UNICA porta de saida para o portal. Tudo passa por aqui, e e por isso que
// a renovacao de token existe num lugar so.
//
// O mutex nao e zelo: o fio do guia (lista) e o fio da reproducao
// (create_link) chamam concorrente. Dois handshakes em paralelo geram dois
// tokens e o segundo invalida o primeiro — o sintoma seria uma lista que
// carrega e um canal que nao abre, alternando, sem erro nenhum no log.
static char *chamar(const char *consulta, int renovarAntes) {
  char tok[128], rota[64];
  char portalLocal[256], macLocal[32];
  char *corpo;

  // A TRAVA COBRE O TOKEN, NAO A REQUISICAO. Esta foi a correcao mais
  // importante deste modulo: com o `pedir` dentro da trava, um create_link
  // disparado pelo zap ficava esperando a LISTAGEM INTEIRA do guia terminar —
  // ate 64 paginas de 15 s — e a tela congelava com "carregando". O que precisa
  // ser serializado e a renovacao do token, que dura milissegundos, e nao o
  // HTTP, que dura segundos.
  pthread_mutex_lock(&travaSessao);
  if (!portal[0] || !mac[0]) { pthread_mutex_unlock(&travaSessao); return NULL; }
  // O pedido conserva o cadastro que encontrou sob a trava. Nao usa copias
  // globais: o guia pode iniciar outra requisicao enquanto este HTTP espera.
  snprintf(portalLocal, sizeof portalLocal, "%s", portal);
  snprintf(macLocal, sizeof macLocal, "%s", mac);
  if (!token[0] ||
      (renovarAntes && tokenEm && time(NULL) - tokenEm > ST_TOKEN_VALIDO_S)) {
    if (!handshakeTravado()) { pthread_mutex_unlock(&travaSessao); return NULL; }
  }
  snprintf(tok, sizeof tok, "%s", token);
  snprintf(rota, sizeof rota, "%s", caminho);
  pthread_mutex_unlock(&travaSessao);

  corpo = pedir(portalLocal, macLocal, rota, tok, consulta);
  if (temJs(corpo)) return corpo;
  if (corpo) free(corpo);

  // UMA renovacao e UMA repeticao. Mais que isso vira laco contra um portal que
  // esta simplesmente fora do ar, e cada volta custa dois HTTP.
  //
  // Dois fios que chegam aqui juntos renovam em fila, nao em paralelo: o
  // segundo encontra um token JA novo e nao refaz o handshake — e por isso a
  // comparacao com `tok`, o token que esta requisicao usou. Sem ela, dois
  // handshakes seguidos gerariam dois tokens e o segundo invalidaria o
  // primeiro.
  pthread_mutex_lock(&travaSessao);
  if (!strcmp(tok, token) && !handshakeTravado()) {
    pthread_mutex_unlock(&travaSessao);
    return NULL;
  }
  snprintf(tok, sizeof tok, "%s", token);
  snprintf(rota, sizeof rota, "%s", caminho);
  pthread_mutex_unlock(&travaSessao);

  corpo = pedir(portalLocal, macLocal, rota, tok, consulta);
  if (temJs(corpo)) return corpo;
  if (corpo) free(corpo);
  return NULL;
}

// ------------------------------------------------------------------ canais

static void tabelaPor(const char *id, const char *cmd) {
  int i;
  pthread_mutex_lock(&travaTabela);
  for (i = 0; i < nTabela; i++)
    if (!strcmp(tabela[i].id, id)) break;
  if (i == nTabela && nTabela < ST_MAX_CANAL) nTabela++;
  if (i < ST_MAX_CANAL) {
    snprintf(tabela[i].id, sizeof tabela[i].id, "%s", id);
    snprintf(tabela[i].cmd, sizeof tabela[i].cmd, "%s", cmd);
  }
  pthread_mutex_unlock(&travaTabela);
}

#define ST_MAX_GEN 64
typedef struct { char id[24], titulo[64]; } StGen;

static int lerGeneros(StGen *gen, int max) {
  char *corpo = chamar("type=itv&action=get_genres", 0);
  const char *p;
  int n = 0;
  if (!corpo) return 0;
  p = strstr(corpo, "\"js\"");
  p = p ? strchr(p, '[') : NULL;
  p = p ? p + 1 : NULL;
  while (p && *p && n < max) {
    const char *fim = js_fim(p);
    if (!fim) break;
    if (js_texto(p, fim, "id", gen[n].id, sizeof gen[n].id) &&
        js_texto(p, fim, "title", gen[n].titulo, sizeof gen[n].titulo))
      n++;
    p = js_prox(fim);
  }
  free(corpo);
  return n;
}

// Nome da categoria a partir do tv_genre_id. Generos e uma lista curta (dezenas)
// e a busca linear custa menos que manter indice.
static const char *tituloDoGenero(const StGen *gen, int nGen, const char *id) {
  int i;
  for (i = 0; i < nGen; i++) if (!strcmp(gen[i].id, id)) return gen[i].titulo;
  return "Outros";
}

int stalker_canais(StalkerCanal *saida, int max) {
  StGen gen[ST_MAX_GEN];
  int nGen, pagina, n = 0;
  if (!stalker_configurado() || !saida || max < 1) return 0;
  nGen = lerGeneros(gen, ST_MAX_GEN);

  // genre=* traz TODOS os canais numa varredura paginada, com o genero de cada
  // um no proprio item. Pedir genero a genero seria uma requisicao por
  // categoria — em portal com 40 generos, 40 idas so para montar a mesma lista.
  for (pagina = 1; pagina <= ST_PAGINA_MAX && n < max; pagina++) {
    char consulta[128];
    char *corpo;
    const char *p;
    int noCiclo = 0;
    snprintf(consulta, sizeof consulta,
             "type=itv&action=get_ordered_list&genre=*&force_ch_link_check="
             "&fav=0&sortby=number&hd=0&p=%d", pagina);
    corpo = chamar(consulta, 0);
    if (!corpo) break;
    p = strstr(corpo, "\"data\"");
    p = p ? strchr(p, '[') : NULL;
    p = p ? p + 1 : NULL;
    while (p && *p && n < max) {
      const char *fim = js_fim(p);
      char id[48], cmd[192], genId[24];
      StalkerCanal c;
      if (!fim) break;
      memset(&c, 0, sizeof c);
      if (js_texto(p, fim, "id", id, sizeof id) &&
          js_texto(p, fim, "name", c.nome, sizeof c.nome)) {
        snprintf(c.id, sizeof c.id, "stalker:%s", id);
        js_texto(p, fim, "logo", c.logo, sizeof c.logo);
        if (!js_texto(p, fim, "tv_genre_id", genId, sizeof genId)) genId[0] = 0;
        snprintf(c.categoria, sizeof c.categoria, "%s",
                 tituloDoGenero(gen, nGen, genId));
        if (js_texto(p, fim, "cmd", cmd, sizeof cmd) && cmd[0])
          tabelaPor(c.id, cmd);
        saida[n++] = c;
        noCiclo++;
      }
      p = js_prox(fim);
    }
    free(corpo);
    if (!noCiclo) break;   // pagina vazia: acabou
  }
  // CONTAGEM, e so contagem. Portal, MAC e token nao entram em log: registro.c
  // le o stdout do app e o desenha NA TELA (ver a nota no topo do header).
  printf("[stalker] portal ok, %d canal(is) em %d categoria(s)\n", n, nGen);
  return n;
}

// ------------------------------------------------------------- reproducao

int stalker_e_id(const char *id) {
  return id && !strncmp(id, "stalker:", 8);
}

static int cmdDe(const char *id, char *dst, unsigned tam) {
  int i, achou = 0;
  pthread_mutex_lock(&travaTabela);
  for (i = 0; i < nTabela; i++)
    if (!strcmp(tabela[i].id, id)) {
      snprintf(dst, tam, "%s", tabela[i].cmd);
      achou = 1;
      break;
    }
  pthread_mutex_unlock(&travaTabela);
  return achou;
}

int stalker_resolver(const char *id, char *url, unsigned n) {
  char cmd[192], enc[640], consulta[768];
  char *corpo;
  char bruto[1024];
  const char *p;
  if (!stalker_e_id(id) || !url || n < 2) return 0;
  if (!stalker_configurado()) return 0;
  if (!cmdDe(id, cmd, sizeof cmd)) return 0;
  urlenc(cmd, enc, sizeof enc);
  snprintf(consulta, sizeof consulta,
           "type=itv&action=create_link&cmd=%s&series=0&forced_storage=0"
           "&disable_ad=0&download=0", enc);
  // 1 no segundo argumento: ESTE e o caminho que nao pode falhar por token
  // velho. A lista pode ser refeita sem ninguem notar; um zap que morre e a
  // pessoa olhando a tela.
  corpo = chamar(consulta, 1);
  if (!corpo) return 0;
  if (!js_texto_raiz(corpo, "cmd", bruto, sizeof bruto)) {
    free(corpo);
    return 0;
  }
  free(corpo);
  // O portal devolve "ffmpeg http://..." (ou "auto http://...") na maioria dos
  // casos: o primeiro token e o player que o STB deveria usar, nao parte da
  // URL. Passar isso ao pipeline daria uma URL invalida sem erro legivel.
  p = strstr(bruto, "http://");
  if (!p) p = strstr(bruto, "https://");
  if (!p) return 0;
  snprintf(url, n, "%s", p);
  // Um espaco depois da URL (alguns portais anexam parametros de STB) termina
  // o endereco: o pipeline nao os entende e eles nao fazem falta.
  { char *esp = strchr(url, ' '); if (esp) *esp = 0; }
  return url[0] != 0;
}
