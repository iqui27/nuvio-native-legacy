// A curadoria da fileira Herói — o resto do contrato está em curadoria.h.
#include "curadoria.h"
#include "js.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- estado -----------------------------------------------------------------

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
// DUAS CÓPIAS, uma em uso e uma sobrando (curadoria.h: "o laço de desenho só
// lê"): o fio de rede escreve na sobrando e troca as duas sob o mutex — quem
// desenha segura o ponteiro da antiga por um quadro sem ler palavra pela
// metade. O próximo fio (30 min depois) escreve na outra; ninguém reescreve
// um buffer que ainda está na mão de quem desenhou.
static char estadoImdb[2][64];   // "" sem curadoria
static char estadoTexto[2][240];
static int ativo;                // qual cópia está na mão dos getters
static unsigned revisao;
static char url[300];

#ifndef CURADORIA_TESTE
#include "dados.h"
#include "rede.h"
#include <SDL2/SDL_timer.h>
// O ciclo do canal do dono (avisos.c): ler de vez em quando, nunca no fio
// do desenho, nunca mais que uma leitura por vez.
#define NV_CURADORIA_MIN 30
static pthread_t fio;
static int fioVivo;
static int urlLida;            // carregar() ainda não rodou neste processo
static unsigned geracaoUrl;    // invalida resposta de fio de endereço antigo
static Uint32 lidaEm;   // 0 = nunca
#endif

unsigned curadoria_revisao(void) {
  unsigned v;
  pthread_mutex_lock(&trava);
  v = revisao;
  pthread_mutex_unlock(&trava);
  return v;
}

const char *curadoria_imdb(void) {
  int i;
  pthread_mutex_lock(&trava);
  i = ativo;
  pthread_mutex_unlock(&trava);
  return estadoImdb[i];   // estático: válido até a próxima chamada
}

const char *curadoria_texto(void) {
  int i;
  pthread_mutex_lock(&trava);
  i = ativo;
  pthread_mutex_unlock(&trava);
  return estadoTexto[i];
}

int curadoria_ativa(void) { return curadoria_imdb()[0] != 0; }

// AS CHAVES QUE O SERVIDOR JA MANDA (rotaReceber, servidor/recomendacoes):
// imdb, tipo, titulo, poster, ano, modelo, texto. Aqui só interessam imdb e
// texto — o título, a arte e o resto vêm do catálogo, que é quem os tem.
static int receberObjeto(const char *ini, const char *fim) {
  char imdb[64] = "", texto[240] = "";
  js_texto(ini, fim, "imdb", imdb, sizeof imdb);
  js_texto(ini, fim, "texto", texto, sizeof texto);
  if (!imdb[0]) return 0;
  pthread_mutex_lock(&trava);
  snprintf(estadoImdb[!ativo], sizeof estadoImdb[0], "%s", imdb);
  snprintf(estadoTexto[!ativo], sizeof estadoTexto[0], "%s", texto);
  ativo = !ativo;   // a cópia velha segue válida para quem a segurava
  revisao++;
  pthread_mutex_unlock(&trava);
  return 1;
}

int curadoria_receber(const char *corpo) {
  const char *p;
  int ok = 0;
  if (!corpo) return 0;
  if ((p = js_raiz_array(corpo))) {           // array: vale a primeira entrada
    ok = receberObjeto(p, js_fim(p));
  } else if ((p = strchr(corpo, '{')))        // objeto único
    ok = receberObjeto(p, corpo + strlen(corpo));
  if (!ok) {
    // SEM CURADORIA É ESTADO, não é erro: o corpo pode ser um 404 em texto,
    // um {"erro":...} ou o JSON sem imdb — em todos eles a fileira cai na
    // fonte própria e o selo não pode dizer "Curadoria".
    pthread_mutex_lock(&trava);
    if (estadoImdb[ativo][0] || estadoTexto[ativo][0]) revisao++;
    estadoImdb[!ativo][0] = 0;
    estadoTexto[!ativo][0] = 0;
    ativo = !ativo;
    pthread_mutex_unlock(&trava);
  }
  return ok;
}

// --- a escolha do cartão ------------------------------------------------------

int curadoria_escolher(const int *itens, int n, int catN, int curadoria,
                       unsigned *semente) {
  unsigned s;
  if (curadoria >= 0 && curadoria < catN) return curadoria;
  if (itens && n > 0) return itens[0];
  if (catN < 1) return -1;
  if (!semente) return 0;
  if (*semente) {
    // O CATÁLOGO ENCOLHEU: o sorteio da sessão pode ter ficado fora dele
    // ("Ocultar filmes assistidos" come um título, o feed encurta a lista).
    // Fora dos limites NÃO é estável — é cartão em branco: sorteia de novo.
    if ((int)*semente <= catN) return (int)*semente - 1;
  }
  s = (unsigned)time(NULL);
  *semente = (unsigned)(rand_r(&s) % (unsigned)catN) + 1;
  return (int)*semente - 1;
}

// --- o endereço e o fio de leitura --------------------------------------------

#ifdef CURADORIA_TESTE
// O teste alimenta o parser direto (curadoria_receber); endereço em memória.
int curadoria_definir_url(const char *u) {
  snprintf(url, sizeof url, "%s", u && u[0] ? u : "");
  return 1;
}
const char *curadoria_url(void) { return url; }
void curadoria_passo(void) {}

