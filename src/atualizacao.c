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
#include "ajustes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif

#define AT_URL   "https://api.github.com/repos/iqui27/nuvio-native-legacy/releases/latest"
#define AT_ARQ   "atualizacao-vista.txt"
#define AT_PAGINA "https://github.com/iqui27/nuvio-native-legacy/releases"
#define AT_APPID  "space.nuvio.native.legacy"
#define AT_LUNA_PUB "/usr/bin/luna-send-pub"
#define AT_LOG_INST "/tmp/nuvio-instalar.log"
#define AT_HB_DIR   "/media/developer/apps/usr/palm/applications/org.webosbrew.hbchannel"

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
// URL do .ipk da release. Vazia quando a release nao anexou um (ou quando este
// alvo nao sabe instalar, e ai nem se procura).
static char ipkUrl[512];
static char ipkHash[80];          // sha256 em hex; vazio quando a release nao diz

// INSTALAR DE DENTRO DO APP so existe no webOS, e a razao e de plataforma:
// aqui o app roda como ROOT (webosbrew) e alcanca o luna-send, que e quem fala
// com o appInstallService. No Tizen o .wgt vive num runtime de navegador
// isolado, sem API para instalar widget — la o cartao continua sendo so o
// aviso. No Mac nao ha o que instalar.
#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
#define AT_INSTALA 1
#else
#define AT_INSTALA 0
#endif

enum { AT_PARADO = 0, AT_BAIXANDO, AT_INSTALANDO, AT_PRONTO, AT_FALHOU };
// Quanto o instalador ja andou (0..100) e em que passo ele esta. Os dois saem
// do log do proprio servico, que publica `progress` e `statusText` a cada
// volta — sem isso a tela ficaria com uma frase parada por dois minutos, que e
// exatamente o tempo em que a pessoa acha que travou.
static float instPct;
static char  instPasso[48];
static int estado;
static int foco;                  // 0 = "Atualizar agora", 1 = "Depois"
static SDL_Thread *fioInst;

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

