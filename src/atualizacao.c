// Cartao de ATUALIZACAO — "saiu a 1.0.54, e isto e o que mudou".
//
// POR QUE EXISTE: o app se instala a mao (.ipk pelo Homebrew Channel, .wgt
// assinado pelo proprio usuario) e nao ha loja que avise. Quem instalou a
// 1.0.50 continua nela ate ler o GitHub por conta propria — e os relatos de
// defeito ja corrigido ("still doing the same thing") sao em parte isso.
//
// DE ONDE VEM: a release mais recente do repositorio, pela API publica do
// GitHub (60 consultas por hora por IP sem token — uma por abertura do app
// nao chega perto). `tag_name` diz a versao, `body` sao as notas em Markdown.
// As notas sao mostradas como texto: titulos "##" viram linhas de secao,
// "- **x**" vira "• x", o resto da marcacao cai. A secao "## Notes" (como
// instalar) e o que vem depois dela nao entram: e boilerplate de toda release.
//
// UMA VEZ POR VERSAO: o arquivo-marca guarda a tag mostrada. Nova release,
// nova tag, novo cartao; a mesma nao volta.
//
// O irmao e novidades.c: mesmo cartao central, mesma regra de fechamento.
#include "atualizacao.h"
#include "dados.h"
#include "rede.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif

#define AT_URL   "https://api.github.com/repos/iqui27/nuvio-native-legacy/releases/latest"
#define AT_ARQ   "atualizacao-vista.txt"
#define AT_PAGINA "https://github.com/iqui27/nuvio-native-legacy/releases"

#define AT_W        1240.0f
#define AT_H         760.0f
#define AT_X        ((NV_TELA_W - AT_W) * 0.5f)
#define AT_Y        ((NV_TELA_H - AT_H) * 0.5f)
#define AT_PAD        64.0f
#define AT_ABRIR_MS   280.0f
#define AT_FECHAR_MS  160.0f
#define AT_LINHAS_MAX  14
#define AT_LEADING    34.0f

static SDL_mutex *mtx;
static SDL_Thread *fio;
static int disparado, pronto, aberto, mostrado;
static float entrada;
static char tagNova[32];          // "1.0.54", vazio se nao ha nada mais novo
static char notas[4096];          // texto ja limpo, linhas separadas por \n

const char *atualizacao_nova(void) { return tagNova; }
int atualizacao_aberta(void) { return aberto; }

// "1.0.54" > "1.0.53"? Compara numero a numero; o que nao e numero vale 0.
static int maisNova(const char *a, const char *b) {
  while (*a || *b) {
    long na = strtol(a, (char **)&a, 10), nb = strtol(b, (char **)&b, 10);
    if (na != nb) return na > nb;
    if (*a == '.') a++;
    if (*b == '.') b++;
    if (!*a && !*b) break;
    if ((*a && *a != '.' && (*a < '0' || *a > '9')) ||
        (*b && *b != '.' && (*b < '0' || *b > '9'))) break;
  }
  return 0;
}

// Copia o valor da string JSON `chave` decodificando escapes de verdade —
// js_texto troca \n por espaco, e aqui a QUEBRA DE LINHA e a estrutura das
// notas. \uXXXX vira espaco (emoji nas notas nao merece decodificador UTF-16).
static int textoJson(const char *corpo, const char *chave, char *dst, size_t tam) {
  char busca[64];
  const char *p;
  size_t k = 0;
  snprintf(busca, sizeof busca, "\"%s\":", chave);
  p = strstr(corpo, busca);
  if (!p) return 0;
  p += strlen(busca);
  while (*p == ' ') p++;
  if (*p != '"') return 0;
  p++;
  while (*p && *p != '"' && k + 1 < tam) {
    if (*p == '\\') {
      p++;
      if (*p == 'n') dst[k++] = '\n';
      else if (*p == 'r' || *p == 't') { /* nada */ }
      else if (*p == 'u') { int q; dst[k++] = ' '; for (q = 0; q < 4 && p[1]; q++) p++; }
      else if (*p) dst[k++] = *p;
      if (*p) p++;
      continue;
    }
    dst[k++] = *p++;
  }
  dst[k] = 0;
  return 1;
}

// Markdown das notas -> linhas de tela. Devolve em `dst`, linhas por \n.
static void limparNotas(const char *md, char *dst, size_t tam) {
  size_t k = 0;
  const char *p = md;
  int linhas = 0;
  while (*p && k + 2 < tam && linhas < AT_LINHAS_MAX) {
    const char *fim = strchr(p, '\n');
    size_t n = fim ? (size_t)(fim - p) : strlen(p);
    char linha[1024];
    size_t i, j = 0;
    if (n >= sizeof linha) n = sizeof linha - 1;
    memcpy(linha, p, n); linha[n] = 0;
    p = fim ? fim + 1 : p + n;
    // recorta espaco a direita
    while (n > 0 && (linha[n - 1] == ' ' || linha[n - 1] == '\r')) linha[--n] = 0;
    if (!n) continue;
    if (!strncmp(linha, "---", 3)) break;
    if (linha[0] == '#') {
      const char *t = linha;
      while (*t == '#') t++;
      while (*t == ' ') t++;
      // "Notes" e o rodape fixo de toda release: instalar, assinar.
      if (!strncmp(t, "Notes", 5) || !strncmp(t, "Notas", 5)) break;
      j = (size_t)snprintf(linha, sizeof linha, "\x01%s", t);   // \x01 = secao
      if (j >= sizeof linha) j = sizeof linha - 1;
    } else {
      char lim[1024];
      const char *s = linha;
      if (*s == '-' || *s == '*') { s++; while (*s == ' ') s++; j = (size_t)snprintf(lim, sizeof lim, "\xe2\x80\xa2 "); }
      for (i = 0; s[i] && j + 4 < sizeof lim; i++) {
        if (s[i] == '*' || s[i] == '`') continue;
        lim[j++] = s[i];
      }
      lim[j] = 0;
      memcpy(linha, lim, j + 1);
    }
    if (k + j + 1 >= tam) break;
    memcpy(dst + k, linha, j); k += j;
    dst[k++] = '\n';
    linhas++;
  }
  dst[k] = 0;
}

