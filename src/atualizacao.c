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
#include "horafmt.h"
#include "app_id.h"
#include "atualizacao.h"
#include "ilha.h"
#include "dados.h"
#include "rede.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "idiomacod.h"
#include "plrui.h"
#include "qr.h"
#ifdef NV_ANDROID
#include "android.h"
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif

// ===================== AUTO-ATUALIZACAO DO .tpk (SO NV_TPK) =====================
// Numa TV Samsung o Nuvio nativo E a libnuvio.so (UI, catalogo, player, sync); o
// host .NET quase nunca muda. Este bloco deixa o app se atualizar SEM o usuario
// reinstalar o .tpk: quando a release anexa uma libnuvio.so mais nova para esta
// ABI, baixamos por HTTPS, CONFERIMOS o sha256 e so entao ENCENAMOS o arquivo em
// data/ (staging). O host, no proximo arranque, memfd-carrega a .so encenada em
// vez da empacotada (Program.cs / Program40.cs).
//
// SEGURANCA: isto carrega CODIGO NATIVO REMOTO. A unica barreira e o sha256, e
// ele vem do MESMO lugar da checagem de versao — o campo `digest` do anexo no
// JSON da release do GitHub, por HTTPS (AT_URL). E o mesmo campo ja usado para o
// ipkHash da LG. Nunca carregamos em processo aqui: so encenamos; quem carrega
// e o host, no proximo arranque, e so depois de reconferir o hash do arquivo
// encenado. Em QUALQUER duvida o host apaga o staging e volta para a empacotada.
// No Android o mesmo SHA-256 confere o APK baixado (auto-atualizacao do .apk).
#if defined(NV_TPK) || defined(NV_ANDROID)
#include <stdint.h>
#include <unistd.h>
#include <strings.h>

// SHA-256 compacto (FIPS 180-4). So para conferir o anexo baixado; nao ha
// sha256 alcancavel sob NV_TPK (a libcrypto do aparelho nao e garantida).
typedef struct { uint32_t h[8]; uint64_t n; unsigned char b[64]; size_t k; } At_Sha;
static void at_sha_bloco(At_Sha *s, const unsigned char *p) {
  static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
  uint32_t w[64], a,b,c,d,e,f,g,h; int i;
  for (i = 0; i < 16; i++)
    w[i] = (uint32_t)p[i*4]<<24 | (uint32_t)p[i*4+1]<<16 | (uint32_t)p[i*4+2]<<8 | p[i*4+3];
  for (i = 16; i < 64; i++) {
    uint32_t s0 = (w[i-15]>>7|w[i-15]<<25) ^ (w[i-15]>>18|w[i-15]<<14) ^ (w[i-15]>>3);
    uint32_t s1 = (w[i-2]>>17|w[i-2]<<15) ^ (w[i-2]>>19|w[i-2]<<13) ^ (w[i-2]>>10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
  for (i = 0; i < 64; i++) {
    uint32_t S1 = (e>>6|e<<26) ^ (e>>11|e<<21) ^ (e>>25|e<<7);
    uint32_t ch = (e&f) ^ (~e&g);
    uint32_t t1 = h + S1 + ch + K[i] + w[i];
    uint32_t S0 = (a>>2|a<<30) ^ (a>>13|a<<19) ^ (a>>22|a<<10);
    uint32_t maj = (a&b) ^ (a&c) ^ (b&c);
    uint32_t t2 = S0 + maj;
    h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
  }
  s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void at_sha_init(At_Sha *s) {
  s->h[0]=0x6a09e667;s->h[1]=0xbb67ae85;s->h[2]=0x3c6ef372;s->h[3]=0xa54ff53a;
  s->h[4]=0x510e527f;s->h[5]=0x9b05688c;s->h[6]=0x1f83d9ab;s->h[7]=0x5be0cd19;
  s->n=0;s->k=0;
}
static void at_sha_up(At_Sha *s, const unsigned char *p, size_t n) {
  s->n += (uint64_t)n * 8;
  while (n) {
    size_t take = 64 - s->k; if (take > n) take = n;
    memcpy(s->b + s->k, p, take); s->k += take; p += take; n -= take;
    if (s->k == 64) { at_sha_bloco(s, s->b); s->k = 0; }
  }
}
static void at_sha_fim(At_Sha *s, char *hex65) {
  unsigned char len[8]; int i;
  uint64_t bits = s->n;
  unsigned char um = 0x80;
  for (i = 0; i < 8; i++) len[7-i] = (unsigned char)(bits >> (i*8));
  at_sha_up(s, &um, 1);
  while (s->k != 56) { unsigned char z = 0; at_sha_up(s, &z, 1); }
  at_sha_up(s, len, 8);
  for (i = 0; i < 8; i++)
    snprintf(hex65 + i*8, 9, "%08x", s->h[i]);
}
// sha256 hex de um buffer inteiro em memoria -> hex65 (64 chars + NUL).
static void at_sha256_hex(const unsigned char *buf, size_t n, char *hex65) {
  At_Sha s; at_sha_init(&s); at_sha_up(&s, buf, n); at_sha_fim(&s, hex65);
}
#endif

#define AT_URL   "https://api.github.com/repos/iqui27/nuvio-native-legacy/releases/latest"
#define AT_ARQ   "atualizacao-vista.txt"
#define AT_PAGINA "https://github.com/iqui27/nuvio-native-legacy/releases"
#define AT_APPID  NV_APP_ID
#define AT_LUNA_PUB "/usr/bin/luna-send-pub"
#define AT_LOG_INST "/tmp/nuvio-instalar.log"
#define AT_HB_DIR   "/media/developer/apps/usr/palm/applications/org.webosbrew.hbchannel"

// AGENDA DA RECONSULTA (ver atualizacao_agenda_vence).
#ifndef AT_INTERVALO_S
#define AT_INTERVALO_S        (6L * 3600L)   // ~6 h entre consultas que responderam
#endif
#define AT_REPETIR_FALHA_S    (30L * 60L)    // sem resposta: tenta de novo em 30 min
#define AT_MIN_ENTRE_MS       (10u * 60u * 1000u)  // nunca duas em menos de 10 min
#define AT_ESPERA_RETOMAR_MS  (45u * 1000u)  // depois de voltar do segundo plano

// O CARTAO E A ILHA DO RELOGIO CRESCIDA (Glass UI v2, mockup ajustes-v2
// "v2-upd-*", aprovado em 03/10): 1300 x 1008, ancorado no canto da pilula
// (cresce para a esquerda com o relogio a direita), raio 40, veu de 45%, miolo
// a 90% no vidro e #15161A no solido. Medidas do mockup em AT_*.
#define AT_W        1300.0f
#define AT_H        1008.0f
#define AT_RAIO       40.0f
#define AT_PADX       52.0f
#define AT_PADY       44.0f
#define AT_VEU        0.45f
// A mola da forma e a do modal da ilha (ilha.c, MODAL_MOLA_W/Z): o mesmo
// "pulo" da pilula crescendo, ~0,7 s ate assentar.
#define AT_MOLA_W      7.5f
#define AT_MOLA_Z     0.80f
// O conteudo entra 80 ms depois de a pilula comecar a crescer.
#define AT_CONTEUDO_MS 80u
// Linhas de NOTAS guardadas (secao, paragrafo ou item). Era 14, e com as notas
// sem rolagem isso nao importava: o cartao ja cortava antes. Agora a area das
// notas rola, entao o teto so protege o buffer.
#define AT_LINHAS_MAX  48
// Notas em 24 px com entrelinha de 1,45 (mockup).
#define AT_LEADING    34.8f
// Um aperto de cima/baixo rola tres linhas de notas.
#define AT_PASSO     (AT_LEADING * 3.0f)
// Linhas por item. Com rolagem nao ha por que cortar um item no meio; o teto
// so impede que um paragrafo gigante vire uma parede.
#define AT_ITEM_LINHAS 8

static SDL_mutex *mtx;
static SDL_Thread *fio;
static int pronto, aberto;
// RECONSULTA (pedido do dono, 01/10/2026: "o aviso so aparece se fechar o app
// totalmente"). Antes a consulta era UMA por processo (`disparado`), e webOS,
// Tizen e Android guardam o app suspenso por dias: a release saia e a TV nunca
// perguntava de novo. Agora atualizacao_verificar() e chamada a cada quadro da
// home e quem decide se ja e hora e atualizacao_agenda_vence() (pura, testada
// em tests/atualizacao_agenda.c).
//
// emCurso: um fio de consulta vivo (nunca dois). consultas: quantas terminaram.
// ultRel/ultTk: relogio de parede (time) e SDL_GetTicks do INICIO da ultima.
// O de parede e o que enxerga a TV dormindo (o monotonico pode parar no
// suspend); o monotonico segura a rajada quando o de parede salta (NTP
// acertando a hora logo depois do boot). naoAntesTk: depois de voltar do
// segundo plano espera um pouco, para nao disputar a rede com o resto do app
// que tambem acorda. ultFalhou: a ultima nao teve resposta (tenta mais cedo).
static int emCurso, disparos, consultas, ultFalhou;
static long ultRel;
static Uint32 ultTk, naoAntesTk;
// GERACAO: sobe a cada consulta que traz uma tag nova diferente da conhecida.
// O cartao automatico abre uma vez por geracao (antes: `mostrado`, uma vez por
// processo) e, dentro dela, a marca AT_ARQ continua dizendo "essa ja foi vista".
static int geracao, geracaoVista = -1;
// O botao dos Ajustes (atualizacao_procurar_agora): o que a ultima consulta
// respondeu, e se a pessoa pediu para abrir o cartao quando ela chegar.
static int busca = ATUALIZACAO_BUSCA_NADA, manualPendente, manualAchou;
// A FORMA (atualizacao_atualizar): cartaoT vai de 0 (a pilula da ilha) a 1 (o
// cartao) na mola subamortecida; `origem` e a pilula de onde ele nasceu e para
// onde volta (ilha_rect). conteudoA e o texto, que entra AT_CONTEUDO_MS depois.
static float cartaoT, cartaoV, conteudoA;
static GfxRect origem;
static Uint32 abriuEm;
// "Atualizar o aplicativo" nos Ajustes: a ilha mostra a consulta e a resposta
// (mockup v2-upd-procurando / v2-upd-em-dia). So a pedida, nunca a da agenda.
static int manualIlha;
static char tagNova[32];          // "1.0.54", vazio se nao ha nada mais novo
static char notas[6144];          // texto ja limpo, linhas separadas por \n
// URL do .ipk da release. Vazia quando a release nao anexou um (ou quando este
// alvo nao sabe instalar, e ai nem se procura).
static char ipkUrl[512];
static char ipkHash[80];          // sha256 em hex; vazio quando a release nao diz
#ifdef NV_TPK
// Anexo libnuvio.so desta ABI (auto-atualizacao do .tpk). soUrl/soHash vem do
// JSON da release; soVer e a tag (== tagNova) que essa .so entrega.
static char soUrl[512];
static char soHash[80];
static char soVer[32];
#endif
#ifdef NV_ANDROID
// Anexo "Nuvio-<versao>-android.apk" da release (auto-atualizacao no Android).
// apkPerm: o sistema pediu a permissao de instalar apps desta fonte; o cartao
// avisa e deixa tentar de novo.
static char apkUrl[512];
static char apkHash[80];
static char apkVer[32];
static int  apkPerm;
#endif

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
#elif !defined(__EMSCRIPTEN__) && !defined(__APPLE__) && !defined(NV_LINUX_DESKTOP) && !defined(NV_TPK) && !defined(NV_ANDROID)
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
// .so nova ENCENADA nesta sessao (so NV_TPK). O host escolhe a lib no arranque
// de um processo NOVO, e na Samsung "sair" nem sempre acaba o processo: a TV o
// guarda e a reabertura retoma o antigo (#184, S90D Tizen 9: cinco arranques
// seguidos na 1.5.4 com a 1.6.0 ja encenada). Por isso o cartao oferece
// "Reiniciar agora", que encerra o app de verdade (SDL_QUIT -> fim do main ->
// o host sai e a vigia dele forca o _exit).
static int soEncenada;
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
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueNotas;
static int toqueNotasRolar(const PonteiroRolagem *e) {
  int r = toquerol_evento(&toqueNotas, e);
  if (toquerol_livre(&toqueNotas)) rolarAlvo = rolar;
  return r;
}
#endif
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
//
// APOSENTADA NA 2.0.5. A build de cache grande (NV_TEX_MB_FIXO) tambem pega o
// .ipk NORMAL: a 2.0.5 e a ultima highcache, e quem a instalou sai dela no
// proximo "Atualizar agora". O teto de 300 MB vem da Ajustes > Memória para
// imagens (migracao unica em ajustes.c). A highcache continua sendo empacotada
// na 2.0.5 so para essa migracao chegar a quem a usa.
#define AT_SUFIXO "_arm.ipk"
#define AT_SUFIXO2 "-webos.ipk"

static int terminaEm(const char *s, size_t n, const char *sufixo) {
  size_t k = strlen(sufixo);
  if (n >= k && !strncmp(s + n - k, sufixo, k)) return 1;
  k = strlen(AT_SUFIXO2);
  return n >= k && !strncmp(s + n - k, AT_SUFIXO2, k);
}

// O sha256 hex do anexo cuja browser_download_url comeca em `ini`. O campo
// `digest` vem ANTES do browser_download_url dentro do mesmo anexo (ordem do
// JSON do GitHub: ... size, digest, download_count, ..., browser_download_url),
// entao a busca e PARA TRAS a partir da url — ir para frente pegaria o digest do
// anexo SEGUINTE. Escreve "" quando o anexo nao tem digest.
static void hashAntesDe(const char *corpo, const char *ini, char *hash, size_t tamHash) {
  const char *d = NULL, *q = corpo;
  if (!hash || !tamHash) return;
  hash[0] = 0;
  while (q < ini) {
    const char *r = strstr(q, "\"digest\":");
    if (!r || r > ini) break;
    d = r; q = r + 8;
  }
  if (!d) return;
  d = strchr(d + 8, '"');
  if (!d) return;
  d++;
  if (!strncmp(d, "sha256:", 7)) d += 7;   // so o hex interessa
  { const char *e = strchr(d, '"');
    if (e && (size_t)(e - d) < tamHash) { memcpy(hash, d, (size_t)(e - d)); hash[e - d] = 0; } }
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
      hashAntesDe(corpo, ini, hash, tamHash);
      return 1;
    }
  }
  return 0;
}

#if defined(NV_TPK) || defined(NV_ANDROID)
// Anexo cujo nome termina em `sufixo` (casamento EXATO, sem o AT_SUFIXO2 do
// .ipk) e o sha256 do MESMO anexo (hashAntesDe). Sem digest ignora o anexo: sem
// hash nao ha como confiar em codigo remoto. Serve ao .so do .tpk e ao .apk.
static int acharAnexo(const char *corpo, char *dst, size_t tam, char *hash, size_t tamHash,
                      const char *sufixo) {
  const char *p = corpo;
  const char *chave = "\"browser_download_url\":";
  size_t k = strlen(sufixo);
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
    if (n > k && n < tam && !strncmp(ini + n - k, sufixo, k)) {
      char h[80] = "";
      hashAntesDe(corpo, ini, h, sizeof h);
      if (!h[0]) continue;                 // sem digest: nao confiar
      memcpy(dst, ini, n); dst[n] = 0;
      snprintf(hash, tamHash, "%s", h);
      return 1;
    }
  }
  return 0;
}
#endif

