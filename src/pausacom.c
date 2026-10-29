// Comentarios do Trakt para o painel de pausa. Ver pausacom.h para o contrato.
//
// COMO SAI NA TELA: texto de uma linha so, no maximo 280 bytes (corte na
// fronteira de caractere UTF-8, com "..."). Comentario com menos de 20
// caracteres nao diz nada e e descartado; spoiler e descartado sempre.
//
// REDE: um fio por vez, com 15 s de limite por pedido, igual a serieaud.c.
// Uma troca de chave no meio de um pedido nao espera o antigo: o fio termina
// o que esta no ar, descarta o resultado (comparado pela geracao) e busca a
// chave nova.
//
// Pedido que falhou (sem rede, HTTP ruim ou sem credencial) so sai de novo quando a pessoa pede a
// mesma chave 30 s depois; no maximo 3 redes por chave. Resposta valida sem comentario nao e falha.
#include "pausacom.h"
#include "comentordem.h"
#include "trakt.h"
#include "rede.h"
#include "js.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define PC_CORTE 280   // bytes de texto antes do "..."
#define PC_MINIMO 20   // caracteres (UTF-8) para o comentario valer
#define PC_BRUTO 1024  // teto do texto cru lido do JSON (o corte e antes)
#define PC_PROF 16     // profundidade maxima de [ e { aceita
#define PC_ESPERA_MS  30000LL  // espera entre tentativas de chave que falhou
#define PC_TENTATIVAS 3        // pedidos de rede por chave (sem credencial nao conta)

typedef struct {
  char imdb[24];
  int  temp, ep;
  char chave[64];   // "tt1234567" ou "tt1234567:T:E"; vazia = nenhuma
} Pedido;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Pedido pedido;
static PcComentario itens[PC_MAX];
static int nItens;
static int mudou, fioVivo;
static unsigned long geracao;
static pthread_t fio;
static char isoPref[4];   // idioma da interface; "" = sem preferencia

// ESTAVEL: os do idioma da interface primeiro; dentro de cada grupo segue a
// ordem de curtidas que o Trakt deu.
static void ordenarIdioma(PcComentario *v, int n, const char *iso) {
  PcComentario tmp[PC_MAX];
  int i, m = 0;
  if (n < 2 || n > PC_MAX) return;
  for (i = 0; i < n; i++) if (coment_mesmo_idioma(v[i].lingua, iso)) tmp[m++] = v[i];
  for (i = 0; i < n; i++) if (!coment_mesmo_idioma(v[i].lingua, iso)) tmp[m++] = v[i];
  memcpy(v, tmp, sizeof(PcComentario) * (size_t)n);
}
static int aguarda;              // o ultimo pedido da chave atual falhou (rede ou credencial)
static int tentativas;           // pedidos de rede feitos para a chave atual
static long long ultimaFalha;    // ms monotonicos da ultima falha da chave atual
static long long (*relogioTeste)(void);   // so o teste troca; NULL = relogio real

