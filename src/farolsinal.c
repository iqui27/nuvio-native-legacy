// Implementacao de farolsinal.h — issue 38 (PLANO-GOSTO 1.3 e 2.2).
//
// O desenho inteiro gira em volta de uma ideia so: o que a tabela 2.2 paga e
// por VEZ (uma entrada do diario, um gesto, uma saida do player), e cada vez
// creditada fica num registro (`Cred`) com a chave da ficha usada e o delta
// que entrou. Desfazer e achar o registro e devolver o delta com sinal
// trocado — apagar entrada do diario, desmarcar visto, tirar do salvos e
// desfazer "nao e para mim" passam pelo mesmo caminho. Pendencia e o mesmo
// registro ainda sem ficha; quando ela chega o delta esperado aplica na
// hora, sem recomputar nada.
//
// Identidades (todas hash, nunca texto):
//   idh = fnv64(id)                    — a obra ("desmarcar o titulo inteiro")
//   hb  = fnv64(id | T | E | dom)      — a vez, no dominio do sinal
//   eh  = hb ^ mix(quando)             — a entrega pontual (dedup de evento)
// K_VEZ e K_GESTO dividem o dominio das vezes assistidas: o eco que a conta
// devolve de um gesto manual (ou a entrada do player que chega DEPOIS do
// check otimista) casa pela hb e vira ajuste, nunca segundo credito.
#include "farolsinal.h"
#include "dados.h"
#include "dormiu.h"
#include "farolanel.h"
#include "js.h"
#include "mapa.h"
#include "perfis.h"
#include "progresso.h"
#include "salvos.h"
#include "sessao.h"
#include "visto.h"
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef NV_UM_FIO
#include "fio1.h"
#endif

// Dominios de sinal: o "tipo" do credito, dobrado na hb para que canais de
// desfazer nao se cruzem (tirar dos salvos nao apaga o abandono etc.).
enum {
  K_VEZ,        // entrada do diario (o caminho central)
  K_GESTO,      // marcou assistido na mao (mesma hb da vez -> fusao no eco)
  K_SAIDA,      // abandono cedo na saida do player (nao desfaz com "nao vi")
  K_SALVO,
  K_TIROU,
  K_INTERESSE,
  K_RECUSA,
  K_MAIS,
  K_TEMP,       // temporada completa (uma vez por temporada)
  K_LARGADA,    // serie largada 21 dias (uma vez por titulo)
  K_NOTA        // nota antiga do Trakt (/sync/ratings, uma vez por conta)
};

#define FSIN_PEND_N   32    // fila de pendencias (secao 2.1: "fila de 32")
#define FSIN_CRED_N   512   // vezes creditadas recentes (por perfil, em disco)
#define FSIN_OBRA_N   1024  // chaves de ficha ja contadas em `titulos`
#define FSIN_PED_N    64    // pedidos de ficha para a fila lenta
#define FSIN_ALIAS_N  64    // chave pedida -> chave real da ficha
#define FSIN_ORI_N    64    // origens de Play recentes
#define FSIN_TEMP_N   8     // temporadas esperando conferida de "completa"
#define FSIN_ABAND_MIN_S  60.0     // parou antes de 1 min: nem sinal
#define FSIN_ABAND_PCT    20.0     // largou antes de 20%: abandono cedo
#define FSIN_JANELA_FALHA (5u*60u*1000u)   // ms; falha de fonte anula abandono
#define FSIN_SONO_JANELA  (2*3600)         // suspeita de sono vale 2 h
#define FSIN_LARGADA_MS   (21LL*86400LL*1000LL)   // serie largada (secao 1.3)
#define FSIN_PEND_DIAS    30
#define FSIN_GRAVA_S      60
#define DIA_EPOCA         1767225600LL    // 1/1/2026 UTC = dia 0 do modelo
#define FSIN_VEZ_MAX      512             // entregas por fatia de varredura
#define FSIN_SEQ_TODA     INT_MAX         // curSeq sentinela: grupo do cursor
                                          // consumido inteiro (cursor FS 1)
                                          // — e "diario inteiro dentro do
                                          // modelo" no modelo restaurado
#define FSIN_RESGATE_FATIA 128            // entradas por fatia do resgate
                                          // incremental do diario
#define FSIN_AQUECE_TITULOS 400           // titulos mais novos por varredura
#define FSIN_NOTAS_MAX    512             // itens de /sync/ratings por entrega

typedef struct {
  uint64_t idh, hb, eh;   // obra / vez / entrega (hashes do topo)
  long long quando;       // epoch s da vez (decai junto; tambem da o dia)
  int64_t  chave;         // chave da ficha usada no credito (ja real, se veio)
  int32_t  delta;         // o que esta valendo no modelo AGORA (Q8.8)
  int16_t  faixa;         // faixaHora usada (para desfazer na mesma celula)
  uint8_t  kind;          // K_*
} Cred;

typedef struct {
  Cred     c;             // o credito esperando ficha (eh/kind identificam a vez)
  char     id[DIARIO_ID_MAX];
  long     tmdb;
  uint8_t  serie, tent;   // tent = buscas ja feitas (teto 2; depois solta)
  uint8_t  devolve;       // 1 = divida de estorno (o delta fica POSITIVO:
                          // sinais de tabela ja nascem negativos — o sinal
                          // nao pode ser o marcador)
} Pend;

typedef struct {
  int64_t  chave;         // a chave que as pendencias usam
  char     id[DIARIO_ID_MAX];
  long     tmdb;
  int      ger;           // esqGer na hora do pedido — esquecer invalida
  uint8_t  serie;
} Ped;

typedef struct { int64_t de, para; } Ali;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condPed = PTHREAD_COND_INITIALIZER;
#define TRAVA()  pthread_mutex_lock(&trava)
#define DTRAVA() pthread_mutex_unlock(&trava)

static struct {
  int    ligado;                    // ouvintes registrados / modulo acordado
  int    perfil;                    // 0 = nenhum modelo carregado
  int    rec;                       // "Recomendar nesta TV"
  long long cur;                    // cursor da varredura (epoch s)
  int    curSeq;                    // consumidas dentro do grupo `quando`==cur
  long long curRest;                // marca do resgate: ate aqui o diario
                                    // ja era do modelo restaurado (R1)
  long long varDe;                  // so entradas mais novas que isto (resgate)
  long long varCur;                 // cursor do resgate por lotes
  int    varSeq;                    // consumidas do grupo `quando`==varCur
  int    varrer;                    // varredura de diario pendente
  int    varGer;                    // geracao do cursor (RECARREGADO/limpeza)
  uint64_t varId[FSIN_AQUECE_TITULOS];    // ids de titulo ja contados na varredura
  int    nVarId;
  int    momentoBom;                // ultimo aviso do passo: sem player aberto
  int    largadaVarrida;            // serie-largada ja conferida nesta carga
  FarolGosto g;
  FarolFichaCache fc;
  Cred   cred[FSIN_CRED_N];   int nCred;
  Pend   pend[FSIN_PEND_N];   int nPend;
  int64_t obra[FSIN_OBRA_N];  int nObra, obraProx;
  Ali    ali[FSIN_ALIAS_N];   int nAli;
  Ped    ped[FSIN_PED_N];     int nPed;
  struct { char id[DIARIO_ID_MAX]; short t; int perfil; int ger; }
         tempChk[FSIN_TEMP_N];
  int    nTempChk;
  struct { uint32_t h; uint8_t o, dia; } ori[FSIN_ORI_N];
  int    nOri, oriProx;
  int    sujo;                      // modelo/estado com gravacao pendente
  int    fichaSujo;
  long long ultGrav;
  int    trocando;                  // init/troca de perfil em andamento
  int    pendTroca;                 // perfil que a troca em curso vai ativar
  int    diaFrio;                   // varredura esperando a carga do diario
  int    recarrega;                 // troca marcada pela restore: so le o disco
  int    esqGer;                    // subiu no esquecer/zera: invalida gravacao
  int    fcLido;                    // arquivo de fichas ja entrou nesta carga
  char   dir[768];
} S;

static FarolSinalBusca busca;
static int      fioVivo;
static _Atomic int fioMorre;   // lido solto pelos fios antes de cada dep. externa
static pthread_t fioId;
// O fio de tras do passo: conferidas de temporada, varredura de largada e
// gravacoes — tudo o que a r2 viu parar o quadro (correcao FAROL38C).
static pthread_cond_t condTrab = PTHREAD_COND_INITIALIZER;
static pthread_cond_t condTroca = PTHREAD_COND_INITIALIZER;  // init/troca feita
static int      fioTrabVivo, fioTrabOcup;
static pthread_t fioTrabId;

static long long (*f_agora_s)(void);
static uint32_t  (*f_agora_ms)(void);
static long long agoraS(void) { return f_agora_s ? f_agora_s() : (long long)time(NULL); }
static uint32_t  agoraMs(void) {
  if (f_agora_ms) return f_agora_ms();
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

// ---------------------------------------------------------------- hash/dia --
static uint64_t mix64(uint64_t x) {
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
  return x;
}
static uint64_t fnv(const char *s) {
  uint64_t h = 1469598103934665603ULL;
  if (s) while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
  return h;
}
static uint64_t h1(const char *id) { return fnv(id); }
static uint64_t h3(const char *id, int t, int e) {
  return fnv(id) ^ mix64((uint64_t)(unsigned)t * 0x9E3779B1u +
                         (uint64_t)(unsigned)e);
}
// hb de um sinal avulso: a obra no dominio `kind` (+ um inteiro, quando o
// sinal vale por temporada/dia).
static uint64_t hDom(const char *id, int kind, int n) {
  return fnv(id) ^ mix64((uint64_t)kind * 0xD6E8FEB86659FD93ULL + (unsigned)n);
}
static int diaDe(long long quando) {
  long long d = (quando - DIA_EPOCA) / 86400;
  if (d < 0) d = 0;
  if (d > 65535) d = 65535;
  return (int)d;
}
// "dia_semana*24 + hora" (DiarioEntrada.hora) -> faixa 0-7 do modelo:
// fds (dom/sab) * 4 + [manha, tarde, noite, madrugada].
static int faixaDeHora(int hora167) {
  int wd, h, p;
  if (hora167 < 0 || hora167 > 167) return -1;
  wd = hora167 / 24; h = hora167 % 24;
  p = h < 6 ? 3 : h < 12 ? 0 : h < 18 ? 1 : 2;
  return ((wd == 0 || wd == 6) ? 4 : 0) + p;
}
static int faixaAgora(long long quando) {
  time_t t = (time_t)quando; struct tm tmv;
  localtime_r(&t, &tmv);
  return faixaDeHora(tmv.tm_wday * 24 + tmv.tm_hour);
}

// ----------------------------------------------------------------- caminho --
static int caminho(const char *nome, char *dst, size_t cap) {
  int n = snprintf(dst, cap, "%s/%s", S.dir, nome);
  return n > 0 && (size_t)n < cap;
}
static void nomeNotas(char *dst, size_t cap) {
  // A marca e da CONTA (uma leitura so por conta, secao 1.3): duas contas na
  // mesma TV dividem dados_dir, entao o nome carrega o hash do usuario —
  // o mesmo FNV do plugins-%08llx de ajustes_device.inc.
  unsigned long long h = 1469598103934665603ULL;
  const char *p;
  for (p = sessao_usuario(); *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211ULL; }
  snprintf(dst, cap, "farol-notas-%08llx.txt", h & 0xffffffffULL);
}
static void nomeEstado(int perfil, char *dst, size_t cap) {
  snprintf(dst, cap, "farol-sinal-p%d.txt", perfil);
}
static void nomeGosto(int perfil, char *dst, size_t cap) {
  farolgosto_nome((uint16_t)perfil, dst, cap);
}

// ------------------------------------------------------------------ modelo --
// Toda E/S de arquivo segue a mesma regra (correcao FAROL38D): o CONTEUDO
// sai do estado com a trava tomada (texto/serializacao = copia) e vai ao
// disco SEM ela — o fio da UI nunca espera flash, nem no fio de tras nem
// na troca de perfil; e a leitura chega em buffer, o parse e que entra na
// trava. tmp por chamada (seq): escritores concorrentes nao se atropelam.
static char *textoModelo(size_t *np) {        // com a trava tomada; malloc
  char *b = malloc(FAROL_GOSTO_TXT_MAX + 1);
  size_t n;
  if (!b) return NULL;
  n = farolgosto_escrever(&S.g, b, FAROL_GOSTO_TXT_MAX);
  if (!n) { free(b); return NULL; }           // nao coube: nao grava por cima
  *np = n;
  return b;
}
static unsigned char *textoFichas(size_t *np) {  // com a trava tomada; malloc
  unsigned char *b = malloc(FAROL_FICHA_BIN_MAX);
  size_t n;
  if (!b) return NULL;
  n = farolficha_escrever(&S.fc, b, FAROL_FICHA_BIN_MAX);
  if (!n) { free(b); return NULL; }
  *np = n;
  return b;
}
// Arquivo inteiro para memoria — chamado FORA da trava.
static unsigned char *lerArq(const char *cam, size_t *np) {
  FILE *f = fopen(cam, "rb");
  unsigned char *b;
  long n;
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET))
    { fclose(f); return NULL; }
  b = malloc((size_t)n ? (size_t)n : 1);
  if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
  fclose(f);
  if (b) *np = (size_t)n;
  return b;
}
// buf -> disco atomico (.tmp + rename), sem a trava na E/S. O rename final
// entra na trava so para a fronteira de encerramento: depois do sair ou se
// o esquecer/zera andou (geracao `ger` mudou), so o escritor `final`
// instala — um flush atrasado nao pisa na saida nem ressuscita arquivo
// apagado. 1 = ok.
static int gravaBuf(const char *cam, const void *buf, size_t n, int final,
                    int ger) {
  static _Atomic int seq;
  char tmp[1200];
  FILE *f;
  int ok;
  snprintf(tmp, sizeof tmp, "%s.tmp.%d", cam,
           atomic_fetch_add(&seq, 1) & 0xffff);
  f = fopen(tmp, "w");
  if (!f) return 0;
  ok = fwrite(buf, 1, n, f) == n;
  if (fclose(f)) ok = 0;
  if (!ok) { remove(tmp); return 0; }
  TRAVA();
  if (final || (!fioMorre && S.esqGer == ger)) rename(tmp, cam);
  else remove(tmp);
  DTRAVA();
  return 1;
}