#ifdef NV_ANDROID
// Match the installed variant exactly. Pointer width follows the running
// process, including 32-bit Fire OS on 64-bit hardware.
#ifdef NV_APK_LITE
#if UINTPTR_MAX == UINT32_MAX
#define AT_APK_SUFIXO "-android-lite-armv7.apk"
#else
#define AT_APK_SUFIXO "-android-lite-arm64.apk"
#endif
#else
#define AT_APK_SUFIXO "-android.apk"
#endif
static int acharApk(const char *corpo, char *dst, size_t tam, char *hash, size_t tamHash) {
  return acharAnexo(corpo, dst, tam, hash, tamHash, AT_APK_SUFIXO);
}
#endif

#ifdef NV_TPK
// O anexo da libnuvio.so DESTA ABI e o seu sha256. Uma release do .tpk anexa UMA
// libnuvio.so (a build de tpk.sh e unica, ARMv7 softfp, compartilhada pelos
// quatro pacotes), com o sufixo AT_SO_SUFIXO. Casa por sufixo EXATO (sem o
// AT_SUFIXO2 do .ipk) e le o digest do MESMO anexo por hashAntesDe. Sem o
// digest, ignora o anexo: sem hash nao ha como confiar em codigo nativo remoto.
// O 4/5 (NV_TPK40) roda a lib SEM TLS, que o carregador ELF exige: baixa o
// anexo proprio. "-tpk-arm.so" nao casa com "-tpk40-arm.so", entao um pacote
// nunca pega a lib do outro.
#ifdef NV_TPK40
#define AT_SO_SUFIXO "-tpk40-arm.so"
#else
#define AT_SO_SUFIXO "-tpk-arm.so"
#endif
static int acharSo(const char *corpo, char *dst, size_t tam, char *hash, size_t tamHash) {
  return acharAnexo(corpo, dst, tam, hash, tamHash, AT_SO_SUFIXO);
}
#endif

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
    // Imagem (banner, captura) nao e texto do cartao: "![x](url)" ou <img/<p>.
    if (!strncmp(linha, "![", 2) || !strncmp(linha, "<img", 4) || !strncmp(linha, "<p", 2)) continue;
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

