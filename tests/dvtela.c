// A MAQUINA DA TELA DO DOLBY VISION EM MKV (dvtela.h). Sem GL, sem rede: os
// sinais do quadro entram a mao, como o player os montaria de video_dv_fase.
//
// O que protege (pedido do dono, 08/10, C9):
//   * a tela fica ate o pipeline CONFIRMAR o DV e tocar — nao sai no
//     loadCompleted, nem com DV sem `playing`, nem com `playing` sem DV;
//   * sai na recusa (perfil 7, audio, sonda) com o motivo, para a nota da ilha;
//   * Voltar cancela; OK no botao escolhe HDR10;
//   * os passos so andam com sinal real e a dica calma so vem com 60 s parado.
#include "dvtela.h"
#include "video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SDL_Event tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  return e;
}

// O caminho feliz, sinal por sinal, com a tela de pe ate o fim.
static void ficaAteConfirmar(void) {
  DvtelaEstado e;
  DvtelaSinais s;
  Uint32 t = 1000;
  memset(&e, 0, sizeof e); memset(&s, 0, sizeof s);
  dvt_entrar(&e, t);
  assert(e.ativa && e.passo == DVT_PASSO_LER);
  assert(dvt_passo(&e, &s, t += 500) == 0 && e.passo == DVT_PASSO_LER);
  s.sondado = 1; s.perfil = 8;
  assert(dvt_passo(&e, &s, t += 500) == 0 && e.passo == DVT_PASSO_ACHOU && e.perfil == 8);
  s.caminho = 1;
  assert(dvt_passo(&e, &s, t += 500) == 0 && e.passo == DVT_PASSO_ABRIR);
  s.fonteAberta = 1;
  assert(dvt_passo(&e, &s, t += 9000) == 0 && e.passo == DVT_PASSO_ABRIR);
  s.carregado = 1;
  assert(dvt_passo(&e, &s, t += 500) == 0 && e.passo == DVT_PASSO_IMAGEM);
  // loadCompleted nao e DV: a tela fica (era aqui que o painel de pausa subia).
  assert(dvt_passo(&e, &s, t += 9000) == 0 && e.ativa);
  // DV sem playing: fica.
  s.dvConfirmado = 1;
  assert(dvt_passo(&e, &s, t += 100) == 0 && e.ativa);
  // playing sem DV (o HDR10 do player da TV, por exemplo): fica.
  s.dvConfirmado = 0; s.tocando = 1;
  assert(dvt_passo(&e, &s, t += 100) == 0 && e.ativa);
  // Os dois: sai, e diz por que.
  s.dvConfirmado = 1;
  assert(dvt_passo(&e, &s, t += 100) == DVT_SAIDA_DV);
  assert(!e.ativa && e.saida == DVT_SAIDA_DV && e.saiuEm == t);
  // Depois de sair, nada a faz voltar sozinha.
  assert(dvt_passo(&e, &s, t += 100) == 0 && !e.ativa);
}

// O HDR10 do player da TV tambem diz "playing" com hdr: so o caminho conta.
static void tocandoAntesDoCaminhoNaoSai(void) {
  DvtelaEstado e;
  DvtelaSinais s;
  memset(&e, 0, sizeof e); memset(&s, 0, sizeof s);
  dvt_entrar(&e, 0);
  s.tocando = 1; s.dvConfirmado = 1;   // sem caminho: nao pode ser o nosso DV
  assert(dvt_passo(&e, &s, 100) == 0 && e.ativa);
}

static void audioTrocadoViraPasso(void) {
  DvtelaEstado e;
  DvtelaSinais s;
  memset(&e, 0, sizeof e); memset(&s, 0, sizeof s);
  dvt_entrar(&e, 0);
  s.sondado = 1; s.perfil = 8; s.audioTrocado = 1;
  snprintf(s.audioDe, sizeof s.audioDe, "A_TRUEHD");
  snprintf(s.audioPara, sizeof s.audioPara, "A_EAC3");
  assert(dvt_passo(&e, &s, 100) == 0);
  assert(e.passo == DVT_PASSO_AUDIO && e.audioTrocado);
  assert(!strcmp(e.audioDe, "A_TRUEHD") && !strcmp(e.audioPara, "A_EAC3"));
}