#else
static void carregar(void);

static void *fioFn(void *u) {
  char *corpo, endereco[sizeof url];
  unsigned geracao;
  int vivo = 1;
  (void)u;
  // O endereço pode mudar em Ajustes enquanto o fio corre: copia sob o
  // mutex, nunca lê `url` solto enquanto definir_url escreve. A geração
  // junto garante que a resposta ainda é DO endereço que ela pediu — um
  // fio antigo não pode republicar recomendação depois de desligar ou
  // trocar a curadoria.
  pthread_mutex_lock(&trava);
  snprintf(endereco, sizeof endereco, "%s", url);
  geracao = geracaoUrl;
  pthread_mutex_unlock(&trava);
  corpo = rede_baixar_com(endereco, 15, NULL);
  pthread_mutex_lock(&trava);
  vivo = geracao == geracaoUrl && endereco[0] && !strcmp(endereco, url);
  pthread_mutex_unlock(&trava);
  if (corpo) {
    if (vivo && !curadoria_receber(corpo))
      printf("[curadoria] corpo sem recomendacao\n");
    free(corpo);
  }
  fflush(stdout);
  // fioVivo e lidaEm sao lidos no passo sem mutex nenhum: sob `trava`, como
  // o resto do estado. E o prazo de uma resposta VENCIDA e zero — o endereco
  // ja trocou, nao vale adiar a leitura do novo pelo ciclo de 30 min do velho.
  pthread_mutex_lock(&trava);
  lidaEm = vivo ? SDL_GetTicks() : 0;
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

void curadoria_passo(void) {
  Uint32 agora;
  // Sem depender de Ajustes: Home chama o passo antes de alguém ter lido
  // curadoria_url(), e é aqui que o endereço gravado entra no ciclo.
  if (!urlLida) { carregar(); urlLida = 1; }
  pthread_mutex_lock(&trava);
  if (!url[0]) { pthread_mutex_unlock(&trava); return; }
  pthread_mutex_unlock(&trava);
  agora = SDL_GetTicks();
  pthread_mutex_lock(&trava);
  if (fioVivo || (lidaEm && agora - lidaEm < (Uint32)NV_CURADORIA_MIN * 60000u)) {
    pthread_mutex_unlock(&trava);
    return;
  }
  // ANTES DO CREATE, como o canal do dono (avisos.c): a rede pode recusar em
  // ~50µs e o fio terminar antes do create voltar — na ordem antiga o 1 do
  // passo cobria o 0 do fio e a curadoria congelava até reiniciar o app.
  fioVivo = 1;
  pthread_mutex_unlock(&trava);
  // Detach como o resto da casa (avisos.c): a TV vive dias, cada ciclo é um
  // fio de 30 em 30 min — sem detach vaza um por ciclo. E se o create falhar
  // (EAGAIN), marcar o prazo também: sem isso tentaria a cada quadro.
  if (pthread_create(&fio, NULL, fioFn, NULL) == 0) pthread_detach(fio);
  else {
    pthread_mutex_lock(&trava);
    fioVivo = 0;
    lidaEm = agora;
    pthread_mutex_unlock(&trava);
  }
}

// O endereço mora em curadoria.txt na pasta de dados, por aparelho — o mesmo
// lugar do endereço do servidor P2P (p2p.txt): é do dono da TV, não da conta.
static void carregar(void) {
  char *t = dados_ler("curadoria.txt");
  url[0] = 0;
  if (t) {
    size_t i = 0;
    while (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r') i++;
    snprintf(url, sizeof url, "%s", t + i);
    url[strcspn(url, " \t\n\r")] = 0;
    free(t);
  }
}

const char *curadoria_url(void) {
  if (!urlLida) { carregar(); urlLida = 1; }
  return url;
}

int curadoria_definir_url(const char *u) {
  char nova[sizeof url] = "";
  size_t i = 0;
  while (u && (u[i] == ' ' || u[i] == '\t')) i++;
  if (u && u[i] && strncmp(u + i, "http://", 7) && strncmp(u + i, "https://", 8))
    return 0;
  if (u && u[i]) snprintf(nova, sizeof nova, "%s", u + i);
  pthread_mutex_lock(&trava);
  if (strncmp(url, nova, sizeof url)) {
    geracaoUrl++;
    // TROCAR DE ENDEREÇO RECOMEÇA O CICLO: sem o zero o passo esperava os
    // 30 min da leitura do endereço VELHO, e o filme do A ficava na tela
    // com a configuração do B (o selo dizendo "Curadoria" de outro JSON).
    lidaEm = 0;
    // E corta a recomendação junto, como no desligar: o filme do A não é a
    // curadoria do B (e um fio em voo não pode republicá-lo — a geração
    // veta a resposta).
    estadoImdb[!ativo][0] = 0;
    estadoTexto[!ativo][0] = 0;
    ativo = !ativo;
    revisao++;
  }
  snprintf(url, sizeof url, "%s", nova);
  pthread_mutex_unlock(&trava);
  if (nova[0]) dados_gravar("curadoria.txt", nova);
  else dados_apagar("curadoria.txt");
  return 1;
}
#endif