// Ida e volta do modelo pela copia em memoria + o caminho de escrita real —
// funcoes que so a sonda da r1 chama direto (correcao FAROL38D: a producao
// grava por gravaBuf fora da trava; estas duas ficam para o teste do formato
// persistido).
__attribute__((unused)) static void gravaModelo(void) {   // com a trava tomada
  char cam[1100], tmp[1200], nome[64];
  size_t n; char *b; FILE *f;
  if (S.perfil < 1) return;
  b = textoModelo(&n);
  if (!b) return;
  nomeGosto(S.perfil, nome, sizeof nome);
  if (caminho(nome, cam, sizeof cam)) {
    snprintf(tmp, sizeof tmp, "%s.tmp", cam);
    if ((f = fopen(tmp, "w"))) {
      int ok = fwrite(b, 1, n, f) == n;
      if (fclose(f)) ok = 0;
      if (ok) rename(tmp, cam); else remove(tmp);
    }
  }
  free(b);
}
__attribute__((unused)) static void leModelo(void) {   // com a trava tomada
  char cam[1100], nome[64];
  size_t n = 0; unsigned char *b;
  farolgosto_iniciar(&S.g, (uint16_t)S.perfil);
  nomeGosto(S.perfil, nome, sizeof nome);
  if (!caminho(nome, cam, sizeof cam)) return;
  b = lerArq(cam, &n);
  if (b) { farolgosto_ler(&S.g, (char *)b, n); free(b); }
}


// ------------------------------------------------------------- conjuntos ----
static int64_t aliasPara(int64_t c) {
  int i;
  for (i = 0; i < S.nAli; i++) if (S.ali[i].de == c) return S.ali[i].para;
  return c;
}
static void aliasPoe(int64_t de, int64_t para) {
  int i;
  if (!de || !para || de == para) return;
  for (i = 0; i < S.nAli; i++)
    if (S.ali[i].de == de) { S.ali[i].para = para; break; }
  if (i >= S.nAli) {
    if (S.nAli < FSIN_ALIAS_N) S.ali[S.nAli].de = de,
                               S.ali[S.nAli].para = para, S.nAli++;
    else { S.ali[S.nAli - 1].de = de; S.ali[S.nAli - 1].para = para; }
  }
  // A reserva ja contada migra para a chave real: sem isso a mesma obra
  // contava duas vezes em `titulos` (correcao FAROL38B). Se as duas chaves
  // ja estavam no anel a reserva sai — o real fica como a unica reserva.
  for (i = 0; i < S.nObra; i++)
    if (S.obra[i] == de) {
      int j, dup = 0;
      for (j = 0; j < S.nObra; j++)
        if (j != i && S.obra[j] == para) { dup = 1; break; }
      if (dup) { S.obra[i] = S.obra[S.nObra - 1]; S.nObra--; }
      else S.obra[i] = para;
      break;
    }
  S.sujo = 1;
}
static int obraVista(int64_t chave) {
  int i;
  for (i = 0; i < S.nObra; i++) if (S.obra[i] == chave) return 1;
  return 0;
}
static void obraMarca(int64_t chave) {
  int i;
  if (obraVista(chave)) return;
  if (S.nObra < FSIN_OBRA_N) i = S.nObra++;
  else { i = S.obraProx; S.obraProx = (S.obraProx + 1) % FSIN_OBRA_N; }
  S.obra[i] = chave; S.sujo = 1;
}
static int achaEh(uint64_t eh) {  // vaga de cred pela entrega exata; -1 sem
  int i;
  for (i = 0; i < S.nCred; i++) if (S.cred[i].eh == eh) return i;
  return -1;
}
// A vez no dominio visto/gesto com o `quando` mais perto (a fusao do diario
// pode mexer alguns segundos; o limite e a mesma janela de 6 h).
static int achaVez(uint64_t hb, long long perto) {
  int i, ach = -1;
  long long melhor = DIARIO_JANELA_S + 1;
  for (i = 0; i < S.nCred; i++) {
    long long d;
    if (S.cred[i].hb != hb || S.cred[i].kind > K_GESTO) continue;
    d = S.cred[i].quando > perto ? S.cred[i].quando - perto
                               : perto - S.cred[i].quando;
    if (d < melhor) { melhor = d; ach = i; }
  }
  return ach;
}
static int achaHb(uint64_t hb) {  // qualquer dominio: "ja recebeu este sinal?"
  int i;
  for (i = 0; i < S.nCred; i++) if (S.cred[i].hb == hb) return i;
  return -1;
}
// Algum registro (de qualquer dominio) ja conta este titulo? A nota antiga
// do Trakt so aquece quem o modelo ainda nao conhece (issue 43).
static int achaIdh(uint64_t idh) {
  int i;
  for (i = 0; i < S.nCred; i++) if (S.cred[i].idh == idh) return i;
  return -1;
}
static int pendTemIdh(uint64_t idh) {   // pendencia viva para o titulo?
  int i;
  for (i = 0; i < S.nPend; i++)
    if (S.pend[i].c.idh == idh && !S.pend[i].devolve) return i;
  return -1;
}
// Quanto deste delta de vez/gesto ainda pode entrar: a nota antiga do
// Trakt e a vez do diario sao o MESMO "gostou" do titulo — a tabela 2.2
// da "terminou e gostou = +2,0", nao a soma dos dois sinais (ressalva R2
// da r1: nota 8 + "terminou" somava +3,0 e o resultado mudava com a ordem
// de chegada). Fica o excedente sobre a nota positiva; sinal negativo nao
// e coberto (a reprova passa inteira). O `delta` gravado no Cred e sempre
// a parcela que entrou no modelo; as diferencas de ajuste/apagada se medem
// sobre o valor LIVRE dele — com a nota chegando depois, o excedente da
// vez encolhe junto.
static int deltaLivre(uint64_t idh, int delta) {
  int j;
  if (delta <= 0) return delta;
  for (j = 0; j < S.nCred; j++)
    if (S.cred[j].idh == idh && S.cred[j].kind == K_NOTA &&
        S.cred[j].delta > 0) {
      delta -= S.cred[j].delta;
      return delta > 0 ? delta : 0;
    }
  return delta;
}
static void credPoe(const Cred *c) {
  int i;
  if (S.nCred < FSIN_CRED_N) i = S.nCred++;
  else {  // cheio: sai a mais velha por `quando`
    int j; i = 0;
    for (j = 1; j < S.nCred; j++) if (S.cred[j].quando < S.cred[i].quando) i = j;
  }
  S.cred[i] = *c;
}
static void credTira(int i) {
  if (i >= 0 && i < S.nCred) S.cred[i] = S.cred[--S.nCred];
}
// A vez (K_VEZ/K_GESTO) desta hb ja creditou? Sem janela — para conferir
// "episodio visto" dentro da trava, sem perguntar ao diario.
static int vezCredida(uint64_t hb) {
  int i;
  for (i = 0; i < S.nCred; i++)
    if (S.cred[i].hb == hb && S.cred[i].kind <= K_GESTO) return 1;
  return 0;
}
static void pendTira(int i) {
  if (i >= 0 && i < S.nPend) S.pend[i] = S.pend[--S.nPend];
}

// ------------------------------------------------------------------ ficha --
// A chave que um sinal pede: com TMDB a do detalhe; sem ele a reserva do
// proprio id — a MESMA conta de reservaDeId+farolficha_chave, entao um
// CatItem sem TMDB gera a mesma chave negativa da ficha que ele monta
// (ressalva do lote: catalogo e detalhe nao podem cobrar duas vezes).
static int64_t chaveObra(const char *id, int serie, long tmdb) {
  int64_t ff = serie ? FF_SERIE : FF_FILME;
  if (tmdb <= 0 && id && !strncmp(id, "tmdb:", 5)) tmdb = strtol(id + 5, NULL, 10);
  if (tmdb > 0) return (ff << 32) | (int64_t)(uint32_t)tmdb;
  if (!id || !id[0]) return 0;
  return -((ff << 32) | (int64_t)(0x40000000u | (mapa_hash_titulo(id) & 0x3fffffffu)));
}
// Tenta a ficha pela chave (ja traduzida por alias), depois pelo catalogo.
// Devolve a CHAVE REAL (a positiva do TMDB ou a reserva da ficha montada), ou
// 0 quando nao ha ficha ainda. Com a trava tomada — cache e arquivo so andam
// aqui dentro (ressalva: nada de leitura e escrita concorrentes).
static int64_t pegaFicha(int64_t chave, const char *id, int serie,
                         FarolFicha *f) {
  int idx;
  const CatItem *ci;
  int64_t real;
  (void)serie;   // a ficha do CatItem ja sabe o tipo dela
  if (farolficha_obter(&S.fc, chave, f))
    return farolficha_chave(f);
  if (id && id[0] && (idx = cat_indice_por_imdb(id)) >= 0 &&
      (ci = cat_item(idx)) != NULL && farolficha_ler_item(ci, f)) {
    real = farolficha_chave(f);
    // Cache ja tem ficha nesta chave (a detalhada do TMDB): NAO sobrescreve
    // com a reconstrucao pobre do catalogo (correcao FAROL38B).
    if (farolficha_obter(&S.fc, real, f)) {
      if (real != chave) aliasPoe(chave, real);
      return real;
    }
    farolficha_guardar(&S.fc, f);
    S.fichaSujo = 1;
    if (real != chave) aliasPoe(chave, real);
    return real;
  }
  return 0;
}

static void pedeFicha(int64_t chave, const char *id, int serie, long tmdb);
static void fichaOk(int64_t chavePed, const FarolFicha *f, int ger);
static void fichaFalhou(int64_t chavePed, int ger);

// Credito esperando ficha; com `devolve` e a devolucao tardia (estorno que
// faltou ficha) — dominio separado: uma intencao e uma divida do mesmo eh
// nao se confundem.
static void pendPoe(const Cred *c, const char *id, int serie, long tmdb,
                    int devolve) {
  int i;
  for (i = 0; i < S.nPend; i++)      // a mesma entrega nao entra duas vezes
    if (S.pend[i].c.eh == c->eh && S.pend[i].c.kind == c->kind &&
        S.pend[i].devolve == !!devolve) {
      S.pend[i].c.delta = c->delta; S.sujo = 1; return;
    }
  if (S.nPend >= FSIN_PEND_N) {  // teto 32: sai a mais velha (secao 2.1)
    int j; i = 0;
    for (j = 1; j < S.nPend; j++)
      if (S.pend[j].c.quando < S.pend[i].c.quando) i = j;
    pendTira(i);
  }
  i = S.nPend++;
  memset(&S.pend[i], 0, sizeof S.pend[i]);
  S.pend[i].c = *c;
  snprintf(S.pend[i].id, sizeof S.pend[i].id, "%s", id ? id : "");
  S.pend[i].tmdb = tmdb; S.pend[i].serie = (uint8_t)!!serie;
  S.pend[i].devolve = (uint8_t)!!devolve;
  S.sujo = 1;
  pedeFicha(c->chave, id, serie, tmdb);
}

// Converte a ficha da 36 para a ficha do modelo (os mesmos campos que a
// integracao de aplicar sempre montou).
static void montaGf(const FarolFicha *f, FarolGostoFicha *g) {
  memset(g, 0, sizeof *g);
  g->tmdb = f->tmdb;
  g->tipo = f->tipo == FF_SERIE ? FG_TIPO_SERIE : FG_TIPO_FILME;
  g->ano = (int16_t)f->ano;
  g->idioma = f->idioma;
  g->minutos = (int16_t)f->minutos;
  memcpy(g->genero, f->genero, sizeof g->genero);
  memcpy(g->pessoa, f->pessoa, sizeof g->pessoa);
  memcpy(g->tema, f->tema, sizeof g->tema);
}

// Converte e aplica; `obraNova` sai do anel de chaves (contrato do plano 3.4:
// quarenta episodios da mesma serie valem um titulo).
static void aplicar(const FarolFicha *f, int64_t real, int delta,
                    int dia, int faixa) {
  FarolGostoFicha g;
  int nova;
  montaGf(f, &g);
  nova = !obraVista(real);
  if (nova) obraMarca(real);
  farolgosto_aplicar(&S.g, &g, delta, (uint16_t)dia, faixa, nova);
}

// Devolve EXATAMENTE o que esta vez somou (correcao FAROL38B): por celula,
// depois[k] conta os creds vivos mais novos que ela na mesma celula — o
// estorno acha a saturacao com que ela entrou mesmo com creds ja despejados.
// So o cache fala: devolver nao remonta ficha do catalogo.
static void estornaCred(const FarolFicha *f, const Cred *c) {
  FarolGostoFicha g;
  int dims[FG_CEL_N], depois[FG_CEL_N], n, i;
  int32_t ids[FG_CEL_N];
  montaGf(f, &g);
  n = farolgosto_celulas(&g, dims, ids, FG_CEL_N);
  memset(depois, 0, sizeof depois);
  for (i = 0; i < S.nCred; i++) {
    const Cred *o = &S.cred[i];
    FarolFicha fo; FarolGostoFicha go;
    int dm[FG_CEL_N], m, a, b;
    int32_t ii[FG_CEL_N];
    int64_t ro;
    if (o->quando <= c->quando) continue;
    ro = aliasPara(o->chave);
    if (!ro || !farolficha_obter(&S.fc, ro, &fo)) continue;
    montaGf(&fo, &go);
    m = farolgosto_celulas(&go, dm, ii, FG_CEL_N);
    for (a = 0; a < m; a++)
      for (b = 0; b < n; b++)
        if (dm[a] == dims[b] && ii[a] == ids[b]) { depois[b]++; break; }
  }
  farolgosto_estornar(&S.g, &g, c->delta, depois,
                      (uint16_t)diaDe(c->quando), c->faixa);
}