// A CONSULTA. Com a reconsulta ela pode rodar com o app ja usando o que a
// anterior achou, entao tudo e lido em variaveis LOCAIS e so copiado para as
// globais sob o mutex, e so quando a release e mais nova que a instalada.
static int fioConsulta(void *arg) {
  char *corpo;
  static char body[8192];          // so um fio de consulta por vez (emCurso)
  char tag[48] = "";
  char lIpk[512] = "", lIpkH[80] = "";
#ifdef NV_TPK
  char lSo[512] = "", lSoH[80] = "";
#endif
#ifdef NV_ANDROID
  char lApk[512] = "", lApkH[80] = "";
#endif
  (void)arg;
  body[0] = 0;
#ifdef NV_VIDAA
  SDL_LockMutex(mtx);
  pronto = 1;
  SDL_UnlockMutex(mtx);
  return 0;  // hosted app e sempre current
#endif
  corpo = rede_baixar(AT_URL, 12);
  if (!corpo) { printf("[atualizacao] sem resposta do GitHub\n"); fflush(stdout); }
  else {
    textoJson(corpo, "tag_name", tag, sizeof tag);
    textoJson(corpo, "body", body, sizeof body);
    // Sem anexo da variante desta build, ipkUrl fica vazio e podeInstalar()
    // devolve 0: o cartao aparece so com a URL da pagina.
    if (AT_INSTALA) acharIpk(corpo, lIpk, sizeof lIpk, lIpkH, sizeof lIpkH, AT_SUFIXO);
#ifdef NV_TPK
    // Anexo libnuvio.so para a auto-atualizacao do .tpk (staging por hash).
    acharSo(corpo, lSo, sizeof lSo, lSoH, sizeof lSoH);
#endif
#ifdef NV_ANDROID
    acharApk(corpo, lApk, sizeof lApk, lApkH, sizeof lApkH);
#endif
    free(corpo);
  }
  SDL_LockMutex(mtx);
  if (tag[0]) {
    const char *v = tag[0] == 'v' ? tag + 1 : tag;
    if (maisNova(v, NV_VERSAO)) {
      if (strcmp(v, tagNova) != 0) geracao++;
      snprintf(tagNova, sizeof tagNova, "%s", v);
      limparNotas(body, notas, sizeof notas);
      snprintf(ipkUrl, sizeof ipkUrl, "%s", lIpk);
      snprintf(ipkHash, sizeof ipkHash, "%s", lIpkH);
#ifdef NV_TPK
      snprintf(soUrl, sizeof soUrl, "%s", lSo);
      snprintf(soHash, sizeof soHash, "%s", lSoH);
      // So a versao mais nova entra: soVer marca a .so encenada e o host a
      // compara com a versao empacotada antes de aplicar.
      snprintf(soVer, sizeof soVer, "%s", v);
#endif
#ifdef NV_ANDROID
      snprintf(apkUrl, sizeof apkUrl, "%s", lApk);
      snprintf(apkHash, sizeof apkHash, "%s", lApkH);
      snprintf(apkVer, sizeof apkVer, "%s", v);
#endif
    }
    printf("[atualizacao] instalada %s, no GitHub %s%s (consulta %d)\n", NV_VERSAO, v,
           tagNova[0] ? " -- NOVA" : "", consultas + 1);
    fflush(stdout);
  }
  ultFalhou = !tag[0];
  busca = !tag[0] ? ATUALIZACAO_BUSCA_ERRO
        : tagNova[0] ? ATUALIZACAO_BUSCA_NOVA : ATUALIZACAO_BUSCA_EM_DIA;
  if (manualPendente) { manualPendente = 0; manualAchou = tagNova[0] != 0; }
  consultas++;
  pronto = 1;
  emCurso = 0;
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
#ifdef NV_DTS_DEBUG
  /* Production release IPKs carry the production ID; never install them from
   * an isolated diagnostic app, including through a stale update card. */
  (void)arg; return 0;
#endif
  char cmd[900];
  (void)arg;
#ifdef NV_VIDAA
  SDL_LockMutex(mtx);
  pronto = 1;
  SDL_UnlockMutex(mtx);
  return 0;  // hosted app e sempre current
#endif
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
#ifdef NV_DTS_DEBUG
  return 0;
#endif
  return AT_INSTALA && ipkUrl[0] && estado == AT_PARADO && temInstalador();
}

#ifdef NV_TPK
// BAIXA E ENCENA a libnuvio.so nova. NUNCA carrega em processo: grava
// data/libnuvio.staged.so (+ .sha256 e .ver) e o host memfd-carrega no proximo
// arranque. A barreira e o sha256: baixa para data/libnuvio.download, confere o
// hash contra soHash (do digest da release, por HTTPS) e so entao renomeia
// atomicamente. Hash errado, download parcial ou erro de escrita => apaga tudo e
// nao encena nada. Em duvida, o app segue com a .so empacotada.
static int fioBaixarSo(void *arg) {
  char dl[600] = "", so[600] = "", sh[600] = "", vr[600] = "";
  char hex[65] = "", verLocal[32];
  char *buf;
  long n = 0;
  FILE *f;
  int ok = 0;
  (void)arg;
  SDL_LockMutex(mtx); snprintf(verLocal, sizeof verLocal, "%s", soVer); SDL_UnlockMutex(mtx);
  if (!dados_caminho(dl, sizeof dl, "libnuvio.download") ||
      !dados_caminho(so, sizeof so, "libnuvio.staged.so") ||
      !dados_caminho(sh, sizeof sh, "libnuvio.staged.sha256") ||
      !dados_caminho(vr, sizeof vr, "libnuvio.staged.ver")) {
    printf("[atualizacao] sem pasta de dados para encenar a .so\n"); fflush(stdout);
    SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
    return 0;
  }
  printf("[atualizacao] baixando libnuvio.so nova (%s)\n", soUrl); fflush(stdout);
  buf = rede_baixar_bin(soUrl, 120, &n);
  if (!buf || n <= 0) {
    printf("[atualizacao] download da .so falhou\n"); fflush(stdout);
    free(buf);
    SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
    return 0;
  }
  SDL_LockMutex(mtx); snprintf(instPasso, sizeof instPasso, "Verificando"); SDL_UnlockMutex(mtx);
  at_sha256_hex((const unsigned char *)buf, (size_t)n, hex);
  if (strcasecmp(hex, soHash) != 0) {
    printf("[atualizacao] sha256 NAO confere: baixado %.12s... esperado %.12s...\n", hex, soHash);
    fflush(stdout);
    free(buf);
    SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
    return 0;
  }
  // Hash confere: grava o arquivo temporario e renomeia atomicamente. So depois
  // vem o .sha256 e o .ver — se o processo morrer no meio, faltar o .sha256 faz
  // o host ignorar o staging (ele reconfere o hash do arquivo encenado).
  f = fopen(dl, "wb");
  if (f) {
    ok = fwrite(buf, 1, (size_t)n, f) == (size_t)n;
    if (fflush(f) != 0) ok = 0;
    fclose(f);
  }
  free(buf);
  if (!ok) {
    printf("[atualizacao] nao consegui gravar %s\n", dl); fflush(stdout);
    unlink(dl);
    SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
    return 0;
  }
  if (rename(dl, so) != 0) {
    printf("[atualizacao] rename para staged falhou\n"); fflush(stdout);
    unlink(dl);
    SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
    return 0;
  }
  { char linha[80]; snprintf(linha, sizeof linha, "%s\n", hex); dados_gravar("libnuvio.staged.sha256", linha); (void)sh; }
  { char linha[48]; snprintf(linha, sizeof linha, "%s\n", verLocal); dados_gravar("libnuvio.staged.ver", linha); (void)vr; }
  printf("[atualizacao] libnuvio.so %s encenada; aplica no proximo arranque\n", verLocal);
  fflush(stdout);
  SDL_LockMutex(mtx); estado = AT_PRONTO; soEncenada = 1; SDL_UnlockMutex(mtx);
  return 0;
}

// 1 quando ha uma libnuvio.so nova para encenar e nada em andamento.
static int podeAtualizarTpk(void) {
  return soUrl[0] && soHash[0] && soVer[0] && estado == AT_PARADO;
}
#else
static int podeAtualizarTpk(void) { return 0; }
#endif

#ifdef NV_ANDROID
// BAIXA O APK para <dados>/atualizacao/Nuvio-<v>.apk, CONFERE o sha256 contra o
// digest da release (HTTPS) e entrega ao instalador do sistema (NuvioActivity).
// Hash errado ou download parcial: apaga e nao instala nada. Se o arquivo ja
// esta la com o hash certo (a pessoa foi conceder a permissao e voltou), nao
// baixa de novo. Termina em AT_PARADO quando o instalador abriu (se ela cancelar,
// o cartao deixa tentar outra vez; se confirmar, o sistema mata o app).
static void apkFalhou(void) {
  SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx);
}

static int fioInstalarApk(void *arg) {
  char dir[600] = "", nome[96], rel[128], arq[600] = "", hex[65] = "", ver[32], hash[80];
  char *buf = NULL;
  long n = 0;
  int ok = 0, r;
  FILE *f;
  (void)arg;
  SDL_LockMutex(mtx);
  snprintf(ver, sizeof ver, "%s", apkVer);
  snprintf(hash, sizeof hash, "%s", apkHash);
  apkPerm = 0;
  SDL_UnlockMutex(mtx);
  snprintf(nome, sizeof nome, "Nuvio-%s.apk", ver);
  snprintf(rel, sizeof rel, "atualizacao/%s", nome);
  if (!dados_caminho(dir, sizeof dir, "atualizacao") || !dados_caminho(arq, sizeof arq, rel)) {
    printf("[atualizacao] sem pasta de dados para o APK\n"); fflush(stdout);
    apkFalhou(); return 0;
  }
  mkdir(dir, 0755);
  if (!android_preparar_apk(arq)) { apkFalhou(); return 0; }
  // Ja baixado e conferido numa tentativa anterior?
  f = fopen(arq, "rb");
  if (f) {
    long t;
    fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
    if (t > 0 && (buf = malloc((size_t)t)) != NULL) {
      if (fread(buf, 1, (size_t)t, f) == (size_t)t) {
        at_sha256_hex((const unsigned char *)buf, (size_t)t, hex);
        ok = strcasecmp(hex, hash) == 0;
      }
      free(buf); buf = NULL;
    }
    fclose(f);
    if (!ok) unlink(arq);
  }
  if (!ok) {
    printf("[atualizacao] baixando o APK (%s)\n", apkUrl); fflush(stdout);
    // TRES TENTATIVAS. Logo depois de um anexo novo o GitHub respondeu 504 por
    // uns minutos (medido em 01/10/2026 com a 1.6.5: o .ipk antigo baixava, o
    // .apk recem-enviado dava 504, e a mesma URL serviu 30 s depois).
    { int tent;
      for (tent = 0; tent < 3; tent++) {
        if (tent) { printf("[atualizacao] tentando de novo em %d s\n", 10 * tent); fflush(stdout); SDL_Delay(10000u * (unsigned)tent); }
        n = 0;
        buf = rede_baixar_bin(apkUrl, 600, &n);
        if (buf && n > 0) break;
        free(buf); buf = NULL;
      } }
    if (!buf || n <= 0) {
      printf("[atualizacao] download do APK falhou\n"); fflush(stdout);
      free(buf); apkFalhou(); return 0;
    }
    SDL_LockMutex(mtx); snprintf(instPasso, sizeof instPasso, "Verificando"); SDL_UnlockMutex(mtx);
    at_sha256_hex((const unsigned char *)buf, (size_t)n, hex);
    if (strcasecmp(hex, hash) != 0) {
      printf("[atualizacao] sha256 do APK NAO confere: baixado %.12s... esperado %.12s...\n", hex, hash);
      fflush(stdout);
      free(buf); apkFalhou(); return 0;
    }
    f = fopen(arq, "wb");
    ok = 0;
    if (f) {
      ok = fwrite(buf, 1, (size_t)n, f) == (size_t)n;
      if (fflush(f) != 0) ok = 0;
      fclose(f);
    }
    free(buf);
    if (!ok) {
      printf("[atualizacao] nao consegui gravar %s\n", arq); fflush(stdout);
      unlink(arq); apkFalhou(); return 0;
    }
  }
  SDL_LockMutex(mtx); snprintf(instPasso, sizeof instPasso, "Install"); SDL_UnlockMutex(mtx);
  // A troca de pacote mata o processo sem saida limpa: grava a despedida "fim"
  // ANTES, para o modo seguro nao contar a atualizacao como queda. A Activity
  // a desfaz se o instalador for cancelado e o app continuar vivo.
  dados_despedida_fim();
  r = android_instalar_apk(arq);
  SDL_LockMutex(mtx);
  if (r == 0) estado = AT_FALHOU;
  else { estado = AT_PARADO; apkPerm = (r == 2); }
  SDL_UnlockMutex(mtx);
  if (r == 2) {   // so a tela de permissao abriu: nada foi instalado, desfaz a despedida
    char d[600];
    if (dados_caminho(d, sizeof d, "despedida.txt")) unlink(d);
  }
  return 0;
}