// O .ipk DENTRO DE assets[]. textoJson acha a PRIMEIRA ocorrencia de uma chave
// e serve para "tag_name"/"body", que sao da raiz; aqui a chave se repete uma
// vez por anexo (o .ipk e o .wgt) e o que decide e o SUFIXO. Por isso o laco:
// varre todas as ocorrencias e fica com a que termina em ".ipk".
static int acharIpk(const char *corpo, char *dst, size_t tam,
                   char *hash, size_t tamHash) {
  const char *p = corpo;
  const char *chave = "\"browser_download_url\":";
  dst[0] = 0;
  if (hash && tamHash) hash[0] = 0;
  while ((p = strstr(p, chave)) != NULL) {
    const char *ini;
    size_t n;
    p += strlen(chave);
    while (*p == ' ') p++;
    if (*p != '"') continue;
    ini = ++p;
    while (*p && *p != '"') p++;
    n = (size_t)(p - ini);
    if (n > 4 && n < tam && !strncmp(ini + n - 4, ".ipk", 4)) {
      memcpy(dst, ini, n); dst[n] = 0;
      // O SHA-256 DO MESMO ANEXO, e ele e obrigatorio na pratica: sem ele o
      // instalador do Homebrew Channel compara o hash calculado contra
      // `undefined` e responde `returnValue: false` com "Invalid file
      // checksum" — MEDIDO na C9, e o arquivo ate chegou a ser instalado, o
      // que e pior: sucesso reportado como falha.
      //
      // O campo vem ANTES do browser_download_url dentro do mesmo anexo (a
      // ordem do JSON do GitHub e ... size, digest, download_count, ...,
      // browser_download_url), entao a busca e PARA TRAS a partir da url. Ir
      // para frente pegaria o digest do anexo SEGUINTE.
      if (hash && tamHash) {
        const char *d = NULL, *q = corpo;
        while (q < ini) {
          const char *r = strstr(q, "\"digest\":");
          if (!r || r > ini) break;
          d = r; q = r + 8;
        }
        if (d) {
          d = strchr(d + 8, '"');
          if (d) {
            const char *e;
            d++;
            if (!strncmp(d, "sha256:", 7)) d += 7;   // so o hex interessa
            e = strchr(d, '"');
            if (e && (size_t)(e - d) < tamHash) {
              memcpy(hash, d, (size_t)(e - d)); hash[e - d] = 0;
            }
          }
        }
      }
      return 1;
    }
  }
  return 0;
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
    if (AT_INSTALA) acharIpk(corpo, ipkUrl, sizeof ipkUrl, ipkHash, sizeof ipkHash);
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

// MANDA O SISTEMA INSTALAR o .ipk da release.
//
// MEDIDO NA C9, e foi o que derrubou a primeira versao disto: o app NAO roda
// como root. O arquivo que ele grava sai com uid 5152, e `/usr/bin/luna-send`
// e `-rwx------ root root` — a chamada morria com "can't execute 'luna-send':
// Permission denied" no log do nohup, depois de ja ter baixado 36 MB. Root
// nesta TV e o que EU tenho por SSH; o app continua no jail.
//
// O que o app alcanca: `/usr/bin/luna-send-pub` (-rwxr-xr-x) e, por ele, o
// servico do Homebrew Channel, que ESTE roda como root e existe justamente
// para instalar ipk. Ele tem os metodos install/uninstall/exec/spawn, e o
// install aceita `ipkUrl` — entao passamos a URL da release direto e nem
// baixamos: quem baixa e ele, sem 36 MB passando pelo nosso heap.
//
// Sem o Homebrew Channel instalado nao ha caminho nenhum, e o cartao volta a
// ser so aviso (ver podeInstalar).
static int fioInstalar(void *arg) {
  char cmd[900];
  (void)arg;
  { char extra[110] = "";
    if (ipkHash[0]) snprintf(extra, sizeof extra, ",\"ipkHash\":\"%s\"", ipkHash);
    snprintf(cmd, sizeof cmd,
      "nohup %s -i -f luna://org.webosbrew.hbchannel.service/install "
      "'{\"ipkUrl\":\"%s\"%s,\"subscribe\":true}' "
      "> %s 2>&1 &", AT_LUNA_PUB, ipkUrl, extra, AT_LOG_INST); }
  printf("[atualizacao] instalando %s\n", ipkUrl);
  fflush(stdout);
  if (system(cmd) != 0) {
    SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
    printf("[atualizacao] o instalador nao aceitou o pedido\n"); fflush(stdout);
    return 0;
  }
  // ACOMPANHA O LOG. O servico responde por subscribe e o luna-send vai
  // escrevendo cada resposta no arquivo; ler o arquivo e mais simples e mais
  // robusto do que abrir o barramento aqui — e o arquivo tem poucos KB.
  //
  // Teto de 8 minutos: sao ~36 MB numa TV, e passar disso e sinal de que a
  // resposta nao vem mais. Sem teto o fio ficaria vivo para sempre.
  { Uint32 ate = SDL_GetTicks() + 8 * 60 * 1000;
    while (SDL_GetTicks() < ate) {
      FILE *f = fopen(AT_LOG_INST, "rb");
      SDL_Delay(300);
      if (!f) continue;
      { static char buf[8192];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        const char *p, *ult;
        float pct = -1.0f;
        char passo[48] = "";
        int fim = 0, erro = 0;
        fclose(f);
        buf[n] = 0;
        // O ARQUIVO CRESCE, entao o que vale e a ULTIMA ocorrencia de cada
        // campo — a primeira e o comeco do download, e ficaria congelada.
        for (p = buf, ult = NULL; (p = strstr(p, "\"progress\":")) != NULL; p += 11) ult = p;
        if (ult) pct = (float)atof(ult + 11);
        for (p = buf, ult = NULL; (p = strstr(p, "\"statusText\":")) != NULL; p += 13) ult = p;
        if (ult) {
          const char *ini = strchr(ult + 13, '"');
          if (ini) {
            const char *e = strchr(++ini, '"');
            size_t k = e ? (size_t)(e - ini) : 0;
            if (k && k < sizeof passo) { memcpy(passo, ini, k); passo[k] = 0; }
          }
        }
        if (strstr(buf, "\"finished\": true") || strstr(buf, "\"finished\":true")) fim = 1;
        if (strstr(buf, "\"errorText\"")) erro = 1;
        SDL_LockMutex(mtx);
        if (pct >= 0.0f) instPct = pct;
        if (passo[0]) snprintf(instPasso, sizeof instPasso, "%s", passo);
        if (fim)       estado = AT_PRONTO;
        else if (erro) estado = AT_FALHOU;
        SDL_UnlockMutex(mtx);
        if (fim || erro) {
          printf("[atualizacao] instalador terminou: %s\n", fim ? "ok" : "falhou");
          fflush(stdout);
          return 0;
        } } } }
  SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
  printf("[atualizacao] instalador nao respondeu no prazo\n"); fflush(stdout);
  return 0;
}

// 1 quando existe o que instalar: alvo que sabe, release com .ipk anexado e
// nenhuma instalacao em andamento.
// O Homebrew Channel precisa ESTAR no aparelho: sem ele o servico nao existe e
// o botao prometeria o que nao acontece. Conferido uma vez, no primeiro uso.
static int temInstalador(void) {
  static int visto = -1;
  FILE *f;
  if (visto >= 0) return visto;
  f = fopen(AT_HB_DIR "/appinfo.json", "r");
  visto = f != NULL;
  if (f) fclose(f);
  return visto;
}

static int podeInstalar(void) {
  return AT_INSTALA && ipkUrl[0] && estado == AT_PARADO && temInstalador();
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

// Fecha e ANOTA a versao vista: o cartao e uma vez por tag.
static void fechar(void) {
  char s[40];
  aberto = 0;
  snprintf(s, sizeof s, "%s\n", tagNova);
  dados_gravar(AT_ARQ, s);
}

void atualizacao_abrir(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (tagNova[0]) { aberto = 1; mostrado = 1; foco = 0; }
  SDL_UnlockMutex(mtx);
}

void atualizacao_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // VOLTAR sempre fecha, inclusive durante o download: quem desistiu no meio
  // nao fica preso olhando uma barra. O fio termina sozinho e, se chegar a
  // instalar, o sistema mata o app de qualquer jeito.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { fechar(); return; }
  if (estado == AT_INSTALANDO) return;
  if (podeInstalar() && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
    foco = k == SDLK_LEFT ? 0 : 1;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
      k == SDLK_DELETE) {
    if (podeInstalar() && foco == 0) {
      SDL_Thread *t;
      SDL_LockMutex(mtx); estado = AT_INSTALANDO; SDL_UnlockMutex(mtx);
      t = SDL_CreateThread(fioInstalar, "nv-instalar", NULL);
      if (t) { SDL_DetachThread(t); fioInst = t; }
      else { SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx); }
      return;
    }
    fechar();
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

  // RODAPE. Onde ha como instalar, ele vira dois botoes; onde nao ha, continua
  // sendo o endereco da pagina, que e a unica coisa util a dizer.
  y = AT_Y + dy + AT_H - 96.0f;
  if (estado == AT_INSTALANDO || estado == AT_PRONTO) {
    // BARRA E PORCENTAGEM, e nao uma frase parada. O numero e o passo vem do
    // proprio instalador (progress/statusText); enquanto ele nao disse nada a
    // barra fica vazia em vez de inventar movimento.
    float pct, larg = w * 0.62f;
    char passo[48];
    SDL_LockMutex(mtx);
    pct = estado == AT_PRONTO ? 100.0f : instPct;
    snprintf(passo, sizeof passo, "%s", instPasso);
    SDL_UnlockMutex(mtx);
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    { const char *msg = estado == AT_PRONTO
        ? i18n("Atualizado. Feche e abra o app para usar.")
        : (!strncmp(passo, "Verif", 5) ? i18n("Conferindo o arquivo...")
        : (!strncmp(passo, "Install", 7) ? i18n("Instalando...")
        :  i18n("Baixando a atualização...")));
      TxtLinha t = txt_linha(TXT_CALLOUT, msg, 232, 236, 246, 255);
      txt_desenhar_alpha(t, x, y, a); }
    { GfxRect trilho = { x, y + 46.0f, larg, 10.0f };
      GfxRect cheio  = { x, y + 46.0f, larg * (pct / 100.0f), 10.0f };
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      gfx_cor(trilho, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, 0.14f * a);
      // Menos de meia altura de barra nao desenha: um retangulo de 2 px com
      // canto arredondado vira um pontinho torto no canto esquerdo.
      if (cheio.w > 12.0f) gfx_cor(cheio, NV_RAIO_PILL, ar, ag, ab, a);
      { char n[16];
        TxtLinha t;
        snprintf(n, sizeof n, "%d%%", (int)(pct + 0.5f));
        t = txt_linha(TXT_CAPTION, n, 200, 204, 214, 255);
        txt_desenhar_alpha(t, x + larg + 20.0f, y + 40.0f, a * 0.95f); } }
  } else if (podeInstalar()) {
    const char *rot[2];
    float bx = x;
    int i;
    rot[0] = i18n("Atualizar agora");
    rot[1] = i18n("Depois");
    for (i = 0; i < 2; i++) {
      int fc = (i == foco);
      // Pilula clara com texto escuro no foco, como o menu de cartaz e a folha
      // de envio — e o vocabulario dos modais deste app, e o cartao e um.
      int cor = fc ? 17 : 236;
      TxtLinha t = txt_linha(TXT_CALLOUT, rot[i], cor, cor, cor, 255);
      GfxRect b = { bx, y, t.w + 64.0f, 64.0f };
      float lum = fc ? 0.961f : 0.176f;
      gfx_cor(b, NV_RAIO_PILL, lum, lum, lum, a);
      txt_desenhar_alpha(t, bx + 32.0f, y + (64.0f - t.h) * 0.5f, a);
      bx += b.w + 16.0f;
    }
  } else {
    TxtLinha t = txt_linha(TXT_CAPTION,
        estado == AT_FALHOU ? i18n("Não foi possível atualizar por aqui.") : AT_PAGINA,
        176, 180, 190, 255);
    txt_desenhar_alpha(t, x, y + 16.0f, a * 0.9f);
  }
  if (estado != AT_INSTALANDO && estado != AT_PRONTO) {
    TxtLinha t = txt_linha(TXT_CAPTION2,
        podeInstalar() ? i18n("Voltar para fechar") : i18n("OK para fechar"),
        150, 154, 165, 255);
    txt_desenhar_alpha(t, AT_X + AT_W - AT_PAD - t.w, y + 24.0f, a * 0.85f);
  }
  gfx_sem_recorte();
}
