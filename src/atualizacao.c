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
#include "botoes.h"
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
// Linhas de NOTAS guardadas (secao, paragrafo ou item). Era 14, e com as notas
// sem rolagem isso nao importava: o cartao ja cortava antes. Agora a area das
// notas rola, entao o teto so protege o buffer.
#define AT_LINHAS_MAX  48
#define AT_LEADING    34.0f
// RODAPE FIXO: os botoes (ou o endereco da pagina) moram nos ultimos
// AT_RODAPE_H px do cartao e as notas NUNCA descem ate la. Ver atualizacao_desenhar.
#define AT_RODAPE_H  132.0f
// Um aperto de cima/baixo rola tres linhas de notas.
#define AT_PASSO     (AT_LEADING * 3.0f)
// Linhas por item. Com rolagem nao ha por que cortar um item no meio; o teto
// so impede que um paragrafo gigante vire uma parede.
#define AT_ITEM_LINHAS 8

static SDL_mutex *mtx;
static SDL_Thread *fio;
static int disparado, pronto, aberto, mostrado;
static float entrada;
static char tagNova[32];          // "1.0.54", vazio se nao ha nada mais novo
static char notas[6144];          // texto ja limpo, linhas separadas por \n
// URL do .ipk da release. Vazia quando a release nao anexou um (ou quando este
// alvo nao sabe instalar, e ai nem se procura).
static char ipkUrl[512];
static char ipkHash[80];          // sha256 em hex; vazio quando a release nao diz

// INSTALAR DE DENTRO DO APP so existe no webOS, e a razao e de plataforma:
// aqui o app roda como ROOT (webosbrew) e alcanca o luna-send, que e quem fala
// com o appInstallService. No Tizen o .wgt vive num runtime de navegador
// isolado, sem API para instalar widget — la o cartao continua sendo so o
// aviso. No Mac nao ha o que instalar.
//
// A CAPTURA PRECISA DO CASO DA LG RODANDO NO MAC. Este cartao so existe quando
// ha versao nova, e o ramo com botoes e barra so existe onde ha instalador —
// ou seja, fotografa-lo de verdade exigiria segurar uma release, uma TV e o
// Homebrew Channel ao mesmo tempo. NV_AT_INSTALA e o unico jeito de o harness
// alcancar esse ramo; ele NAO e definido por nenhum build de produto (ver
// tools/env.sh e tools/arm.sh), so por tests/atualizacao_shot.sh.
#if defined(NV_AT_INSTALA)
#define AT_INSTALA NV_AT_INSTALA
#elif !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
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
// ROLAGEM DAS NOTAS. Relato de mackojanko (Samsung Tizen 6.0, 1.4.5): "quando
// o aviso aparece nao consigo descer e nao vejo o botao de atualizar". As
// notas da 1.4.4 e da 1.4.5 sao longas, e o laco de desenho so parava de COMECAR itens perto do
// rodape — o ultimo item, com ate 3 linhas, descia por cima dos botoes (e, no
// Tizen, por cima do endereco e do "OK para fechar"), e cima/baixo nao faziam
// nada. Agora o rodape e fixo, as notas ficam recortadas na area de cima e
// cima/baixo rolam essa area. `rolarAlvo` e para onde o controle mandou,
// `rolar` e onde o desenho esta (anda ate o alvo); `notasH` e a altura do texto
// inteiro medida no quadro anterior e `vistaH` a da janela visivel.
static float rolar, rolarAlvo, notasH, vistaH;
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
// SUFIXO DO ANEXO QUE ESTA BUILD DEVE BAIXAR.
//
// Desde a v1.1.0 a release traz DOIS .ipk — o normal e o "-highcache", que so
// muda o teto de textura (NV_TEX_MB_FIXO). O id do pacote e a versao sao
// IGUAIS nos dois, entao instalar um por cima do outro troca a variante sem
// dizer nada.
//
// E era isso que acontecia: acharIpk devolvia o PRIMEIRO anexo terminado em
// ".ipk", e o GitHub lista em ordem alfabetica, onde "_arm-highcache.ipk" vem
// antes de "_arm.ipk" ('-' e menor que '.'). Ou seja, "Atualizar agora"
// instalava a highcache em TODA LG, inclusive em quem nunca a escolheu.
//
// DOIS NOMES POR VARIANTE, desde 20/09/2026. As releases 1.3.1 e 1.3.2 subiram
// os anexos como "NuvioTV-1.3.N-webos.ipk" / "-webos-highcache.ipk" — nome
// mais legivel, mas "-webos.ipk" nao termina em "_arm.ipk", entao toda LG
// normal ficou SEM o botao "Atualizar agora" por duas versoes (a highcache
// nao sentiu: "-webos-highcache.ipk" ainda termina em "-highcache.ipk"). A
// 1.3.3 volta ao nome de contrato, e este codigo passa a aceitar os dois para
// o proximo nome bonito nao quebrar de novo. A regra que nao muda: a normal
// NUNCA casa com um nome que tenha "highcache".
#ifdef NV_TEX_MB_FIXO
#  define AT_SUFIXO "-highcache.ipk"
#  define AT_SUFIXO2 "-highcache.ipk"
#else
#  define AT_SUFIXO "_arm.ipk"
#  define AT_SUFIXO2 "-webos.ipk"
#endif

