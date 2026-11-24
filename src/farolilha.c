// Farol na Ilha — a politica de silencio. Ver farolilha.h.
#include "farolilha.h"
#include "farolregra.h"     // FAROL_C_*, FAROL_A_* (so constantes)
#include "faroltexto.h"     // FT_*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static long long relogioSeg(void) { return (long long)time(NULL); }
static unsigned relogioMs(void) {
#ifdef CLOCK_MONOTONIC
  struct timespec ts;
  if (!clock_gettime(CLOCK_MONOTONIC, &ts))
    return (unsigned)((unsigned long long)ts.tv_sec * 1000ull + (unsigned long long)ts.tv_nsec / 1000000ull);
#endif
  return (unsigned)((unsigned long long)time(NULL) * 1000ull);
}

static long long (*agoraSeg)(void) = relogioSeg;
static unsigned (*agoraMs)(void) = relogioMs;

static int ativo = 1;

// Titulos ja avisados nesta sessao (o teto de 1 por titulo).
static char titulos[FAROLILHA_TITULOS][FAROLILHA_TITULO_MAX];
static int nTitulos;
static int codigoVez = -1;

// Codigos dispensados com Voltar e quando.
typedef struct { int codigo; long long quando; } Dispensa;
static Dispensa disp[FAROLILHA_DISPENSAS];
static int nDisp;

// Conserto calado: o ultimo, para nao abrir duas pilulas do mesmo.
static int consAcao = -1;
static unsigned consMs;

// Atividade: desde quando esta ocupada (0 = livre).
static int atvOcupada;
static unsigned atvDesde;

void farolilha_relogio(long long (*seg)(void), unsigned (*ms)(void)) {
  agoraSeg = seg ? seg : relogioSeg;
  agoraMs = ms ? ms : relogioMs;
}

void farolilha_sessao_nova(void) {
  memset(titulos, 0, sizeof titulos);
  nTitulos = 0;
  codigoVez = -1;
  consAcao = -1;
  consMs = 0;
  atvOcupada = 0;
  atvDesde = 0;
}

void farolilha_zerar(void) {
  farolilha_sessao_nova();
  memset(disp, 0, sizeof disp);
  nDisp = 0;
  ativo = 1;
}

void farolilha_definir_ativo(int ligado) { ativo = ligado ? 1 : 0; }
int  farolilha_ativo(void) { return ativo; }

static const char *chaveTitulo(const char *t) { return t ? t : ""; }

static int tituloAvisado(const char *t) {
  int i;
  for (i = 0; i < nTitulos; i++)
    if (!strncmp(titulos[i], t, FAROLILHA_TITULO_MAX - 1)) return 1;
  return 0;
}

static void tituloMarcar(const char *t) {
  if (nTitulos == FAROLILHA_TITULOS) {
    memmove(titulos[0], titulos[1], sizeof titulos[0] * (size_t)(FAROLILHA_TITULOS - 1));
    nTitulos--;
  }
  snprintf(titulos[nTitulos++], sizeof titulos[0], "%s", t);
}

static int dispIndice(int codigo) {
  int i;
  for (i = 0; i < nDisp; i++) if (disp[i].codigo == codigo) return i;
  return -1;
}

int farolilha_dispensado(int codigo) {
  int i = dispIndice(codigo);
  long long agora;
  if (i < 0) return 0;
  agora = agoraSeg();
  // Relogio que andou para tras (TV sem hora no arranque): na duvida, cala.
  if (agora < disp[i].quando) return 1;
  return agora - disp[i].quando < FAROLILHA_DISPENSA_S;
}

void farolilha_dispensar(int codigo) {
  int i = dispIndice(codigo);
  if (i < 0) {
    if (nDisp == FAROLILHA_DISPENSAS) {
      // Cheio: sai a mais velha.
      int j, velha = 0;
      for (j = 1; j < nDisp; j++) if (disp[j].quando < disp[velha].quando) velha = j;
      i = velha;
    } else i = nDisp++;
  }
  disp[i].codigo = codigo;
  disp[i].quando = agoraSeg();
}

