#include "parental.h"
#include "rede.h"
#include "js.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Endereco em js/config.js do app web: PARENTAL_GUIDE_API_URL.
#define PG_URL "https://api.tiffara.com/titles/%s/parentsGuide"

// A ORDEM importa: e a mesma em que o web lista as categorias
// (parentalGuideRepository.js:86), e por isso a mesma em que aparecem na tela.
static const struct { const char *chave, *rotulo; } CATS[PG_MAX] = {
  { "SEXUAL_CONTENT",              "Nudez" },
  { "VIOLENCE",                    "Viol\xc3\xaa" "ncia" },
  { "PROFANITY",                   "Linguagem Impr\xc3\xb3" "pria" },
  { "ALCOHOL_DRUGS",               "Drogas/\xc3\x81" "lcool" },
  { "FRIGHTENING_INTENSE_SCENES",  "Conte\xc3\xba" "do Assustador" },
};

static const struct { const char *nivel, *rotulo; } NIVEIS[] = {
  { "mild",     "Leve" },
  { "moderate", "Moderado" },
  { "severe",   "Severo" },
  { NULL, NULL }
};

static struct { const char *rotulo, *gravidade; } linhas[PG_MAX];
static int  nLinhas;
static char idPedido[16];
static char idEmCurso[16];
static int  fioVivo;
static pthread_t fio;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static const char *rotuloNivel(const char *nivel) {
  int i;
  for (i = 0; NIVEIS[i].nivel; i++)
    if (!strcmp(NIVEIS[i].nivel, nivel)) return NIVEIS[i].rotulo;
  return NULL;
}

// Gravidade DOMINANTE de uma categoria, pela regra do web (resolveSeverity):
// o nivel mais votado entre os diferentes de "none"; descartado quando "none"
// tem tantos votos quanto ele ou mais. Sem essa segunda metade, todo filme
// mostraria as cinco linhas, porque sempre ha algum voto em tudo.
static const char *dominante(const char *ini, const char *fim) {
  const char *bq = js_array(ini, fim, "severityBreakdowns");
  char melhor[16] = "";
  double vMelhor = 0.0, vNone = 0.0;
  while (bq) {
    const char *bf = js_fim(bq);
    char nivel[16] = "";
    double votos;
    js_texto(bq, bf, "severityLevel", nivel, sizeof nivel);
    votos = js_num(bq, bf, "voteCount", 0.0);
    if (!strcmp(nivel, "none")) {
      vNone = votos;
    } else if (nivel[0] && votos > vMelhor) {
      vMelhor = votos;
      snprintf(melhor, sizeof melhor, "%s", nivel);
    }
    bq = js_prox(bf);
  }
  if (!melhor[0] || vMelhor <= vNone) return NULL;
  return rotuloNivel(melhor);
}

// O FIO ATENDE O ULTIMO PEDIDO, e nao so o primeiro.
//
// Era: `parental_pedir` guardava o id novo em `idPedido` e, se ja houvesse um
// fio no ar, VOLTAVA SEM FAZER NADA. O fio no ar estava buscando o id ANTIGO e,
// no fim, so publica se `id == idPedido` — que agora nao bate mais. Ou seja: o
// pedido novo era registrado e nunca atendido, e o resultado do antigo era
// descartado. Ninguem retentava.
//
// Na tela isso e o relato do rawldon (#31): "sometimes the badges appear
// correctly, but most of the time they are missing". Abrir um titulo enquanto
// a consulta do anterior ainda estava em voo — que e o caso comum, porque a
// consulta leva ate 8 s — deixava aquele titulo sem selo para sempre.
//
// O conserto e o mesmo padrao de desc_episodios/desc_episodios_pendente: em vez
// de desistir, o fio olha de novo antes de morrer e recomeca se o alvo mudou.
static void *buscar(void *arg) {
  char url[160], id[16];
  char *corpo;
  (void)arg;
  for (;;) {
  pthread_mutex_lock(&trava);
  snprintf(id, sizeof id, "%s", idEmCurso);
  pthread_mutex_unlock(&trava);

  snprintf(url, sizeof url, PG_URL, id);
  corpo = rede_baixar(url, 8);
  if (corpo) {
    struct { const char *r, *g; } achado[PG_MAX];
    int n = 0, k;
    // Uma passada por categoria e nao uma pelo array: assim a ordem da tela e
    // a de CATS, e nao a ordem em que a API devolveu.
    for (k = 0; k < PG_MAX; k++) {
      const char *q = js_array(corpo, NULL, "parentsGuide");
      while (q) {
        const char *f = js_fim(q);
        char cat[48] = "";
        js_texto(q, f, "category", cat, sizeof cat);
        if (!strcmp(cat, CATS[k].chave)) {
          const char *g = dominante(q, f);
          if (g) { achado[n].r = CATS[k].rotulo; achado[n].g = g; n++; }
          break;
        }
        q = js_prox(f);
      }
    }
    free(corpo);
    pthread_mutex_lock(&trava);
    // So publica se o titulo ainda e este: trocar de filme durante a busca
    // deixaria o aviso do anterior na tela do seguinte.
    if (!strcmp(id, idPedido)) {
      for (k = 0; k < n; k++) {
        linhas[k].rotulo = achado[k].r;
        linhas[k].gravidade = achado[k].g;
      }
      nLinhas = n;
    }
    pthread_mutex_unlock(&trava);
    printf("[parental] %s -> %d linhas\n", id, n); fflush(stdout);
  }
  // O ALVO MUDOU ENQUANTO EU BUSCAVA? Entao ha um pedido pendente que ninguem
  // mais vai atender: `parental_pedir` nao cria fio quando ja existe um.
  pthread_mutex_lock(&trava);
  if (strcmp(idPedido, id)) {
    snprintf(idEmCurso, sizeof idEmCurso, "%s", idPedido);
    pthread_mutex_unlock(&trava);
    continue;
  }
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
  }
}

void parental_pedir(const char *imdb) {
  if (!imdb || imdb[0] != 't') return;
  pthread_mutex_lock(&trava);
  if (!strcmp(idPedido, imdb)) { pthread_mutex_unlock(&trava); return; }
  snprintf(idPedido, sizeof idPedido, "%s", imdb);
  nLinhas = 0;
  if (fioVivo) { pthread_mutex_unlock(&trava); return; }
  snprintf(idEmCurso, sizeof idEmCurso, "%s", imdb);
  fioVivo = 1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&fio, NULL, buscar, NULL) != 0) fioVivo = 0;
  else pthread_detach(fio);
}

int parental_n(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = nLinhas;
  pthread_mutex_unlock(&trava);
  return n;
}
// SOB A TRAVA, como parental_n. Estes dois liam `nLinhas` e `linhas` enquanto
// o fio de busca os reescrevia segurando o mutex — o desenho podia ver a
// contagem nova com a linha velha. Sao ~10 chamadas por quadro; o mutex nao
// custa nada aqui, e o ponteiro devolvido aponta para literal (CATS/NIVEIS),
// que nao muda depois de solto.
const char *parental_rotulo(int i) {
  const char *r = "";
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nLinhas) r = linhas[i].rotulo;
  pthread_mutex_unlock(&trava);
  return r;
}
const char *parental_gravidade(int i) {
  const char *r = "";
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nLinhas) r = linhas[i].gravidade;
  pthread_mutex_unlock(&trava);
  return r;
}
