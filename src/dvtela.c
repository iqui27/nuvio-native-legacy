// A tela do Dolby Vision em MKV. Ver dvtela.h.
#include "dvtela.h"
#include "video.h"
#include "anim.h"
#include "layout.h"
#include "teclavoltar.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- a maquina
// "Progresso" para a dica calma: qualquer sinal que mudou conta, nao so o passo
// (o demux abrir a fonte e progresso mesmo sem trocar o passo da tela).
static unsigned assinatura(const DvtelaSinais *s) {
  return (unsigned)(!!s->sondado) | (unsigned)(!!s->perfil) << 1 | (unsigned)(!!s->audioTrocado) << 2 |
         (unsigned)(!!s->caminho) << 3 | (unsigned)(!!s->fonteAberta) << 4 |
         (unsigned)(!!s->carregado) << 5 | (unsigned)(!!s->dvConfirmado) << 6 |
         (unsigned)(!!s->tocando) << 7;
}

void dvt_entrar(DvtelaEstado *e, Uint32 agora) {
  memset(e, 0, sizeof *e);
  e->ativa = 1;
  e->passo = DVT_PASSO_LER;
  e->entrouEm = e->mudouEm = agora;
}

void dvt_sair(DvtelaEstado *e, int saida, Uint32 agora) {
  if (!e->ativa) return;
  e->ativa = 0;
  e->saida = saida;
  e->saiuEm = agora;
  e->dica = 0;
}

int dvt_passo(DvtelaEstado *e, const DvtelaSinais *s, Uint32 agora) {
  int novo;
  unsigned a;
  if (!e->ativa || !s) return 0;
  // A fonte morreu: o erro de sempre do player cuida (Abrir fontes / Voltar).
  if (s->perfil) e->perfil = s->perfil;   // a nota da recusa diz o perfil
  if (s->falhou) { dvt_sair(e, DVT_SAIDA_FONTE, agora); return DVT_SAIDA_FONTE; }
  if (s->recusa) {
    e->recusa = s->recusa;
    dvt_sair(e, s->recusa == VIDEO_DV_NAO_PESSOA ? DVT_SAIDA_HDR10 : DVT_SAIDA_RECUSA, agora);
    return e->saida;
  }
  // SO SAI COM O DV DE VERDADE: o pipeline do NOSSO caminho disse DolbyVision
  // e esta tocando. loadCompleted nao basta (o preroll leva ate 10 s na C9), e
  // o "playing" do player da TV (o HDR10 coberto) nao conta.
  if (s->caminho && s->dvConfirmado && s->tocando) {
    if (s->perfil) e->perfil = s->perfil;
    dvt_sair(e, DVT_SAIDA_DV, agora);
    return DVT_SAIDA_DV;
  }
  novo = DVT_PASSO_LER;
  if (s->sondado && s->perfil) novo = s->audioTrocado ? DVT_PASSO_AUDIO : DVT_PASSO_ACHOU;
  if (s->caminho) novo = s->carregado ? DVT_PASSO_IMAGEM : DVT_PASSO_ABRIR;
  if (s->perfil) e->perfil = s->perfil;
  if (s->audioTrocado && !e->audioTrocado) {
    e->audioTrocado = 1;
    snprintf(e->audioDe, sizeof e->audioDe, "%s", s->audioDe);
    snprintf(e->audioPara, sizeof e->audioPara, "%s", s->audioPara);
  }
  // O passo nunca volta (a sonda que repete nao "desacende" o que ja foi lido).
  if (novo > e->passo) e->passo = novo;
  a = assinatura(s);
  if (a != e->sinaisVistos) { e->sinaisVistos = a; e->mudouEm = agora; e->dica = 0; }
  if ((Uint32)(agora - e->mudouEm) >= DVT_DICA_MS) e->dica = 1;
  return 0;
}

// ---------------------------------------------------------------- a instancia
static DvtelaEstado E;
static float alfa;            // a tela inteira (entrada e o esvair sobre o filme)
static float dicaA;           // a linha da dica
static int passoLogado = -1;
static int dicaLogada;
// O OK que sobra do Play nao pode escolher HDR10: a tecla so vale depois disto.
#define DVT_OK_ESPERA_MS 800u

const char *dvtela_nome_passo(int p) {
  switch (p) {
    case DVT_PASSO_LER:    return "ler";
    case DVT_PASSO_ACHOU:  return "achou";
    case DVT_PASSO_AUDIO:  return "audio";
    case DVT_PASSO_ABRIR:  return "abrir";
    case DVT_PASSO_IMAGEM: return "imagem";
    default:               return "?";
  }
}
const char *dvtela_nome_saida(int s) {
  switch (s) {
    case DVT_SAIDA_DV:     return "dv";
    case DVT_SAIDA_RECUSA: return "hdr10";
    case DVT_SAIDA_VOLTAR: return "voltar";
    case DVT_SAIDA_HDR10:  return "pessoa-hdr10";
    case DVT_SAIDA_FONTE:  return "fonte";
    default:               return "-";
  }
}
static const char *nomeRecusa(int r) {
  switch (r) {
    case VIDEO_DV_NAO_SEM_DV:  return "sem-dv";
    case VIDEO_DV_NAO_PERFIL:  return "perfil";
    case VIDEO_DV_NAO_AUDIO:   return "audio";
    case VIDEO_DV_NAO_SONDA:   return "sonda";
    case VIDEO_DV_NAO_LENTO:   return "lento";
    case VIDEO_DV_NAO_FALHOU:  return "falhou";
    case VIDEO_DV_NAO_PESSOA:  return "pessoa";
    case VIDEO_DV_NAO_CONTENTOR: return "nao-mkv";
    default:                   return "-";
  }
}

