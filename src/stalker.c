#include "stalker.h"
#include "iptvid.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "perfis.h"
#include "perfisessao.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define ST_ARQ_FMT     "stalker-p%d.txt"
#define ST_MAX_CANAL   1200
#define ST_PRAZO_S     15
// Piso de renovacao do token. O 401 e o sinal de verdade e e ele que manda;
// isto so cobre o caso em que o portal aceita o token velho para a LISTA e o
// recusa para o create_link — visto em portais que separam as duas validacoes.
// Renovar antes de tocar custa uma requisicao e evita o zap morrer na mao.
#define ST_TOKEN_VALIDO_S (55 * 60)

typedef struct { char id[80], cmd[192]; } StCmd;

typedef struct FonteContexto {
  unsigned sid; struct FonteContexto *proximo;
  char portal_[256];
  char mac_[32];
  char deviceId_[80];
  char serial_[64];
  char token_[128];
  char caminho_[96];
  char baseEf_[256];
  time_t tokenEm_;
  int perfilLido_;
  int lido_;
  NvPerfilSessao sessaoLida_;
  StCmd tabela_[ST_MAX_CANAL];
  int nTabela_;
  pthread_mutex_t travaSessao_;
  pthread_mutex_t travaTabela_;
  pthread_mutex_t travaHandshake_;
} FonteContexto;
#define IPTV_CTX_INICIALIZAR(c) do { (c)->perfilLido_ = -1; pthread_mutex_init(&(c)->travaSessao_, NULL); pthread_mutex_init(&(c)->travaTabela_, NULL); pthread_mutex_init(&(c)->travaHandshake_, NULL); } while (0)
#define IPTV_CTX_EMERGENCIA { .sid = UINT_MAX, .perfilLido_ = -1, .travaSessao_ = PTHREAD_MUTEX_INITIALIZER, .travaTabela_ = PTHREAD_MUTEX_INITIALIZER, .travaHandshake_ = PTHREAD_MUTEX_INITIALIZER }
// NV_TPK40: rebuild this unit with pthread storage (tools/tpk.sh).
#include "iptvcontexto.inc"
#define portal (contexto()->portal_ )
#define mac (contexto()->mac_ )
#define deviceId (contexto()->deviceId_ )
#define serial (contexto()->serial_ )
#define token (contexto()->token_ )
#define caminho (contexto()->caminho_ )
#define baseEf (contexto()->baseEf_ )
#define tokenEm (contexto()->tokenEm_ )
#define perfilLido (contexto()->perfilLido_ )
#define lido (contexto()->lido_ )
#define sessaoLida (contexto()->sessaoLida_ )
#define tabela (contexto()->tabela_ )
#define nTabela (contexto()->nTabela_ )
#define travaSessao (contexto()->travaSessao_ )
#define travaTabela (contexto()->travaTabela_ )
#define travaHandshake (contexto()->travaHandshake_ )

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

// Tira esquema e barra final, e guarda o PREFIXO de caminho que a pessoa colou
// ("http://portal.tv:8080/c/" e "portal.tv:8080" viram a mesma coisa;
// "portal.tv/meuportal/c/" vira "http://portal.tv/meuportal"). O "/c" final e
// a pagina do cliente, nao parte do prefixo. Consulta e fragmento caem fora.
static void normalizarPortal(const char *entrada, char *dst, unsigned tam) {
  const char *p = entrada ? entrada : "";
  char lim[256];
  unsigned n;
  while (*p == ' ') p++;
  if (!strncmp(p, "http://", 7)) p += 7;
  else if (!strncmp(p, "https://", 8)) p += 8;
  snprintf(lim, sizeof lim, "%s", p);
  n = (unsigned)strcspn(lim, "?#");
  lim[n] = 0;
  while (n > 0 && (lim[n - 1] == '/' || lim[n - 1] == ' ')) lim[--n] = 0;
  // "/c", "/c/" ja caram acima ou aqui; "/c/index.html" e "/c/index.php"
  // (um unico nome de arquivo depois do /c) tambem.
  {
    char *c = NULL, *q = lim;
    while ((q = strstr(q, "/c")) != NULL) {
      if (q[2] == 0 || (q[2] == '/' && !strchr(q + 3, '/'))) { c = q; break; }
      q += 2;
    }
    if (c) {
      *c = 0; n = (unsigned)(c - lim);
      while (n > 0 && lim[n - 1] == '/') lim[--n] = 0;
    }
  }
  // O esquema volta como http: portal Stalker com TLS e raro, e quando existe
  // o host costuma vir com a porta 443 explicita — caso em que quem configurou
  // digitou https e a normalizacao acima o removeu. Preferir http aqui e o que
  // funciona nos portais reais; um https quebrado apareceria como "sem
  // resposta", que e o mesmo diagnostico de portal errado.
  snprintf(dst, tam, "http://%s", lim);
}