static int terminaEm(const char *s, size_t n, const char *sufixo) {
  size_t k = strlen(sufixo);
  if (n >= k && !strncmp(s + n - k, sufixo, k)) return 1;
  k = strlen(AT_SUFIXO2);
  return n >= k && !strncmp(s + n - k, AT_SUFIXO2, k);
}

// `sufixo` obrigatorio. NAO ha reserva para "qualquer .ipk": uma release sem o
// anexo desta variante e motivo para NAO oferecer o botao e mandar a pessoa
// para a pagina — trocar de variante calada e justamente o defeito.
static int acharIpk(const char *corpo, char *dst, size_t tam,
                   char *hash, size_t tamHash, const char *sufixo) {
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
    if (n > 4 && n < tam && terminaEm(ini, n, sufixo)) {
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
    // Sem anexo da variante desta build, ipkUrl fica vazio e podeInstalar()
    // devolve 0: o cartao aparece so com a URL da pagina.
    if (AT_INSTALA) acharIpk(corpo, ipkUrl, sizeof ipkUrl, ipkHash, sizeof ipkHash, AT_SUFIXO);
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
#if defined(NV_AT_INSTALA)
  // No harness nao ha Homebrew Channel em disco para achar; quem forcou
  // AT_INSTALA esta dizendo justamente "encene a TV que tem".
  return NV_AT_INSTALA;
#endif
  if (visto >= 0) return visto;
  f = fopen(AT_HB_DIR "/appinfo.json", "r");
  visto = f != NULL;
  if (f) fclose(f);
  return visto;
}

static int podeInstalar(void) {
  return AT_INSTALA && ipkUrl[0] && estado == AT_PARADO && temInstalador();
}

// Quanto da para rolar: o que sobra das notas alem da janela. 0 = cabe tudo.
static float rolarMax(void) {
  float m = notasH - vistaH;
  return m > 0.0f ? m : 0.0f;
}

// Cada abertura comeca do topo das notas e com o foco no botao de atualizar.
static void reiniciarVista(void) {
  foco = 0;
  rolar = rolarAlvo = 0.0f;
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
  reiniciarVista();
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
  if (tagNova[0]) { aberto = 1; mostrado = 1; reiniciarVista(); }
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
  // CIMA/BAIXO ROLAM AS NOTAS, em qualquer estado (inclusive baixando): o
  // rodape nao depende delas, entao rolar nunca tira o botao da tela.
  if (k == SDLK_UP || k == SDLK_DOWN) {
    rolarAlvo += k == SDLK_DOWN ? AT_PASSO : -AT_PASSO;
    if (rolarAlvo > rolarMax()) rolarAlvo = rolarMax();
    if (rolarAlvo < 0.0f) rolarAlvo = 0.0f;
    return;
  }
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
  // Rolagem suave, mas curta (~120 ms para chegar): quem segura a seta quer
  // ver o texto andar, nao esperar.
  { float d = rolarAlvo - rolar, f = dt * 1000.0f / 120.0f;
    if (f > 1.0f) f = 1.0f;
    rolar = (d > -0.5f && d < 0.5f) ? rolarAlvo : rolar + d * f; }
}

void atualizacao_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y, x, w;
  char buf[160];
  const char *p;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido e UMA luz difusa na cor de
  // realce entrando pelo canto superior esquerdo, presa aos cantos do cartao
  // (GFX_LUZ). Com o veu de tela cheia ja pago, a luz e a segunda e ultima
  // camada grande desta tela.
  { GfxRect c = { AT_X, AT_Y + dy, AT_W, AT_H };
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_cor(c, 28.0f / AT_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(c, 28.0f / AT_H, AT_H * 0.1f, -AT_H * 0.1f, AT_H * 0.65f, ar, ag, ab, 0.22f * a);
    gfx_recorte(AT_X, AT_Y + dy, AT_W, AT_H); }

  x = AT_X + AT_PAD; w = AT_W - 2.0f * AT_PAD;
  y = AT_Y + dy + 56.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("ATUALIZAÇÃO DISPONÍVEL"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.92f); }
  y += 30.0f;
  snprintf(buf, sizeof buf, i18n("Nuvio %s"), tagNova);
  { TxtLinha t = txt_linha(TXT_TITULO1, buf, 246, 247, 252, 255);
    txt_desenhar_alpha(t, x, y, a); y += t.h + 6.0f; }
  // A VARIANTE NO CARTAO. Sem isto a pessoa le "Você está na 1.1.2" e vai para
  // uma pagina com dois .ipk sem saber qual e o dela.
#ifdef NV_TEX_MB_FIXO
  snprintf(buf, sizeof buf, i18n("Você está na %s · cache grande"), NV_VERSAO);
#else
  snprintf(buf, sizeof buf, i18n("Você está na %s"), NV_VERSAO);
#endif
  { TxtLinha t = txt_linha(TXT_CAPTION, buf, 176, 180, 190, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); y += t.h + 28.0f; }

  // NOTAS, linha a linha; \x01 marca secao. Janela fixa entre o cabecalho e o
  // rodape, recortada: o que nao cabe fica para a rolagem, nunca por cima dos
  // botoes. A altura total sai do proprio desenho e vale para o quadro
  // seguinte (o texto nao muda enquanto o cartao esta aberto).
  { float topo = y, base = AT_Y + dy + AT_H - AT_RODAPE_H, y0;
  vistaH = base - topo;
  if (rolarAlvo > rolarMax()) rolarAlvo = rolarMax();
  if (rolar > rolarMax()) rolar = rolarMax();
  gfx_recorte(AT_X, topo, AT_W, vistaH);
  y = y0 = topo - rolar;
  p = notas;
  while (*p) {
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
                     a * 0.95f, AT_ITEM_LINHAS) + 10.0f;
    }
  }
  notasH = y - y0;
  // DICA DE QUE HA MAIS: o fim da janela some no fundo do cartao (em vez de
  // cortar uma linha ao meio) e uma trilha fina a direita diz onde se esta.
  if (rolarMax() > 0.5f) {
    float ar, ag, ab, tH = vistaH - 16.0f, pH, pY;
    if (rolar < rolarMax() - 0.5f) {
      GfxRect veu = { AT_X, base - 72.0f, AT_W, 72.0f };
      gfx_rect(veu, 0, GFX_VEU_BAIXO, 0, 0, 0, 0.0f, 0.055f, 0.058f, 0.068f, a);
    }
    gfx_recorte(AT_X, AT_Y + dy, AT_W, AT_H);
    ajustes_acento(&ar, &ag, &ab);
    pH = tH * vistaH / notasH;
    if (pH < 40.0f) pH = 40.0f;
    pY = topo + 8.0f + (tH - pH) * (rolar / rolarMax());
    // O raio do SDF e relativo a ALTURA: NV_RAIO_PILL numa trilha em pe vira
    // uma lente. Meia largura sobre a altura e que da a pilula.
    gfx_cor((GfxRect){ AT_X + AT_W - 30.0f, topo + 8.0f, 6.0f, tH },
            3.0f / tH, 1.0f, 1.0f, 1.0f, 0.12f * a);
    gfx_cor((GfxRect){ AT_X + AT_W - 30.0f, pY, 6.0f, pH },
            3.0f / pH, ar, ag, ab, 0.9f * a);
  }
  gfx_recorte(AT_X, AT_Y + dy, AT_W, AT_H); }

  // RODAPE. Onde ha como instalar, ele vira dois botoes; onde nao ha, continua
  // sendo o endereco da pagina, que e a unica coisa util a dizer. Posicao FIXA,
  // abaixo da janela das notas: por mais longas que elas sejam, o botao esta
  // sempre na tela.
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
    // A PILULA DA TABELA (botoes.h): "Atualizar agora" e o primario (72 px,
    // cheio), "Depois" o secundario (56 px, contorno), alinhados pela base.
    for (i = 0; i < 2; i++) {
      int primario = (i == 0);
      float h = primario ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
      GfxRect b = { bx, y + (BOTAO_H_PRIMARIO - h), botao_largura(rot[i], NULL, primario), h };
      botao_pilula(b, rot[i], NULL, i == foco ? 1.0f : 0.0f, primario, 0, a);
      bx += b.w + BOTAO_GAP;
    }
  } else {
    TxtLinha t = txt_linha(TXT_CAPTION,
        estado == AT_FALHOU ? i18n("Não foi possível atualizar por aqui.") : AT_PAGINA,
        176, 180, 190, 255);
    txt_desenhar_alpha(t, x, y + 16.0f, a * 0.9f);
  }
  if (estado != AT_INSTALANDO && estado != AT_PRONTO) {
    int mais = rolarMax() > 0.5f;
    TxtLinha t = txt_linha(TXT_CAPTION2,
        podeInstalar()
          ? (mais ? i18n("↑ ↓  Mais notas   ·   Voltar para fechar") : i18n("Voltar para fechar"))
          : (mais ? i18n("↑ ↓  Mais notas   ·   OK para fechar") : i18n("OK para fechar")),
        150, 154, 165, 255);
    txt_desenhar_alpha(t, AT_X + AT_W - AT_PAD - t.w, y + 24.0f, a * 0.85f);
  }
  gfx_sem_recorte();
}