// Solta as pendencias cuja chave (traduzida) e a ficha que acabou de entrar.
static void drenaPends(int64_t real) {
  int i;
  FarolFicha f;
  for (i = S.nPend - 1; i >= 0; i--) {
    Pend p;
    if (aliasPara(S.pend[i].c.chave) != real) continue;
    // Coleta desligada: a ficha em voo nao credita nada — a intencao cai;
    // a DIVIDA de estorno espera a coleta voltar, porque ela e correcao,
    // nao coleta (correcao FAROL38B).
    if (!S.rec) {
      if (!S.pend[i].devolve) { pendTira(i); S.sujo = 1; }
      continue;
    }
    if (!farolficha_obter(&S.fc, real, &f)) continue;
    p = S.pend[i]; pendTira(i);
    if (p.devolve) {   // devolucao tardia: estorno, nunca credito novo
      estornaCred(&f, &p.c);
      S.sujo = 1;
      continue;          // a devolucao nao vira Cred: a vez ja saiu do anel
    }
    aplicar(&f, real, p.c.delta, diaDe(p.c.quando), p.c.faixa);
    p.c.chave = real;
    credPoe(&p.c);
  }
}
// Depois de um credito direto, a pendencia da MESMA entrega nao pode ficar:
// senao a ficha que chegar depois a aplicaria de novo.
static void pendCancelaEntrega(uint64_t eh, int kind) {
  int i;
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.eh == eh && S.pend[i].c.kind == kind)
      { pendTira(i); S.sujo = 1; }
}

// O credito de uma vez: resolve a ficha (cache -> catalogo) ou pendencia +
// pedido para a fila lenta. Com a trava tomada.
static void credita(const char *id, int serie, long tmdb, const Cred *c) {
  int64_t real;
  FarolFicha f;
  Cred cc = *c;
  if (!cc.chave) return;                 // id vazio: sem ficha nem fila
  real = pegaFicha(aliasPara(cc.chave), id, serie, &f);
  if (!real) { pendPoe(&cc, id, serie, tmdb, 0); return; }
  if (real != cc.chave) aliasPoe(cc.chave, real);
  aplicar(&f, real, cc.delta, diaDe(cc.quando), cc.faixa);
  cc.chave = real;
  credPoe(&cc);
  pendCancelaEntrega(cc.eh, cc.kind);
  drenaPends(real);
  S.sujo = 1;
}

// Devolve a contribuicao gravada de um Cred — o caminho unico de "desfazer":
// a chave guardada busca a ficha pela mesma estrada do credito e o estorno
// sai celula a celula, sem residuo e sem evidencia a mais (correcao
// FAROL38B). Sem ficha por perto a devolucao vira pendencia DEVOLVE (o dia
// e a faixa originais vao junto, para sair da mesma celula onde entrou) —
// e divida de estorno NAO e cancelada pelas varreduras de intencao.
static void desfaz(Cred *c, const char *id, int serie, long tmdb) {
  int64_t real;
  FarolFicha f;
  real = pegaFicha(aliasPara(c->chave), id, serie, &f);
  if (real) { estornaCred(&f, c); S.sujo = 1; }
  else pendPoe(c, id, serie, tmdb, 1);
}

// ----------------------------------------------------------------- estado --
// Serializa o estado inteiro para um buffer — mesmo texto do antigo
// gravaEstado, so muda o destino (correcao FAROL38D: o flush sai da trava).
static char *textoEstado(size_t *np) {   // com a trava tomada; malloc
  size_t cap = 64 + (size_t)S.nPend * 240 + (size_t)S.nCred * 160 +
               (size_t)S.nObra * 24 + (size_t)S.nAli * 48 +
               (size_t)S.nOri * 24;
  char *b = malloc(cap), *w = b, *fim;
  int i;
  if (!b) return NULL;
  fim = b + cap;
  w += snprintf(w, (size_t)(fim - w), "FS 2 %d %d %lld %d\n",
                S.perfil, S.rec, S.cur, S.curSeq);
  for (i = 0; i < S.nPend; i++) {
    const Pend *p = &S.pend[i];
    w += snprintf(w, (size_t)(fim - w),
                  "p %lld %llu %llu %llu %lld %d %d %u %u %u %u %ld %s\n",
                  (long long)p->c.chave, (unsigned long long)p->c.idh,
                  (unsigned long long)p->c.hb, (unsigned long long)p->c.eh,
                  p->c.quando, (int)p->c.delta, (int)p->c.faixa,
                  (unsigned)p->c.kind, (unsigned)p->tent, (unsigned)p->serie,
                  (unsigned)p->devolve, p->tmdb, p->id);
  }
  for (i = 0; i < S.nCred; i++) {
    const Cred *c = &S.cred[i];
    w += snprintf(w, (size_t)(fim - w), "c %llu %llu %llu %lld %lld %d %d %u\n",
                  (unsigned long long)c->idh, (unsigned long long)c->hb,
                  (unsigned long long)c->eh, c->quando, (long long)c->chave,
                  (int)c->delta, (int)c->faixa, (unsigned)c->kind);
  }
  for (i = 0; i < S.nObra; i++)
    w += snprintf(w, (size_t)(fim - w), "o %lld\n", (long long)S.obra[i]);
  for (i = 0; i < S.nAli; i++)
    w += snprintf(w, (size_t)(fim - w), "a %lld %lld\n",
                  (long long)S.ali[i].de, (long long)S.ali[i].para);
  for (i = 0; i < S.nOri; i++)
    w += snprintf(w, (size_t)(fim - w), "i %u %u %u\n",
                  (unsigned)S.ori[i].h, (unsigned)S.ori[i].o,
                  (unsigned)S.ori[i].dia);
  // "r <quando>": marca do resgate — entradas do diario ate esse instante
  // ja constavam do modelo quando ele foi restaurado; so o que e mais novo
  // volta a pesar (ressalva R1 da r1: o cursor sentinela deixava todo lote
  // novo do diario de fora para sempre).
  if (S.curRest > 0)
    w += snprintf(w, (size_t)(fim - w), "r %lld\n", S.curRest);
  *np = (size_t)(w - b);
  return b;
}
// Aplica o estado lido em buffer — o texto de gravaEstado, exigindo o
// terminador de cada linha: registro cortado no fim (sem '\n') rejeita o
// arquivo INTEIRO, igual a qualquer outro malformado (correcao FAROL38D —
// ressalva da r3: um id mordido pelo %31s ainda entrava).
static void aplicaEstado(char *b, size_t n) {   // com a trava tomada
  char *s = b, *fim = b + n, *nl;
  int i, mal = 0;
  // A primeira linha TEM de ser o cabecalho "FS <ver> <perfil> <rec> <cur>
  // [<curSeq>]": versao desconhecida, perfil de outro ou arquivo
  // truncado/corrompido — recusa o arquivo INTEIRO (correcao FAROL38B). FS 1
  // nao guardava a posicao dentro do grupo de mesmo `quando`: o cursor
  // exclusivo dele vale como "o grupo de S.cur ja entrou inteiro".
  nl = memchr(s, '\n', (size_t)(fim - s));
  if (!nl) return;
  *nl = 0;
  {
    int p = 0, r = 0, sq = 0; long long c = 0; char resto[8] = "";
    if (sscanf(s, "FS 2 %d %d %lld %d %7s", &p, &r, &c, &sq, resto) == 4 &&
        p == S.perfil && sq >= 0) {
      S.rec = !!r; S.cur = c; S.curSeq = sq;
    } else if (sscanf(s, "FS 1 %d %d %lld %7s", &p, &r, &c, resto) == 3 &&
               p == S.perfil) {
      S.rec = !!r; S.cur = c; S.curSeq = FSIN_SEQ_TODA;
    } else return;
  }
  for (s = nl + 1; s < fim; s = nl + 1) {
    nl = memchr(s, '\n', (size_t)(fim - s));
    if (!nl) { mal = 1; break; }            // ultimo registro sem LF: cortado
    *nl = 0;
    if (s[0] == 'p' && S.nPend < FSIN_PEND_N) {
      Pend *p = &S.pend[S.nPend];
      int d, fx, k, te, sr, dv, pos = -1;
      long long ch, q;
      unsigned long long ih, hb, eh;
      memset(p, 0, sizeof *p);
      // Registro de pendencia e TUDO ou nada: 13 campos e kind valido, sem
      // sobra depois do id (o %n no fim pega o id mordido ou o campo a mais).
      if (sscanf(s + 1,
                 "%lld %llu %llu %llu %lld %d %d %d %d %d %d %ld %31s%n",
                 &ch, &ih, &hb, &eh, &q, &d, &fx, &k, &te, &sr, &dv, &p->tmdb,
                 p->id, &pos) == 13 && k >= 0 && k <= K_NOTA &&
          pos > 0 && !s[1 + pos]) {
        p->c.chave = ch; p->c.idh = ih; p->c.hb = hb; p->c.eh = eh;
        p->c.quando = q; p->c.delta = d; p->c.faixa = (int16_t)fx;
        p->c.kind = (uint8_t)k; p->tent = (uint8_t)te; p->serie = (uint8_t)sr;
        p->devolve = (uint8_t)!!dv;
        S.nPend++;
      } else { mal = 1; break; }
    } else if (s[0] == 'c' && S.nCred < FSIN_CRED_N) {
      Cred *c = &S.cred[S.nCred];
      int d, fx, k, pos = -1; long long ch, q;
      unsigned long long ih, hb, eh;
      if (sscanf(s + 1, "%llu %llu %llu %lld %lld %d %d %d%n",
                 &ih, &hb, &eh, &q, &ch, &d, &fx, &k, &pos) == 8 &&
          pos > 0 && !s[1 + pos]) {
        c->idh = ih; c->hb = hb; c->eh = eh; c->quando = q; c->chave = ch;
        c->delta = d; c->faixa = (int16_t)fx; c->kind = (uint8_t)k;
        S.nCred++;
      } else { mal = 1; break; }
    } else if (s[0] == 'o' && S.nObra < FSIN_OBRA_N) {
      long long ch; int pos = -1;
      if (sscanf(s + 1, "%lld%n", &ch, &pos) == 1 && pos > 0 && !s[1 + pos])
        S.obra[S.nObra++] = ch;
      else { mal = 1; break; }
    } else if (s[0] == 'a') {
      long long de, para; int pos = -1;
      if (sscanf(s + 1, "%lld %lld%n", &de, &para, &pos) == 2 &&
          pos > 0 && !s[1 + pos])
        aliasPoe(de, para);
      else { mal = 1; break; }
    } else if (s[0] == 'i' && S.nOri < FSIN_ORI_N) {
      unsigned h, o, d; int pos = -1;
      if (sscanf(s + 1, "%u %u %u%n", &h, &o, &d, &pos) == 3 &&
          pos > 0 && !s[1 + pos]) {
        S.ori[S.nOri].h = h; S.ori[S.nOri].o = (uint8_t)o;
        S.ori[S.nOri].dia = (uint8_t)d; S.nOri++;
      } else { mal = 1; break; }
    } else if (s[0] == 'r') {
      long long m; int pos = -1;
      if (sscanf(s + 1, "%lld%n", &m, &pos) == 1 && pos > 0 && !s[1 + pos] &&
          m > S.curRest)
        S.curRest = m;
      else { mal = 1; break; }
    }
    // Linha de tipo desconhecido, ou alem do teto da fila, nao e corte:
    // ignora, como a leitura anterior fazia.
  }
  if (mal) {   // estado parcial nao sobe: recomeça do zero como o cabecalho
    S.nPend = S.nCred = S.nObra = S.obraProx = S.nAli = 0;
    S.nOri = S.oriProx = 0;
    S.rec = 1; S.cur = 0; S.curSeq = 0; S.curRest = 0;
    return;
  }
  // Pendencia que veio do disco volta a fila de busca: o reinicio nao pode
  // deixar ficha pedida para tras (correcao FAROL38B).
  for (i = 0; i < S.nPend; i++)
    if (S.pend[i].tent < 2)
      pedeFicha(S.pend[i].c.chave, S.pend[i].id, S.pend[i].serie,
                S.pend[i].tmdb);
}

// Arma uma varredura do diario de volta ao comeco — cursor zerado e o
// conjunto de titulos contados esvaziado junto: a leva cobre os
// FSIN_AQUECE_TITULOS titulos mais novos (issue 43, PLANO-GOSTO 1.3), nao
// os 400 primeiros que passarem por acaso. A dupla (cur no fundo, curSeq
// TODA) e o marcador "o diario inteiro ja esta no modelo" — gravado no
// estado quando um modelo de backup sobe sem o anel de creds (issue 43):
// repassar dobraria cada peso restaurado, e entradas novas continuam
// chegando pela NOVA de sempre.
static void armaVarredura(void) {   // com a trava tomada
  if (S.cur == LLONG_MIN && S.curSeq == FSIN_SEQ_TODA) {
    // Modelo restaurado (sentinela): a marca curRest diz onde o diario que
    // ja era dele termina — o resgate em lotes cobre so o mais novo que
    // ela (ressalva R1 da r1: antes a sentinela segurava o passe para
    // sempre e o lote novo da conta nunca pesava). Sem marca ainda —
    // diario frio, nao medido — nao ha fronteira segura: espera.
    if (!S.curRest) return;
    S.varDe = S.curRest; S.varCur = 0; S.varSeq = 0; S.nVarId = 0;
    S.varrer = 1; S.varGer++;
    return;
  }
  S.cur = 0; S.curSeq = 0; S.varDe = 0; S.varCur = 0; S.varSeq = 0;
  S.varrer = 1; S.varGer++; S.nVarId = 0;
}

// Varredura vazia que so dispara a carga do diario neste fio (de impossivel
// casa nada): a leitura fria fica no fio de tras, nunca no passo.
static int cbNada(const DiarioEntrada *e, void *ctx) {
  (void)e; (void)ctx; return 0;
}
static void aqueceDiario(int perfil) {
  DiarioFiltro f;
  memset(&f, 0, sizeof f);
  f.de = LLONG_MAX;
  diario_percorrer(perfil, &f, cbNada, NULL);
}
// O `quando` da entrada mais nova do diario — a marca que separa "ja era do
// modelo restaurado" de "chegou depois" (ressalva R1 da r1).
static int cbMarca(const DiarioEntrada *e, void *ctx) {
  *(long long *)ctx = e->quando;
  return 0;
}
static long long marcaDiario(int perfil) {
  DiarioFiltro f;
  long long marca = 0;
  memset(&f, 0, sizeof f);
  f.decrescente = 1;
  diario_percorrer(perfil, &f, cbMarca, &marca);
  return marca;
}

