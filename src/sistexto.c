// Ver sistexto.h.
//
// EVENTOS (fila do NuvioActivity, uma string por evento, primeira letra = tipo):
//   T<texto>  o texto inteiro do IME mudou       D<texto>  IME: Concluir
//   X         IME fechou pelo Voltar            P<texto>  voz: parcial
//   V<texto>  voz: final                        R<0-100>  voz: nivel do som
//   S<estado> ouvindo | permissao | sistema | teclado (caiu no IME)
//   E<motivo> nada | negada | erro:N | ocupado  (a voz terminou sem texto)
//   I<texto>  so para o log (reconhecedor=sim permissao=nao...)
// Um motivo depois de ":" no S diz por que caiu de degrau (S sistema:negada).
#include "sistexto.h"
#include "descoberta.h"
#include "entrada_texto.h"
#include "arteurl.h"
#include "ajustes.h"
#ifdef NV_ANDROID
#include "android.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int   dono, estado, pedeTeclado, quedaTeclado;
static float nivel;
static const char *aviso = "";
// O aviso do botao de microfone da Samsung: vale so com o teclado dela aberto.
static const char AVISO_MIC_CONTROLE[] = "Aperte o microfone do controle e fale.";
// Teclado da Samsung aberto pelo botao de microfone. Ele cobre a tela e nao da
// para esconde-lo e espiar o app: o texto do meio do caminho NAO vai ao campo
// (nada de busca, resultados nem cartazes montados por baixo dele). So o
// texto final, ao fechar, entra e busca.
static int micTeclado;

#define ST_FILA 16
// One event tag plus a complete poster template and terminator.
#define ST_EVENTO_MAX (NV_ARTE_URL_MAX + 1)
static char  fila[ST_FILA][ST_EVENTO_MAX];
static int   filaIni, filaN;

static int modoTeste = -1;
void st_teste_ligar(int on) { modoTeste = on ? 1 : 0; }
static int teste(void) {
  if (modoTeste < 0) { const char *e = getenv("NUVIO_SISTEXTO_TESTE"); modoTeste = e && *e == '1'; }
  return modoTeste;
}

// FORA DO ANDROID quem fala com o teclado da TV e entrada_texto.h (agente
// imetv: SDL_StartTextInput do SDL da LG, MEDIDO na C9 com microfone no
// teclado; <input> escondido no .wgt; host .NET no .tpk canario). Este modulo
// so traduz os avisos dele para a mesma fila do Android (st_evento).
int st_teclado_modo(void) {
#ifdef NV_TECLADO_SIS
  return ajustes_teclado_android();
#else
  return st_abre_sozinho() ? ajustes_teclado_android() : 0;   // teste() faz o papel do Android
#endif
}
int st_teclado_do_app(void) { return !st_abre_sozinho() && st_teclado_modo() == 1; }
int st_ime_disponivel(void) {
#ifdef NV_ANDROID
  return 1;
#else
  return !st_teclado_do_app() && (teste() || (texto_sistema_disponivel() & TS_TECLADO) != 0);
#endif
}
int st_voz_disponivel(void) {
#ifdef NV_ANDROID
  return 1;
#else
  return teste() || (texto_sistema_disponivel() & TS_VOZ) != 0;
#endif
}
int st_abre_sozinho(void) {
#ifdef NV_ANDROID
  return 1;
#else
  return teste();
#endif
}
static int pelaPlataforma(void) {
#ifdef NV_ANDROID
  return 0;
#else
  return !teste();
#endif
}

int st_estado(void) { return estado; }
int st_dono(void) { return dono; }
float st_nivel(void) { return estado == ST_OUVINDO ? nivel : 0.0f; }
const char *st_aviso(void) { return aviso; }

static int proximo(char *dst, size_t n);
// O que sobrou de uma entrada anterior (a fala que terminou depois de a tela
// fechar) nao pode cair no campo novo.
static void descartar(void) { char ev[ST_EVENTO_MAX]; int k = 0; while (k++ < 64 && proximo(ev, sizeof ev)) {} }