// O prefixo ja termina em /stalker_portal? Entao a rota "/stalker_portal/..."
// da lista cairia duplicada.
static int prefixoTemStalkerPortal(const char *base) {
  size_t n = strlen(base), k = strlen("/stalker_portal");
  return n >= k && !strcmp(base + n - k, "/stalker_portal");
}

// ----------------------------------------------------------------- cadastro

static const char *arquivo(void) {
  FonteFio *f = fonteFio();
  if (contexto()->sid) snprintf(f->nome, sizeof f->nome, "stalker-p%d-s%u.txt", perfis_ativo(), contexto()->sid);
  else snprintf(f->nome, sizeof f->nome, ST_ARQ_FMT, perfis_ativo());
  return f->nome;
}

static void limparSessao(void) {
  token[0] = 0; caminho[0] = 0; baseEf[0] = 0; tokenEm = 0;
}

// Corpo da leitura, SEM travar: quem chama ja segura travaSessao. Existe
// separado porque os setters precisam ler-e-escrever numa seccao critica so.
static void carregarTravado(void) {
  int p = perfis_ativo();
  char *b, *linha, *fim;
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (lido && p == perfilLido && sessao == sessaoLida) return;
  perfilLido = p; sessaoLida = sessao; lido = 1;
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

static int gravar(void) {
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
  return dados_gravar(arquivo(), txt);
}

// DOIS CAMPOS, DOIS SETTERS, e nao um `definir` com tudo. A tela de Ajustes
// edita um de cada vez, e um setter unico a obrigaria a ler o MAC atual para
// reescreve-lo ao trocar so o endereco — ou seja, a devolver a credencial em
// claro para a camada de desenho so para poder grava-la de volta.
int stalker_definir_portal(const char *p) {
  pthread_mutex_lock(&travaSessao);
  carregarTravado();
  char anterior[sizeof portal];memcpy(anterior,portal,sizeof anterior);
  normalizarPortal(p, portal, sizeof portal);
  int ok=gravar();
  if(ok)limparSessao();else memcpy(portal,anterior,sizeof anterior);
  pthread_mutex_unlock(&travaSessao);
  if(ok) { pthread_mutex_lock(&travaTabela); nTabela = 0; pthread_mutex_unlock(&travaTabela); }
  return ok;
}
int stalker_conta_definir(const char *p,const char *m) {
  if(!p || strlen(p)>=sizeof portal || strpbrk(p,"\r\n\t") || !m || strlen(m)!=17)return 0;
  for(int i=0;i<17;i++)if(i%3==2?m[i]!=':':!((m[i]>='0' && m[i]<='9') || (m[i]>='a' && m[i]<='f') || (m[i]>='A' && m[i]<='F')))return 0;
  char normal[sizeof portal];normalizarPortal(p,normal,sizeof normal);if(!strcmp(normal,"http://"))return 0;
  pthread_mutex_lock(&travaSessao);carregarTravado();
  char antigoP[sizeof portal],antigoM[sizeof mac];memcpy(antigoP,portal,sizeof antigoP);memcpy(antigoM,mac,sizeof antigoM);
  snprintf(portal,sizeof portal,"%s",normal);snprintf(mac,sizeof mac,"%s",m);
  int atual=nv_perfil_sessao_travar(sessaoLida),ok=atual && gravar();
  if(atual)nv_perfil_sessao_destravar();
  if(!ok) { memcpy(portal,antigoP,sizeof antigoP);memcpy(mac,antigoM,sizeof antigoM); }
  else limparSessao();
  memset(antigoM,0,sizeof antigoM);pthread_mutex_unlock(&travaSessao);
  if(ok) { pthread_mutex_lock(&travaTabela);nTabela=0;pthread_mutex_unlock(&travaTabela); }
  return ok;
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
  static char curto[256];
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
                   const char *rota, const char *tok, const char *consulta,
                   NvPerfilSessao sessao) {
  const char *cab[7];
  char url[2048], c1[128], c2[192], c3[320];
  cabecalhos(portalLocal, macLocal, cab, tok, c1, sizeof c1, c2, sizeof c2, c3, sizeof c3);
  snprintf(url, sizeof url, "%s%s?%s&JsHttpRequest=1-xml", portalLocal, rota, consulta);
  if (!nv_perfil_sessao_atual(sessao)) return NULL;
  char *corpo = rede_baixar_com(url, ST_PRAZO_S, cab);
  if (!nv_perfil_sessao_atual(sessao)) { free(corpo); return NULL; }
  return corpo;
}

// Resposta util e a que traz o objeto `js`. Portal que recusou o token devolve
// corpo vazio, HTML de login, ou um `js` falso — e nenhum deles tem o campo
// pedido. Tratar "sem js" como "token morreu" e o que faz a renovacao disparar
// no caso real, em que o 401 nem sempre vem com codigo.
// Ministra normalmente envolve token/cmd em {"js": {...}}; alguns portais
// devolvem as chaves na raiz. Ler a raiz do envelope escolhido, nao um campo
// homonimo de outro objeto da resposta.
static int textoResposta(const char *corpo,const char *chave,char *out,unsigned n) {
  const char *p=corpo?strstr(corpo,"\"js\""):NULL;
  if(p && (p=strchr(p,':'))) {
    p++;while(*p==' '||*p=='\n'||*p=='\r'||*p=='\t')p++;
    if(*p=='{')return js_texto_raiz_em(p,js_fim(p),chave,out,n);
  }
  return js_texto_raiz(corpo,chave,out,n);
}
static int temJs(const char *corpo) {
  return corpo && strstr(corpo, "\"js\"") != NULL;
}

// Caminhos conhecidos de portal. A ordem e a frequencia com que aparecem.
static const char *ROTAS[] = {
  "/server/load.php", "/portal.php", "/stalker_portal/server/load.php", NULL
};

// Fim do "http://host[:porta]" dentro de `base`: onde comeca o prefixo.
static size_t tamHost(const char *base) {
  const char *p = base + 7;               // normalizarPortal sempre poe http://
  const char *barra = strchr(p, '/');
  return barra ? (size_t)(barra - base) : strlen(base);
}

// Candidatos da varredura, na ordem. Com prefixo (o portal ja leva o caminho
// colado), ele entra antes: "prefixo/server/load.php" e "prefixo/portal.php"
// ("/stalker_portal/..." so se o prefixo nao o traz). Se NENHUM responder, as
// rotas da raiz contra o host puro: quem colou "host/qualquer/c/" com o portal
// na raiz continua funcionando. Devolve 0 quando acabou; `rota` NULL = pular.
static int candidato(int k, const char *portalBase, char *base, unsigned tam, const char **rota) {
  size_t h = tamHost(portalBase);
  int prefixo = portalBase[h] != 0;
  if (k < 3) {
    snprintf(base, tam, "%s", portalBase);
    *rota = (k == 2 && prefixoTemStalkerPortal(portalBase)) ? NULL : ROTAS[k];
    return 1;
  }
  if (k < 6 && prefixo) {
    snprintf(base, tam, "%.*s", (int)h, portalBase);
    *rota = ROTAS[k - 3];
    return 1;
  }
  return 0;
}

typedef struct {
  char portalCopia[256], macCopia[32], tokenCopia[128], rota[96], efetiva[256];
  time_t tokenEmCopia;
} StConexao;

static int conexaoLer(StConexao *c, NvPerfilSessao sessao) {
  pthread_mutex_lock(&travaSessao);
  if (!nv_perfil_sessao_travar(sessao)) { pthread_mutex_unlock(&travaSessao); return 0; }
  carregarTravado();
  snprintf(c->portalCopia, sizeof c->portalCopia, "%s", portal);
  snprintf(c->macCopia, sizeof c->macCopia, "%s", mac);
  snprintf(c->tokenCopia, sizeof c->tokenCopia, "%s", token);
  snprintf(c->rota, sizeof c->rota, "%s", caminho);
  snprintf(c->efetiva, sizeof c->efetiva, "%s", baseEf);
  c->tokenEmCopia = tokenEm;
  nv_perfil_sessao_destravar();
  pthread_mutex_unlock(&travaSessao);
  return c->portalCopia[0] && c->macCopia[0];
}

// Serializa so as renovacoes, nunca os getters do desenho. O HTTP usa uma
// copia do cadastro; trocar perfil/editar cadastro nao espera o portal.
static int renovarToken(NvPerfilSessao sessao, const char *usado, int renovarAntes) {
  StConexao c;
  int k, lembrada, ok = 0;
  pthread_mutex_lock(&travaHandshake);
  if (!conexaoLer(&c, sessao)) goto fim;
  if (c.tokenCopia[0] &&
      (usado ? strcmp(usado, c.tokenCopia) != 0
             : !(renovarAntes && c.tokenEmCopia && time(NULL) - c.tokenEmCopia > ST_TOKEN_VALIDO_S))) {
    ok = 1; goto fim;
  }
  lembrada = c.rota[0] != 0;
  for (k = 0; nv_perfil_sessao_atual(sessao); k++) {
    char *corpo, base[256];
    const char *rota;
    // Rota que ja provou funcionar entra primeiro nas vezes seguintes; na
    // primeira, `caminho` esta vazio e a varredura e a ordem acima.
    if (lembrada) {
      snprintf(base, sizeof base, "%s", c.efetiva[0] ? c.efetiva : c.portalCopia);
      rota = c.rota;
    } else if (!candidato(k, c.portalCopia, base, sizeof base, &rota)) {
      break;
    }
    if (!rota) continue;
    corpo = pedir(base, c.macCopia, rota, "", "type=stb&action=handshake&token=&prehash=0", sessao);
    if (temJs(corpo)) {
      char t[128];
      if (textoResposta(corpo, "token", t, sizeof t) && t[0]) {
        free(corpo);
        // get_profile confirma o MAC. Portal que recusa a assinatura responde
        // aqui, e nao no handshake — sem esta chamada o app so descobriria no
        // create_link, com a pessoa ja olhando a tela de carregar.
        corpo = pedir(base, c.macCopia, rota, t, "type=stb&action=get_profile&hd=1&num_banks=2"
                                           "&stb_type=MAG250&image_version=218&auth_second_step=0", sessao);
        if (corpo) free(corpo);
        pthread_mutex_lock(&travaSessao);
        if (nv_perfil_sessao_travar(sessao)) {
          if (!strcmp(portal, c.portalCopia) && !strcmp(mac, c.macCopia)) {
            snprintf(token, sizeof token, "%s", t);
            snprintf(caminho, sizeof caminho, "%s", rota);
            snprintf(baseEf, sizeof baseEf, "%s", base);
            tokenEm = time(NULL); ok = 1;
          }
          nv_perfil_sessao_destravar();
        }
        pthread_mutex_unlock(&travaSessao);
        goto fim;
      }
    }
    if (corpo) free(corpo);
    // A rota lembrada falhou: esquecer e varrer de novo a partir do inicio.
    if (lembrada) { lembrada = 0; c.rota[0] = c.efetiva[0] = 0; k = -1; }
  }
fim:
  pthread_mutex_unlock(&travaHandshake);
  return ok;
}

// A UNICA porta de saida para o portal. Tudo passa por aqui, e e por isso que
// a renovacao de token existe num lugar so.
//
// O mutex nao e zelo: o fio do guia (lista) e o fio da reproducao
// (create_link) chamam concorrente. Dois handshakes em paralelo geram dois
// tokens e o segundo invalida o primeiro — o sintoma seria uma lista que
// carrega e um canal que nao abre, alternando, sem erro nenhum no log.
static char *chamar(const char *consulta, int renovarAntes, NvPerfilSessao sessao) {
  StConexao c;
  char *corpo;
  if (!renovarToken(sessao, NULL, renovarAntes) || !conexaoLer(&c, sessao) || !c.tokenCopia[0]) return NULL;
  corpo = pedir(c.efetiva[0] ? c.efetiva : c.portalCopia, c.macCopia, c.rota, c.tokenCopia, consulta, sessao);
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
  if (!renovarToken(sessao, c.tokenCopia, 0) || !conexaoLer(&c, sessao) || !c.tokenCopia[0]) return NULL;
  corpo = pedir(c.efetiva[0] ? c.efetiva : c.portalCopia, c.macCopia, c.rota, c.tokenCopia, consulta, sessao);
  if (temJs(corpo)) return corpo;
  if (corpo) free(corpo);
  return NULL;
}

// ------------------------------------------------------------------ canais

static void tabelaPor(const char *id, const char *cmd, NvPerfilSessao sessao) {
  int i;
  if (!nv_perfil_sessao_travar(sessao)) return;
  pthread_mutex_lock(&travaTabela);
  for (i = 0; i < nTabela; i++)
    if (!strcmp(tabela[i].id, id)) break;
  if (i == nTabela) {
    if (nTabela < ST_MAX_CANAL) nTabela++;
    else i = 0; // Keep storage bounded; missing commands are reloaded on playback.
  }
  if (i < ST_MAX_CANAL) {
    snprintf(tabela[i].id, sizeof tabela[i].id, "%s", id);
    snprintf(tabela[i].cmd, sizeof tabela[i].cmd, "%s", cmd);
  }
  pthread_mutex_unlock(&travaTabela);
  nv_perfil_sessao_destravar();
}

typedef struct { char id[24], titulo[64]; } StGen;

static int lerGeneros(StGen **saida, NvPerfilSessao sessao) {
  char *corpo = chamar("type=itv&action=get_genres", 0, sessao);
  const char *p;
  int n = 0;
  if (!corpo) return 0;
  p = strstr(corpo, "\"js\"");
  p = p ? strchr(p, '[') : NULL;
  p = p ? p + 1 : NULL;
  while (p && *p) {
    const char *fim = js_fim(p);
    StGen gen;
    if (!fim) break;
    if (js_texto(p, fim, "id", gen.id, sizeof gen.id) &&
        js_texto(p, fim, "title", gen.titulo, sizeof gen.titulo)) {
      if (!(n % 64)) {
        StGen *v = realloc(*saida, sizeof *v * (size_t)(n + 64));
        if (!v) break;
        *saida = v;
      }
      (*saida)[n++] = gen;
    }
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

int stalker_canais_cb_sessao(StalkerCb cb, void *u, uint64_t sessao) {
  StGen *gen = NULL;
  int nGen, pagina, n = 0, parar = 0;
  unsigned long anterior = 0;
  if (!nv_perfil_sessao_atual(sessao) || !stalker_configurado() || !cb) return 0;
  nGen = lerGeneros(&gen, sessao);

  // genre=* traz TODOS os canais numa varredura paginada, com o genero de cada
  // um no proprio item. Pedir genero a genero seria uma requisicao por
  // categoria — em portal com 40 generos, 40 idas so para montar a mesma lista.
  for (pagina = 1; nv_perfil_sessao_atual(sessao) && !parar; pagina++) {
    char consulta[128];
    char *corpo;
    const char *p;
    int noCiclo = 0;
    snprintf(consulta, sizeof consulta,
             "type=itv&action=get_ordered_list&genre=*&force_ch_link_check="
             "&fav=0&sortby=number&hd=0&p=%d", pagina);
    corpo = chamar(consulta, 0, sessao);
    if (!corpo) break;
    // Stop portals ignoring p without truncating valid later pages.
    unsigned long hash = 5381;
    for (const unsigned char *s = (const unsigned char *)corpo; *s; s++) hash = hash * 33 + *s;
    if (pagina > 1 && hash == anterior) { free(corpo); break; }
    anterior = hash;
    p = strstr(corpo, "\"data\"");
    p = p ? strchr(p, '[') : NULL;
    p = p ? p + 1 : NULL;
    while (p && *p && !parar) {
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
          tabelaPor(c.id, cmd, sessao);
        n++;
        noCiclo++;
        parar = !cb(&c, u);
      }
      p = js_prox(fim);
    }
    free(corpo);
    if (!noCiclo) break;   // pagina vazia: acabou
  }
  // CONTAGEM, e so contagem. Portal, MAC e token nao entram em log: registro.c
  // le o stdout do app e o desenha NA TELA (ver a nota no topo do header).
  printf("[stalker] portal ok, %d canal(is) em %d categoria(s)\n", n, nGen);
  free(gen);
  return nv_perfil_sessao_atual(sessao) ? n : 0;
}

typedef struct { StalkerCanal *saida; int n, max; } StVetor;
static int canalNoVetor(const StalkerCanal *c, void *u) {
  StVetor *v = u;
  v->saida[v->n++] = *c;
  return v->n < v->max;
}
int stalker_canais_sessao(StalkerCanal *saida, int max, uint64_t sessao) {
  if (!saida || max < 1) return 0;
  StVetor v = {saida, 0, max < ST_MAX_CANAL ? max : ST_MAX_CANAL};
  stalker_canais_cb_sessao(canalNoVetor, &v, sessao);
  return nv_perfil_sessao_atual(sessao) ? v.n : 0;
}

int stalker_canais(StalkerCanal *saida, int max) {
  return stalker_canais_sessao(saida, max, nv_perfil_sessao_ler());
}

// ------------------------------------------------------------- reproducao

int stalker_e_id(const char *id) {
  return iptvid_ler("stalker:", id, NULL, NULL, 0);
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

static int procurarCmd(const StalkerCanal *c, void *u) {
  return strcmp(c->id, (const char *)u) != 0;
}

static int resolverSessao(const char *id, char *url, unsigned n, uint64_t sessao) {
  char cmd[192], enc[640], consulta[768];
  char *corpo;
  char bruto[1024];
  const char *p;
  if (!nv_perfil_sessao_atual(sessao) || !stalker_e_id(id) || !url || n < 2) return 0;
  if (!stalker_configurado()) return 0;
  if (!cmdDe(id, cmd, sizeof cmd)) {
    stalker_canais_cb_sessao(procurarCmd, (void *)id, sessao);
    if (!cmdDe(id, cmd, sizeof cmd)) return 0;
  }
  urlenc(cmd, enc, sizeof enc);
  snprintf(consulta, sizeof consulta,
           "type=itv&action=create_link&cmd=%s&series=0&forced_storage=0"
           "&disable_ad=0&download=0", enc);
  // 1 no segundo argumento: ESTE e o caminho que nao pode falhar por token
  // velho. A lista pode ser refeita sem ninguem notar; um zap que morre e a
  // pessoa olhando a tela.
  corpo = chamar(consulta, 1, sessao);
  if (!corpo) return 0;
  if (!textoResposta(corpo, "cmd", bruto, sizeof bruto)) {
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
  if (!nv_perfil_sessao_atual(sessao)) { url[0] = 0; return 0; }
  return url[0] != 0;
}

int stalker_resolver(const char *id, char *url, unsigned n) {
  return stalker_resolver_sessao(id, url, n, nv_perfil_sessao_ler());
}

void stalker_fonte_selecionar(unsigned id){atomic_store(&contaEditada,id);}
int stalker_fonte_configurado(unsigned id){unsigned a=usarFonte(id);int r=stalker_configurado();usarFonte(a);return r;}
int stalker_fonte_canais(unsigned id,StalkerCanal *out,int max,uint64_t sessao){
  unsigned a=usarFonte(id);int n=stalker_canais_sessao(out,max,sessao);
  for(int i=0;i<n;i++){char original[80];snprintf(original,sizeof original,"%s",out[i].id);
    if(!iptvid_montar("stalker:",id,original,out[i].id,sizeof out[i].id)){n=i;break;}}
  usarFonte(a);return n;
}
typedef struct { unsigned id; StalkerCb cb; void *u; } StFonteCb;
static int canalDaFonte(const StalkerCanal *c, void *u) {
  StFonteCb *x = u;
  StalkerCanal out = *c;
  if (!iptvid_montar("stalker:", x->id, c->id, out.id, sizeof out.id)) return 0;
  return x->cb(&out, x->u);
}
int stalker_fonte_canais_cb(unsigned id, StalkerCb cb, void *u, uint64_t sessao) {
  unsigned anterior = usarFonte(id);
  StFonteCb x = {id, cb, u};
  int n = cb ? stalker_canais_cb_sessao(canalDaFonte, &x, sessao) : 0;
  usarFonte(anterior);
  return n;
}
void stalker_fonte_esquecer(unsigned id){unsigned a=usarFonte(id);stalker_esquecer();usarFonte(a);}
int stalker_resolver_sessao(const char *id,char *out,unsigned n,uint64_t sessao){
  unsigned f;char original[80];if(!iptvid_ler("stalker:",id,&f,original,sizeof original))return 0;
  unsigned a=usarFonte(f);int r=resolverSessao(original,out,n,sessao);usarFonte(a);return r;
}

void stalker_fontes_invalidar(void) {
  pthread_mutex_lock(&fontesTrava);
  for(FonteContexto *c=fontesContextos;c;c=c->proximo){ pthread_mutex_lock(&c->travaSessao_);c->lido_=0;c->token_[0]=0;c->tokenEm_=0;
    c->portal_[0]=c->mac_[0]=c->deviceId_[0]=c->serial_[0]=0;
    pthread_mutex_lock(&c->travaTabela_);memset(c->tabela_,0,sizeof c->tabela_);c->nTabela_=0;pthread_mutex_unlock(&c->travaTabela_);
    pthread_mutex_unlock(&c->travaSessao_); }
  pthread_mutex_unlock(&fontesTrava);
}