// Troca o modelo carregado — a fronteira entre perfis: grava o que sai,
// zera tudo o que e do perfil e carrega o que entra. NENHUMA vaga de
// cred/pend cruza a fronteira (ressalva: delta nao vaza para outro perfil).
// Roda NO FIO DE TRAS (correcao FAROL38D): o conteudo que sai e serializado
// sob a trava e os arquivos que entram sao lidos fora dela; a E/S nunca
// segura a trava e a aplicacao so acontece se a troca ainda for a pedida —
// perfil mudou de novo, esquecer ou sair descartam o resultado.
static void fioTrocaPerfil(void) {
  char cam[1100], nome[64];
  char *gm = NULL, *ge = NULL, *bG = NULL, *bE = NULL;
  unsigned char *gf = NULL, *bF = NULL;
  size_t nGm = 0, nGe = 0, nGf = 0, nBG = 0, nBE = 0, nBF = 0;
  int pAnt, pNovo, ger, eGer, leF, soLer;

  TRAVA();
  pAnt = S.perfil; pNovo = S.pendTroca;
  ger = S.varGer; eGer = S.esqGer; leF = !S.fcLido;
  // Recarrega do backup (issue 43): troca para o MESMO perfil so para ler o
  // que a restauracao gravou — o modelo em RAM nao volta ao disco primeiro,
  // senao a versao velha em memoria pisava no arquivo recem-restaurado.
  soLer = S.recarrega && pNovo == pAnt;
  S.recarrega = 0;
  if (pAnt > 0 && !soLer) { gm = textoModelo(&nGm); ge = textoEstado(&nGe); }
  if (S.fc.sujo && !soLer) gf = textoFichas(&nGf);
  if (!soLer) { S.sujo = 0; S.fichaSujo = 0; }
  DTRAVA();

  if (pAnt > 0) {
    if (gm) { nomeGosto(pAnt, nome, sizeof nome);
      if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, gm, nGm, 0, eGer); }
    if (ge) { nomeEstado(pAnt, nome, sizeof nome);
      if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, ge, nGe, 0, eGer); }
  }
  if (gf && caminho("farol-fichas.bin", cam, sizeof cam))
    gravaBuf(cam, gf, nGf, 0, eGer);
  free(gm); free(ge); free(gf);

  if (!fioMorre && pNovo > 0) {
    nomeGosto(pNovo, nome, sizeof nome);
    if (caminho(nome, cam, sizeof cam)) bG = (char *)lerArq(cam, &nBG);
    nomeEstado(pNovo, nome, sizeof nome);
    if (caminho(nome, cam, sizeof cam)) bE = (char *)lerArq(cam, &nBE);
    if (leF && caminho("farol-fichas.bin", cam, sizeof cam))
      bF = lerArq(cam, &nBF);
  } else if (!fioMorre && leF && caminho("farol-fichas.bin", cam, sizeof cam))
    bF = lerArq(cam, &nBF);

  TRAVA();
  // So aplica se a troca ainda e a pedida e a fronteira nao andou: perfil
  // mudou de novo, esquecer ou sair descartam tudo o que foi lido.
  if (!fioMorre && S.trocando && S.pendTroca == pNovo &&
      S.perfil == pAnt && S.varGer == ger && S.esqGer == eGer) {
    memset(&S.g, 0, sizeof S.g);
    S.nCred = S.nPend = S.nObra = S.obraProx = 0;
    S.nTempChk = S.nOri = S.oriProx = 0;
    S.cur = 0; S.curSeq = 0; S.curRest = 0; S.varDe = 0;
    S.varCur = 0; S.varSeq = 0;
    S.varrer = 1; S.varGer++; S.nVarId = 0;
    S.largadaVarrida = 0;
    S.rec = 1; S.sujo = 0; S.perfil = pNovo;
    if (pNovo > 0) {
      farolgosto_iniciar(&S.g, (uint16_t)pNovo);
      if (bG) farolgosto_ler(&S.g, bG, nBG);
      if (bE) aplicaEstado(bE, nBE);
      // Modelo quente sem anel de creds (o backup so carrega o modelo —
      // secao 2.3): a dupla cursor-no-fundo + grupo-consumido persiste
      // como "o diario inteiro ja esta dentro dele" — repassar dobraria
      // cada peso restaurado (issue 43). Nao e mais bloqueio permanente:
      // quando a marca `curRest` do diario existir (gravada ou medida no
      // aquecimento), o resgate por lotes corre o que for mais novo que
      // ela — o lote de hoje volta a pesar (ressalva R1 da r1).
      if (!bE && S.g.titulos > 0) { S.cur = LLONG_MIN; S.curSeq = FSIN_SEQ_TODA; }
      // a carga fria do diario e unidade separada do fio de tras — a troca
      // nao toma a trava do diario e pode ser roubada por quem a pede
      // (correcao FAROL38D; a r3 viu a leitura fria no passo).
      S.diaFrio = 1;
    }
    if (bF) { farolficha_ler_mem(&S.fc, bF, nBF); S.fc.sujo = 0; }
    S.fcLido = 1;
    S.trocando = 0;
  }
  pthread_cond_broadcast(&condTroca);
  DTRAVA();
  free(bG); free(bE); free(bF);
}

// ---------------------------------------------------------------- garante --
static void cbDiario(int perfil, int ev, const DiarioEntrada *e, void *ctx);
static void cbVisto(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto);
static void cbSalvos(const CatItem *ci, int salvo);
static void cbTirou(const char *imdb);

// O registro dos ouvintes e UMA VEZ e FORA de qualquer trava: diario_ouvir
// toma o mutex do diario e diario_percorrer chama cbVar com ele tomado —
// registrar sob a nossa trava inverteria a ordem e trancaria os dois
// (correcao FAROL38B). Chamado por pthread_once no iniciar.
static void ligaOuvintes(void) {
  diario_ouvir(cbDiario, NULL);
  salvos_ao_definir(cbSalvos);
  visto_ao_marcar(cbVisto);
  prog_ao_retirar(cbTirou);
}
static pthread_once_t onceOuv = PTHREAD_ONCE_INIT;

// COM A TRAVA TOMADA: o chamador faz a conferencia e o uso do perfil no
// mesmo fecho — a corrida do TSan 66 era ler/decidir S.perfil fora do fecho
// (correcao FAROL38B).
static void *fioTrab(void *);   // definido com o fio de tras, adiante

// Acorda (ou cria) o fio de tras: so nasce com trabalho a fazer e leitura
// e criacao ficam no mesmo fecho (correcao FAROL38B). Com a trava tomada.
static void acordaTrab(void) {
  if (fioMorre) return;
  if (!fioTrabVivo) {
    if (!pthread_create(&fioTrabId, NULL, fioTrab, NULL)) fioTrabVivo = 1;
  } else pthread_cond_signal(&condTrab);
}

// O perfil pedido esta residindo? A troca de perfil saiu do fio da UI
// (correcao FAROL38D): quem chama com a trava so MARCA a troca e acorda o
// fio de tras; enquanto S.trocando valer, as funcoes devolvem o seguro —
// nem visto, nem pendencia, nem varredura do perfil velho vaza no novo.
static int garante_i(void) {
  int p;
  if (!S.dir[0]) snprintf(S.dir, sizeof S.dir, "%s", dados_dir());
  if (!S.ligado) {
    S.ligado = 1;
    farolficha_cache_iniciar(&S.fc);
    S.trocando = 1;
    S.momentoBom = 1;   // ate o primeiro passo avisar, trabalha normal
  }
  p = perfis_ativo();
  if (S.trocando ? S.pendTroca != p : S.perfil != p) {
    S.trocando = 1;
    S.pendTroca = p;
  }
  return !S.trocando && S.perfil > 0;
}

// travaPronta: como o antigo garante_i sincrono, so que a E/S que a troca
// de perfil pede NAO segura a trava — quem chama ROUBA a unidade de troca
// e a executa neste fio (fioTrocaPerfil nao toma a trava do diario, entao
// e seguro rouba-la ate de dentro dos ouvintes). Devolve COM A TRAVA
// TOMADA quando o perfil ativo ja esta residindo; 0 sem trava quando nao
// ha perfil ou o app ja saiu. O passo e o sair NAO usam: la a troca so e
// agendada, nunca executada no quadro (correcao FAROL38D).
static int travaPronta(void) {
  for (;;) {
    int troca;
    TRAVA();
    if (garante_i()) return 1;
    troca = S.trocando;
    DTRAVA();
    if (!troca || fioMorre) return 0;
    fioTrocaPerfil();
  }
}

// "Terminou" so vale para quem viu ate o fim: registro do player com
// posicao (seg/dur) pede 90% — 10 s de 1 h nao e fim (correcao FAROL38B).
// Historico sem posicao (sync da conta, nota manual) continua valendo.
static int concluiu(const DiarioEntrada *e) {
  if (e->origem != DIARIO_ORIG_PLAYER || e->durSeg <= 0) return 1;
  return (long long)e->seg * 10 >= (long long)e->durSeg * 9;
}

// ----------------------------------------------------------- delta da vez --
// A tabela 2.2, linha a linha, para uma entrada do diario. Sono e parcial
// nao pesam (secao 1.3: "adormeceu nao e dislike"); "mais ou menos" (nota
// 4-6) pesa 0 — a ficha conta a obra, o sinal e neutro.
static int deltaDeEntrada(const DiarioEntrada *e) {
  int d;
  if (e->flags & (DIARIO_F_DORMIU | DIARIO_F_DORMIU_SUS | DIARIO_F_PARCIAL))
    return 0;
  if (e->nota10 >= 1 && e->nota10 <= 3) d = FG_D_REPROVOU;
  else if ((e->flags & DIARIO_F_CORACAO) || e->nota10 >= 7) d = FG_D_GOSTOU;
  else if (e->flags & DIARIO_F_REVER) d = FG_D_REVIU;
  else if (e->nota10 >= 4 && e->nota10 <= 6) d = 0;
  else d = concluiu(e) ? FG_D_TERMINOU : 0;
  // Serie pontua 0,1 por episodio; a temporada completa conta o valor cheio
  // uma vez — cobrado em confereTemporada.
  if (d > 0 && e->tipo == DIARIO_SERIE && (e->temporada > 0 || e->episodio > 0))
    d = (d * 26 + 128) / 256;
  if (e->flags & DIARIO_F_JUNTOS) d = (d + (d >= 0 ? 1 : -1)) / 2;
  return d;
}

// NOVA (e o passe da varredura): a vez ainda nao tem registro... ou tem o de
// um gesto/eco — a segunda entrega casa pela hb e so cobra a diferenca.
static void vezNova(const DiarioEntrada *e) {   // com a trava tomada
  Cred c;
  uint64_t hb = h3(e->id, e->temporada, e->episodio);
  uint64_t eh = hb ^ mix64((uint64_t)e->quando);
  uint64_t idh = h1(e->id);
  int i, delta = deltaDeEntrada(e), viaVez = 0;
  int faixa = e->hora >= 0 ? faixaDeHora(e->hora) : faixaAgora(e->quando);
  int serie = e->tipo == DIARIO_SERIE;
  i = achaEh(eh);
  if (i < 0) { i = achaVez(hb, e->quando); viaVez = i >= 0; }
  // O player com REVER entregou uma vez NOVA (a fusao nao colou porque a
  // segunda ida e de verdade): cai fora e credita cheio — mas so quando o
  // registro foi achado pela hb de vez; o achado pela eh exata e a MESMA
  // entrega, nunca vez nova (correcao FAROL38B). Nos demais casos e o eco
  // de um gesto ou a fusao re-entregue — so o que faltava, e so para cima.
  if (i >= 0 && !(viaVez && e->origem == DIARIO_ORIG_PLAYER &&
                  (e->flags & DIARIO_F_REVER) && S.cred[i].kind == K_VEZ)) {
    // A diferenca mede o LIVRE: o que a nota antiga do Trakt ja cobriu no
    // titulo nao soma de novo (ressalva R2 da r1).
    int diff = deltaLivre(idh, delta) - deltaLivre(idh, S.cred[i].delta);
    if (diff > 0) {
      int64_t real; FarolFicha f;
      if ((real = pegaFicha(aliasPara(S.cred[i].chave), e->id, serie, &f))) {
        aplicar(&f, real, diff, diaDe(e->quando), faixa);
      } else {   // sem ficha: a diferenca espera como a vez esperou
        Cred cc = S.cred[i]; cc.delta = diff; cc.eh = eh ^ 0xD1FF;
        pendPoe(&cc, e->id, serie, e->tmdb, 0);
      }
      S.cred[i].delta += diff;
      S.sujo = 1;
    }
    if (S.cred[i].kind == K_GESTO) {  // a entrada e a verdade do gesto
      S.cred[i].eh = eh; S.cred[i].quando = e->quando; S.cred[i].kind = K_VEZ;
    }
    return;
  }
  memset(&c, 0, sizeof c);
  c.idh = idh; c.hb = hb; c.eh = eh; c.quando = e->quando;
  c.chave = chaveObra(e->id, serie, e->tmdb);
  c.delta = deltaLivre(idh, delta); c.faixa = (int16_t)faixa; c.kind = K_VEZ;
  // Sinal neutro credita mesmo assim (a obra conta — regra antiga); so o
  // delta positivo coberto pela nota do Trakt fica de fora — nada aplica
  // e nem pendencia fica (R2).
  if (c.delta || !delta) credita(e->id, serie, e->tmdb, &c);
  // Episodio de serie: talvez fechou a temporada.
  if (delta > 0 && serie && e->temporada > 0 && S.nTempChk < FSIN_TEMP_N) {
    int k;
    for (k = 0; k < S.nTempChk; k++)
      if (S.tempChk[k].t == e->temporada && !strcmp(S.tempChk[k].id, e->id))
        break;
    if (k == S.nTempChk) {
      snprintf(S.tempChk[k].id, sizeof S.tempChk[k].id, "%s", e->id);
      S.tempChk[k].t = e->temporada;
      // o fio de tras leva (perfil, geracao) junto: ao reentrar na trava
      // descarta se houve troca/esquecer/sair (correcao FAROL38D).
      S.tempChk[k].perfil = S.perfil; S.tempChk[k].ger = S.varGer;
      S.nTempChk++;
    }
  }
}

// A temporada que tinha o bonus K_TEMP ficou incompleta? O bonus volta —
// a contagem e pelas vezes creditadas (a trava do diario nao se toma aqui
// dentro). Com a trava tomada.
static void temporadaDesfeita(const char *id, int t, long tmdb) {
  int j = achaHb(hDom(id, K_TEMP, t));
  int idx, ne, k, tot = 0, vist = 0;
  if (t <= 0 || j < 0) return;
  if ((idx = cat_indice_por_imdb(id)) < 0) return;
  ne = cat_n_episodios(idx);
  for (k = 0; k < ne; k++) {
    const CatEp *ep = cat_episodio(idx, k);
    if (ep && ep->temporada == t) {
      tot++;
      if (vezCredida(h3(id, t, ep->episodio))) vist++;
    }
  }
  if (tot && vist < tot) {
    Cred cc = S.cred[j];
    credTira(j);
    desfaz(&cc, id, 1, tmdb);
  }
}