int farolilha_proativo(const FarolIlhaFatos *f) {
  const char *t;
  if (!f || !ativo) return 0;
  // Nunca com video andando: o aviso so nasce ja fora do player.
  if (f->playerAberto || f->videoAndando) return 0;
  if (f->falhasSeguidas < FAROLILHA_FALHAS_MIN && f->paradas5min < FAROLILHA_PARADAS_MIN) return 0;
  t = chaveTitulo(f->titulo);
  if (tituloAvisado(t)) return 0;
  if (farolilha_dispensado(f->codigo)) return 0;
  tituloMarcar(t);
  codigoVez = f->codigo;
  return 1;
}

int farolilha_sintoma(const FarolIlhaFatos *f) {
  if (f && f->falhasSeguidas < FAROLILHA_FALHAS_MIN && f->paradas5min >= FAROLILHA_PARADAS_MIN)
    return RES_S_TRAVA;
  return RES_S_NAO_COMECA;
}

int farolilha_codigo_vez(void) { return codigoVez; }

const char *farolilha_conserto_frase(int acao) {
  switch (acao) {
    case FAROL_A_PROXIMA_FONTE:
    case FAROL_A_FONTE_MENOR:
    case FAROL_A_FONTE_SEM_DV:
    case FAROL_A_FONTE_SEM_DTS:
    case FAROL_A_FONTE_OUTRO_FORMATO: return FT_FONTE_TROCADA_SIMPLES;
    case FAROL_A_CONVERTER_AUDIO:     return FT_AUDIO_CONVERTIDO;
    case FAROL_A_TROCAR_FAIXA:        return FT_AUDIO_OUTRA_FAIXA_L2;
    case FAROL_A_ISOLAR_ADDON:        return FT_ADDON_ISOLADO;
    case FAROL_A_RECONECTAR_REDE:     return FT_REDE_RECONECTADA;
    case FAROL_A_LIMPAR_CACHE:        return FT_CACHE_LIMPO;
    case FAROL_A_DESLIGAR_HDR:        return FT_HDR_DESLIGADO;
    case FAROL_A_ALIVIAR_REABRIR:     return FT_MEMORIA_LIBERADA;
    case FAROL_A_TENTAR_DE_NOVO:      return FT_VOLTOU_A_TOCAR;
    default:                          return NULL;
  }
}

const char *farolilha_conserto(int acao, int sozinho) {
  const char *frase;
  unsigned agora;
  if (!ativo || !sozinho) return NULL;
  frase = farolilha_conserto_frase(acao);
  if (!frase) return NULL;
  agora = agoraMs();
  if (consAcao == acao && agora - consMs < FAROLILHA_CONSERTO_MS) return NULL;
  consAcao = acao;
  consMs = agora;
  return frase;
}

int farolilha_atividade(int ocupado) {
  unsigned agora = agoraMs();
  if (!ocupado) { atvOcupada = 0; atvDesde = 0; return 0; }
  if (!atvOcupada) { atvOcupada = 1; atvDesde = agora; }
  return agora - atvDesde >= FAROLILHA_ATIVIDADE_MS;
}

size_t farolilha_exportar(char *dst, size_t tam) {
  size_t n = 0;
  int i;
  if (!dst || !tam) return 0;
  dst[0] = 0;
  for (i = 0; i < nDisp; i++) {
    int w = snprintf(dst + n, tam - n, "%d %lld\n", disp[i].codigo, disp[i].quando);
    if (w < 0 || (size_t)w >= tam - n) { dst[n] = 0; break; }
    n += (size_t)w;
  }
  return n;
}

void farolilha_importar(const char *s) {
  long long agora = agoraSeg();
  nDisp = 0;
  memset(disp, 0, sizeof disp);
  while (s && *s && nDisp < FAROLILHA_DISPENSAS) {
    char *fim;
    long cod = strtol(s, &fim, 10);
    long long quando;
    if (fim != s && *fim == ' ') {
      const char *q = fim + 1;
      quando = strtoll(q, &fim, 10);
      if (fim != q && (*fim == '\n' || *fim == 0) && cod >= 0 && cod < FAROL_C_N && quando > 0 &&
          (agora < quando || agora - quando < FAROLILHA_DISPENSA_S) && dispIndice((int)cod) < 0) {
        disp[nDisp].codigo = (int)cod;
        disp[nDisp].quando = quando;
        nDisp++;
      }
    }
    s = strchr(s, '\n');
    if (s) s++;
  }
}