static int fioConsulta(void *arg) {
  char *corpo;
  char tag[48] = "", body[8192] = "";
  (void)arg;
  corpo = rede_baixar(AT_URL, 12);
  if (!corpo) { printf("[atualizacao] sem resposta do GitHub\n"); fflush(stdout); }
  else {
    textoJson(corpo, "tag_name", tag, sizeof tag);
    textoJson(corpo, "body", body, sizeof body);
    free(corpo);
  }
  SDL_LockMutex(mtx);
  if (tag[0]) {
    const char *v = tag[0] == 'v' ? tag + 1 : tag;
    if (maisNova(v, NV_VERSAO)) {
      snprintf(tagNova, sizeof tagNova, "%s", v);
      limparNotas(body, notas, sizeof notas);
    }
    printf("[atualizacao] instalada %s, no GitHub %s%s\n", NV_VERSAO, v,
           tagNova[0] ? " -- NOVA" : "");
    fflush(stdout);
  }
  pronto = 1;
  SDL_UnlockMutex(mtx);
  return 0;
}

void atualizacao_verificar(void) {
  if (disparado) return;
  disparado = 1;
  if (!mtx) mtx = SDL_CreateMutex();
  fio = SDL_CreateThread(fioConsulta, "nv-atualizacao", NULL);
  if (fio) SDL_DetachThread(fio);
}

void atualizacao_mostrar_se_houver(void) {
  char *visto;
  if (mostrado || aberto) return;
  SDL_LockMutex(mtx);
  if (!pronto || !tagNova[0]) { SDL_UnlockMutex(mtx); return; }
  SDL_UnlockMutex(mtx);
  mostrado = 1;
  visto = dados_ler(AT_ARQ);
  if (visto) {
    int igual = !strncmp(visto, tagNova, strlen(tagNova)) &&
                (visto[strlen(tagNova)] == '\n' || visto[strlen(tagNova)] == 0);
    free(visto);
    if (igual) return;
  }
  aberto = 1;
}

void atualizacao_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
      k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    char s[40];
    aberto = 0;
    snprintf(s, sizeof s, "%s\n", tagNova);
    dados_gravar(AT_ARQ, s);
  }
}

void atualizacao_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? AT_ABRIR_MS : AT_FECHAR_MS);
}

void atualizacao_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y, x, w;
  char buf[160];
  const char *p;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  { GfxRect c = { AT_X, AT_Y + dy, AT_W, AT_H };
    gfx_cor(c, 0.030f, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  gfx_recorte(AT_X, AT_Y + dy, AT_W, AT_H);

  x = AT_X + AT_PAD; w = AT_W - 2.0f * AT_PAD;
  y = AT_Y + dy + 56.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("ATUALIZAÇÃO DISPONÍVEL"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.92f); }
  y += 30.0f;
  snprintf(buf, sizeof buf, i18n("Nuvio %s"), tagNova);
  { TxtLinha t = txt_linha(TXT_TITULO1, buf, 246, 247, 252, 255);
    txt_desenhar_alpha(t, x, y, a); y += t.h + 6.0f; }
  snprintf(buf, sizeof buf, i18n("Você está na %s"), NV_VERSAO);
  { TxtLinha t = txt_linha(TXT_CAPTION, buf, 176, 180, 190, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); y += t.h + 28.0f; }

  // notas, linha a linha; \x01 marca secao
  p = notas;
  while (*p && y < AT_Y + dy + AT_H - 120.0f) {
    const char *fim = strchr(p, '\n');
    size_t n = fim ? (size_t)(fim - p) : strlen(p);
    char linha[512];
    if (n >= sizeof linha) n = sizeof linha - 1;
    memcpy(linha, p, n); linha[n] = 0;
    p = fim ? fim + 1 : p + n;
    if (linha[0] == '\x01') {
      y += 10.0f;
      { TxtLinha t = txt_linha(TXT_CALLOUT, i18n(linha + 1), 246, 247, 252, 255);
        txt_desenhar_alpha(t, x, y, a); y += t.h + 10.0f; }
    } else {
      y += txt_bloco(TXT_BODY, linha, 200, 204, 214, x, y, w, AT_LEADING,
                     a * 0.95f, 3) + 10.0f;
    }
  }

  // rodape: onde baixar, e como fechar
  y = AT_Y + dy + AT_H - 88.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION, AT_PAGINA, 176, 180, 190, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); }
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("OK para fechar"), 150, 154, 165, 255);
    txt_desenhar_alpha(t, AT_X + AT_W - AT_PAD - t.w, y + 4.0f, a * 0.85f); }
  gfx_sem_recorte();
}