static void logSaida(void) {
  printf("[dvtela] saiu (%s%s%s, %u ms)\n", dvtela_nome_saida(E.saida),
         E.recusa ? ": " : "", E.recusa ? nomeRecusa(E.recusa) : "",
         (unsigned)(E.saiuEm - E.entrouEm));
  fflush(stdout);
}

void dvtela_entrar(Uint32 agora) {
  // Fonte nova com a tela de pe (a troca automatica caiu noutra fonte DV): os
  // passos recomecam, a tela fica.
  if (E.ativa) printf("[dvtela] fonte nova (%u ms)\n", (unsigned)(agora - E.entrouEm));
  dvt_entrar(&E, agora);
  passoLogado = DVT_PASSO_LER; dicaLogada = 0;
  dicaA = 0.0f;
  printf("[dvtela] entrou\n[dvtela] passo %d %s (0 ms)\n", DVT_PASSO_LER, dvtela_nome_passo(DVT_PASSO_LER));
  fflush(stdout);
}

void dvtela_sair(int saida, Uint32 agora) {
  if (!E.ativa) return;
  dvt_sair(&E, saida, agora);
  logSaida();
}

int  dvtela_ativa(void) { return E.ativa; }
int  dvtela_visivel(void) { return E.ativa || alfa > 0.004f; }
float dvtela_alfa(void) { return alfa; }
float dvtela_dica_alfa(void) { return dicaA; }
const DvtelaEstado *dvtela_estado(void) { return &E; }
// Um botao so: o foco e dele enquanto a tela esta de pe.
int  dvtela_foco_botao(void) { return E.ativa; }

int dvtela_atualizar(const DvtelaSinais *s, float dt, Uint32 agora) {
  int saiu = 0;
  if (E.ativa) {
    saiu = dvt_passo(&E, s, agora);
    if (saiu) logSaida();
    else {
      if (E.passo != passoLogado) {
        passoLogado = E.passo;
        printf("[dvtela] passo %d %s (%u ms)", E.passo, dvtela_nome_passo(E.passo),
               (unsigned)(agora - E.entrouEm));
        if (E.passo == DVT_PASSO_ACHOU || E.passo == DVT_PASSO_AUDIO) printf(" perfil=%d", E.perfil);
        if (E.passo == DVT_PASSO_AUDIO) printf(" audio=%s->%s", E.audioDe, E.audioPara);
        printf("\n"); fflush(stdout);
      }
      if (E.dica && !dicaLogada) {
        dicaLogada = 1;
        printf("[dvtela] dica: %u ms sem mudanca no passo %s\n", (unsigned)(agora - E.mudouEm),
               dvtela_nome_passo(E.passo));
        fflush(stdout);
      }
      if (!E.dica) dicaLogada = 0;
    }
  }
  if (dt > 0.1f) dt = 0.1f;
  // Entrada pela mola da tela (NV_MOLA_TELA, mais firme); a saida e mais lenta:
  // o esvair sobre o filme e o que a pessoa ve por ultimo. Reduzidas: corta
  // (anim_mola obedece a politica).
  alfa = anim_mola(alfa, E.ativa ? 1.0f : 0.0f, dt, E.ativa ? NV_MOLA_TELA * 1.6f : NV_MOLA_TELA * 0.75f);
  if (!E.ativa && alfa < 0.004f) alfa = 0.0f;
  if (E.ativa && alfa > 0.996f) alfa = 1.0f;
  dicaA = anim_mola(dicaA, E.ativa && E.dica ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  return saiu;
}

static int ehOk(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

int dvtela_evento(const SDL_Event *e, Uint32 agora) {
  if (!E.ativa || !e) return DVT_EV_NADA;
  if (e->type != SDL_KEYDOWN) return DVT_EV_ENGOLIU;
  if (nv_tecla_voltar(e)) { dvtela_sair(DVT_SAIDA_VOLTAR, agora); return DVT_EV_VOLTAR; }
  if (ehOk(e)) {
    // Tecla segurada ou o segundo toque do Play: nao e escolha.
    if (e->key.repeat || (Uint32)(agora - E.entrouEm) < DVT_OK_ESPERA_MS) return DVT_EV_ENGOLIU;
    dvtela_sair(DVT_SAIDA_HDR10, agora);
    return DVT_EV_HDR10;
  }
  // Setas, Pause, numeros: nada a fazer, e nada passa ao OSD que esta coberto.
  return DVT_EV_ENGOLIU;
}

// O BACKEND DOS ALVOS SEM O CAMINHO DO DV (Tizen, .tpk, Android). So o ramo
// webOS e o coto do Mac de video.c tem a implementacao; nos outros a tela
// nunca entra e o preroll nao existe. `weak`: a definicao de video.c vence.
__attribute__((weak)) void video_dv_fase(VideoDvFase *f) { if (f) memset(f, 0, sizeof *f); }
__attribute__((weak)) int  video_dv_candidato(const char *url) { (void)url; return 0; }
__attribute__((weak)) void video_dv_tela(int cobrindo) { (void)cobrindo; }
__attribute__((weak)) void video_dv_recusar(void) {}
__attribute__((weak)) void video_dv_segurar(int segurar) { (void)segurar; }
__attribute__((weak)) int  video_iniciando(void) { return 0; }