static int podeAtualizarApk(void) {
  return apkUrl[0] && apkHash[0] && apkVer[0] && estado == AT_PARADO;
}
#else
static int podeAtualizarApk(void) { return 0; }
#endif

// Ha um botao de acao (instalar .ipk na LG, ou encenar .so nova no .tpk)?
static int podeAgir(void) { return podeInstalar() || podeAtualizarTpk() || podeAtualizarApk(); }

// Quanto da para rolar: o que sobra das notas alem da janela. 0 = cabe tudo.
static float rolarMax(void) {
  float m = notasH - vistaH;
  return m > 0.0f ? m : 0.0f;
}

// Cada abertura comeca do topo das notas e com o foco no botao de atualizar.
static void reiniciarVista(void) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueNotas);
#endif
  foco = 0;
  rolar = rolarAlvo = 0.0f;
}

int atualizacao_agenda_vence(long rel, long relUlt, Uint32 tk, Uint32 tkUlt,
                             Uint32 tkNaoAntes, int jaConsultou, int falhou) {
  long intervaloS = falhou ? AT_REPETIR_FALHA_S : AT_INTERVALO_S;
  Uint32 desdeTk = tk - tkUlt;
  if (!jaConsultou) return 1;                        // a primeira, com a home de pe
  if ((Sint32)(tk - tkNaoAntes) < 0) return 0;       // acabou de voltar do segundo plano
  if (desdeTk < AT_MIN_ENTRE_MS) return 0;           // nunca em rajada
  if (desdeTk >= (Uint32)intervaloS * 1000u) return 1;  // o app ficou aberto o tempo todo
  // Relogio de parede: anda com a TV dormindo. Para tras = hora acertada
  // depois da ultima consulta; nao da para saber quanto passou, entao consulta.
  if (rel < relUlt || rel - relUlt >= intervaloS) return 1;
  return 0;
}

// Dispara o fio. Chamar so com o mutex criado e nada em curso.
static void disparar(void) {
  SDL_Thread *t;
  SDL_LockMutex(mtx);
  if (emCurso) { SDL_UnlockMutex(mtx); return; }
  emCurso = 1;
  disparos++;
  ultRel = (long)time(NULL);
  ultTk = SDL_GetTicks();
  if (busca != ATUALIZACAO_BUSCA_NOVA) busca = ATUALIZACAO_BUSCA_PROCURANDO;
  SDL_UnlockMutex(mtx);
  t = SDL_CreateThread(fioConsulta, "nv-atualizacao", NULL);
  if (t) { SDL_DetachThread(t); fio = t; }
  else {
    SDL_LockMutex(mtx);
    emCurso = 0; ultFalhou = 1; busca = ATUALIZACAO_BUSCA_ERRO;
    if (manualPendente) { manualPendente = 0; manualAchou = 0; }
    SDL_UnlockMutex(mtx);
  }
}

// NAO RECONSULTA com o cartao aberto, instalando ou com a .so ja encenada: a
// consulta reescreveria a URL/hash que o instalador esta usando.
static int podeReconsultar(void) {
  return !aberto && estado != AT_INSTALANDO && estado != AT_PRONTO && !soEncenada;
}

void atualizacao_verificar(void) {
#ifdef NV_DTS_DEBUG
  return; /* Debug IPKs are installed explicitly, outside production updates. */
#endif
  static long ultChamada;
  long rel = (long)time(NULL);
  int vence;
  if (!mtx) mtx = SDL_CreateMutex();
  if (!mtx) return;
  // VOLTA DO SEGUNDO PLANO SEM EVENTO. Nem toda plataforma avisa o C (o host
  // .NET do .tpk nao repassa o OnResume; no webOS o relaunch nao e garantido
  // como evento de janela). Um buraco de mais de 2 min no relogio de parede
  // entre dois quadros da home e o mesmo sinal: a TV dormiu ou o app ficou
  // fora da home. A consulta vencida espera um pouco do mesmo jeito.
  if (ultChamada && (rel - ultChamada > 120 || rel < ultChamada)) atualizacao_retomou();
  ultChamada = rel;
  if (!podeReconsultar()) return;
  SDL_LockMutex(mtx);
  vence = !emCurso && atualizacao_agenda_vence(rel, ultRel, SDL_GetTicks(), ultTk,
                                               naoAntesTk, disparos > 0, ultFalhou);
  SDL_UnlockMutex(mtx);
  if (vence) disparar();
}

void atualizacao_procurar_agora(void) {
#ifdef NV_DTS_DEBUG
  return;
#endif
  if (!mtx) mtx = SDL_CreateMutex();
  if (!mtx || !podeReconsultar()) return;
  SDL_LockMutex(mtx);
  manualPendente = 1; manualAchou = 0; manualIlha = 1;
  busca = ATUALIZACAO_BUSCA_PROCURANDO;
  SDL_UnlockMutex(mtx);
  disparar();
}

int atualizacao_busca(void) {
  int b;
  if (!mtx) return ATUALIZACAO_BUSCA_NADA;
  SDL_LockMutex(mtx); b = busca; SDL_UnlockMutex(mtx);
  return b;
}

int atualizacao_busca_achou(void) {
  int r;
  if (!mtx) return 0;
  SDL_LockMutex(mtx); r = manualAchou; manualAchou = 0; SDL_UnlockMutex(mtx);
  return r;
}

void atualizacao_retomou(void) {
  if (!mtx) return;               // ainda nem consultou: a home faz a primeira
  SDL_LockMutex(mtx);
  naoAntesTk = SDL_GetTicks() + AT_ESPERA_RETOMAR_MS;
  SDL_UnlockMutex(mtx);
}

#ifdef AJUSTES_TESTE
// tests/ajustes_shot.c (NUVIO_AJUSTES_ATUALIZAR): fotografa a linha dos Ajustes
// em cada resposta sem rede. `tag` "" = nenhuma versao nova conhecida.
void atualizacao_teste_estado(int b, const char *tag) {
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  busca = b;
  snprintf(tagNova, sizeof tagNova, "%s", tag ? tag : "");
  SDL_UnlockMutex(mtx);
}
#endif

// A PILULA DE ONDE O CARTAO NASCE: a ilha do quadro anterior (o aviso em que
// a pessoa apertou, ou o relogio). Sem ilha na tela (relogio desligado), uma
// pilula no canto de sempre. Reabrir no meio do recolhimento continua dali.
static void origemDaIlha(void) {
  float x, y, w, h;
  // A pilula vem da ilha, que e camada ampliada (tela virtual, escala.h); o
  // cartao (1300x1008) nao cabe ampliado e fica em 1080p: a origem vai para a
  // tela real.
  if (ilha_rect(&x, &y, &w, &h) && w > 1.0f && h > 1.0f) {
    float e = gfx_escala_ui();
    origem = (GfxRect){ x * e, y * e, w * e, h * e };
  }
  else if (origem.w <= 0.0f) origem = (GfxRect){ ajustes_conteudo_x(), NV_ILHA_Y, 220.0f, NV_ILHA_H_ABERTA };
}
static void nascer(void) {
  if (cartaoT < 0.02f) { origemDaIlha(); cartaoT = 0.0f; cartaoV = 0.0f; conteudoA = 0.0f; }
  abriuEm = SDL_GetTicks();
}

void atualizacao_mostrar_se_houver(void) {
  char *visto;
  int g;
  if (aberto || !mtx) return;
  SDL_LockMutex(mtx);
  if (!pronto || !tagNova[0] || geracaoVista == geracao) { SDL_UnlockMutex(mtx); return; }
  g = geracao;
  SDL_UnlockMutex(mtx);
  geracaoVista = g;
  visto = dados_ler(AT_ARQ);
  if (visto) {
    int igual = !strncmp(visto, tagNova, strlen(tagNova)) &&
                (visto[strlen(tagNova)] == '\n' || visto[strlen(tagNova)] == 0);
    free(visto);
    if (igual) return;
  }
  aberto = 1;
  nascer();
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
  if (tagNova[0]) { aberto = 1; geracaoVista = geracao; nascer(); reiniciarVista(); }
  SDL_UnlockMutex(mtx);
}

void atualizacao_evento(const SDL_Event *e) {
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) {
    if (toquerol_livre(&toqueNotas)) rolarAlvo = rolar;
    toquerol_limpar(&toqueNotas);
  }
#endif
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
  if (estado == AT_PRONTO && soEncenada &&
      (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)) {
    SDL_Event q;
    printf("[atualizacao] reiniciar agora: encerrando para o host carregar a lib nova\n");
    fflush(stdout);
    memset(&q, 0, sizeof q);
    q.type = SDL_QUIT;
    SDL_PushEvent(&q);
    fechar();
    return;
  }
  if (podeAgir() && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
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
#ifdef NV_TPK
    if (podeAtualizarTpk() && foco == 0) {
      SDL_Thread *t;
      SDL_LockMutex(mtx); estado = AT_INSTALANDO; SDL_UnlockMutex(mtx);
      t = SDL_CreateThread(fioBaixarSo, "nv-baixar-so", NULL);
      if (t) { SDL_DetachThread(t); fioInst = t; }
      else { SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx); }
      return;
    }