// Ms no relogio monotonico (ou o do teste). Chamada com a trava.
static long long agora(void) {
  struct timespec ts;
  if (relogioTeste) return relogioTeste();
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// --- ESTRUTURA DO JSON -------------------------------------------------------

// Confere so a ESTRUTURA: [ ] e { } casados fora de texto, texto fechado, e
// nada depois do ']' final. Sem isto, uma resposta cortada pela metade
// devolveria os comentarios que chegaram inteiros, como se a resposta fosse
// completa. Tipos nao sao conferidos aqui; isso e dos js_*.
static int bemFormado(const char *s) {
  char pilha[PC_PROF];
  int n = 0, texto = 0;
  const char *p = s;
  if (!s) return 0;
  while (*p && (unsigned char)*p <= ' ') p++;
  if (*p != '[') return 0;
  for (; *p; p++) {
    char c = *p;
    if (texto) {
      if (c == '\\') { if (!p[1]) return 0; p++; }
      else if (c == '"') texto = 0;
      continue;
    }
    if (c == '"') texto = 1;
    else if (c == '[' || c == '{') {
      if (n == PC_PROF) return 0;
      pilha[n++] = c == '[' ? ']' : '}';
    } else if (c == ']' || c == '}') {
      if (!n || pilha[--n] != c) return 0;
      if (!n) {
        for (p++; *p; p++) if ((unsigned char)*p > ' ') return 0;
        return 1;
      }
    }
  }
  return 0;
}

// --- TEXTO DE UMA LINHA ------------------------------------------------------

// Controles e espacos viram UM espaco (runs colapsam, sem espaco nas pontas).
// Corta em PC_CORTE bytes, recuando ate a fronteira de caractere, e marca com
// "...". Devolve quantos caracteres (UTF-8) o texto tinha antes do corte.
static int linhaUnica(const char *src, char *dst, size_t tam) {
  size_t k = 0, cut;
  int letras = 0, espaco = 1;
  if (!tam) return 0;
  dst[0] = 0;
  for (; *src; src++) {
    unsigned char c = (unsigned char)*src;
    if (c <= ' ' || c == 0x7f) {
      if (!espaco && k + 1 < tam) { dst[k++] = ' '; espaco = 1; }
      continue;
    }
    if (k + 1 >= tam) break;
    dst[k++] = (char)c;
    espaco = 0;
    if ((c & 0xC0) != 0x80) letras++;
  }
  if (k && dst[k - 1] == ' ') k--;
  dst[k] = 0;
  if (k > PC_CORTE) {
    cut = PC_CORTE;
    while (cut > 0 && ((unsigned char)dst[cut] & 0xC0) == 0x80) cut--;
    while (cut > 0 && dst[cut - 1] == ' ') cut--;
    memcpy(dst + cut, "...", 4);
  }
  return letras;
}

// 1 quando o texto traz a marca "[spoiler]" (qualquer caixa).
static int tagSpoiler(const char *s) {
  static const char tag[] = "[spoiler]";
  for (; *s; s++) {
    size_t i = 0;
    while (tag[i] && s[i]) {
      char c = s[i];
      if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
      if (c != tag[i]) break;
      i++;
    }
    if (!tag[i]) return 1;
  }
  return 0;
}

// --- PARSER ------------------------------------------------------------------

int pausacom_interpretar(const char *json, PcComentario *out, int max) {
  char bruto[PC_BRUTO];
  const char *p, *f = NULL;
  int n = 0;
  if (!json || !out) return 0;
  if (max > PC_MAX) max = PC_MAX;
  if (!bemFormado(json)) return 0;
  for (p = js_raiz_array(json); p && n < max; p = js_prox(f)) {
    PcComentario c;
    char spoiler[8] = "";
    double nota;
    if (*p != '{') break;
    f = js_fim(p);
    memset(&c, 0, sizeof c);
    bruto[0] = 0;
    if (!js_texto(p, f, "comment", bruto, sizeof bruto)) continue;
    if (js_bruto(p, f, "spoiler", spoiler, sizeof spoiler) &&
        !strcmp(spoiler, "true")) continue;
    // O Trakt manda o spoiler tambem EMBUTIDO ([spoiler]..[/spoiler]) com o
    // atributo em false; o texto escondido nunca pode aparecer sem pedir.
    if (tagSpoiler(bruto)) continue;
    if (linhaUnica(bruto, c.texto, sizeof c.texto) < PC_MINIMO) continue;
    js_texto(p, f, "username", c.autor, sizeof c.autor);
    nota = js_num(p, f, "user_rating", 0.0);
    c.nota = (nota >= 1.0 && nota <= 10.0) ? (int)nota : 0;
    nota = js_num(p, f, "likes", 0.0);
    c.curtidas = (nota >= 0.0 && nota <= 1000000000.0) ? (int)nota : 0;
    c.lingua[0] = 0;
    { char lg[16] = "";
      js_texto(p, f, "language", lg, sizeof lg);
      if (lg[0] >= 'a' && lg[0] <= 'z' && lg[1] >= 'a' && lg[1] <= 'z') { c.lingua[0] = lg[0]; c.lingua[1] = lg[1]; c.lingua[2] = 0; } }
    out[n++] = c;
  }
  return n;
}

// --- PEDIDO ------------------------------------------------------------------

// Copia `s` ate ':' (a chave composta "tt1234567:2:4" da lista de episodios
// traz o sufixo) e exige um id do IMDb de verdade: "tt" + digitos. Ids de
// addon/servidor pessoal (kitsu:, tmdb:, jf:, em:, px:) NUNCA vao ao Trakt —
// seriam lidos como slug de outro titulo (e vazariam o que se assiste).
static int imdbValido(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  if (!s || s[0] != 't' || s[1] != 't') return 0;
  for (; s[k] && s[k] != ':'; k++) {
    char c = s[k];
    int ok = k < 2 ? 1 : (c >= '0' && c <= '9');
    if (!ok || k + 1 >= tam) return 0;
    dst[k] = c;
  }
  if (k < 3) return 0;
  dst[k] = 0;
  return 1;
}

// Credencial como em serieaud.c: token quando ha, chave do aplicativo senao.
// Sem nenhuma, *semChave = 1 e nada sai da maquina.
static char *pedirCorpo(const Pedido *p, int *semChave) {
  const char *cab[4];
  char aut[200], chave[140], url[220];
  *semChave = 0;
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave) &&
      !trakt_cabecalhos_publicos(cab, chave, sizeof chave)) {
    *semChave = 1;
    return NULL;
  }
  if (p->ep > 0)
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/seasons/%d/episodes/%d/comments/likes?limit=10",
             p->imdb, p->temp, p->ep);
  else
    snprintf(url, sizeof url,
             "https://api.trakt.tv/movies/%s/comments/likes?limit=10", p->imdb);
  return rede_baixar_com(url, 15, cab);
}