// A entrada apagada desfaz o que a vez somou — so pelo registro, nunca por
// palpite: entrada que nunca creditou nao tira nada.
static void vezApagada(const DiarioEntrada *e) {  // com a trava tomada
  uint64_t hb = h3(e->id, e->temporada, e->episodio);
  uint64_t eh = hb ^ mix64((uint64_t)e->quando);
  int i = achaEh(eh), serie = e->tipo == DIARIO_SERIE;
  if (i < 0) i = achaVez(hb, e->quando);
  if (i >= 0) {
    Cred c = S.cred[i];
    credTira(i);
    // Devolve so o que a vez somou de verdade: a parte que a nota antiga ja
    // cobria nunca entrou — estorna-la tiraria peso que nao e dela (R2).
    c.delta = deltaLivre(c.idh, c.delta);
    if (c.delta) desfaz(&c, e->id, serie, e->tmdb);
    // a temporada que esta entrada fechava nao fecha mais (correcao FAROL38B)
    temporadaDesfeita(e->id, e->temporada, e->tmdb);
  }
  // e se ainda era so pendencia, cancela antes da ficha chegar — mas NUNCA
  // a pendencia DEVOLVE de estorno: ela e divida, nao intencao (correcao
  // FAROL38B).
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.hb == hb && S.pend[i].c.kind <= K_GESTO &&
        !S.pend[i].devolve &&
        (S.pend[i].c.eh == eh ||
         (S.pend[i].c.quando > e->quando - DIARIO_JANELA_S &&
          S.pend[i].c.quando < e->quando + DIARIO_JANELA_S)))
      { pendTira(i); S.sujo = 1; }
}

// ALTERADA (nota, flags, data ou fusao que enriqueceu): so a diferenca anda.
static void vezAlterada(const DiarioEntrada *e) {   // com a trava tomada
  uint64_t hb = h3(e->id, e->temporada, e->episodio);
  uint64_t eh = hb ^ mix64((uint64_t)e->quando);
  int i = achaEh(eh), serie = e->tipo == DIARIO_SERIE;
  int delta = deltaDeEntrada(e);
  if (i < 0) i = achaVez(hb, e->quando);
  if (i >= 0) {
    uint64_t idh = h1(e->id);
    int diff = deltaLivre(idh, delta) - deltaLivre(idh, S.cred[i].delta);
    if (diff) {
      int64_t real; FarolFicha f;
      int fx = e->hora >= 0 ? faixaDeHora(e->hora) : S.cred[i].faixa;
      if ((real = pegaFicha(aliasPara(S.cred[i].chave), e->id, serie, &f))) {
        aplicar(&f, real, diff, diaDe(e->quando), fx);
        S.sujo = 1;
      } else {
        Cred cc = S.cred[i]; cc.delta = diff;   // ajusta quando a ficha voltar
        pendPoe(&cc, e->id, serie, e->tmdb, 0);
      }
      S.cred[i].delta += diff;
    }
    S.cred[i].eh = eh; S.cred[i].quando = e->quando;
    S.sujo = 1;
    return;
  }
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.hb == hb && S.pend[i].c.kind == K_VEZ &&
        !S.pend[i].devolve)
      { S.pend[i].c.delta = deltaLivre(S.pend[i].c.idh, delta);
        S.pend[i].c.eh = eh;
        S.pend[i].c.quando = e->quando; S.sujo = 1; return; }
  vezNova(e);   // registro desconhecido: e como se fosse nova
}

static void cbDiario(int perfil, int ev, const DiarioEntrada *e, void *ctx) {
  (void)ctx;
  if (!travaPronta()) return;
  if (ev == DIARIO_EV_RECARREGADO) armaVarredura();
  else if (e && perfil == S.perfil && S.rec) {
    if (ev == DIARIO_EV_NOVA) vezNova(e);
    else if (ev == DIARIO_EV_ALTERADA) vezAlterada(e);
    else if (ev == DIARIO_EV_APAGADA) vezApagada(e);
  }
  DTRAVA();
}

// -------------------------------------------------------- visto / desvisto --
// Marcar assistido vale +1,0 por vez — a entrada do diario que a conta (ou o
// player) devolve depois casa pela hb e so completa o que faltava.
static void cbVisto(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto) {
  int serie, i, idx;
  long tmdb = 0;
  const CatItem *ci;
  if (!imdb || !imdb[0]) return;
  serie = tipo && !strcmp(tipo, "series");
  if ((idx = cat_indice_por_imdb(imdb)) >= 0 &&
      (ci = cat_item(idx)) != NULL) tmdb = ci->tmdb;
  if (visto) {
    long long agora = agoraS();
    int faixa = faixaAgora(agora);
    if (!travaPronta()) return;
    if (S.rec) {
      int total = n < 1 ? 1 : n;
      for (i = 0; i < total; i++) {
        int t = n < 1 ? 0 : pares[i].temporada;
        int e = n < 1 ? 0 : pares[i].episodio;
        uint64_t hb = h3(imdb, t, e);
        Cred c;
        if (achaVez(hb, agora) >= 0) continue;   // a vez ja conta
        memset(&c, 0, sizeof c);
        c.idh = h1(imdb); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
        c.quando = agora; c.chave = chaveObra(imdb, serie, tmdb);
        // episodio de serie preve a entrada de 0,1 — nao a vez inteira
        // (tabela 2.2; correcao FAROL38B).
        c.delta = serie && (t > 0 || e > 0)
                  ? (FG_D_TERMINOU * 26 + 128) / 256 : FG_D_TERMINOU;
        c.delta = deltaLivre(c.idh, c.delta);   // coberto pela nota? (R2)
        c.faixa = (int16_t)faixa; c.kind = K_GESTO;
        if (c.delta) credita(imdb, serie, tmdb, &c);
      }
    }
    DTRAVA();
    return;
  }
  // DESMARCOU: devolve os creditos de vez/gesto do alvo (e, no titulo
  // inteiro, o bonus de temporada e o "largada"). O DIARIO NAO E TOCADO:
  // desmarcar visto desfaz so o sinal do Farol — a entrada continua la
  // (correcao FAROL38B: o gesto apagava o historico do usuario).
  if (!travaPronta()) return;
  if (S.rec) {
    uint64_t idh = h1(imdb);
    for (i = S.nCred - 1; i >= 0; i--) {
      int alvo = 0, k;
      if (S.cred[i].idh != idh) continue;
      if (S.cred[i].kind > K_GESTO &&
          !(n < 1 && (S.cred[i].kind == K_TEMP || S.cred[i].kind == K_LARGADA)))
        continue;
      if (n < 1) alvo = 1;
      else for (k = 0; k < n; k++)
        if (S.cred[i].hb == h3(imdb, pares[k].temporada, pares[k].episodio))
          alvo = 1;
      if (alvo) {
        Cred cc = S.cred[i];
        credTira(i);
        // So devolve o que entrou de verdade: vez/gesto coberto pela nota
        // antiga nao estorna a parcela dela (R2). TEMP/LARGADA sao de outro
        // dominio — deltaLivre nao as toca.
        if (cc.kind <= K_GESTO) cc.delta = deltaLivre(cc.idh, cc.delta);
        if (cc.delta) desfaz(&cc, imdb, serie, tmdb);
      }
    }
    for (i = S.nPend - 1; i >= 0; i--) {
      int alvo = 0, k;
      if (S.pend[i].devolve || S.pend[i].c.idh != idh) continue;
      if (S.pend[i].c.kind > K_GESTO &&
          !(n < 1 && (S.pend[i].c.kind == K_TEMP ||
                      S.pend[i].c.kind == K_LARGADA)))
        continue;
      if (n < 1) alvo = 1;
      else for (k = 0; k < n; k++)
        if (S.pend[i].c.hb == h3(imdb, pares[k].temporada, pares[k].episodio))
          alvo = 1;
      if (alvo) { pendTira(i); S.sujo = 1; }
    }
    // episodio desmarcado pode ter deixado a temporada incompleta
    if (serie && n >= 1)
      for (i = 0; i < n; i++) temporadaDesfeita(imdb, pares[i].temporada, tmdb);
  }
  DTRAVA();
}