#endif
#ifdef NV_ANDROID
    if (podeAtualizarApk() && foco == 0) {
      SDL_Thread *t;
      SDL_LockMutex(mtx); estado = AT_INSTALANDO; instPct = 0.0f; instPasso[0] = 0; SDL_UnlockMutex(mtx);
      t = SDL_CreateThread(fioInstalarApk, "nv-instalar-apk", NULL);
      if (t) { SDL_DetachThread(t); fioInst = t; }
      else { SDL_LockMutex(mtx); estado = AT_FALHOU; SDL_UnlockMutex(mtx); }
      return;
    }
#endif
    fechar();
  }
}


// O PASSO EM QUE O INSTALADOR ESTA, para as etapas Baixar · Conferir · Instalar:
// 0 baixando, 1 conferindo (o sha256 do .so/.apk, ou o "Verificando" da LG), 2
// instalando, 3 tudo feito. O texto vem de quem instala (instPasso).
static int etapaDe(int est, const char *passo) {
  if (est == AT_PRONTO) return 3;
  if (!strncmp(passo, "Install", 7)) return 2;
  if (!strncmp(passo, "Verif", 5)) return 1;
  return 0;
}
static const char *fraseDaEtapa(int etapa) {
  return etapa == 1 ? i18n("Conferindo o arquivo...")
       : etapa == 2 ? i18n("Instalando...")
       : i18n("Baixando a atualização...");
}
// A barra: o numero do instalador. Passado o download, o arquivo inteiro ja
// chegou: sem numero do instalador (o .tpk e o .apk nao informam), a barra
// fica cheia em vez de voltar a zero.
static float pctDaEtapa(int etapa, float pct) {
  if (etapa >= 1 && pct <= 0.0f) pct = 100.0f;
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

int atualizacao_cobre_ilha(void) { return aberto || cartaoT > 0.02f; }

void atualizacao_atualizar(float dt, Uint32 agora) {
  // VOLTAR FECHA O CARTAO NO MEIO DO DOWNLOAD (ver atualizacao_evento) e o
  // fio segue: sem isto a pessoa ficava sem saber se ainda baixava. A barra
  // passa para a ilha do relogio enquanto o cartao estiver fechado (mockup
  // v2-upd-ilha: icone, a frase da etapa, o trilho e a porcentagem); pronto,
  // vira um aviso curto.
  { static int estAnt;
    int est; float pct; char passo[48];
    if (mtx) SDL_LockMutex(mtx);
    est = estado; pct = instPct; snprintf(passo, sizeof passo, "%s", instPasso);
    if (mtx) SDL_UnlockMutex(mtx);
    if (!aberto && est == AT_INSTALANDO) {
      int etapa = etapaDe(est, passo);
      ilha_atividade_ex(fraseDaEtapa(etapa),
                        pct < 0.0f && etapa == 0 ? -1.0f : pctDaEtapa(etapa, pct) / 100.0f, "aj_download");
    }
    if (!aberto && estAnt == AT_INSTALANDO && est == AT_PRONTO)
      ilha_avisar("atualizacao", ILHA_OK, NULL, i18n("Atualizado. Feche e abra o app para usar."), 7000u, 0);
    estAnt = est; }
  // A CONSULTA PEDIDA NOS AJUSTES fala pela ilha: a atividade enquanto o
  // GitHub nao responde e, no fim, "Em dia" (ou o neutro sem resposta). Com
  // versao nova quem fala e o cartao, que os Ajustes abrem.
  if (manualIlha) {
    int b = atualizacao_busca();
    if (b == ATUALIZACAO_BUSCA_PROCURANDO) ilha_atividade_ex(i18n("Procurando atualização…"), -1.0f, "");
    else {
      manualIlha = 0;
      if (b == ATUALIZACAO_BUSCA_EM_DIA) {
        char t[160];
        IlhaAvisoEx e;
        memset(&e, 0, sizeof e);
        snprintf(t, sizeof t, i18n("Você está na versão mais recente (%s)"), NV_VERSAO);
        e.chave = "atualizacao"; e.tipo = ILHA_OK; e.icone = "check"; e.texto = t;
        e.kicker = i18n("Em dia"); e.ms = 6000u;
        ilha_avisar_ex(&e);
      } else if (b == ATUALIZACAO_BUSCA_ERRO)
        ilha_avisar("atualizacao", ILHA_INFO, NULL, i18n("Não deu para consultar agora"), 6000u, 0);
    }
  }
  // FECHANDO, a forma volta para a pilula ONDE ELA ESTA AGORA (com o download
  // em curso, ja e a da atividade).
  if (!aberto) origemDaIlha();
  if (!aberto && cartaoT < 0.02f && conteudoA < 0.01f) {
    cartaoT = cartaoV = conteudoA = 0.0f;
    return;
  }
  if (dt > 0.05f) dt = 0.05f;
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) {
    cartaoT = aberto ? 1.0f : 0.0f; cartaoV = 0.0f; conteudoA = aberto ? 1.0f : 0.0f;
  } else {
    int k;
    float alvo = aberto ? 1.0f : 0.0f;
    for (k = 0; k < 4; k++) {
      float h = dt * 0.25f, ac = AT_MOLA_W * AT_MOLA_W * (alvo - cartaoT) - 2.0f * AT_MOLA_Z * AT_MOLA_W * cartaoV;
      cartaoV += ac * h;
      cartaoT += cartaoV * h;
    }
    // Abrindo, o texto entra 80 ms depois da forma; fechando, ele sai antes
    // dela (a pilula nunca encolhe com o cartao escrito dentro).
    conteudoA = anim_mola(conteudoA,
                          aberto && (Sint32)(agora - abriuEm) >= (Sint32)AT_CONTEUDO_MS ? 1.0f : 0.0f,
                          dt, aberto ? 12.0f : 30.0f);
  }
  if (!aberto && cartaoT < 0.0f) { cartaoT = 0.0f; cartaoV = 0.0f; }
  // Rolagem suave, mas curta (~120 ms para chegar): quem segura a seta quer
  // ver o texto andar, nao esperar.
#ifdef NV_TOUCH_UI
  if (!toquerol_livre(&toqueNotas))
#endif
  { float d = rolarAlvo - rolar, f = dt * 1000.0f / 120.0f;
    if (f > 1.0f) f = 1.0f;
    rolar = (d > -0.5f && d < 0.5f) ? rolarAlvo : rolar + d * f; }
}

// O retangulo final, no canto da pilula: alinhado pela esquerda dela, ou pela
// direita quando a ilha mora no lado direito (cresce para a esquerda).
static GfxRect cartaoAlvo(void) {
  GfxRect c = { origem.x, origem.y, AT_W, AT_H };
  if (origem.x + origem.w * 0.5f > NV_TELA_W * 0.5f) c.x = origem.x + origem.w - AT_W;
  if (c.x + c.w > NV_TELA_W - NV_FOLHA_MARGEM) c.x = NV_TELA_W - NV_FOLHA_MARGEM - c.w;
  if (c.x < NV_FOLHA_MARGEM) c.x = NV_FOLHA_MARGEM;
  if (c.y + c.h > NV_TELA_H - NV_FOLHA_MARGEM) c.y = NV_TELA_H - NV_FOLHA_MARGEM - c.h;
  if (c.y < NV_FOLHA_MARGEM) c.y = NV_FOLHA_MARGEM;
  return c;
}

// --- pecas do cartao (medidas do mockup) ----------------------------------------
// Texto claro do mockup: #F3F2EF, com a opacidade do CSS no alfa.
#define AT_TX 243, 242, 239

// <kbd> 36 de altura (18/700, minimo 40, recuo 9) + rotulo 20 a 50%, vao 10.
static float kbd(float x, float yc, const char *tecla, const char *rotulo, float a) {
  TxtLinha k = txt_linha(TXT_AJ_CHIP, tecla, AT_TX, 255);
  TxtLinha l = txt_linha(TXT_AJ_TEXTO, rotulo, AT_TX, 255);
  float kw = (float)k.w + 18.0f;
  if (kw < 40.0f) kw = 40.0f;
  if (x > -9000.0f && a > 0.002f) {
    GfxRect r = { x, yc - 18.0f, kw, 36.0f };
    if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, 0.09f * a);
    else gfx_cor(r, 0.5f, 0.141f, 0.149f, 0.173f, a);
    txt_desenhar_alpha(k, x + (kw - (float)k.w) * 0.5f, yc - (float)k.h * 0.5f, a * 0.82f);
    txt_desenhar_alpha(l, x + kw + 10.0f, yc - (float)l.h * 0.5f, a * 0.50f);
  }
  return kw + 10.0f + (float)l.w;
}
// Dicas lado a lado (vao 24), alinhadas pela BORDA DIREITA xDir.
static void dicas(float xDir, float yc, const char *const *t, const char *const *r, int n, float a) {
  float tot = 0.0f, x;
  int i;
  for (i = 0; i < n; i++) tot += kbd(-10000.0f, yc, t[i], r[i], 0.0f) + (i ? 24.0f : 0.0f);
  x = xDir - tot;
  for (i = 0; i < n; i++) x += kbd(x, yc, t[i], r[i], a) + 24.0f;
}

// PONTEIRO (#99): foco pela mesma variavel das setas (so com os dois botoes
// na tela, como as setas); o OK do clique segue por atualizacao_evento.
static void ponteiroBotao(int i, int b) {
  (void)b;
  if (!aberto || estado == AT_INSTALANDO || !podeAgir() || (i != 0 && i != 1)) return;
  foco = i;
}
int atualizacao_teste_foco(void) { return foco; }

