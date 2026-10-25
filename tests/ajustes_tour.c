/* "O que o Nuvio faz?": tabela de cartoes do tour. Sem UI, rede ou sync. */
#include "../src/gfx.h"
#include "../src/text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int subtitleText, secondaryText, seekProgress, seekFrame, squareVideo;
static void previewCor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)ca;
  if (r.w > 145 && r.w < 155 && r.h > 80 && r.h < 90 && cr > .9f && cg > .9f && cb > .9f) seekFrame++;
  if (r.x == 0 && r.y == 0 && r.w == 432 && raio == 0) squareVideo++;
  if (r.y > 130 && r.y < 152 && r.h < 5 && r.w > 100 && r.w < 300) seekProgress++;
}
static void previewLuz(GfxRect r, float raio, float cx, float cy, float alcance,
                       float cr, float cg, float cb, float ca) {
  (void)cx; (void)cy; (void)alcance;
  previewCor(r, raio, cr, cg, cb, ca);
}
static TxtLinha previewTexto(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  (void)e; (void)r; (void)g; (void)b; (void)a;
  if (!strcmp(s, "Legenda no tempo certo") || !strcmp(s, "Eu disse que ia voltar.")) subtitleText++;
  if (!strcmp(s, "I told you I'd come back.")) secondaryText++;
  return (TxtLinha){ .w = 200, .h = 20 };
}
static int previewLargura(TxtEstilo e, const char *s) { (void)e; return (int)strlen(s) * 10; }
static void previewDesenhar(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
static void previewRecorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
static void previewFim(void) {}
#define gfx_cor previewCor
#define gfx_luz_canto previewLuz
#define txt_linha previewTexto
#define txt_largura previewLargura
#define txt_desenhar_alpha previewDesenhar
#define gfx_recorte previewRecorte
#define gfx_sem_recorte previewFim
#include "../src/ajustes.c"

static int naTela(int op) {
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return 1;
  return 0;
}

int main(void) {
  int n = tourN();
  const int required[] = { AJ_LEG_SYNC_AUTO, AJ_LEG_LINGUA2, AJ_SEEKR_LIGADO, AJ_RELOGIO,
    AJ_FONTE_PRIORIDADE, AJ_TAM_MAX, AJ_DTS_AC3, AJ_PERFIL_ATIVO, AJ_PERFIL_PESQ,
    AJ_XTREAM_SERVIDOR, AJ_JF_LIGADO, AJ_ESMAECER, AJ_DV_MKV, -1 };
  for (unsigned k = 0; k < sizeof required / sizeof *required; k++) {
    int found = 0;
    for (int i = 0; i < n; i++) if (tourOp(i) == required[k]) found = 1;
    assert(found && "required functional tour card missing");
  }
  aj4Iniciar(); focoIndice = 1; aj4Linha = 7;
  aj4Evento(SDLK_RETURN);
  assert(tourAberto && "Settings v4 must open functional tour");
  tourSair();
  assert(focoIndice && aj4Linha == 7);
  for (int i = 0; i < n; i++) {
    int op = tourOp(i);
    if (op < 0) {
      assert(!strcmp(TOUR[i].tit, "Assistir Juntos"));
      int before = valor[AJ_SOCIAL];
      tourLigar(i); assert(ajustes_pediu_tela() == GT_BIBLIOTECA);
      tourOnde(i); assert(ajustes_pediu_tela() == GT_BIBLIOTECA);
      assert(valor[AJ_SOCIAL] == before);
      continue;
    }
    assert(op >= 0 && op < AJ_N);          /* cada cartao tem op valido */
    assert(naTela(op));                    /* e a opcao existe na lista de Ajustes */
    assert(tourTitulo(i)[0] && tourFrase(i)[0]);
    for (int j = 0; j < i; j++) assert(tourOp(j) != op);
  }
  /* "Ligar agora": foco na opcao + edicao aberta quando a opcao existe aqui. */
  for (int i = 0; i < n; i++) {
    if (!tourDisponivel(i) || tourOp(i) < 0) continue;
    int op = tourOp(i);
    uxAbrirOp = -1; uxAbrirEdicao = 0;
    tourLigar(i);
    assert(uxAbrirOp == tourOp(i) && uxAbrirEdicao == 1);
    uxAbrirOp = -1; uxAbrirEdicao = 0;
    tourOnde(i);
    assert(uxAbrirOp == tourOp(i) && uxAbrirEdicao == 0);
    tourAtualizar(0);
    int target = op == AJ_XTREAM_SERVIDOR ? AJ_IPTV_LISTA_XTREAM : op;
    assert(!tourAberto && !focoIndice);
    assert(aj4Atual == aj4Secao(target) && aj4Opcoes[aj4Linha] == target);
    assert(!uxEditor);
    int before = valor[op];
    tourLigar(i); tourAtualizar(0);
    if (op == AJ_PERFIL_ATIVO) { assert(aj4Util == 4); aj4Util = 0; }
    else assert(aj4Opcoes[aj4Linha] == target && uxOp == target && uxEditor);
    assert(valor[op] == before);
    uxCancelar();
  }
  /* "Novo" por 30 dias a contar da primeira vez que o tour foi visto. */
  assert(!tourNovoJanela(1000, 999));
  assert(!tourNovoJanela(0, 1000));
  assert(tourNovoJanela(1000, 1000 + 29 * 86400));
  assert(!tourNovoJanela(1000, 1000 + 31 * 86400));
  int failures = 0;
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  tourPrevia(AJ_LEG_SYNC_AUTO, (GfxRect){0, 0, 432, 176}, 0);
  tourPrevia(AJ_LEG_LINGUA2, (GfxRect){0, 0, 432, 176}, 0);
  tourPrevia(AJ_SEEKR_LIGADO, (GfxRect){0, 0, 432, 176}, 0);
  if (subtitleText != 2 || secondaryText != 1 || !seekProgress || !seekFrame || squareVideo) {
    fprintf(stderr, "FAIL: approved previews: subtitles=%d secondary=%d seek=%d frame=%d square=%d\n",
            subtitleText, secondaryText, seekProgress, seekFrame, squareVideo);
    failures++;
  }
  {
    valor[AJ_RELOGIO] = 1;
    int off = tourLigadas();
    valor[AJ_RELOGIO] = 0;
    if (tourLigadas() != off + 1) {
      fputs("FAIL: enabled/total counter does not respond to toggles\n", stderr);
      failures++;
    }
    if (failures) return 1;
    valor[AJ_ESMAECER] = 0;
    off = tourLigadas();
    valor[AJ_ESMAECER] = 3;
    assert(tourLigadas() == off + 1);
    ling_local_legenda2("none");
    off = tourLigadas();
    ling_local_legenda2("pt-BR");
    assert(tourLigadas() == off + 1);
    ling_local_legenda2("none");
    valor[AJ_DTS_AC3] = 0;
    off = tourLigadas();
    valor[AJ_DTS_AC3] = 1;
    assert(tourLigadas() == off + 1);
    valor[AJ_TAM_MAX] = 0;
    off = tourLigadas();
    valor[AJ_TAM_MAX] = 2;
    assert(tourLigadas() == off + 1);
    valor[AJ_SEEKR_LIGADO] = 0; seekrChave[0] = 0;
    off = tourLigadas();
    strcpy(seekrChave, "fixture");
    assert(tourLigadas() == off + 1);
    assert(tourLigadas() >= 0 && tourLigadas() <= tourN());
    puts("PASS: enabled/total counter responds to settings and Seekr key");
  }
  if (failures) return 1;
  puts("PASS: approved subtitle text, Seekr progress and rounded previews");
  puts("ok");
  return 0;
}