// Salvos ("+"): +0,6; tirar devolve. Nivel de titulo, dominio proprio.
static void cbSalvos(const CatItem *ci, int salvo) {
  uint64_t hb;
  int i;
  if (!ci || !ci->imdb[0]) return;
  hb = hDom(ci->imdb, K_SALVO, 0);
  if (!travaPronta()) return;
  if (S.rec) {
    if (salvo) {
      if (achaHb(hb) < 0) {
        Cred c; long long agora = agoraS();
        memset(&c, 0, sizeof c);
        c.idh = h1(ci->imdb); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
        c.quando = agora;
        c.chave = chaveObra(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
        c.delta = FG_D_SALVOU; c.faixa = (int16_t)faixaAgora(agora);
        c.kind = K_SALVO;
        credita(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb, &c);
      }
    } else {
      if ((i = achaHb(hb)) >= 0) {
        Cred c = S.cred[i];
        credTira(i);
        desfaz(&c, ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
      }
      // Retirou antes da ficha chegar: a intencao pendente morre junto —
      // a ficha tardia nao pode creditar um salvo que ja nao existe
      // (correcao FAROL38B). Pendencia DEVOLVE (estorno) fica: e divida.
      for (i = S.nPend - 1; i >= 0; i--)
        if (S.pend[i].c.hb == hb && S.pend[i].c.kind == K_SALVO &&
            !S.pend[i].devolve) { pendTira(i); S.sujo = 1; }
    }
  }
  DTRAVA();
}

// "Tirou do Continuar" (prog_marcar_removido / prog_ocultar_continuar): -0,6,
// uma vez por titulo — a recusa leve da secao 1.3.
static void cbTirou(const char *imdb) {
  char id[DIARIO_ID_MAX];
  uint64_t hb;
  int idx, serie = 0;
  long tmdb = 0;
  const CatItem *ci;
  if (!imdb || !imdb[0]) return;
  // o id composto so se divide nos ":T:E" finais — "tmdb:321" inteiro e o
  // proprio id (correcao FAROL38B: cortar no primeiro ':' juntava tmdb:321
  // e tmdb:654 na mesma obra).
  diario_separar_id(imdb, id, sizeof id, NULL, NULL);
  if ((idx = cat_indice_por_imdb(id)) >= 0 && (ci = cat_item(idx)) != NULL) {
    serie = !strcmp(ci->tipo, "series"); tmdb = ci->tmdb;
  }
  hb = hDom(id, K_TIROU, 0);
  if (!travaPronta()) return;
  if (S.rec && achaHb(hb) < 0) {
    Cred c; long long agora = agoraS();
    memset(&c, 0, sizeof c);
    c.idh = h1(id); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
    c.quando = agora; c.chave = chaveObra(id, serie, tmdb);
    c.delta = FG_D_TIROU; c.faixa = (int16_t)faixaAgora(agora);
    c.kind = K_TIROU;
    credita(id, serie, tmdb, &c);
  }
  DTRAVA();
}

// Um sinal avulso de CatItem (interesse, mais assim, recusa): dedup pela hb
// do dominio e credito direto. Com a trava tomada. 1 = sinal novo.
static int avulso(const CatItem *ci, int kind, int nDom, int delta) {
  Cred c;
  uint64_t hb = hDom(ci->imdb, kind, nDom);
  long long agora = agoraS();
  if (achaHb(hb) >= 0) return 0;
  memset(&c, 0, sizeof c);
  c.idh = h1(ci->imdb); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
  c.quando = agora;
  c.chave = chaveObra(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
  c.delta = delta; c.faixa = (int16_t)faixaAgora(agora); c.kind = (uint8_t)kind;
  credita(ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb, &c);
  return 1;
}
static int desfazAvulso(const CatItem *ci, int kind, int nDom) {
  int i = achaHb(hDom(ci->imdb, kind, nDom));
  if (i >= 0) {
    Cred c = S.cred[i];
    credTira(i);
    desfaz(&c, ci->imdb, !strcmp(ci->tipo, "series"), ci->tmdb);
    return 1;
  }
  return 0;
}

// ------------------------------------------------------------------ saida --
void farolsinal_saida(const char *id, char tipo, int temporada, int episodio,
                      long tmdb, double posSeg, double durSeg, int concluiu) {
  DiarioEntrada sus;
  int falha, serie;
  Cred c;
  long long agora;
  (void)temporada; (void)episodio;   // o abandono e da obra inteira
  if (!id || !id[0] || concluiu || posSeg < FSIN_ABAND_MIN_S ||
      durSeg <= 0 || posSeg * 100.0 >= durSeg * FSIN_ABAND_PCT) return;
  // Sono confirmado ou suspeito recente neste titulo: a saida era descanso,
  // nao desgosto (secao 1.3: "adormeceu nao e dislike"). A suspeita precisa
  // estar na janela DOS DOIS LADOS — nem velha demais nem no futuro
  // (correcao FAROL38B: relogio torto dispensava abandono real).
  if (dormiu_suspeita(id, &sus) && sus.quando > agoraS() - FSIN_SONO_JANELA &&
      sus.quando <= agoraS() + 300)
    return;
  // Falha de fonte/video nos ultimos 5 min: o player morreu, nao o gosto.
  falha = farolanel_contar(FE_ERRO_FONTE, FSIN_JANELA_FALHA, agoraMs()) ||
          farolanel_contar(FE_ERRO_NATIVO, FSIN_JANELA_FALHA, agoraMs()) ||
          farolanel_contar(FE_PIPELINE_MORREU, FSIN_JANELA_FALHA, agoraMs()) ||
          farolanel_contar(FE_PARADO, FSIN_JANELA_FALHA, agoraMs());
  if (falha) return;
  serie = tipo == DIARIO_SERIE;
  agora = agoraS();
  if (!travaPronta()) return;
  if (S.rec) {
    memset(&c, 0, sizeof c);
    c.idh = h1(id);
    c.hb = hDom(id, K_SAIDA, 0) ^ mix64((uint64_t)agora);
    c.eh = c.hb; c.quando = agora;
    c.chave = chaveObra(id, serie, tmdb);
    c.delta = FG_D_ABANDONOU; c.faixa = (int16_t)faixaAgora(agora);
    c.kind = K_SAIDA;
    credita(id, serie, tmdb, &c);
  }
  DTRAVA();
}

// --------------------------------------------------------- sinais avulsos --
void farolsinal_interesse(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  if (!travaPronta()) return;
  if (S.rec)
    avulso(ci, K_INTERESSE, diaDe(agoraS()), FG_D_INTERESSE);
  DTRAVA();
}
void farolsinal_mais_assim(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  if (!travaPronta()) return;
  if (S.rec) avulso(ci, K_MAIS, 0, FG_D_MAIS_ASSIM);
  DTRAVA();
}
void farolsinal_recusar(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  if (!travaPronta()) return;
  if (S.rec && avulso(ci, K_RECUSA, 0, FG_D_REPROVOU))
    { farolgosto_recusar(&S.g, mapa_hash_titulo(ci->imdb)); S.sujo = 1; }
  DTRAVA();
}
void farolsinal_desfazer_recusa(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  if (!travaPronta()) return;
  // a recusa sai tambem do anel — o titulo volta a aparecer nas fileiras
  // (correcao FAROL38B: "desfazer" deixava o hash para sempre).
  if (desfazAvulso(ci, K_RECUSA, 0))
    farolgosto_desrecusar(&S.g, mapa_hash_titulo(ci->imdb));
  DTRAVA();
}
void farolsinal_mostrado(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return;
  if (!travaPronta()) return;
  if (S.rec) {
    farolgosto_mostrado(&S.g, mapa_hash_titulo(ci->imdb),
                        (uint8_t)(diaDe(agoraS()) & 0xff));
    S.sujo = 1;
  }
  DTRAVA();
}
void farolsinal_origem(const CatItem *ci, int origem) {
  int i;
  if (!ci || !ci->imdb[0] ||
      origem <= FS_ORIGEM_NENHUMA || origem >= FS_ORIGEM_N) return;
  if (!travaPronta()) return;
  if (S.nOri < FSIN_ORI_N) i = S.nOri++;
  else { i = S.oriProx; S.oriProx = (S.oriProx + 1) % FSIN_ORI_N; }
  S.ori[i].h = mapa_hash_titulo(ci->imdb);
  S.ori[i].o = (uint8_t)origem;
  S.ori[i].dia = (uint8_t)(diaDe(agoraS()) & 0xff);
  S.sujo = 1;
  DTRAVA();
}

// ------------------------------------------------------------- fila lenta --
static void pedeFicha(int64_t chave, const char *id, int serie, long tmdb) {
  int i;
  if (!chave) return;
  chave = aliasPara(chave);
  for (i = 0; i < S.nPed; i++) if (S.ped[i].chave == chave) return;
  if (S.nPed >= FSIN_PED_N) return;
  i = S.nPed++;
  S.ped[i].chave = chave; S.ped[i].tmdb = tmdb; S.ped[i].serie = (uint8_t)!!serie;
  S.ped[i].ger = S.esqGer;
  snprintf(S.ped[i].id, sizeof S.ped[i].id, "%s", id ? id : "");
  pthread_cond_signal(&condPed);
}

static void *fioLenta(void *u) {
  FarolFichaTmp tmp;
  char *corpo = malloc(128 * 1024);   // do fio, nao estatico: o fio solto
  (void)u;                            // na saida nao divide buffer com outro
  memset(&tmp, 0, sizeof tmp);
  if (!corpo) return NULL;
  for (;;) {
    Ped p;
    FarolSinalBusca b;
    int n;
    TRAVA();
    // Nunca com player aberto (secao 4.4): com pedido na fila e momento ruim
    // o fio dorme em fatias de 1 s ate o passo avisar que a TV liberou — sem
    // espera ocupada e sem pedido a mais no ar (sempre 1 por vez).
    while (!fioMorre && (!S.nPed || !S.momentoBom)) {
      if (!S.nPed) pthread_cond_wait(&condPed, &trava);
      else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&condPed, &trava, &ts);
      }
    }
    if (fioMorre) { DTRAVA(); break; }
    p = S.ped[0];
    memmove(&S.ped[0], &S.ped[1], sizeof(Ped) * (size_t)(--S.nPed));
    b = busca;
    DTRAVA();
    if (!b) continue;
    n = b(p.id[0] ? p.id : NULL, p.tmdb, p.serie, corpo, 128 * 1024 - 1);
    if (n > 0) {
      FarolFicha f;
      corpo[n] = 0;
      if (farolficha_ler_tmdb(corpo, p.serie, &tmp, &f))
        { fichaOk(p.chave, &f, p.ger); continue; }
    }
    fichaFalhou(p.chave, p.ger);
  }
  free(corpo);
  return NULL;
}

// ger < 0: chamada direta (teste/servico), sem fronteira a conferir. ger >=
// 0: veio do fio de tras com a geracao do pedido — se houve esquecer no
// meio do transporte, a ficha tardia nao ressuscita o cache (correcao
// FAROL38D).
static void fichaOk(int64_t chavePed, const FarolFicha *f, int ger) {
  int64_t real;
  if (!f) return;
  real = farolficha_chave(f);
  if (!travaPronta()) return;
  if (real && (ger < 0 || S.esqGer == ger)) {
    farolficha_guardar(&S.fc, f);
    S.fichaSujo = 1;
    if (real != chavePed) aliasPoe(chavePed, real);
    drenaPends(real);
  }
  DTRAVA();
}
void farolsinal_ficha_ok(int64_t chavePed, const FarolFicha *f) {
  fichaOk(chavePed, f, -1);
}
static void fichaFalhou(int64_t chavePed, int ger) {
  int i;
  if (!travaPronta()) return;
  if (ger >= 0 && S.esqGer != ger) { DTRAVA(); return; }
  for (i = S.nPend - 1; i >= 0; i--)
    if (S.pend[i].c.chave == chavePed) {
      if (++S.pend[i].tent < 2)
        pedeFicha(chavePed, S.pend[i].id, S.pend[i].serie, S.pend[i].tmdb);
      else { pendTira(i); S.sujo = 1; }   // solta: nao fica pendente pra sempre
    }
  DTRAVA();
}
void farolsinal_ficha_falhou(int64_t chavePed) {
  fichaFalhou(chavePed, -1);
}
void farolsinal_busca(FarolSinalBusca fn) {
  TRAVA(); busca = fn; pthread_cond_signal(&condPed); DTRAVA();
}
int  farolsinal_pendencias(void) {
  int n;
  if (!travaPronta()) return 0;
  n = S.nPend; DTRAVA(); return n;
}

// ------------------------------------------------- notas antigas do Trakt --
// /sync/ratings, UMA leitura por conta, no fio do Trakt (secao 1.3, issue
// 43): quem baixou entrega o corpo aqui; cada item vira um Cred do dominio
// K_NOTA — hb por titulo, entao uma segunda entrega nunca dobra — e a marca
// farol-notas.txt grava so quando o perfil que pediu ainda e o residente.
// A nota so aquece titulo que o modelo ainda nao conhece: com qualquer
// sinal vivo (credito ou pendencia) a verdade mais fresca e a do aparelho.
// Sem a nota do Trakt ha nada: nota 4-6 vira delta 0 e nem registro fica.

// Bloco "<nome>":{...} dentro de [p, f). NULL se nao ha. (O mesmo
// decodificador de diariotrakt.c — os dois leem respostas do Trakt.)
static const char *bloco(const char *p, const char *f, const char *nome,
                         const char **fim) {
  char k[24];
  const char *b, *a;
  snprintf(k, sizeof k, "\"%s\"", nome);
  b = strstr(p, k);
  if (!b || b >= f) return NULL;
  a = strchr(b + strlen(k), '{');
  if (!a || a >= f) return NULL;
  *fim = js_fim(a);
  return *fim ? a : NULL;
}

typedef struct {
  char id[DIARIO_ID_MAX];
  long tmdb;
  int serie, nota;
  long long quando;
} NotaTrakt;

// Um item de /sync/ratings: {"rated_at":...,"rating":8,"type":"movie",
// "movie":{"ids":{"imdb":"tt0133093","tmdb":603}}} (e "show" para serie).
// Episodio/temporada avaliados nao entram — o modelo e por titulo (2.1).
static int lerNotaItem(const char *p, const char *f, NotaTrakt *r) {
  char tipo[16], quando[48], imdb[DIARIO_ID_MAX];
  const char *ob = NULL, *of = NULL, *ib, *ifim;
  memset(r, 0, sizeof *r);
  if (!js_texto(p, f, "type", tipo, sizeof tipo)) return 0;
  if (!strcmp(tipo, "movie")) ob = bloco(p, f, "movie", &of);
  else if (!strcmp(tipo, "show")) { r->serie = 1; ob = bloco(p, f, "show", &of); }
  else return 0;
  r->nota = (int)js_num(p, f, "rating", 0);
  if (r->nota < 1 || r->nota > 10 || !ob || !of) return 0;
  ib = bloco(ob, of, "ids", &ifim);
  if (!ib) return 0;
  imdb[0] = 0;
  js_texto(ib, ifim, "imdb", imdb, sizeof imdb);
  r->tmdb = (long)js_num(ib, ifim, "tmdb", 0);
  if (!imdb[0]) {
    if (r->tmdb <= 0) return 0;        // o app inteiro fala imdb (ou tmdb:N)
    snprintf(imdb, sizeof imdb, "tmdb:%ld", r->tmdb);
  }
  snprintf(r->id, sizeof r->id, "%s", imdb);
  r->quando = agoraS();
  if (js_texto(p, f, "rated_at", quando, sizeof quando)) {
    long long ms = js_ms_iso(quando);
    if (ms > 0) r->quando = ms / 1000;
  }
  return 1;
}

int farolsinal_trakt_quer(void) {
  char cam[1100], nome[64];
  int tem;
  FILE *f;
  TRAVA();
  if (!S.dir[0]) snprintf(S.dir, sizeof S.dir, "%s", dados_dir());
  nomeNotas(nome, sizeof nome);
  tem = caminho(nome, cam, sizeof cam);
  DTRAVA();
  if (!tem) return 0;
  f = fopen(cam, "r");
  if (f) { fclose(f); return 0; }
  return 1;
}

void farolsinal_trakt_notas(int perfil, const char *json) {
  NotaTrakt itens[FSIN_NOTAS_MAX];
  const char *p;
  char cam[1100], nome[64], marca[48];
  int n = 0, gravaMarca = 0, eGer = 0, nMarca = 0;
  if (!json || perfil < 1) return;
  for (p = js_raiz_array(json); p && n < FSIN_NOTAS_MAX;
       p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if (!f) break;
    if (lerNotaItem(p, f, &itens[n])) n++;
  }
  if (!travaPronta()) return;
  if (S.perfil == perfil) {
    int i;
    eGer = S.esqGer;
    if (S.rec) for (i = 0; i < n; i++) {
      Cred c;
      uint64_t hb = hDom(itens[i].id, K_NOTA, 0);
      int delta = itens[i].nota <= 3 ? FG_D_REPROVOU :
                  itens[i].nota >= 7 ? FG_D_GOSTOU : 0;
      if (!delta || achaHb(hb) >= 0) continue;
      memset(&c, 0, sizeof c);
      c.idh = h1(itens[i].id);
      {
        int j = achaIdh(c.idh);
        if (j >= 0 || pendTemIdh(c.idh) >= 0) {
          // O aparelho ja conhece o titulo — a nota nao aquece. MAS uma vez
          // do diario e o MESMO "gostou" da nota: se a vez somou menos que
          // ela, a nota so completa ate o maior sinal — nunca a soma dos
          // dois (ressalva R2 da r1: "terminou" + nota 8 dava +3,0).
          if (j >= 0 && delta > 0) {
            int k;
            for (k = 0; k < S.nCred; k++)
              if (S.cred[k].idh == c.idh && S.cred[k].kind <= K_GESTO &&
                  S.cred[k].delta > 0 && S.cred[k].delta < delta) {
                int64_t real; FarolFicha f;
                if ((real = pegaFicha(aliasPara(S.cred[k].chave),
                                      itens[i].id, itens[i].serie, &f))) {
                  aplicar(&f, real, delta - S.cred[k].delta,
                          diaDe(itens[i].quando),
                          (int16_t)faixaAgora(itens[i].quando));
                  S.sujo = 1;
                }
                break;
              }
          }
          // A nota fica registrada mesmo sem aplicar: ela limita as vezes
          // futuras do titulo via deltaLivre e deduba reentregas pela hb.
          c.hb = hb; c.eh = hb ^ mix64((uint64_t)itens[i].quando);
          c.quando = itens[i].quando;
          c.chave = chaveObra(itens[i].id, itens[i].serie, itens[i].tmdb);
          c.delta = delta; c.faixa = (int16_t)faixaAgora(itens[i].quando);
          c.kind = K_NOTA;
          credPoe(&c); S.sujo = 1;
          continue;
        }
      }
      c.hb = hb; c.eh = hb ^ mix64((uint64_t)itens[i].quando);
      c.quando = itens[i].quando;
      c.chave = chaveObra(itens[i].id, itens[i].serie, itens[i].tmdb);
      c.delta = delta; c.faixa = (int16_t)faixaAgora(itens[i].quando);
      c.kind = K_NOTA;
      credita(itens[i].id, itens[i].serie, itens[i].tmdb, &c);
    }
    nomeNotas(nome, sizeof nome);
    gravaMarca = caminho(nome, cam, sizeof cam);
  }
  DTRAVA();
  if (gravaMarca) {
    nMarca = snprintf(marca, sizeof marca, "1 %lld\n", agoraS());
    gravaBuf(cam, marca, (size_t)nMarca, 0, eGer);
  }
}

// ---------------------------------------------------------------- ciclo ----
// Leituras de quadro NAO roubam a unidade de troca: quem pergunta do fio da
// UI (o clima do mapa e quem vier) nao pode pagar a releitura do perfil que
// esta em curso no fio de tras — roubar aqui devolvia arquivo e serializacao
// para o fio chamador (ressalva R6 da r1). Devolvem o seguro ate a troca
// fechar; o passo acorda o fio de tras e a resposta certa vem no proximo
// quadro.
static int prontoLeve(void) {        // trava + agenda, sem executar E/S
  int ok;
  TRAVA();
  ok = garante_i();
  if (!ok) DTRAVA();
  return ok;
}
int  farolsinal_recomendar(void) {
  int r; if (!prontoLeve()) return 0; r = S.rec; DTRAVA(); return r;
}
void farolsinal_definir_recomendar(int ligado) {
  if (travaPronta()) {
    if (S.rec != !!ligado) { S.rec = !!ligado; S.sujo = 1; }
    DTRAVA();
  }
}
int farolsinal_modelo(FarolGosto *dst) {
  int ok;
  if (!dst) return 0;
  ok = prontoLeve();
  if (ok) *dst = S.g;
  if (ok) DTRAVA();
  return ok;
}
int farolsinal_origens(uint32_t *h, uint8_t *o, uint8_t *d, int max) {
  int i, n = 0;
  if (!prontoLeve()) return 0;
  n = S.nOri < max ? S.nOri : max;
  for (i = 0; i < n; i++) {
    if (h) h[i] = S.ori[i].h;
    if (o) o[i] = S.ori[i].o;
    if (d) d[i] = S.ori[i].dia;
  }
  DTRAVA();
  return n;
}

// O backup do aparelho (issue 43) regrava os arquivos por baixo quando a
// restauracao traz um modelo mais novo — o que esta em RAM seguiria pisando
// nele na gravacao seguinte. Uma troca para o proprio perfil com `recarrega`
// marcado: o fio de tras le o que a restauracao trouxe SEM serializar o que
// sai. Se ja ha troca em curso nao faz nada — ela le o disco do mesmo jeito.
void farolsinal_recarregar(void) {
  TRAVA();
  garante_i();
  if (S.perfil > 0 && !S.trocando) {
    S.recarrega = 1;
    S.trocando = 1;
    S.pendTroca = S.perfil;
    acordaTrab();
  }
  DTRAVA();
}
// A restauracao do backup marca a releitura ANTES de o arquivo restaurado
// pousar no disco: enquanto `recarrega` valer sem troca agendada, nem a
// gravacao de folga nem o instantaneo serializam modelo/estado — uma
// escrita da RAM velha nesse intervalo pisaria no arquivo que esta sendo
// trocado (ressalva R5 da r1). farolsinal_recarregar() ao fim agenda a
// unidade; se a aplicacao morrer antes, a marca some com ela.
void farolsinal_restaura_marcar(void) {
  TRAVA();
  garante_i();
  if (S.perfil > 0) S.recarrega = 1;
  DTRAVA();
}

// Forca o descarrego do que ainda esta so na folga de FSIN_GRAVA_S — o
// instantaneo do backup por aparelho (issue 43) le os arquivos do disco e
// precisa do estado de agora. Mesma estrada do fio de tras: serializa sob a
// trava, grava fora dela. Chamavel de qualquer fio.
void farolsinal_gravar(void) {
  char cam[1100], nome[64];
  char *gm = NULL, *ge = NULL;
  unsigned char *gf = NULL;
  size_t nGm = 0, nGe = 0, nGf = 0;
  int p, eGer;
  if (!travaPronta()) return;
  p = S.perfil; eGer = S.esqGer;
  // `recarrega` alto = uma restauracao trocando o arquivo embaixo da RAM:
  // descarregar agora pisaria nele — modelo/estado ficam para a troca que
  // so le (ressalva R5 da r1). O cache de fichas nao entra no backup e
  // desce normal.
  if (p > 0 && S.sujo && !S.recarrega) {
    gm = textoModelo(&nGm); ge = textoEstado(&nGe); S.sujo = 0;
  }
  if (S.fichaSujo) { gf = textoFichas(&nGf); S.fichaSujo = 0; }
  S.ultGrav = agoraS();
  DTRAVA();
  if (p > 0) {
    if (gm) { nomeGosto(p, nome, sizeof nome);
      if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, gm, nGm, 0, eGer); }
    if (ge) { nomeEstado(p, nome, sizeof nome);
      if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, ge, nGe, 0, eGer); }
  }
  if (gf && caminho("farol-fichas.bin", cam, sizeof cam))
    gravaBuf(cam, gf, nGf, 0, eGer);
  free(gm); free(ge); free(gf);
}