// Botao do cartao: primario 72 (26/600, recuo 34, icone 26) ou secundario 64
// (24/600, recuo 28). Focado = pilula cheia no acento; senao branco 8%.
static float botaoAt(float x, float yc, int primario, const char *rotulo, const char *icone,
                     float icT, float foco, float a) {
  TxtEstilo es = primario ? TXT_G28B : TXT_G26B;
  float h = primario ? 72.0f : 64.0f, pad = primario ? 34.0f : 28.0f;
  TxtLinha l;
  float w, tx;
  int c;
  c = foco > 0.5f ? plrui_tinta() : 243;
  l = txt_linha(es, rotulo, c, c, c, 255);
  w = pad * 2.0f + (float)l.w + (icone ? icT + 10.0f : 0.0f);
  if (x < -9000.0f) return w;
  { GfxRect r = { x, yc - h * 0.5f, w, h };
    if (foco > 0.5f) plrui_pilula_foco(r, a);
    else plrui_botao_repouso(r, a); }
  tx = x + pad;
  if (icone) {
    float k = (float)c / 255.0f;
    gfx_icone((GfxRect){ tx, yc - icT * 0.5f, icT, icT }, icone, k, k, k, a);
    tx += icT + 10.0f;
  }
  txt_desenhar_alpha(l, tx, yc - (float)l.h * 0.5f, foco > 0.5f ? a : a * 0.88f);
  return w;
}

// AS ETAPAS Baixar · Conferir · Instalar (mockup passosAt): disco de 30 — feito
// = acento a 22% com o check; a da vez = ponto no acento com halo; a seguir =
// branco 7% com o numero. `falhou` >= 0 marca aquela em vermelho com o x.
static void etapas(float x, float yc, int atual, int falhou, float a) {
  const char *rot[3];
  float ar, ag, ab;
  int i;
  rot[0] = i18n("Baixar"); rot[1] = i18n("Conferir"); rot[2] = i18n("Instalar");
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < 3; i++) {
    GfxRect d = { x, yc - 15.0f, 30.0f, 30.0f };
    int feito = falhou >= 0 ? i < falhou : i < atual;
    int vez = falhou < 0 && i == atual, erro = i == falhou;
    TxtLinha t;
    float op;
    if (i) {
      gfx_cor((GfxRect){ x, yc - 1.0f, 40.0f, 2.0f }, 0.0f, 1, 1, 1, 0.14f * a);
      x += 40.0f + 14.0f;
      d.x = x;
    }
    if (erro) {
      gfx_cor(d, 0.5f, 0.898f, 0.325f, 0.294f, 0.20f * a);
      gfx_icone((GfxRect){ x + 7.0f, yc - 8.0f, 16.0f, 16.0f }, "aj_x", 0.898f, 0.325f, 0.294f, a);
    } else if (feito) {
      gfx_cor(d, 0.5f, ar, ag, ab, 0.22f * a);
      gfx_icone((GfxRect){ x + 7.0f, yc - 8.0f, 16.0f, 16.0f }, "aj_check", ar, ag, ab, a);
    } else if (vez) {
      gfx_cor((GfxRect){ x + 3.0f, yc - 12.0f, 24.0f, 24.0f }, 0.5f, ar, ag, ab, 0.22f * a);
      gfx_cor((GfxRect){ x + 9.0f, yc - 6.0f, 12.0f, 12.0f }, 0.5f, ar, ag, ab, a);
    } else {
      char n[4];
      TxtLinha tn;
      gfx_cor(d, 0.5f, 1, 1, 1, 0.07f * a);
      snprintf(n, sizeof n, "%d", i + 1);
      tn = txt_linha(TXT_ILHA_HORA, n, AT_TX, 255);
      txt_desenhar_alpha(tn, x + (30.0f - (float)tn.w) * 0.5f, yc - (float)tn.h * 0.5f, a * 0.38f);
    }
    x += 30.0f + 10.0f;
    if (erro) {
      t = txt_linha(TXT_ILHA_CORPO, rot[i], 229, 83, 75, 255); op = 1.0f;
    } else if (vez) {
      t = txt_linha(TXT_ILHA_ITEM, rot[i], 255, 255, 255, 255); op = 1.0f;
    } else {
      t = txt_linha(TXT_ILHA_CORPO, rot[i], AT_TX, 255); op = feito ? 0.60f : 0.38f;
    }
    txt_desenhar_alpha(t, x, yc - (float)t.h * 0.5f, a * op);
    x += (float)t.w + 14.0f;
  }
}

// O QR da pagina da release (src/qr.c), numa textura com 3 modulos de
// silencio (o do mockup). Gerado uma vez.
static GLuint texQr;
static GLuint qrPagina(void) {
  Qr q;
  int lado, xq, yq;
  unsigned char *px;
  if (texQr) return texQr;
  if (!qr_gerar(&q, AT_PAGINA)) return 0;
  lado = q.lado + 6;
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return 0;
  memset(px, 255, (size_t)lado * lado * 3);
  for (yq = 0; yq < q.lado; yq++)
    for (xq = 0; xq < q.lado; xq++)
      if (qr_modulo(&q, xq, yq)) {
        size_t k = ((size_t)(yq + 3) * lado + (xq + 3)) * 3;
        px[k] = 11; px[k + 1] = 12; px[k + 2] = 14;
      }
  glGenTextures(1, &texQr);
  glBindTexture(GL_TEXTURE_2D, texQr);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  // NEAREST: um modulo borrado com o vizinho e ilegivel para a camera.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  free(px);
  return texQr;
}

// O endereco sem o "https://" (o que cabe na linha e o que se digita).
static const char *paginaCurta(void) {
  const char *p = AT_PAGINA;
  return !strncmp(p, "https://", 8) ? p + 8 : p;
}

// UMA LINHA DE NOTAS ja quebrada, com o esmaecimento do fim da janela (o mask
// do mockup: opaco ate 75% da altura, transparente na borda de baixo).
static float fadeNotas(float yMeio, float topo, float vista) {
  float ini = topo + vista * 0.75f, f;
  if (yMeio <= ini) return 1.0f;
  f = 1.0f - (yMeio - ini) / (vista * 0.25f);
  return f < 0.0f ? 0.0f : f;
}
// Quebra `s` em linhas de ate `larg` e desenha cada uma com o seu esmaecimento.
// Devolve a altura usada (linhas x AT_LEADING). Mesma quebra de txt_bloco
// (txt_token_tam: palavra, ou um caractere CJK).
static float notasBloco(TxtEstilo es, const char *s, float x, float y, float larg,
                        float topo, float vista, float op, float a) {
  char linha[512];
  const char *p = s;
  float usado = 0.0f;
  int n = 0, espacoAntes = 0;
  linha[0] = 0;
  while (*p && n < AT_ITEM_LINHAS) {
    const char *ini = p;
    size_t np, nl;
    char tent[512];
    int esp;
    p += txt_token_tam(p);
    if (p == ini) p++;
    np = (size_t)(p - ini);
    esp = *p == ' ';
    while (*p == ' ') p++;
    nl = strlen(linha);
    if (nl + np + 2 >= sizeof tent) break;
    memcpy(tent, linha, nl);
    if (nl && espacoAntes) tent[nl++] = ' ';
    memcpy(tent + nl, ini, np);
    tent[nl + np] = 0;
    if (linha[0] && (float)txt_largura(es, tent) > larg) {
      float ym = y + usado + AT_LEADING * 0.5f;
      if (ym > topo - AT_LEADING && ym < topo + vista + AT_LEADING) {
        TxtLinha l = txt_linha(es, linha, AT_TX, 255);
        txt_desenhar_alpha(l, x, ym - (float)l.h * 0.5f, a * op * fadeNotas(ym, topo, vista));
      }
      usado += AT_LEADING; n++;
      memcpy(linha, ini, np); linha[np] = 0;
    } else memcpy(linha, tent, strlen(tent) + 1);
    espacoAntes = esp;
  }
  if (linha[0] && n < AT_ITEM_LINHAS) {
    float ym = y + usado + AT_LEADING * 0.5f;
    if (ym > topo - AT_LEADING && ym < topo + vista + AT_LEADING) {
      TxtLinha l = txt_linha(es, linha, AT_TX, 255);
      txt_desenhar_alpha(l, x, ym - (float)l.h * 0.5f, a * op * fadeNotas(ym, topo, vista));
    }
    usado += AT_LEADING;
  }
  return usado;
}

// O RESUMO: o primeiro paragrafo das notas, quando vem antes de qualquer secao
// ("Where to watch in the source picker, ..."), mora no cabecalho e nao rola.
// Devolve o comprimento dele em `notas` (0 = nao ha) e copia em `dst`.
static size_t resumoDasNotas(char *dst, size_t tam) {
  const char *fim = strchr(notas, '\n');
  size_t n = fim ? (size_t)(fim - notas) : strlen(notas);
  dst[0] = 0;
  if (!n || notas[0] == '\x01' || !strncmp(notas, "\xe2\x80\xa2 ", 4)) return 0;
  if (n >= tam) n = tam - 1;
  memcpy(dst, notas, n); dst[n] = 0;
  return fim ? (size_t)(fim - notas) + 1 : strlen(notas);
}