static int proximoTipo;
void st_ime_tipo(int tipo) { proximoTipo = tipo; }

int st_ime_abrir(int d, const char *inicial, int max) {
  int ok = 0, tipo = proximoTipo;
  proximoTipo = ST_IME_TEXTO;
  if (!st_ime_disponivel()) return 0;
  descartar();
  micTeclado = 0;
  dono = d; nivel = 0.0f;
  if (!quedaTeclado) aviso = "";      // caiu da voz: o aviso de por que fica
  quedaTeclado = 0;
#ifdef NV_ANDROID
  // O tipo vai nos bits altos do max: a assinatura JNI fica a mesma.
  ok = android_st_teclado(inicial ? inicial : "", (max & 0xFFFF) | (tipo << 16));
#else
  (void)max; (void)tipo;
  if (pelaPlataforma()) { texto_sistema_abrir(inicial ? inicial : "", 0); ok = texto_sistema_aberto(); }
  else ok = 1;
#endif
  estado = ok ? ST_DIGITANDO : ST_PARADO;
  if (!ok) aviso = "O teclado do sistema não abriu.";
  printf("[texto] teclado do sistema: %s (dono %d)\n", ok ? "aberto" : "falhou", d);
  fflush(stdout);
  return ok;
}

int st_voz_iniciar(int d) {
  int ok = 0;
  if (!st_voz_disponivel()) {
    dono = d; estado = ST_PARADO;
    aviso = "Esta TV não tem reconhecimento de voz. Use o microfone do teclado do sistema.";
    printf("[voice] unavailable: no registered voice backend (owner=%d)\n", d);
    fflush(stdout);
    return 0;
  }
  descartar();
  micTeclado = 0;
  dono = d; aviso = ""; nivel = 0.0f;
#ifdef NV_ANDROID
  ok = android_st_ditar(desc_tmdb_idioma());
#else
  if (pelaPlataforma()) { texto_sistema_abrir("", 1); ok = texto_sistema_aberto(); }
  else ok = 1;
#endif
  estado = ok ? ST_OUVINDO : ST_PARADO;
#ifndef NV_ANDROID
  // Samsung (.tpk e .wgt): o botao abriu o teclado da Samsung, que ainda NAO
  // esta ouvindo. Nada de "Ouvindo…": o campo diz onde esta o microfone.
  if (ok && pelaPlataforma() && texto_sistema_voz_pelo_teclado()) {
    estado = ST_DIGITANDO;
    aviso = AVISO_MIC_CONTROLE;
    micTeclado = 1;
  }
#endif
  if (!ok) aviso = "O ditado não respondeu. Tente de novo ou digite.";
  printf("[voice] %s (owner=%d, language=%s)\n", ok ? "requested" : "startup failed", d, desc_tmdb_idioma());
  fflush(stdout);
  return ok;
}

void st_fechar(int d) {
  if (d != dono || estado == ST_PARADO) { if (d == dono) dono = ST_DONO_NENHUM; return; }
#ifdef NV_ANDROID
  android_st_fechar();
#else
  if (pelaPlataforma()) texto_sistema_fechar();
#endif
  estado = ST_PARADO;
  dono = ST_DONO_NENHUM;
  nivel = 0.0f;
  micTeclado = 0;
  if (aviso == AVISO_MIC_CONTROLE) aviso = "";
}

void st_teste_evento(const char *ev) {
  if (!ev || filaN >= ST_FILA) return;
  snprintf(fila[(filaIni + filaN) % ST_FILA], sizeof fila[0], "%s", ev);
  filaN++;
}

static int proximo(char *dst, size_t n) {
#ifdef NV_ANDROID
  if (android_st_evento(dst, n)) return 1;
#endif
  if (!filaN) return 0;
  snprintf(dst, n, "%s", fila[filaIni]);
  filaIni = (filaIni + 1) % ST_FILA;
  filaN--;
  return 1;
}