// --- FIO ---------------------------------------------------------------------

// Um pedido por vez. Enquanto o fio busca, uma troca de chave so marca
// `mudou`; o laco pega a chave nova quando o pedido atual termina. Resultado
// de chave antiga e descartado pela geracao.
static void *buscar(void *arg) {
  (void)arg;
  for (;;) {
    Pedido p;
    PcComentario tmp[PC_MAX];
    unsigned long g;
    char *corpo;
    int n = 0, semChave = 0, caiu = 0;

    pthread_mutex_lock(&trava);
    if (!mudou) {
      fioVivo = 0;
      pthread_mutex_unlock(&trava);
      return NULL;
    }
    mudou = 0;
    p = pedido;
    g = geracao;
    pthread_mutex_unlock(&trava);

    corpo = pedirCorpo(&p, &semChave);
    caiu = corpo == NULL || !bemFormado(corpo);   // corte no meio ou pagina de erro: tenta de novo
    if (corpo) {
      n = pausacom_interpretar(corpo, tmp, PC_MAX);
      free(corpo);
    }
    if (!semChave) {
      printf("[pausacom] %s: %d comentarios\n", p.chave, n);
      fflush(stdout);
    }

    pthread_mutex_lock(&trava);
    if (g == geracao) {
      if (!semChave) tentativas++;
      aguarda = caiu;
      if (caiu) ultimaFalha = agora();
      if (n > 0) { ordenarIdioma(tmp, n, isoPref); memcpy(itens, tmp, sizeof(PcComentario) * (size_t)n); }
      nItens = n;
    }
    pthread_mutex_unlock(&trava);
  }
}

// Com a trava: marca `mudou` e cria o fio se nao ha um vivo. 0 quando o fio nao
// pode ser criado (nada fica marcado; quem chamou tenta de novo depois).
static int arrancarLocked(void) {
  mudou = 1;
  if (fioVivo) return 1;
  fioVivo = 1;
  if (pthread_create(&fio, NULL, buscar, NULL) == 0) { pthread_detach(fio); return 1; }
  fioVivo = 0;
  mudou = 0;
  return 0;
}

// --- API ---------------------------------------------------------------------

void pausacom_pedir(const char *imdb, int temporada, int episodio) {
  Pedido p;
  memset(&p, 0, sizeof p);
  if (!imdbValido(p.imdb, sizeof p.imdb, imdb)) { pausacom_limpar(); return; }
  if (episodio > 0 && temporada >= 0) {   // temporada 0 = especiais
    p.temp = temporada;
    p.ep = episodio;
    snprintf(p.chave, sizeof p.chave, "%s:%d:%d", p.imdb, temporada, episodio);
  } else {
    snprintf(p.chave, sizeof p.chave, "%s", p.imdb);
  }

  pthread_mutex_lock(&trava);
  if (!strcmp(p.chave, pedido.chave)) {
    // Repete so pedido que falhou: 30 s depois da falha, ate PC_TENTATIVAS redes.
    // Chamada por quadro, entao o caminho comum e so esta comparacao.
    if (aguarda && tentativas < PC_TENTATIVAS &&
        agora() - ultimaFalha >= PC_ESPERA_MS) {
      aguarda = 0;
      if (!arrancarLocked()) aguarda = 1;   // tenta de novo no proximo quadro
    }
    pthread_mutex_unlock(&trava);
    return;
  }
  pedido = p;
  nItens = 0;
  geracao++;
  aguarda = 0;
  tentativas = 0;
  ultimaFalha = 0;
  if (!arrancarLocked()) pedido.chave[0] = 0;   // tenta de novo no proximo quadro
  pthread_mutex_unlock(&trava);
}

int pausacom_n(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = nItens;
  pthread_mutex_unlock(&trava);
  return n;
}

int pausacom_obter(int i, PcComentario *out) {
  int ok = 0;
  if (!out) return 0;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nItens) { *out = itens[i]; ok = 1; }
  pthread_mutex_unlock(&trava);
  if (!ok) memset(out, 0, sizeof *out);
  return ok;
}

void pausacom_definir_idioma(const char *iso) {
  pthread_mutex_lock(&trava);
  isoPref[0] = 0;
  if (iso && iso[0] && iso[1]) { isoPref[0] = iso[0]; isoPref[1] = iso[1]; isoPref[2] = 0; }
  pthread_mutex_unlock(&trava);
}

void pausacom_limpar(void) {
  pthread_mutex_lock(&trava);
  memset(&pedido, 0, sizeof pedido);
  nItens = 0;
  mudou = 0;
  geracao++;
  aguarda = 0;
  tentativas = 0;
  ultimaFalha = 0;
  pthread_mutex_unlock(&trava);
}

void pausacom_relogio_teste(long long (*ms)(void)) {
  pthread_mutex_lock(&trava);
  relogioTeste = ms;
  pthread_mutex_unlock(&trava);
}