void farolsinal_iniciar(void) {
  // ouvintes: uma vez, fora de toda trava (ver ligaOuvintes)
  pthread_once(&onceOuv, ligaOuvintes);
  // O iniciar espera a carga do perfil fechar: a volta do travaPronta rouba
  // a unidade de troca e a executa aqui, com a E/S fora da trava — nem
  // fopen nem leitura de arquivo sob o mutex (correcao FAROL38D).
  if (travaPronta()) DTRAVA();
}
void farolsinal_pasta(const char *dir) {
  TRAVA();
  // pasta nova so faz sentido com o modelo ainda por carregar
  if (dir && *dir && S.perfil == 0) snprintf(S.dir, sizeof S.dir, "%s", dir);
  DTRAVA();
}
void farolsinal_relogio(long long (*s)(void), uint32_t (*ms)(void)) {
  f_agora_s = s; f_agora_ms = ms;
}

// O passe da varredura virou FATIA de teto rigido (correcao FAROL38C): no
// maximo FSIN_VEZ_MAX entregas por chamada, cortadas ate no meio de uma
// leva de mesmo `quando` — antes 700 vezes no mesmo instante entravam
// juntas num quadro so porque o teto nao cortava em empate. O cursor e a
// chave composta (S.cur, S.curSeq): tudo com `quando` maior que S.cur ja
// entrou e, dentro do grupo de mesmo `quando`, as S.curSeq primeiras na
// ordem da varredura tambem — f.saltar pula exatamente elas na fatia
// seguinte, sem entrega e sem processar de novo.
typedef struct {
  int n;              // processadas nesta fatia
  int cortou;         // 1 = bateu o teto, ha mais por varrer
  int abortou;        // 1 = perfil/geracao trocou ou fio morreu no meio
  int perfil;
  int ger;            // varGer capturado — esquecer/restart invalidam a vez
  int teto;           // entregas por fatia (o resgate anda em lotes menores)
  long long qUlt;     // `quando` da ultima processada
  int nUlt;           // processadas nesse `quando` nesta fatia
} VarCtx;
static int cbVar(const DiarioEntrada *e, void *ctx) {
  VarCtx *c = (VarCtx *)ctx;
  if (c->n >= c->teto) { c->cortou = 1; return 0; }
  TRAVA();
  if (S.perfil != c->perfil || S.varGer != c->ger || S.trocando || fioMorre)
    c->abortou = 1;                                  // correcao FAROL38D
  else if (S.rec) {
    // O aquecimento cobre os FSIN_AQUECE_TITULOS titulos mais novos do passe
    // (issue 43): alem do teto a fatia anda e o cursor avanca, mas a entrada
    // nao pesa. Episodios da mesma serie dividem um titulo (idh e do id).
    uint64_t idh = h1(e->id);
    int k;
    for (k = 0; k < S.nVarId && S.varId[k] != idh; k++);
    if (k < S.nVarId) vezNova(e);
    else if (S.nVarId < FSIN_AQUECE_TITULOS)
      { S.varId[S.nVarId++] = idh; vezNova(e); }
  }
  DTRAVA();
  if (c->abortou) return 0;
  c->n++;
  if (c->n == 1 || e->quando != c->qUlt) { c->qUlt = e->quando; c->nUlt = 1; }
  else c->nUlt++;
  return 1;
}

// Uma fatia da varredura, chamada pelo passo: a entrega mais pesada do
// quadro fica limitada a FSIN_VEZ_MAX vezes por passada, nunca mais a uma
// leva inteira de mesmo instante (correcao FAROL38C). O cursor so anda se a
// geracao nao mudou — um RECARREGADO/esquecer no meio da fatia manda
// recomeçar, nunca herda um cursor velho.
static void fatiaVarre(int perfil) {
  VarCtx c;
  DiarioFiltro f;
  long long q0, de;
  int seq0, ger, resg;
  memset(&c, 0, sizeof c); memset(&f, 0, sizeof f);
  TRAVA();
  c.perfil = perfil; c.ger = ger = S.varGer;
  c.teto = FSIN_VEZ_MAX;
  q0 = S.cur; seq0 = S.curSeq; de = S.varDe;
  resg = q0 == LLONG_MIN && seq0 == FSIN_SEQ_TODA && de > 0;
  if (resg) {
    // Cursor sentinela do modelo restaurado com a marca `curRest`: o
    // resgate anda em lotes so nas entradas MAIS NOVAS que ela — o diario
    // que ja era do modelo nao repassa (ressalva R1 da r1). O cursor
    // principal nao anda: o resgate tem o seu proprio.
    c.teto = FSIN_RESGATE_FATIA;
    f.de = de + 1;                 // de e INCLUSIVO: na marca ja era dela
    q0 = S.varCur; seq0 = S.varSeq;
    if (q0) {
      if (seq0 >= FSIN_SEQ_TODA) f.ate = q0;
      else { f.ate = q0 + 1; f.saltar = seq0; }
    }
  } else if (q0) {
    if (seq0 >= FSIN_SEQ_TODA) f.ate = q0;         // grupo do cursor consumido
    else { f.ate = q0 + 1; f.saltar = seq0; }      // retoma dentro do grupo
  }
  f.decrescente = 1;
  DTRAVA();
  diario_percorrer(perfil, &f, cbVar, &c);
  TRAVA();
  if (!c.abortou && S.varGer == ger) {
    if (resg) {
      if (c.cortou) {
        S.varCur = c.qUlt;
        S.varSeq = c.nUlt + (c.qUlt == q0 && seq0 < FSIN_SEQ_TODA ? seq0 : 0);
      } else { S.varDe = 0; S.varCur = 0; S.varSeq = 0; S.varrer = 0; }
    } else if (c.cortou) {
      S.cur = c.qUlt;
      S.curSeq = c.nUlt + (c.qUlt == q0 && seq0 < FSIN_SEQ_TODA ? seq0 : 0);
    } else S.varrer = 0;
    S.sujo = 1;
  }
  DTRAVA();
}

// O episodio completou a temporada? Credita o valor cheio UMA vez por
// temporada (regra da serie na tabela 2.2). Roda no fio de tras, fora do
// diario, com (perfil, geracao) capturados: o sinal de saida e conferido
// antes de cada dependencia externa e a reentrada na trava descarta se o
// perfil/geracao andou ou se houve sair (correcao FAROL38D — a r3 viu o
// credito da temporada cair no perfil errado e desfazer um esquecer).
static void confereTemporada(const char *id, int t, int perfil, int ger) {
  int idx, ne, i, tot = 0, vistos = 0;
  const CatEp *ep;
  uint64_t hb;
  if (fioMorre) return;
  if ((idx = cat_indice_por_imdb(id)) < 0) return;
  ne = cat_n_episodios(idx);
  for (i = 0; i < ne && !fioMorre; i++) {
    if ((ep = cat_episodio(idx, i)) == NULL || ep->temporada != t) continue;
    tot++;
    if (diario_episodio(perfil, id, t, ep->episodio, NULL)) vistos++;
  }
  if (fioMorre || !tot || vistos < tot) return;
  hb = hDom(id, K_TEMP, t);
  TRAVA();
  if (!fioMorre && S.perfil == perfil && S.varGer == ger &&
      achaHb(hb) < 0) {
    Cred c;
    const CatItem *ci = cat_item(idx);
    long tmdb = ci ? ci->tmdb : 0;
    long long agora = agoraS();
    memset(&c, 0, sizeof c);
    c.idh = h1(id); c.hb = hb; c.eh = hb ^ mix64((uint64_t)agora);
    c.quando = agora; c.chave = chaveObra(id, 1, tmdb);
    c.delta = FG_D_TERMINOU; c.faixa = (int16_t)faixaAgora(agora);
    c.kind = K_TEMP;
    credita(id, 1, tmdb, &c);
  }
  DTRAVA();
}

// Conta episodios vistos do titulo — a regra §1.3 e "1-3 episodios".
static int contaEps(const DiarioEntrada *e, void *c) {
  int *n = (int *)c;
  if (e->temporada || e->episodio) (*n)++;
  return *n < 4;                       // so interessa saber 1..3
}

// "Serie largada" (secao 1.3): progresso parado ha 21+ dias, 1-3 episodios
// vistos, ultima vez ha 21+ dias, episodio mais adiante existe no catalogo
// e o titulo nunca tomou este sinal. Uma varredura por carga do modelo, no
// fio de tras.
static void varreLargada(int perfil, int ger) {
  ProgRegistro regs[64];
  int n, i;
  long long agora = agoraS();
  long long agoraMs = agora * 1000LL;
  if (fioMorre) return;
  n = prog_ler(regs, 64);
  for (i = 0; i < n && !fioMorre; i++) {
    int t = 0, e = 0, idx, ne, k, ha = 0, neps = 0, invalido = 0;
    long long q = 0;
    uint64_t hb;
    DiarioFiltro df;
    if (strcmp(regs[i].tipo, "series") || !regs[i].contentId[0] ||
        regs[i].perfil != perfil ||
        regs[i].lastWatchedMs <= 0 ||
        agoraMs - regs[i].lastWatchedMs < FSIN_LARGADA_MS) continue;
    hb = hDom(regs[i].contentId, K_LARGADA, 0);
    TRAVA();
    if (fioMorre || S.perfil != perfil || S.varGer != ger) invalido = 1;
    else ha = achaHb(hb) >= 0;
    DTRAVA();
    if (invalido) return;
    if (ha) continue;
    // §1.3: a data do ultimo episodio tambem tem de fazer os 21 dias — um
    // episodio visto esta semana nao e serie largada (correcao FAROL38B).
    if (!diario_ultimo_episodio(perfil, regs[i].contentId, &t, &e, &q) ||
        q <= 0 || q > agora || agora - q < FSIN_LARGADA_MS / 1000) continue;
    // e so com 1-3 episodios vistos: quem viu mais nao "largou", parou.
    memset(&df, 0, sizeof df);
    df.id = regs[i].contentId; df.tipo = DIARIO_SERIE;
    diario_percorrer(perfil, &df, contaEps, &neps);
    if (fioMorre) return;
    if (neps < 1 || neps > 3) continue;
    if ((idx = cat_indice_por_imdb(regs[i].contentId)) < 0) continue;
    ne = cat_n_episodios(idx);
    for (k = 0; k < ne && !ha && !fioMorre; k++) {
      const CatEp *ep = cat_episodio(idx, k);
      if (ep && (ep->temporada > t || (ep->temporada == t && ep->episodio > e)))
        ha = 1;
    }
    if (!ha) continue;
    TRAVA();
    if (!fioMorre && S.perfil == perfil && S.varGer == ger &&
        achaHb(hb) < 0) {
      Cred c;
      long long agora = agoraS();
      memset(&c, 0, sizeof c);
      c.idh = h1(regs[i].contentId); c.hb = hb;
      c.eh = hb ^ mix64((uint64_t)agora);
      c.quando = agora; c.chave = chaveObra(regs[i].contentId, 1, 0);
      c.delta = FG_D_LARGOU; c.faixa = (int16_t)faixaAgora(agora);
      c.kind = K_LARGADA;
      credita(regs[i].contentId, 1, 0, &c);
    }
    DTRAVA();
  }
}