// Um estado que chegou do Android. O aviso diz POR QUE caiu de degrau.
static void mudouEstado(const char *s) {
  const char *motivo = strchr(s, ':');
  if (!strncmp(s, "ouvindo", 7)) estado = ST_OUVINDO;
  else if (!strncmp(s, "permissao", 9)) estado = ST_PERMISSAO;
  else if (!strncmp(s, "sistema", 7)) {
    estado = ST_VOZ_SISTEMA;
    if (motivo && !strcmp(motivo, ":negada"))
      aviso = "Sem permissão do microfone: usando a voz do sistema.";
  } else if (!strncmp(s, "teclado", 7)) {
    estado = ST_PARADO;
    pedeTeclado = 1;
    aviso = motivo && !strcmp(motivo, ":negada")
      ? "Sem permissão do microfone. Use o microfone do teclado do sistema."
      : "Esta TV não tem reconhecimento de voz. Use o microfone do teclado do sistema.";
  }
}

int st_ler(int d, char *dst, size_t n) {
  return st_ler_valor(d, dst, n, NULL);
}

int st_ler_valor(int d, char *dst, size_t n, int *valorRecebido) {
  char ev[ST_EVENTO_MAX];
  int r = ST_NADA, k;
  if (valorRecebido) *valorRecebido = 0;
  if (n) dst[0] = 0;
  for (k = 0; k < 32 && proximo(ev, sizeof ev); k++) {
    char t = ev[0];
    const char *v = ev + 1;
    if (d != dono) continue;              // de outro campo: descarta
    switch (t) {
      case 'T': case 'P':
        if (micTeclado) break;            // por baixo do teclado: espera o fim
        if (valorRecebido) *valorRecebido = 1;
        snprintf(dst, n, "%s", v); r = ST_TEXTO; break;
      case 'D': case 'V':
        if (valorRecebido) *valorRecebido = 1;
        micTeclado = 0;
        if (aviso == AVISO_MIC_CONTROLE) aviso = "";
        snprintf(dst, n, "%s", v);
        printf("[texto] %s: %d bytes\n", t == 'D' ? "teclado concluiu" : "voz final", (int)strlen(v));
        fflush(stdout);
        estado = ST_PARADO; nivel = 0.0f;
        return ST_FIM;
      case 'X':
        micTeclado = 0;
        if (aviso == AVISO_MIC_CONTROLE) aviso = "";
        estado = ST_PARADO;
        return ST_CANCELOU;
      case 'R': nivel = (float)atoi(v) / 100.0f; break;
      case 'S':
        mudouEstado(v);
        printf("[texto] estado: %s\n", v); fflush(stdout);
        if (pedeTeclado) { pedeTeclado = 0; quedaTeclado = 1; return ST_PEDE_TECLADO; }
        break;
      case 'I': printf("[texto] android: %s\n", v); fflush(stdout); break;
      case 'E':
        micTeclado = 0;
        printf("[voice] failed: %s\n", !strcmp(v, "nada") ? "no speech recognized"
               : !strcmp(v, "negada") ? "microphone permission denied"
               : !strcmp(v, "ocupado") ? "recognizer busy" : v);
        fflush(stdout);
        aviso = !strcmp(v, "nada") ? "Não entendi. Tente de novo."
              : !strcmp(v, "negada") ? "Sem permissão do microfone."
              : "O ditado não respondeu. Tente de novo ou digite.";
        estado = ST_PARADO; nivel = 0.0f;
        return ST_CANCELOU;
      default: break;
    }
  }
  return r;
}

int st_evento(const SDL_Event *e) {
  if (e->type == texto_sistema_evento()) {
    char ev[ST_EVENTO_MAX];
    if (e->user.code == TS_EV_TEXTO) {
      snprintf(ev, sizeof ev, "T%s", texto_sistema_valor());
      st_teste_evento(ev);
    } else if (e->user.code == TS_EV_ERRO) {
      st_teste_evento("Ekeyboard startup failed");
    } else if (e->user.code == TS_EV_FIM) {
      if (e->user.data1) { snprintf(ev, sizeof ev, "D%s", texto_sistema_valor()); st_teste_evento(ev); }
      else st_teste_evento("X");
    }
    return 1;
  }
  return texto_sistema_aberto() && texto_sistema_engole(e);
}