void atualizacao_desenhar(Uint32 agora) {
  GfxRect C, R;
  float t, tr, rpx, raio, a, ca, x, y, w, cr, cg, cb;
  int est, etapa, falhaEm = -1;
  float pct;
  char passo[48], buf[200], resumo[512];
  size_t pulo;
  (void)agora;
  if (cartaoT <= 0.0f && conteudoA < 0.01f && !aberto) return;
  if (!aberto && cartaoT < 0.02f) return;

  // A FORMA: da pilula (origem) ao cartao (C), raio de meia altura a 40 px.
  C = cartaoAlvo();
  t = cartaoT > 1.06f ? 1.06f : cartaoT < 0.0f ? 0.0f : cartaoT;
  tr = t > 1.0f ? 1.0f : t;
  R.x = origem.x + (C.x - origem.x) * t; R.y = origem.y + (C.y - origem.y) * t;
  R.w = origem.w + (C.w - origem.w) * t; R.h = origem.h + (C.h - origem.h) * t;
  rpx = origem.h * 0.5f + (AT_RAIO - origem.h * 0.5f) * tr;
  raio = rpx / (R.h > 1.0f ? R.h : 1.0f);
  if (raio > 0.5f) raio = 0.5f;
  a = 1.0f;
  ca = conteudoA;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, AT_VEU * tr);
  // MATERIAL: o da ilha (miolo a 80%) indo ao do cartao (90%, mockup) no vidro;
  // no solido, o miolo da ilha indo ao #15161A opaco. Sombra curta, sem aro.
  if (ajustes_vidro()) {
    gfx_rect((GfxRect){ R.x - 20.0f, R.y - 6.0f, R.w + 40.0f, R.h + 40.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, 0.36f * a);
    gfx_vidro_material(R, raio, GFX_VIDRO_DENS + 0.10f * tr, a);   // + gfx_vidro_luz
  } else {
    gfx_rect((GfxRect){ R.x - 16.0f, R.y - 4.0f, R.w + 32.0f, R.h + 30.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, 0.45f * a);
    gfx_cor(R, raio, 0.055f + 0.027f * tr, 0.058f + 0.028f * tr, 0.068f + 0.034f * tr,
            (0.86f + 0.14f * tr) * a);
  }
  if (ca < 0.01f) return;

  // O CONTEUDO mora no retangulo FINAL e e revelado pela forma que cresce.
  gfx_recorte(R.x, R.y, R.w, R.h);
  SDL_LockMutex(mtx);
  est = estado;
  pct = instPct;
  snprintf(passo, sizeof passo, "%s", instPasso);
  SDL_UnlockMutex(mtx);
  etapa = etapaDe(est, passo);
  if (est == AT_FALHOU) falhaEm = etapaDe(AT_INSTALANDO, passo);

  x = C.x + AT_PADX; w = C.w - 2.0f * AT_PADX;
  // KICKER POR ESTADO (aprovado): disco de 44 com o icone + a marca em caixa
  // alta. Disponivel/Baixando/Instalando no acento, Pronto verde, Falhou vermelho.
  { const char *kic = "aj_download", *kt = i18n("Atualização disponível");
    char up[96];
    float yc = C.y + AT_PADY + 22.0f;
    ajustes_acento(&cr, &cg, &cb);
    if (est == AT_INSTALANDO) kt = etapa >= 2 ? i18n("Instalando") : i18n("Baixando");
    else if (est == AT_PRONTO) { kic = "aj_check"; kt = i18n("Pronto"); cr = 0.298f; cg = 0.765f; cb = 0.541f; }
    else if (est == AT_FALHOU) { kic = "aj_x"; kt = i18n("Falhou"); cr = 0.898f; cg = 0.325f; cb = 0.294f; }
    gfx_cor((GfxRect){ x, yc - 22.0f, 44.0f, 44.0f }, 0.5f, cr, cg, cb, 0.22f * ca);
    gfx_icone((GfxRect){ x + 11.0f, yc - 11.0f, 22.0f, 22.0f }, kic, cr, cg, cb, ca);
    idioma_maiusc(up, sizeof up, kt);
    { TxtLinha k = txt_linha(TXT_AJ_CHIP, "M", AT_TX, 255);
      txt_tracking(TXT_AJ_CHIP, up, AT_TX, x + 44.0f + 14.0f, yc - (float)k.h * 0.5f, ca * 0.45f, 2.5f); }
    // A HORA no canto: o cartao E a ilha do relogio, e ela continua dizendo.
    { time_t tt = time(NULL);
      struct tm lt;
      char h[12] = "";
      if (localtime_r(&tt, &lt)) hora_tela(h, sizeof h, &lt);
      if (h[0]) {
        TxtLinha l = txt_linha(TXT_ILHA_NOME, h, AT_TX, 255);
        txt_desenhar_alpha(l, C.x + C.w - AT_PADX - (float)l.w, yc - (float)l.h * 0.5f, ca * 0.60f);
      } } }

  // TITULO 64/800, "Voce esta na" 26 a 60%, e o resumo 24 a 72%.
  y = C.y + AT_PADY + 44.0f + 18.0f;
  snprintf(buf, sizeof buf, i18n("Nuvio %s"), tagNova);
  { TxtLinha l = txt_linha(TXT_AJ_NUM64, buf, AT_TX, 255);
    txt_desenhar_alpha(l, x - 1.0f, y + (77.0f - (float)l.h) * 0.5f, ca); y += 77.0f; }
  // A VARIANTE NO CARTAO. Sem isto a pessoa le "Você está na 1.1.2" e vai para
  // uma pagina com dois .ipk sem saber qual e o dela.
#ifdef NV_TEX_MB_FIXO
  snprintf(buf, sizeof buf, i18n("Você está na %s · cache grande"), NV_VERSAO);
#else
  snprintf(buf, sizeof buf, i18n("Você está na %s"), NV_VERSAO);
#endif
  y += 6.0f;
  { TxtLinha l = txt_linha(TXT_G28R, buf, AT_TX, 255);
    txt_desenhar_alpha(l, x, y + (31.5f - (float)l.h) * 0.5f, ca * 0.60f); y += 31.5f; }
  pulo = resumoDasNotas(resumo, sizeof resumo);
  if (resumo[0]) {
    y += 20.0f;
    y += txt_bloco(TXT_DET_SIN, resumo, AT_TX, x, y + (AT_LEADING - 29.0f) * 0.5f, w,
                   AT_LEADING, ca * 0.72f, 3);
  }

  // RODAPE FIXO, de baixo para cima: a altura sai do estado (mockup).
  { float rodH, rodY, notTopo = y, vista;
    int notasOff = est == AT_PRONTO || est == AT_FALHOU;
    if (est == AT_PRONTO) rodH = 26.0f + 30.0f + 22.0f + 72.0f + 40.0f;
    else if (est == AT_FALHOU) rodH = 26.0f + 30.0f + 22.0f + 67.0f + 40.0f;
    else if (est == AT_INSTALANDO) rodH = 26.0f + 30.0f + 22.0f + 36.0f + 14.0f + 36.0f + 40.0f;
    else if (podeAgir()) rodH = 26.0f + 72.0f + 40.0f;
    else rodH = 26.0f + 162.0f + 40.0f;
    rodY = C.y + C.h - rodH;
    vista = rodY - notTopo;
    vistaH = vista;
#ifdef NV_TOUCH_UI
    toquerol_vincular(&toqueNotas, (GfxRect){R.x, notTopo, R.w, vista}, gfx_escala(),
                     0.0f, rolarMax(), 1, &rolar);
    ponteiro_rolagem(toqueNotasRolar);
#endif

    // NOTAS, recortadas na janela entre o cabecalho e o rodape, roláveis.
    // \x01 marca secao (28/700); "• " e item (24, entrelinha 1,45, a bolinha a
    // 40%); o resto e paragrafo. A altura total vale para o quadro seguinte.
    if (rolarAlvo > rolarMax()) rolarAlvo = rolarMax();
    if (rolar > rolarMax()) rolar = rolarMax();
    gfx_recorte(R.x, notTopo > R.y ? notTopo : R.y, R.w,
                (rodY < R.y + R.h ? rodY : R.y + R.h) - (notTopo > R.y ? notTopo : R.y));
    { float op = notasOff ? 0.5f : 1.0f, yy = notTopo - rolar, y0 = yy;
      const char *p = notas + pulo;
      float mb = 0.0f;   // a margem de baixo do bloco anterior (colapsa com a de cima)
      while (*p) {
        const char *fim = strchr(p, '\n');
        size_t n = fim ? (size_t)(fim - p) : strlen(p);
        char linha[512];
        if (n >= sizeof linha) n = sizeof linha - 1;
        memcpy(linha, p, n); linha[n] = 0;
        p = fim ? fim + 1 : p + n;
        if (linha[0] == '\x01') {
          TxtLinha l = txt_linha(TXT_G30B, i18n(linha + 1), AT_TX, 255);
          float ym;
          yy += mb > 22.0f ? mb : 22.0f;
          ym = yy + 17.0f;
          if (ym > notTopo - 40.0f && ym < rodY + 40.0f)
            txt_desenhar_alpha(l, x, ym - (float)l.h * 0.5f, ca * op * fadeNotas(ym, notTopo, vista));
          yy += 34.0f;
          mb = 10.0f;
        } else if (!strncmp(linha, "\xe2\x80\xa2 ", 4)) {
          float ym;
          yy += mb > 8.0f ? mb : 8.0f;
          ym = yy + AT_LEADING * 0.5f;
          if (ym > notTopo - 40.0f && ym < rodY + 40.0f) {
            TxtLinha b = txt_linha(TXT_DET_SIN, "\xe2\x80\xa2", AT_TX, 255);
            txt_desenhar_alpha(b, x, ym - (float)b.h * 0.5f, ca * op * 0.40f * fadeNotas(ym, notTopo, vista));
          }
          yy += notasBloco(TXT_DET_SIN, linha + 4, x + 30.0f, yy, w - 30.0f - 24.0f,
                           notTopo, vista, op * 0.78f, ca);
          mb = 8.0f;
        } else {
          yy += mb > 8.0f ? mb : 8.0f;
          yy += notasBloco(TXT_DET_SIN, linha, x, yy, w - 24.0f, notTopo, vista, op * 0.72f, ca);
          mb = 8.0f;
        }
      }
      yy += mb;
      notasH = yy - y0; }
    gfx_recorte(R.x, R.y, R.w, R.h);
    // A TRILHA a direita (6 px, branco 10%, o polegar no acento), 20 px para
    // dentro do topo e da base da janela, a 22 da borda.
    if (rolarMax() > 0.5f) {
      float ar, ag, ab, tY = notTopo + 20.0f, tH = vista - 40.0f, pH, pY;
      ajustes_acento(&ar, &ag, &ab);
      pH = tH * vista / notasH;
      if (pH < 40.0f) pH = 40.0f;
      pY = tY + (tH - pH) * (rolar / rolarMax());
      gfx_cor((GfxRect){ C.x + C.w - 28.0f, tY, 6.0f, tH }, 3.0f / tH, 1, 1, 1, 0.10f * ca);
      gfx_cor((GfxRect){ C.x + C.w - 28.0f, pY, 6.0f, pH }, 3.0f / pH, ar, ag, ab, ca);
    }
    // O fio do rodape: 1 px a 7%, de borda a borda.
    gfx_cor((GfxRect){ C.x, rodY, C.w, 1.0f }, 0.0f, 1, 1, 1, 0.07f * ca);

    y = rodY + 26.0f;
    { const char *k2[2], *r2[2];
      int mais = rolarMax() > 0.5f;
      float xd = C.x + C.w - AT_PADX;
    if (est == AT_PRONTO) {
      // PRONTO: as tres etapas feitas, o check verde e a frase; no .tpk/.apk a
      // lib nova esta encenada e o botao reinicia. Na LG nao ha botao: OK fecha.
      float yc;
      etapas(x, y + 15.0f, 3, -1, ca);
      yc = y + 30.0f + 22.0f + 36.0f;
      gfx_cor((GfxRect){ x, yc - 26.0f, 52.0f, 52.0f }, 0.5f, 0.298f, 0.765f, 0.541f, 0.18f * ca);
      gfx_icone((GfxRect){ x + 12.0f, yc - 14.0f, 28.0f, 28.0f }, "aj_check", 0.298f, 0.765f, 0.541f, ca);
      { TxtLinha l = txt_linha(TXT_G30B, soEncenada ? i18n("Pronto. Reinicie o Nuvio para usar a versão nova.")
                                                     : i18n("Atualizado. Feche e abra o app para usar."), AT_TX, 255);
        txt_desenhar_alpha(l, x + 52.0f + 22.0f, yc - (float)l.h * 0.5f, ca); }
      if (soEncenada) {
        float bw = botaoAt(-10000.0f, yc, 1, i18n("Reiniciar agora"), "aj_rotate-cw", 24.0f, 1.0f, ca);
        botaoAt(xd - bw, yc, 1, i18n("Reiniciar agora"), "aj_rotate-cw", 24.0f, 1.0f, ca);
      } else {
        k2[0] = "OK"; r2[0] = i18n("Fechar");
        dicas(xd, yc, k2, r2, 1, ca);
      }
    } else if (est == AT_FALHOU) {
      // FALHOU: a etapa que falhou em vermelho; o caminho que resta e a pagina.
      float yc;
      etapas(x, y + 15.0f, 0, falhaEm, ca);
      yc = y + 30.0f + 22.0f + 33.5f;
      { TxtLinha l = txt_linha(TXT_G30B, i18n("Não foi possível atualizar por aqui."), AT_TX, 255);
        TxtLinha u = txt_linha(TXT_ILHA_CORPO, paginaCurta(), AT_TX, 255);
        txt_desenhar_alpha(l, x, y + 52.0f + (34.0f - (float)l.h) * 0.5f, ca);
        txt_desenhar_alpha(u, x, y + 52.0f + 40.0f + (27.0f - (float)u.h) * 0.5f, ca * 0.60f); }
      k2[0] = "\xe2\x86\x91 \xe2\x86\x93"; r2[0] = i18n("Mais notas");
      k2[1] = "OK"; r2[1] = i18n("Fechar");
      if (mais) dicas(xd, yc, k2, r2, 2, ca); else dicas(xd, yc, k2 + 1, r2 + 1, 1, ca);
    } else if (est == AT_INSTALANDO) {
      // BAIXANDO / CONFERINDO / INSTALANDO: as etapas, a barra com o numero do
      // proprio instalador (vazia ate ele dizer algo) e a frase do passo.
      float p = pctDaEtapa(etapa, pct), bx = x, bw = w - 22.0f - 90.0f, yb = y + 30.0f + 22.0f + 18.0f;
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      etapas(x, y + 15.0f, etapa, -1, ca);
      gfx_cor((GfxRect){ bx, yb - 7.0f, bw, 14.0f }, 0.5f, 1, 1, 1, 0.10f * ca);
      if (bw * p / 100.0f > 0.5f) {
        float fw = bw * p / 100.0f;
        gfx_cor((GfxRect){ bx, yb - 7.0f, fw, 14.0f }, fw >= 14.0f ? 0.5f : 0.0f, ar, ag, ab, ca);
        // O brilho perto da ponta (mockup: 84 px a 22%, comecando 14% antes).
        if (p < 100.0f) {
          float gx = bx + bw * (p - 14.0f > 0.0f ? p - 14.0f : 0.0f) / 100.0f, gw = 84.0f;
          if (gx + gw > bx + bw) gw = bx + bw - gx;
          if (gw > 1.0f) gfx_cor((GfxRect){ gx, yb - 7.0f, gw, 14.0f }, 0.5f, 1, 1, 1, 0.22f * ca);
        }
      }
      { char n[16];
        TxtLinha l;
        snprintf(n, sizeof n, "%d%%", (int)(p + 0.5f));
        l = txt_linha(TXT_G30B, n, AT_TX, 255);
        txt_desenhar_alpha(l, xd - (float)l.w, yb - (float)l.h * 0.5f, ca); }
      { float yc = yb + 18.0f + 14.0f + 18.0f;
        TxtLinha l = txt_linha(TXT_DET_SIN, fraseDaEtapa(etapa), AT_TX, 255);
        txt_desenhar_alpha(l, x, yc - (float)l.h * 0.5f, ca * 0.75f);
        k2[0] = "\xe2\x86\x91 \xe2\x86\x93"; r2[0] = i18n("Mais notas");
        k2[1] = i18n("Voltar"); r2[1] = i18n("Continua na ilha");
        if (mais) dicas(xd, yc, k2, r2, 2, ca); else dicas(xd, yc, k2 + 1, r2 + 1, 1, ca); }
    } else if (podeAgir()) {
      // DISPONIVEL: "Atualizar agora" (primario, 72) e "Depois" (64).
      float yc = y + 36.0f, bx = x;
#ifdef NV_ANDROID
      if (apkPerm) {
        TxtLinha l = txt_linha(TXT_DET_SIN, i18n("Permita instalar apps do Nuvio e tente de novo"), AT_TX, 255);
        txt_desenhar_alpha(l, x, rodY - 12.0f - (float)l.h, ca * 0.80f);
      }
#endif
      // So assentado: forma no lugar (a mola passa de 1) e texto inteiro.
      if (aberto && ca > 0.99f && cartaoT > 0.99f && cartaoT < 1.01f) {
        float w0 = botaoAt(-10000.0f, yc, 1, i18n("Atualizar agora"), "aj_download", 26.0f, foco == 0 ? 1.0f : 0.0f, ca);
        float w1 = botaoAt(-10000.0f, yc, 0, i18n("Depois"), NULL, 0.0f, foco == 1 ? 1.0f : 0.0f, ca);
        ponteiro_alvo(bx, yc - 36.0f, w0, 72.0f, ponteiroBotao, NULL, 0, 0);
        ponteiro_alvo(bx + w0 + 14.0f, yc - 32.0f, w1, 64.0f, ponteiroBotao, NULL, 1, 0);
      }
      bx += botaoAt(bx, yc, 1, i18n("Atualizar agora"), "aj_download", 26.0f, foco == 0 ? 1.0f : 0.0f, ca) + 14.0f;
      botaoAt(bx, yc, 0, i18n("Depois"), NULL, 0.0f, foco == 1 ? 1.0f : 0.0f, ca);
      k2[0] = "\xe2\x86\x91 \xe2\x86\x93"; r2[0] = i18n("Mais notas");
      k2[1] = i18n("Voltar"); r2[1] = i18n("Fechar");
      if (mais) dicas(xd, yc, k2, r2, 2, ca); else dicas(xd, yc, k2 + 1, r2 + 1, 1, ca);
    } else {
      // SEM INSTALADOR NESTA PLATAFORMA: o endereco da pagina e o QR dele.
      GLuint q = qrPagina();
      float yc = y + 81.0f, tx = x;
      if (q) {
        gfx_cor((GfxRect){ x, y, 162.0f, 162.0f }, 16.0f / 162.0f, 1, 1, 1, ca);
        gfx_rect((GfxRect){ x + 6.0f, y + 6.0f, 150.0f, 150.0f }, q, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, ca);
        tx = x + 162.0f + 26.0f;
      }
      { TxtLinha l = txt_linha(TXT_DET_SIN, i18n("Baixe a versão nova no celular ou no computador:"), AT_TX, 255);
        TxtLinha u = txt_linha(TXT_G30B, paginaCurta(), AT_TX, 255);
        float hT = 29.0f + 6.0f + 36.0f, y0 = yc - hT * 0.5f;
        txt_desenhar_alpha(l, tx, y0 + (29.0f - (float)l.h) * 0.5f, ca * 0.60f);
        txt_desenhar_alpha(u, tx, y0 + 35.0f + (36.0f - (float)u.h) * 0.5f, ca); }
      { float kx = xd - kbd(-10000.0f, 0, "\xe2\x86\x91 \xe2\x86\x93", i18n("Mais notas"), 0.0f);
        if (mais) {
          kbd(kx, yc - 24.0f, "\xe2\x86\x91 \xe2\x86\x93", i18n("Mais notas"), ca);
          kbd(kx, yc + 24.0f, "OK", i18n("Fechar"), ca);
        } else kbd(xd - kbd(-10000.0f, 0, "OK", i18n("Fechar"), 0.0f), yc, "OK", i18n("Fechar"), ca); }
    } }
  }
  gfx_sem_recorte();
}