// ------------------------------------------------------------- fio de tras --
// O que o passo delegava ao quadro e a r2 marcou: as conferidas de
// temporada, a varredura de serie-largada e as gravacoes de
// modelo/estado/fichas. Ficaram num fio proprio (correcao FAROL38C): o
// passo so acorda o fio; nenhuma escrita de arquivo volta ao fio chamador.
// Cada volta do fio executa UMA unidade e o NV_UM_FIO cede a fibra entre
// elas — sem fio de verdade o trabalho continua fatiado por rodada.
static int trabPendente(long long agora) {   // com a trava tomada
  return S.trocando || S.diaFrio ||
         (S.perfil > 0 &&
          (S.nTempChk > 0 || !S.largadaVarrida ||
           ((S.sujo || S.fichaSujo) && agora - S.ultGrav >= FSIN_GRAVA_S)));
}
static void trabFatia(void) {
  char id[DIARIO_ID_MAX]; int t, p, ger;
  long long agora;
  TRAVA();
  if (fioMorre) { DTRAVA(); return; }
  // a troca de perfil vem primeiro: nada de conferida/gravacao na fronteira
  if (S.trocando) { DTRAVA(); fioTrocaPerfil(); return; }
  if (!S.ligado || S.perfil <= 0) { DTRAVA(); return; }
  agora = agoraS();
  // temporada completa? (fila alimentada pelas vezes de episodio) — a unidade
  // leva (perfil, geracao) junto: a conferida descarta se a fronteira andou.
  if (S.nTempChk) {
    S.nTempChk--;
    snprintf(id, sizeof id, "%s", S.tempChk[S.nTempChk].id);
    t = S.tempChk[S.nTempChk].t;
    p = S.tempChk[S.nTempChk].perfil;
    ger = S.tempChk[S.nTempChk].ger;
    DTRAVA();
    confereTemporada(id, t, p, ger);
    return;
  }
  if (!S.largadaVarrida) {
    S.largadaVarrida = 1;
    p = S.perfil; ger = S.varGer;
    DTRAVA();
    varreLargada(p, ger);
    return;
  }
  // diario descarregado depois da troca (evicao)? a carga fria volta aqui,
  // neste fio — nunca no passo (correcao FAROL38D).
  if (S.diaFrio) {
    p = S.perfil;
    S.diaFrio = 0;
    DTRAVA();
    if (!fioMorre) aqueceDiario(p);
    // Diario quente: o modelo restaurado (cursor sentinela) ganha a marca
    // `curRest` — a entrada mais nova que ja era dele. Sem arquivo de
    // estado e sem marca gravada, a medida e do diario como esta; o lote
    // que chegar depois dela volta a pesar (ressalva R1 da r1). A medida
    // vai SEM a trava: diario_percorrer segura a trava do diario e cbVar,
    // la dentro, pega m — medir sob m inverteria a ordem das travas.
    {
      long long marca = !fioMorre ? marcaDiario(p) : 0;
      TRAVA();
      if (!fioMorre && p == S.perfil && !S.curRest &&
          S.cur == LLONG_MIN && S.curSeq == FSIN_SEQ_TODA)
        S.curRest = marca;
      DTRAVA();
    }
    return;
  }
  // gravacoes juntas, com folga de 60 s — a serializacao sai sob a trava e
  // a E/S vai sem ela (correcao FAROL38D: nem este fio segura a trava no
  // disco; a fronteira do esquecer/sair e conferida no rename).
  if ((S.sujo || S.fichaSujo) && agora - S.ultGrav >= FSIN_GRAVA_S) {
    char cam[1100], nome[64];
    char *gm = NULL, *ge = NULL; unsigned char *gf = NULL;
    size_t nGm = 0, nGe = 0, nGf = 0;
    int pGrav = S.perfil, eGer = S.esqGer;
    S.ultGrav = agora;
    // `recarrega` alto = restauracao pisando o arquivo: a gravacao da RAM
    // velha espera a troca que so le o disco (ressalva R5 da r1).
    if (S.sujo && !S.recarrega) {
      gm = textoModelo(&nGm); ge = textoEstado(&nGe); S.sujo = 0;
    }
    if (S.fichaSujo) { gf = textoFichas(&nGf); S.fichaSujo = 0; }
    DTRAVA();
    if (pGrav > 0) {
      if (gm) { nomeGosto(pGrav, nome, sizeof nome);
        if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, gm, nGm, 0, eGer); }
      if (ge) { nomeEstado(pGrav, nome, sizeof nome);
        if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, ge, nGe, 0, eGer); }
    }
    if (gf && caminho("farol-fichas.bin", cam, sizeof cam))
      gravaBuf(cam, gf, nGf, 0, eGer);
    free(gm); free(ge); free(gf);
    return;
  }
  DTRAVA();
}
static void *fioTrab(void *u) {
  (void)u;
  for (;;) {
    TRAVA();
    while (!fioMorre && !trabPendente(agoraS()))
      pthread_cond_wait(&condTrab, &trava);
    if (fioMorre) { DTRAVA(); break; }
    fioTrabOcup = 1;
    DTRAVA();
    trabFatia();
    TRAVA();
    fioTrabOcup = 0;
    DTRAVA();
#ifdef NV_UM_FIO
    fio1_ceder();
#endif
  }
  return NULL;
}

void farolsinal_passo(int momentoBom) {
  int i, perfil, varre, pronto;
  long long agora;
  TRAVA();
  pronto = garante_i();      // so agenda a troca; a E/S fica no fio de tras
  perfil = S.perfil; agora = agoraS();
  S.momentoBom = momentoBom;   // o fio lento le aqui — nunca com player aberto
  // a fila lenta so nasce quando ha transporte e pedido — leitura e
  // criacao no mesmo fecho (correcao FAROL38B: busca/fioVivo liam soltos)
  if (pronto && busca && !fioVivo && !fioMorre && S.nPed &&
      !pthread_create(&fioId, NULL, fioLenta, NULL)) fioVivo = 1;
  if (pronto) {
    // pendencias velhas demais morrem: a ficha nao veio em 30 dias
    for (i = S.nPend - 1; i >= 0; i--)
      if (S.pend[i].c.quando < agora - (long long)FSIN_PEND_DIAS * 86400)
        { pendTira(i); S.sujo = 1; }
    varre = S.varrer;
  } else varre = 0;
  // conferidas, trocas de perfil, cargas e gravacoes vao para o fio de
  // tras — o quadro so agenda (correcao FAROL38C/D: a UI nao espera
  // diario, catalogo nem disco, nem no init nem na troca de perfil).
  if (trabPendente(agora)) acordaTrab();
  DTRAVA();
  // varredura do diario (pos-RECARREGADO ou primeira carga): UMA fatia por
  // passada com teto rigido de FSIN_VEZ_MAX entregas, cursor composto que
  // avanca ate em empate de `quando` — termina sempre (correcao FAROL38C).
  // diario_carregado NAO dispara a carga: se a loja ainda esta fria, quem
  // esquenta e o fio de tras e a varredura espera a proxima passada.
  if (varre && !diario_carregado(perfil)) {
    varre = 0;
    TRAVA();
    if (!S.trocando && S.perfil == perfil) { S.diaFrio = 1; acordaTrab(); }
    DTRAVA();
  }
  // O aquecimento (ate FSIN_AQUECE_TITULOS titulos) so anda em momentoBom —
  // o mesmo !player_aberto do caca_passo: com o player na frente o quadro
  // nao faz nem a leitura do diario nem a fila lenta (issue 43, secao 4.4).
  if (varre && momentoBom) fatiaVarre(perfil);
}

void farolsinal_sair(void) {
  char cam[1100], nome[64];
  char *gm = NULL, *ge = NULL;
  unsigned char *gf = NULL;
  size_t nGm = 0, nGe = 0, nGf = 0;
  int vivo, tvivo, p;
  TRAVA();
  garante_i();
  // NAO espera busca, troca nem varredura em curso: o fio da UI nao pode
  // pendurar na saida (correcao FAROL38B/C). Os fios veem fioMorre e
  // morrem sozinhos; o que ainda faltava grava do estado — a pendencia
  // volta na proxima carga. A copia sai sob a trava e a escrita vai SEM
  // ela (correcao FAROL38D): escritor em voo atrasado nao instala nada —
  // so estes buffers `final` pisam no disco depois do sinal.
  p = S.perfil;
  fioMorre = 1;
  pthread_cond_signal(&condPed);
  pthread_cond_signal(&condTrab);
  pthread_cond_broadcast(&condTroca);
  vivo = fioVivo; fioVivo = 0;
  tvivo = fioTrabVivo; fioTrabVivo = 0;
  if (p > 0) { gm = textoModelo(&nGm); ge = textoEstado(&nGe); }
  if (S.fc.sujo || S.fc.n) gf = textoFichas(&nGf);
  S.fc.sujo = 0; S.sujo = 0; S.trocando = 0;
  DTRAVA();
  if (p > 0) {
    if (gm) { nomeGosto(p, nome, sizeof nome);
      if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, gm, nGm, 1, 0); }
    if (ge) { nomeEstado(p, nome, sizeof nome);
      if (caminho(nome, cam, sizeof cam)) gravaBuf(cam, ge, nGe, 1, 0); }
  }
  if (gf && caminho("farol-fichas.bin", cam, sizeof cam))
    gravaBuf(cam, gf, nGf, 1, 0);
  free(gm); free(ge); free(gf);
  if (vivo) pthread_detach(fioId);
  if (tvivo) pthread_detach(fioTrabId);
}

void farolsinal_esquecer(void) {
  char cam[1100], nome[64];
  DIR *d;
  struct dirent *de;
  int p;
  // travaPronta no lugar do antigo TRAVA+garante_i (ressalva herdada da 38,
  // ligada ao logout na issue 43): com a troca de perfil pendente a rotina
  // cancelava a unidade e apagava o modelo do perfil ERRADO — roubar a
  // unidade e esperar a carga fechar apaga os arquivos certos. Sem perfil
  // residente (retorno 0) faz o reset do mesmo jeito, sob a trava.
  if (!travaPronta()) { TRAVA(); garante_i(); }
  // O logout esquece a CONTA: todos os perfis dela, nao so o residente —
  // o gosto do perfil 2 ficava no disco e subia para a conta seguinte
  // (revisao FAROL43-r1). Mesmo par de arquivos que sorg_esquecer e
  // fontepref_esquecer varrem.
  for (p = 1; p <= CONTA_PERFIL_MAX; p++) {
    nomeGosto(p, nome, sizeof nome);
    if (caminho(nome, cam, sizeof cam)) remove(cam);
    nomeEstado(p, nome, sizeof nome);
    if (caminho(nome, cam, sizeof cam)) remove(cam);
  }
  // A marca das notas e por CONTA (hash do usuario) — e o logout de verdade
  // limpa a sessao ANTES do esquecer (ajustes.c), entao o nome nao pode
  // mais ser calculado aqui: varre o que houver de farol-notas-* na pasta.
  if (S.dir[0] && (d = opendir(S.dir)) != NULL) {
    while ((de = readdir(d)) != NULL)
      if (!strncmp(de->d_name, "farol-notas-", 12)) {
        if (caminho(de->d_name, cam, sizeof cam)) remove(cam);
      }
    closedir(d);
  }
  // Legado da primeira versao, quando a marca era um nome fixo.
  if (caminho("farol-notas.txt", cam, sizeof cam)) remove(cam);
  if (caminho("farol-fichas.bin", cam, sizeof cam)) remove(cam);
  farolgosto_iniciar(&S.g, (uint16_t)S.perfil);
  farolficha_cache_iniciar(&S.fc);
  S.nCred = S.nPend = S.nObra = S.obraProx = S.nAli = 0;
  S.nPed = S.nTempChk = S.nOri = S.oriProx = 0;
  S.cur = 0; S.curSeq = 0; S.curRest = 0; S.varDe = 0; S.varCur = 0;
  S.varSeq = 0; S.varrer = 1; S.varGer++; S.nVarId = 0;
  S.largadaVarrida = 0; S.rec = 1;
  S.sujo = 0;
  S.fichaSujo = 0;
  S.esqGer++;                 // invalida gravacao/varredura/conferida em voo
  S.diaFrio = 0;
  S.recarrega = 0;
  if (S.trocando) { S.trocando = 0; S.pendTroca = 0; }
  DTRAVA();
}

// 1 enquanto houver trabalho de fundo pendente ou em curso — fatia de
// varredura, conferida, largada ou gravacao (teste/servico; correcao
// FAROL38C).
int farolsinal_teste_trabalho(void) {
  int r;
  TRAVA();
  garante_i();
  r = S.trocando || S.diaFrio || S.varrer || fioTrabOcup ||
      trabPendente(agoraS());
  DTRAVA();
  return r;
}

void farolsinal_teste_zera(void) {
  TRAVA();
  memset(&S.g, 0, sizeof S.g);
  S.perfil = 0; S.rec = 1; S.cur = 0; S.curSeq = 0;
  S.curRest = 0; S.varDe = 0; S.varCur = 0; S.varSeq = 0;
  S.varrer = 0; S.varGer++; S.nVarId = 0; S.largadaVarrida = 0;
  S.momentoBom = 1;
  S.nCred = S.nPend = S.nObra = S.obraProx = S.nAli = 0;
  S.nPed = S.nTempChk = S.nOri = S.oriProx = 0;
  S.sujo = S.fichaSujo = 0; S.ultGrav = 0;
  S.trocando = S.pendTroca = S.diaFrio = S.recarrega = 0; S.esqGer++;
  fioMorre = 0;                          // "reboot" tambem do fio lento
  DTRAVA();
}