// A recusa sai com o motivo; cada um vira uma nota diferente na ilha.
static void saiNaRecusa(void) {
  static const int MOTIVOS[] = { VIDEO_DV_NAO_SEM_DV, VIDEO_DV_NAO_PERFIL, VIDEO_DV_NAO_AUDIO,
                                 VIDEO_DV_NAO_SONDA, VIDEO_DV_NAO_LENTO, VIDEO_DV_NAO_FALHOU };
  unsigned i;
  for (i = 0; i < sizeof MOTIVOS / sizeof *MOTIVOS; i++) {
    DvtelaEstado e;
    DvtelaSinais s;
    memset(&e, 0, sizeof e); memset(&s, 0, sizeof s);
    dvt_entrar(&e, 0);
    s.sondado = 1; s.perfil = 7; s.recusa = MOTIVOS[i];
    assert(dvt_passo(&e, &s, 2000) == DVT_SAIDA_RECUSA);
    assert(!e.ativa && e.recusa == MOTIVOS[i]);
  }
  // A fonte falhou de vez: sai para o erro de sempre do player.
  { DvtelaEstado e;
    DvtelaSinais s;
    memset(&e, 0, sizeof e); memset(&s, 0, sizeof s);
    dvt_entrar(&e, 0);
    s.falhou = 1;
    assert(dvt_passo(&e, &s, 100) == DVT_SAIDA_FONTE && !e.ativa); }
}

// Sem mudanca por 60 s: a dica. Um passo novo a apaga e o relogio recomeca.
static void dicaCalma(void) {
  DvtelaEstado e;
  DvtelaSinais s;
  memset(&e, 0, sizeof e); memset(&s, 0, sizeof s);
  dvt_entrar(&e, 1000);
  assert(dvt_passo(&e, &s, 1000 + DVT_DICA_MS - 1) == 0 && !e.dica);
  assert(dvt_passo(&e, &s, 1000 + DVT_DICA_MS) == 0 && e.dica && e.ativa);
  s.sondado = 1; s.perfil = 8;
  assert(dvt_passo(&e, &s, 1000 + DVT_DICA_MS + 10) == 0 && !e.dica);
  assert(dvt_passo(&e, &s, 1000 + 2 * DVT_DICA_MS) == 0 && !e.dica);
  assert(dvt_passo(&e, &s, 1000 + 2 * DVT_DICA_MS + 10) == 0 && e.dica);
}

// A instancia do app: Voltar cancela, OK no botao escolhe HDR10, o resto e da tela.
static void teclas(void) {
  DvtelaSinais s;
  SDL_Event ev;
  memset(&s, 0, sizeof s);
  dvtela_entrar(100);
  assert(dvtela_ativa() && dvtela_visivel());
  ev = tecla(SDLK_LEFT);   assert(dvtela_evento(&ev, 150) == DVT_EV_ENGOLIU && dvtela_ativa());
  ev = tecla(SDLK_UP);     assert(dvtela_evento(&ev, 160) == DVT_EV_ENGOLIU && dvtela_ativa());
  assert(dvtela_foco_botao());
  ev = tecla(SDLK_ESCAPE); assert(dvtela_evento(&ev, 200) == DVT_EV_VOLTAR);
  assert(!dvtela_ativa() && dvtela_estado()->saida == DVT_SAIDA_VOLTAR);

  dvtela_entrar(300);
  assert(dvtela_ativa());
  // O OK que sobra do Play (segurado, ou o segundo toque) nao escolhe nada:
  // baixar a qualidade e decisao da pessoa, nunca de um toque perdido.
  ev = tecla(SDLK_RETURN); assert(dvtela_evento(&ev, 350) == DVT_EV_ENGOLIU && dvtela_ativa());
  ev = tecla(SDLK_RETURN); ev.key.repeat = 1;
  assert(dvtela_evento(&ev, 2000) == DVT_EV_ENGOLIU && dvtela_ativa());
  ev = tecla(SDLK_RETURN); assert(dvtela_evento(&ev, 2000) == DVT_EV_HDR10);
  assert(!dvtela_ativa() && dvtela_estado()->saida == DVT_SAIDA_HDR10);
  // Com a tela fora a tecla nao e dela.
  ev = tecla(SDLK_RETURN); assert(dvtela_evento(&ev, 2100) == DVT_EV_NADA);
  (void)s;
}

// A mola de saida esvai sobre o filme; Animacoes reduzidas corta.
static void esvai(void) {
  DvtelaSinais s;
  int i;
  memset(&s, 0, sizeof s);
  dvtela_entrar(1000);
  for (i = 0; i < 30; i++) dvtela_atualizar(&s, 1.0f / 60, 1000 + (Uint32)i * 16);
  assert(dvtela_alfa() > 0.99f);
  s.sondado = 1; s.perfil = 8; s.caminho = 1; s.fonteAberta = 1; s.carregado = 1;
  s.dvConfirmado = 1; s.tocando = 1;
  assert(dvtela_atualizar(&s, 1.0f / 60, 2000) == DVT_SAIDA_DV);
  assert(!dvtela_ativa() && dvtela_visivel());
  for (i = 0; i < 120; i++) dvtela_atualizar(&s, 1.0f / 60, 2000 + (Uint32)i * 16);
  assert(!dvtela_visivel() && dvtela_alfa() < 0.01f);
}

int main(void) {
  ficaAteConfirmar();
  tocandoAntesDoCaminhoNaoSai();
  audioTrocadoViraPasso();
  saiNaRecusa();
  dicaCalma();
  teclas();
  esvai();
  puts("dvtela: ok");
  return 0;
}
