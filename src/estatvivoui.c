// Ver estatvivoui.h. Geometria do mockup stats-velocidade (telas 2 e 3), em
// pixels de layout 1920x1080.
#include "estatvivoui.h"
#include "ajustes.h"
#include "audioinfo.h"
#include "badges.h"
#include "linguas.h"
#include "catalogo.h"
#include "dados.h"
#include "escala.h"
#include "gfx.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "perfiltv.h"
#include "perfis.h"
#include "player.h"
#include "plrui.h"
#include "plrilha.h"
#include "ponteiro.h"
#include "streams.h"
#include "text.h"
#include "video.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "anim.h"

// Match the clock's top and screen inset. Fixed sizes keep live samples from
// moving the panel; reserve an internal footer at every size.
#define EVU_MARG       NV_ILHA_MARGEM_D
#define EVU_TOPO       NV_ILHA_Y
#define EVU_RAIO       36.0f     // RAIO_CORPO de plrilha.c
#define EVU_PAD        32.0f
#define EVU_TILE_PAD   32.0f
#define EVU_KEY_H      32.0f
#define EVU_W          720.0f    // IL_W de faixas.c
#define EVU_H          1008.0f
#define EVU_TILE_W     ((EVU_W - 2.0f * EVU_PAD - EVU_GAP) * 0.5f)
#define EVU_GAP        12.0f
#define EVU_CW         448.0f    // compacto
#define EVU_CH         410.0f
#define EVU_MW         480.0f    // minimo: a linha fina
#define EVU_PH         56.0f     // PIL_H de plrilha.c
#define EVU_TX         243, 242, 239, 255
// Mola do retangulo (frequencia da critica), do alfa de entrada e de cada conteudo.
#define EVU_W_RET      15.0f
#define EVU_K_ENTRA    11.0f
#define EVU_K_SAI      14.0f
#define EVU_K_CONT     18.0f
#define EVU_K_NUM      9.0f

typedef struct {
  char titulo[160], sub[160], addon[64], resolucao[32], codecHdr[64], taxa[24], audio[96], legenda[64];
  int plataforma;
} EvTela;

// OK logo depois de abrir e eco da tecla que abriu (o OK da central, o clique do
// Magic Remote que vira tecla, a repeticao de quem segura): nao anda de tamanho.
#define EVU_GRACA_OK 600u
#define EVU_ARQ_TAMANHO "estatvivo-tamanho.txt"

static int aberto, fixado, temSessao, tamanho;
static int editando, focoEd, perfilTiles = -1, nTiles[EV_TAM_N], tiles[EV_TAM_N][6];
static const char *const tileNome[] = {
  "Velocidade", "Buffer à frente", "Fonte", "Conexões e servidor", "Erros e reconexões", "Quadros perdidos"
};
static const char *const tileIcone[] = {
  "aj_gauge", "aj_database", "aj_film", "aj_server", "aj_rotate-cw", "aj_monitor-play"
};
static void nomeTiles(char *nome, size_t n, int t) {
  snprintf(nome, n, "estatvivo-tiles-p%d-%d.txt", perfis_ativo(), t);
}
static void carregarTiles(void) {
  if (perfilTiles == perfis_ativo()) return;
  perfilTiles = perfis_ativo();
  editando = 0;
  for (int t = 0; t < EV_TAM_N; t++) {
    char nome[64], *texto;
    nomeTiles(nome, sizeof nome, t);
    texto = dados_ler(nome);
    nTiles[t] = 0;
    if (!texto) {
      nTiles[t] = t == EV_TAM_MINIMO ? 2 : 6;
      for (int i = 0; i < nTiles[t]; i++) tiles[t][i] = i;
    } else {
      for (char *p = texto; *p; p++) {
        if (*p < '0' || *p > '5') continue;
        int id = *p - '0', tem = 0;
        for (int i = 0; i < nTiles[t]; i++) tem |= tiles[t][i] == id;
        if (!tem) tiles[t][nTiles[t]++] = id;
      }
      free(texto);
    }
  }
}
static int ofertaTile(int row) {
  if (row < nTiles[tamanho]) return tiles[tamanho][row];
  int r = nTiles[tamanho];
  for (int id = 0; id < 6; id++) {
    int tem = 0;
    for (int i = 0; i < nTiles[tamanho]; i++) tem |= tiles[tamanho][i] == id;
    if (!tem && r++ == row) return id;
  }
  return 0;
}
static int gravarTiles(void) {
  char nome[64], texto[32];
  int u = 0;
  nomeTiles(nome, sizeof nome, tamanho);
  // Sem ladrilhos o arquivo vai embora: a proxima carga acha o padrao do
  // tamanho. Gravar "\n" era lido como lista vazia e travava o editor — OK
  // numa linha oferecida salvava e revertia na hora, sem saida. E um arquivo
  // que ja nao existe e SUCESSO: o estado pedido (nada gravado) ja vale.
  if (!nTiles[tamanho]) { dados_apagar(nome); return 1; }
  for (int i = 0; i < nTiles[tamanho]; i++) u += snprintf(texto + u, sizeof texto - u, "%d ", tiles[tamanho][i]);
  if (u) u--;
  texto[u++] = '\n'; texto[u] = 0;
  return dados_gravar(nome, texto);
}
static void editarTile(int row, int mover) {
  int salvo[6], n = nTiles[tamanho], id = ofertaTile(row);
  if (row >= 6 || (row > n && n >= 6)) return;
  memcpy(salvo, tiles[tamanho], sizeof salvo);
  if (mover) {
    int destino = row + mover;
    if (row >= n || destino < 0 || destino >= n) return;
    tiles[tamanho][row] = tiles[tamanho][destino];
    tiles[tamanho][destino] = id;
    focoEd = destino;
  } else if (row < n) {
    memmove(&tiles[tamanho][row], &tiles[tamanho][row + 1], (n - row - 1) * sizeof(int));
    nTiles[tamanho]--;
  } else tiles[tamanho][nTiles[tamanho]++] = id;
  if (!gravarTiles()) {
    memcpy(tiles[tamanho], salvo, sizeof salvo); nTiles[tamanho] = n; focoEd = row;
  }
}
static void ptEditar(int x, int y) {
  (void)x; (void)y;
  if (tamanho == EV_TAM_COMPACTO) return;
  carregarTiles(); editando = 1; focoEd = 0;
}
static Uint32 abriuEm, proxAmostra, amostraEm, quadroEm;
static unsigned long hashUrl;
static EvEstado est;    // a amostra de verdade (1x/s)
static EvEstado vis;    // o que se DESENHA: os numeros andam ate ela na mola
static EvTela tela;
// Animacao: retangulo do painel (x y w h) com a velocidade de cada, alfa geral,
// alfa de cada tamanho e os numeros suavizados (-1 = ainda sem valor).
static float ret[4], retV[4], alvoRet[4], entraA, contA[EV_TAM_N];
static float sm[5], dtq = 1.0f / 60.0f;
static float larg[EV_TAM_N * 8], larguraI, escP = 1;
static int larguraBase;
#ifdef ESTATVIVO_TESTE
static int teste;
static float testePainel[4], testeDicas[4];
static int testeTiles[6], testeNTiles;
static EvGeometriaTeste testeGeo;
void estatvivo_teste_geometria(EvGeometriaTeste *g) { *g = testeGeo; }
// Fit test (tests/estatvivo_cabe.sh): record each text box with its owner card.
static GfxRect donoTeste;
static void anotaTexto(float x, float y, float w, float h) {
  if (donoTeste.w <= 0 || w <= 0 || testeGeo.nTextos >= 96) return;
  testeGeo.donos[testeGeo.nTextos] = donoTeste;
  testeGeo.textos[testeGeo.nTextos++] = (GfxRect){ x, y, w, h };
}
void estatvivo_teste_texto(GfxRect r) { anotaTexto(r.x, r.y, r.w, r.h); }
static TxtLinha cortaTeste(TxtEstilo e, const char *s, int r, int g, int b, int al, float w) {
  if (txt_largura(e, s) > w) testeGeo.cortes++;
  return txt_linha_corta(e, s, r, g, b, al, w);
}
#define txt_linha_corta cortaTeste
int estatvivo_teste_tiles(int *ids) { memcpy(ids, testeTiles, testeNTiles * sizeof(int)); return testeNTiles; }
void estatvivo_teste_layout(float *painel, float *dicas) {
  memcpy(painel, testePainel, sizeof testePainel);
  memcpy(dicas, testeDicas, sizeof testeDicas);
}
#endif

static char sepDec(void) { return idioma_ponto_decimal(ajustes_idioma()) ? '.' : ','; }

// O ultimo tamanho escolhido com o cartao aberto (vale para "Lembrar o ultimo").
static int ultimoTamanho(void) {
  char *t = dados_ler(EVU_ARQ_TAMANHO);
  int v = t && t[0] >= '0' && t[0] < '0' + EV_TAM_N ? t[0] - '0' : EV_TAM_COMPLETO;
  free(t);
  return v;
}
static void lembrarTamanho(void) {
  char b[4] = { (char)('0' + tamanho), '\n', 0, 0 };
  dados_gravar(EVU_ARQ_TAMANHO, b);
}

// Focus is modal at every size; changing size preserves an existing pin. The rectangle
// persegue o alvo novo na mola, sem saltar.
static void aplicarTamanho(int t) {
  tamanho = t;
  aberto = 1;
  lembrarTamanho();
}

// Abrir pela central: o tamanho dos Ajustes (ou o ultimo usado). Ja aberto nao faz
// nada (o eco do OK nao refaz o fade nem o prazo de graca). Do canto, reabre
// no tamanho escolhido antes de fixar.
void estatvivo_abrir(void) {
  int t = ajustes_estat_tamanho();
  if (aberto) { fixado = 0; return; }   // focused pinned panel: Central/F12 unpins, hints return
  carregarTiles();
  abriuEm = SDL_GetTicks();
  if (!fixado) tamanho = t >= 0 && t < EV_TAM_N ? t : ultimoTamanho();
  aberto = 1;
  fixado = 0;
  printf("[estatisticas] aberto (tamanho %d)\n", tamanho);
  fflush(stdout);
}
void estatvivo_fechar(void) { aberto = 0; fixado = 0; editando = 0; }
int  estatvivo_aberto(void) { return aberto; }
int  estatvivo_fixado(void) { return fixado; }
int  estatvivo_tamanho(void) { return tamanho; }
int  estatvivo_kbps(int *estimada) {
  if (estimada) *estimada = est.velOrigem != EVO_MEDIDO;
  return temSessao ? est.kbps : -1;
}

// Completo -> Compacto -> Minimo. Dentro da graca do OK o eco da tecla que abriu
// nao conta: era isso que encolhia o cartao na hora ("o live nao abre").
static int graca(void) { return (Sint32)(SDL_GetTicks() - abriuEm) < (Sint32)EVU_GRACA_OK; }
static void proximoTamanho(void) {
  if (graca()) return;
  aplicarTamanho((tamanho + 1) % EV_TAM_N);
}
static void ptTamanho(int a, int b) { (void)a; (void)b; proximoTamanho(); }

int estatvivo_evento(const SDL_Event *e, int cimaDela) {
  SDL_Keycode k;
  if (!e || e->type != SDL_KEYDOWN) return aberto && e && e->type == SDL_KEYUP;
  k = e->key.keysym.sym;
  if (!aberto) {
    // CIMA foca o painel fixado — MENOS quando outra camada/foco e dona da
    // tecla neste quadro (cimaDela: folha, avisos, guia, barra, "Pular
    // abertura"). Sem isso o primeiro CIMA era engolido e a lista nao andava.
    if (!fixado || cimaDela || k != SDLK_UP || e->key.repeat) return 0;
#ifndef ESTATVIVO_TESTE
    if (!player_aberto()) return 0;
#endif
    aberto = 1;   // Focus the pinned panel without changing its pin or size.
    return 1;
  }
  carregarTiles();
  if (editando) {
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE) editando = 0;
    else if (!e->key.repeat) {
      if (k == SDLK_UP && focoEd > 0) focoEd--;
      else if (k == SDLK_DOWN && focoEd < 5) focoEd++;
      else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) editarTile(focoEd, 0);
      else if (k == SDLK_LEFT) editarTile(focoEd, -1);
      else if (k == SDLK_RIGHT) editarTile(focoEd, 1);
    }
    return 1;
  }
  if (k == SDLK_MENU && !e->key.repeat) { ptEditar(0, 0); return 1; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE) {
    if (fixado) aberto = 0;
    else estatvivo_fechar();
    return 1;
  }
  if (e->key.repeat) return 1;   // segurar OK/seta nao corre pelos tres tamanhos
  // OK anda de tamanho; Esquerda/Direita mandam o painel para o lado (e o
  // Ajustes guarda); Baixo fixa onde esta, no tamanho escolhido.
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) proximoTamanho();
  else if (k == SDLK_LEFT && !graca()) ajustes_definir_estat_lado(1);
  else if (k == SDLK_RIGHT && !graca()) ajustes_definir_estat_lado(0);
  else if (k == SDLK_DOWN && !graca()) { aberto = 0; fixado = 1; }
  else if (k == SDLK_UP) { if (fixado) fixado = 0; else ptEditar(0, 0); }   // UP edits: not every remote has Menu
  return 1;   // modal: as setas nao vazam para o player por baixo
}

// Numero na mola: o valor novo chega 1x/s e o que se desenha anda ate ele, entao
// nada salta nem pisca. O primeiro valor entra direto; sem valor (< 0) apaga.
static float suave(float *s, float alvo) {
  if (alvo < 0.0f) return *s = -1.0f;
  if (*s < 0.0f) return *s = alvo;
  return *s = anim_mola(*s, alvo, dtq, EVU_K_NUM);
}
// Largura de cada numero grande (para a unidade nao tremer): um slot por chamada
// de numero(), reiniciado a cada conteudo desenhado.
static float suaveLargura(float w) {
  float *s = &larg[larguraBase + ((int)larguraI++ & 7)];
  if (*s <= 0.0f) return *s = w;
  return *s = anim_mola(*s, w, dtq, EVU_K_NUM);
}


static void zerarAnim(void) {
  int i;
  for (i = 0; i < 5; i++) sm[i] = -1.0f;
  for (i = 0; i < EV_TAM_N * 8; i++) larg[i] = 0;
}

// --- A AMOSTRA -----------------------------------------------------------------
static unsigned long fnv(const char *s) {
  unsigned long h = 2166136261UL;
  for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
  return h;
}

static const char *codecDe(uint64_t b) {
  if (b & badges_bit("co-x265")) return "HEVC";
  if (b & badges_bit("co-av1")) return "AV1";
  if (b & badges_bit("co-x264")) return "H.264";
  return "";
}

// As linhas do cartao: o que o add-on declara da fonte e o que o pipeline diz.
static void montarTela(int taxaKbps) {
  const CatItem *c = cat_item(player_indice());
  const Stream *s = stream_item(stream_atual());
  const char *hdr = video_hdr(), *ep = player_linha_episodio();
  int w = video_largura(), h = video_altura(), samsung;
  size_t u;
  memset(&tela, 0, sizeof tela);
  tela.plataforma = (int)ptv_plataforma();
  samsung = tela.plataforma == PTV_TIZEN || tela.plataforma == PTV_TPK;
  if (c) snprintf(tela.titulo, sizeof tela.titulo, "%s", c->titulo);
  u = 0;
  if (ep && ep[0]) u += (size_t)snprintf(tela.sub + u, sizeof tela.sub - u, "%s", ep);
  else if (c && c->meta[0]) u += (size_t)snprintf(tela.sub + u, sizeof tela.sub - u, "%s", c->meta);
  if (h > 0 && u < sizeof tela.sub)
    u += (size_t)snprintf(tela.sub + u, sizeof tela.sub - u, "%s%s", u ? " · " : "",
                          h >= 2000 || w >= 3200 ? "4K" : h >= 1000 ? "1080p" : h >= 700 ? "720p" : "SD");
  if (s) snprintf(tela.addon, sizeof tela.addon, "%s", s->provedor);
  if (w > 0 && h > 0) snprintf(tela.resolucao, sizeof tela.resolucao, "%d × %d", w, h);
  else if (s && s->altura > 0) snprintf(tela.resolucao, sizeof tela.resolucao, "%dp", s->altura);
  { const char *cod = s ? codecDe(s->badges) : "";
    char hd[24] = "";
    int doAddon = 0;
    if (!samsung && hdr && hdr[0] && strcmp(hdr, "none") && strcmp(hdr, "SDR"))
      snprintf(hd, sizeof hd, "%s", !strcmp(hdr, "DolbyVision") ? "Dolby Vision" : hdr);
    else if (s) {
      int m = badges_fonte_hdr_marca(s->badges);
      if (s->dolbyVision && !samsung) snprintf(hd, sizeof hd, "Dolby Vision");
      else if (m >= 0) snprintf(hd, sizeof hd, "%s", marca_formato_nome((FormatoMarca)m));
      doAddon = hd[0] != 0;
    }
    if (!hd[0]) snprintf(hd, sizeof hd, "SDR");
    snprintf(tela.codecHdr, sizeof tela.codecHdr, "%s%s%s%s", cod, cod[0] ? " · " : "", hd,
             doAddon && samsung ? " (add-on)" : "");
    if (h > 0 && u < sizeof tela.sub && strcmp(hd, "SDR"))
      snprintf(tela.sub + u, sizeof tela.sub - u, " %s", hd); }
  // #293: o audio e a legenda EM USO, do que o pipeline listou. Codec/canais so
  // quando a plataforma os da (audioinfo vazio = fica so o idioma); sem faixa, vazio.
  { const VideoFaixa *fa = video_audio(video_audio_atual());
    const VideoFaixa *fl = video_legenda(video_legenda_atual());
    if (fa) {
      char cc[40] = "";
      audioinfo_texto(fa->codec, fa->canais, strstr(fa->rotulo, "Atmos") != NULL, cc, sizeof cc);
      { const char *lg = fa->idioma[0] ? i18n(ling_nome(fa->idioma)) : "";
        snprintf(tela.audio, sizeof tela.audio, "%s%s%s", cc, cc[0] && lg[0] ? " · " : "", lg); }
    }
    if (fl) snprintf(tela.legenda, sizeof tela.legenda, "%s", fl->rotulo);
  }
  if (taxaKbps > 0) {
    char m[16];
    ev_fmt_mbps(m, sizeof m, taxaKbps, sepDec());
    snprintf(tela.taxa, sizeof tela.taxa, "%s Mbps", m);
  }
}

void estatvivo_atualizar(Uint32 agora) {
  EvEntrada in;
  const Stream *s;
  time_t t;
  struct tm *lt;
  unsigned long hu;
#ifdef ESTATVIVO_TESTE
  if (teste) return;
#endif
  if (!player_aberto() || !player_com_video()) {
    if (temSessao) { ev_zerar(&est); temSessao = 0; }
    aberto = fixado = 0;
    entraA = 0;
    zerarAnim();
    return;
  }
  hu = fnv(video_url_atual());
  // Outra URL = outro video (ou outra fonte): tudo recomeca. A reconexao
  // reabre a MESMA URL e segue contando.
  if (!temSessao || hu != hashUrl) { ev_zerar(&est); hashUrl = hu; temSessao = 1; proxAmostra = 0; }
  if (proxAmostra && (Sint32)(agora - proxAmostra) < 0) return;
  proxAmostra = agora + 1000u;
  amostraEm = agora;
  memset(&in, 0, sizeof in);
  video_estatisticas(&in.vs);
  in.pos = video_pos();
  in.bufferando = video_bufferando_ms() > 0;
  in.tocando = video_tocando() && !in.bufferando;
  in.reconectando = video_reconectando() != 0;
  in.falhou = video_falhou();
  s = stream_item(stream_atual());
  in.taxaKbps = s ? ev_taxa_arquivo(s->tamanhoBytes, s->tamanhoMB, video_duracao()) : 0;
  t = time(NULL);
  lt = localtime(&t);
  ev_amostra(&est, agora, &in, lt ? lt->tm_hour : 0, lt ? lt->tm_min : 0);
  if (aberto || fixado) montarTela(in.taxaKbps);
}


// --- PECAS DO DESENHO ------------------------------------------------------------
static float caps(const char *s, float x, float y, float w, float a) {
  char up[160];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  if (txt_tracking(TXT_V2_KICK, up, 154, 154, 156, -1, y, 0, 3.4f) > w) {
    TxtEstilo e = txt_largura(TXT_G16B, up) <= w ? TXT_G16B : TXT_G14B;
    TxtLinha t = txt_linha(e, up, 154, 154, 156, 255);
    txt_desenhar_alpha(t, x, y, a);
    return (float)t.w;
  }
  return txt_tracking(TXT_V2_KICK, up, 154, 154, 156, x, y, a, 3.4f);
}

// Text that must fit its card: the biggest style of `e` (biggest first) that
// holds `s` in one line of `w`, then in up to `n` lines; else the last style
// wrapped with an ellipsis, a cut only long add-on names and hosts may take
// (`corta`). `dir` aligns right at x + w. Returns the height used; a = 0 measures.
typedef struct { TxtEstilo e; float lead; } EvuFonte;
#define EVU_FONTES(...) (const EvuFonte[]){ __VA_ARGS__ }, (int)(sizeof (EvuFonte[]){ __VA_ARGS__ } / sizeof(EvuFonte))
static int palavrasCabem(TxtEstilo e, const char *s, float w) {
  char p[160];
  for (size_t t; *s; s += t) {
    while (*s == ' ' || *s == '\n') s++;
    if (!*s) break;
    t = txt_token_tam(s);
    snprintf(p, sizeof p, "%.*s", (int)(t < sizeof p - 1 ? t : sizeof p - 1), s);
    if (txt_largura(e, p) > w) return 0;
  }
  return 1;
}
static float texto(const EvuFonte *e, int ne, const char *s, int cr, int cg, int cb,
                   float x, float y, float w, int n, int dir, int corta, float a) {
  float h = 0;
  EvuFonte fontes[8];
  for (int i = 0; i < ne; i++) {
    fontes[i] = e[i];
    fontes[i].lead = fmaxf(e[i].lead, (float)txt_linha(e[i].e, "Ag", 255, 255, 255, 255).h);
  }
  e = fontes;
  for (int l = 1; l <= n && !h; l++)
    for (int i = 0; i < ne && !h; i++) {
      if (l == 1 && txt_largura(e[i].e, s) <= w) {
        TxtLinha t = txt_linha(e[i].e, s, cr, cg, cb, 255);
        if (a > 0) txt_desenhar_alpha(t, dir ? x + w - (float)t.w : x, y, a);
        h = e[i].lead;
      } else if (l > 1 && palavrasCabem(e[i].e, s, w) &&
                 txt_bloco(e[i].e, s, cr, cg, cb, x, y, w, e[i].lead, 0, 0) <= l * e[i].lead + 0.5f) {
        h = dir ? txt_bloco_dir(e[i].e, s, cr, cg, cb, x + w, y, w, e[i].lead, a, l)
                : txt_bloco(e[i].e, s, cr, cg, cb, x, y, w, e[i].lead, a, l);
      }
    }
  if (!h) {
    h = txt_bloco_corta(e[ne - 1].e, s, cr, cg, cb, x, y, w, e[ne - 1].lead, a, n);
#ifdef ESTATVIVO_TESTE
    if (!corta && a > 0.5f) testeGeo.cortes++;
#endif
  }
  (void)corta;
  return h;
}

static const char *iconeOrigem(int o) {
  switch (o) {
    case EVO_ADDON: return "aj_tag";
    case EVO_LINK: return "aj_link";
    case EVO_SEM: return "aj_info";
    default: return "aj_activity";
  }
}

// Card header: wrap labels, keep icons small enough to leave text room.
static void ladrilho(GfxRect r, const char *icone, const char *rotulo, int origem, float a) {
  float x = r.x + EVU_TILE_PAD, y = r.y + EVU_TILE_PAD;
  if (ajustes_vidro()) gfx_vidro_painel(r, 18.0f / r.h, 0.72f, a);
  else gfx_cor(r, 18.0f / r.h, 0.118f, 0.118f, 0.125f, a);
  gfx_icone((GfxRect){ x, y + 2, 24, 24 }, icone, 0.953f, 0.949f, 0.937f, a);
  gfx_icone((GfxRect){ r.x + r.w - EVU_TILE_PAD - 18, y + 4, 18, 18 },
            iconeOrigem(origem), 0.85f, 0.85f, 0.85f, a);
  texto(EVU_FONTES({ TXT_G20M, 24 }), rotulo, 180, 180, 182, x + 36, y,
        r.w - 2 * EVU_TILE_PAD - 62, 2, 0, 0, a);
}

// Numero grande e unidade na mesma linha de base. Devolve a largura. A unidade
// anda na largura SUAVIZADA do numero (um slot por chamada, ver desenharConteudo):
// 9,9 -> 10,1 empurrava o "Mbps" de um quadro para o outro.
// Reserved slot: the same digit count drawn as 8s, so the unit holds its X while
// the value changes without a wide gap after short numbers.
static float casa(TxtEstilo e, const char *n) {
  char r[24];
  size_t i;
  for (i = 0; n[i] && i < sizeof r - 1; i++) r[i] = n[i] >= '0' && n[i] <= '9' ? '8' : n[i];
  r[i] = 0;
  return (float)txt_linha(e, r, EVU_TX).w;
}
static float numero(const char *n, const char *unidade, float x, float y, float a, int apagado, float maxW) {
  TxtEstilo ea = TXT_ILHA_TITULO;
  if (apagado && txt_largura(ea, n) > maxW) ea = txt_largura(TXT_ILHA_PERGUNTA, n) <= maxW ? TXT_ILHA_PERGUNTA : TXT_V2_LN_B;
  TxtLinha t = apagado ? txt_linha(ea, n, 111, 111, 114, 255) : txt_linha(TXT_AJ_NUM58, n, EVU_TX);
  float w = fmaxf(suaveLargura((float)t.w), apagado ? 0.0f : casa(TXT_AJ_NUM58, n));
  txt_desenhar_alpha(t, x, y, a);
  if (unidade && unidade[0]) {
    TxtLinha u = txt_linha(TXT_V2_LN, unidade, 180, 180, 182, 255);
    txt_desenhar_alpha(u, x + w + 8, y + (float)t.h * 0.80f - (float)u.h, a);
    w += 8 + (float)u.w;
  }
  return w;
}

// Linha de apoio com trechos fortes: `p` alterna normal, forte, normal...
static void apoio(const char *const *p, int n, float x, float y, float maxW, float a) {
  int i;
  for (i = 0; i < n && maxW > 20; i++) {
    TxtLinha t = (i & 1) ? txt_linha_corta(TXT_G20M, p[i], 255, 255, 255, 255, maxW)
                         : txt_linha_corta(TXT_ILHA_SUB, p[i], 180, 180, 182, 255, maxW);
    if (!p[i][0]) continue;
    txt_desenhar_alpha(t, x, y + ((i & 1) ? 1.0f : 0.0f), a);
    // O espaco da borda do trecho nao entra na largura medida: o vao e daqui.
    x += (float)t.w + 7.0f;
    maxW -= (float)t.w + 7.0f;
  }
}

static void segmento(float x0, float y0, float x1, float y1, float esp, float r, float g, float b, float a) {
  float dx = x1 - x0, dy = y1 - y0, halo = 2.0f;
  GfxRect q = { fminf(x0, x1) - halo, fminf(y0, y1) - halo, fabsf(dx) + 2 * halo, fabsf(dy) + 2 * halo };
  if (dx * dx + dy * dy < 1.0f) return;
  gfx_rect(q, 0, GFX_LINHA, esp / q.h, (dx * dy < 0.0f) ? 1.0f : 0.0f, 0, halo / q.h, r, g, b, a);
}

// A linha de velocidade com a area embaixo: um retangulo por amostra. O passo e
// FIXO (r.w / EV_HIST) e o grafico desliza um passo entre uma amostra e a outra
// (`fase` 0..1 desde a ultima): sem o salto de 1 s e sem esticar enquanto enche.
// O recorte e feito na conta (nao com gfx_recorte): o painel inteiro ja esta
// recortado e o recorte de tesoura nao empilha.
static void faisca(const int *v, int n, GfxRect r, float fase, float a) {
  int i, mx = 1;
  float passo = r.w / (float)(EV_HIST - 1), px = 0, py = 0, desl = passo * (1.0f - anim_suave(fase));
  float xe = r.x, xd = r.x + r.w;
  if (n < 2) return;
  for (i = 0; i < n; i++) if (v[i] > mx) mx = v[i];
  for (i = 0; i < n; i++) {
    float x = r.x + r.w - (float)(n - 1 - i) * passo + desl;
    float y = r.y + r.h - r.h * 0.85f * (float)v[i] / (float)mx;
    if (i && x > xe && px < xd) {
      float x0 = px, y0 = py, x1 = x, y1 = y;
      if (x0 < xe) { y0 += (y1 - y0) * (xe - x0) / (x1 - x0); x0 = xe; }
      if (x1 > xd) { y1 = y0 + (y1 - y0) * (xd - x0) / (x1 - x0); x1 = xd; }
      gfx_cor((GfxRect){ x0, fminf(y0, y1), x1 - x0, r.y + r.h - fminf(y0, y1) }, 0, 1, 1, 1, 0.07f * a);
      segmento(x0, y0, x1, y1, 2.0f, 0.95f, 0.95f, 0.94f, 0.9f * a);
    }
    px = x; py = y;
  }
}

static void barra(float x, float y, float w, float frac, float a) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  gfx_cor((GfxRect){ x, y, w, 10 }, 0.5f, 0.18f, 0.18f, 0.192f, a);
  if (w * frac > 2) gfx_cor((GfxRect){ x, y, w * frac, 10 }, 0.5f, 0.902f, 0.902f, 0.894f, a);
}

// Label left, value right. A value that does not fit shrinks, then wraps to two
// lines; only `corta` (add-on name) may end in an ellipsis. Returns the height
// added to the row by wrapping.
#define EVU_TEXTO  EVU_FONTES({ TXT_AJ_TEXTO, 26 }, { TXT_ILHA_SUB, 23 })
#define EVU_SUB    EVU_FONTES({ TXT_ILHA_SUB, 24 }, { TXT_ILHA_HORA_L, 21 })
#define EVU_ROTULO EVU_FONTES({ TXT_ILHA_SUB, 23 }, { TXT_ILHA_HORA_L, 20 })
#define EVU_VALOR  EVU_FONTES({ TXT_G20M, 24 }, { TXT_G18M, 21 })
static float kv(const char *k, const char *v, float x, float y, float w, int fio, int corta, float a) {
  float labelW = fmaxf(w * 0.34f, fminf((float)txt_largura(TXT_ILHA_SUB, k), w * 0.5f)), vx = x + labelW + 12;
  float base = fio < 0 ? 26 : 32;
  if (!v[0]) v = "—";
  float hk = texto(EVU_ROTULO, k, 154, 154, 156, x, y, labelW, 2, 0, 0, 0);
  float hv = texto(EVU_VALOR, v, 236, 236, 236, vx, y, x + w - vx, 2, 1, corta, 0);
  float h = fmaxf(base, fmaxf(hk, hv) + 8);
  if (fio > 0) gfx_cor((GfxRect){ x, y, w, 1 }, 0, 0.227f, 0.227f, 0.235f, a);
  texto(EVU_ROTULO, k, 154, 154, 156, x, y + (h - hk) * 0.5f, labelW, 2, 0, 0, a);
  texto(EVU_VALOR, v, 236, 236, 236, vx, y + (h - hv) * 0.5f, x + w - vx, 2, 1, corta, a);
  return h - base;
}

// --- OS SEIS LADRILHOS -------------------------------------------------------------
#ifdef ESTATVIVO_TESTE
static void tileGeo(int n, GfxRect r) {
  if (n >= 0 && n < 6) testeGeo.tiles[n] = r;
}
#endif

// Each card returns the bottom of its content (from r.y): the row grows to fit it.
static float tileVelocidade(GfxRect r, float fase, float a) {
  char n[24], p[24], m[24], t[64];
  float x = r.x + EVU_TILE_PAD, y = r.y + 96, w = r.w - 2 * EVU_TILE_PAD;
  int medido = vis.velOrigem == EVO_MEDIDO;
  ladrilho(r, "aj_gauge", medido ? i18n("Velocidade agora") : i18n("Velocidade"), vis.velOrigem, a);
  ev_fmt_mbps(n, sizeof n, vis.kbps, sepDec());
  // Estimado nao tem casa decimal: a precisao seria inventada.
  if (vis.kbps >= 0 && !medido) snprintf(n, sizeof n, "%d", (vis.kbps + 500) / 1000);
  if (vis.kbps >= 0 && !medido) {
    TxtLinha q = txt_linha(TXT_AJ_NUM58, "≈", EVU_TX);
    txt_desenhar_alpha(q, x, y, a);
    x += (float)q.w + 14;
  }
  numero(n, "Mbps", x, y, a, 0, w);
  x = r.x + EVU_TILE_PAD;
  if (medido && vis.picoKbps > 0) {
    const char *partes[4];
    ev_fmt_mbps(p, sizeof p, vis.picoKbps, sepDec());
    ev_fmt_mbps(m, sizeof m, vis.mediaKbps, sepDec());
    snprintf(t, sizeof t, "%s Mbps", p);
    partes[0] = i18n("Pico"); partes[1] = t; partes[2] = i18n("· média"); partes[3] = m;
    apoio(partes, 2, x, r.y + 160, w, a);
    apoio(partes + 2, 2, x, r.y + 188, w, a);
  } else {
    const char *s = vis.velOrigem == EVO_SEM ? "Esta TV não informa a velocidade"
                  : vis.kbps < 0 && !vis.nHist ? "Medindo…"
                  : vis.bufOrigem == EVO_MEDIDO ? "Pelo ritmo em que o buffer enche"
                  : "Pelo ritmo em que o vídeo toca";
    float h = texto(EVU_TEXTO, i18n(s), 180, 180, 182, x, r.y + 160, w, 3, 0, 0, a);
    if (h > 52) {   // a third line pushes the sparkline down
      faisca(vis.hist, vis.nHist, (GfxRect){ x, r.y + 166 + h, w, 18 }, fase, a);
      return 184 + h;
    }
  }
  faisca(vis.hist, vis.nHist, (GfxRect){ x, r.y + 218, w, 18 }, fase, a);
  return 236;
}

static float tileBuffer(GfxRect r, float a) {
  char n[24], t[96];
  float x = r.x + EVU_TILE_PAD, y = r.y + 96, w = r.w - 2 * EVU_TILE_PAD;
  ladrilho(r, "aj_database", i18n("Buffer à frente"), vis.bufOrigem, a);
  if (vis.bufOrigem == EVO_MEDIDO) {
    float nw;
    snprintf(n, sizeof n, "%d", (int)(vis.bufSeg + 0.5));
    nw = numero(n, "s", x, y, a, 0, w);
    if (vis.bufMb >= 0) {
      TxtLinha l;
      snprintf(t, sizeof t, "≈ %d MB", (int)(vis.bufMb + 0.5));
      l = txt_linha(TXT_V2_LN, t, 180, 180, 182, 255);
      txt_desenhar_alpha(l, x + nw + 32, y + (float)txt_linha(TXT_AJ_NUM58, "0", EVU_TX).h * 0.80f - (float)l.h, a);
    }
    barra(x, r.y + 166, w, (float)(vis.bufSeg / (vis.bufAlvo > 0 ? vis.bufAlvo : 60.0)), a);
    if (vis.bufAlvo > 0) snprintf(t, sizeof t, i18n("de %d s que o player tenta manter"), (int)(vis.bufAlvo + 0.5));
    else snprintf(t, sizeof t, "%s", i18n("segundos de vídeo já baixados"));
    return 188 + texto(EVU_SUB, t, 180, 180, 182, x, r.y + 188, w, 3, 0, 0, a);
  } else if (vis.bufOrigem == EVO_SO_PCT) {
    snprintf(n, sizeof n, "%d", vis.bufPct);
    numero(n, "%", x, y, a, 0, w);
    barra(x, r.y + 166, w, (float)vis.bufPct / 100.0f, a);
    return 182 + texto(EVU_TEXTO, i18n("A Samsung informa a porcentagem, não os segundos"), 180, 180, 182,
                       x, r.y + 182, w, 3, 0, 0, a);
  }
  numero(i18n("Indisponível"), NULL, x, y + 8, a, 1, w);
  return 160 + texto(EVU_TEXTO, i18n("Esta TV não informa o buffer"), 180, 180, 182, x, r.y + 160, w, 3, 0, 0, a);
}

static float tileFonte(GfxRect r, float a) {
  float x = r.x + EVU_TILE_PAD, y = r.y + 76, w = r.w - 2 * EVU_TILE_PAD;
  ladrilho(r, "aj_film", i18n("Fonte"), tela.addon[0] ? EVO_ADDON : EVO_SEM, a);
  float step = 32;
  int fio = 1;
  y += step + kv(i18n("Add-on"), tela.addon, x, y, w, fio < 0 ? -1 : 0, 1, a);
  y += step + kv(i18n("Resolução"), tela.resolucao, x, y, w, fio, 0, a);
  y += step + kv(i18n("Codec · HDR"), tela.codecHdr, x, y, w, fio, 0, a);
  y += step + kv(i18n("Taxa do arquivo"), tela.taxa, x, y, w, fio, 0, a);
  y += step + kv(i18n("Áudio"), tela.audio, x, y, w, fio, 0, a);
  y += kv(i18n("Legenda"), tela.legenda, x, y, w, fio, 0, a);
  return y - r.y + 32;
}

static float tileServidor(GfxRect r, float a) {
  char n[16], t[64];
  float x = r.x + EVU_TILE_PAD, y = r.y + 96, w = r.w - 2 * EVU_TILE_PAD;
  if (vis.servOrigem == EVO_MEDIDO) {
    const char *p[2];
    ladrilho(r, "aj_server", i18n("Conexões e servidor"), EVO_MEDIDO, a);
    snprintf(n, sizeof n, "%d", vis.conexoes);
    numero(n, i18n(vis.conexoes == 1 ? "conexão" : "conexões"), x, y, a, 0, w);
    float hh = texto(EVU_SUB, vis.host[0] ? vis.host : "—", 180, 180, 182, x, r.y + 164, w, 2, 0, 1, a);
    if (vis.primeiroByteMs >= 0) {
      float yy = fmaxf(r.y + (r.h < 284 ? 212 : 224), r.y + 168 + hh);
      snprintf(t, sizeof t, "%d ms", vis.primeiroByteMs);
      p[0] = i18n("Resposta do 1º byte"); p[1] = t;
      apoio(p, 2, x, yy, w, a);
      return yy - r.y + 26;
    }
    return 164 + hh;
  } else if (vis.servOrigem == EVO_LINK) {
    ladrilho(r, "aj_server", i18n("Servidor"), EVO_LINK, a);
    // A host is one long word: it shrinks, then ends in an ellipsis.
    float hh = texto(EVU_FONTES({ TXT_ILHA_PERGUNTA, 44 }, { TXT_V2_LN_B, 32 }, { TXT_G23B, 28 }),
                     vis.host, 243, 242, 239, x, y, w, 1, 0, 1, a);
    return y - r.y + hh + 14 + texto(EVU_TEXTO, i18n("A TV não diz quantas conexões o player abre"), 180, 180, 182,
                                     x, y + hh + 14, w, 3, 0, 0, a);
  }
  ladrilho(r, "aj_server", i18n("Servidor"), EVO_SEM, a);
  numero(i18n("Indisponível"), NULL, x, y + 8, a, 1, w);
  return 152;
}

static float tileErros(GfxRect r, float a) {
  char n[16], t[128];
  float x = r.x + EVU_TILE_PAD, y = r.y + 96, w = r.w - 2 * EVU_TILE_PAD;
  ladrilho(r, "aj_rotate-cw", i18n("Falhas"), EVO_MEDIDO, a);
  snprintf(n, sizeof n, "%d", vis.erros);
  numero(n, i18n(vis.erros == 1 ? "erro" : "erros"), x, y, a, 0, w);
  if (!vis.reconexoes) snprintf(t, sizeof t, "%s", i18n("nenhuma reconexão"));
  else if (vis.reconexoes == 1) snprintf(t, sizeof t, i18n("1 reconexão às %02d:%02d"), vis.reconHora, vis.reconMin);
  else snprintf(t, sizeof t, i18n("%d reconexões, a última às %02d:%02d"), vis.reconexoes, vis.reconHora, vis.reconMin);
  if (vis.reconexoes && !vis.erros) {
    size_t u = strlen(t);
    snprintf(t + u, sizeof t - u, " · %s", i18n("sem falha"));
  }
  return 160 + texto(EVU_TEXTO, t, 180, 180, 182, x, r.y + 160, w, 2, 0, 0, a);
}

static float tileQuadros(GfxRect r, float a) {
  char n[24], q[32], p[24], t[64];
  float x = r.x + EVU_TILE_PAD, y = r.y + 96, w = r.w - 2 * EVU_TILE_PAD;
  ladrilho(r, "aj_monitor-play", i18n("Quadros perdidos"), vis.quadrosOrigem, a);
  if (vis.quadrosOrigem == EVO_MEDIDO) {
    const char *partes[2];
    ev_fmt_milhar(n, sizeof n, vis.perdidos);
    ev_fmt_milhar(q, sizeof q, vis.quadros);
    snprintf(t, sizeof t, i18n("de %s"), q);
    numero(n, t, x, y, a, 0, w);
    ev_fmt_pct2(p, sizeof p, vis.perdidos, vis.quadros, sepDec());
    snprintf(t, sizeof t, "%s %%", p);
    (void)partes;
    { TxtLinha l = txt_linha(TXT_G21B, t, 255, 255, 255, 255);
      txt_desenhar_alpha(l, x, r.y + 161, a);
      return 160 + texto(EVU_SUB, i18n("desde que o filme abriu"), 180, 180, 182,
                         x + (float)l.w + 7, r.y + 160, w - (float)l.w - 7, 2, 0, 0, a); }
  }
  int samsung = tela.plataforma == PTV_TIZEN || tela.plataforma == PTV_TPK;
  numero(i18n("Indisponível"), NULL, x, y + 8, a, 1, w);
  return 154 + texto(EVU_TEXTO, i18n(samsung ? "O decodificador da Samsung não conta quadros"
                                             : "O player desta TV não conta quadros"),
                     180, 180, 182, x, r.y + 154, w, 3, 0, 0, a);
}

static const char *nomePlataforma(int p) {
  switch (p) {
    case PTV_TIZEN: return "Samsung · Tizen";
#ifdef NV_TPK40
    case PTV_TPK: return "Samsung · Tizen 4/5";
#else
    case PTV_TPK: return "Samsung · Tizen 6+";
#endif
    case PTV_ANDROID: return "Android TV";
    default: return "LG webOS";
  }
}

static void ptFora(int x, int y) {
  (void)x; (void)y;
  if (fixado) aberto = 0;
  else estatvivo_fechar();
}
static void ptAbrir(int x, int y) { (void)x; (void)y; aberto = 1; }
static void ptFixar(int x, int y) {
  (void)x; (void)y;
  if (!graca()) { aberto = 0; fixado = 1; }
}

// "≈ 41" (estimada, sem casa decimal: a precisao seria inventada) ou "42,8".
static void fmtVel(char *n, size_t cap) {
  ev_fmt_mbps(n, cap, vis.kbps, sepDec());
  if (vis.kbps >= 0 && vis.velOrigem != EVO_MEDIDO) snprintf(n, cap, "≈ %d", (vis.kbps + 500) / 1000);
}

// --- OS TRES TAMANHOS --------------------------------------------------------------
// Cada conteudo mora no SEU canto do painel (completo e compacto no de cima a
// direita, minimo no da esquerda) e o painel recorta: ao trocar de tamanho nada
// se reposiciona, o painel anda e o conteudo cobre/descobre.

// Row of cards from tile index i: the approved height, or taller when a card's
// content (measured last frame, `fundo`) needs it.
static float fundo[6];
static float linhaH(int i) {
  float h = 244;
  if (nTiles[EV_TAM_COMPLETO] == 6 && !memcmp(tiles[EV_TAM_COMPLETO], (int[]){0, 1, 2, 3, 4, 5}, 6 * sizeof(int)))
    h = (float[]){252, 284, 196}[i / 2];
  for (int j = i; j < i + 2 && j < nTiles[EV_TAM_COMPLETO]; j++) h = fmaxf(h, fundo[tiles[EV_TAM_COMPLETO][j]] + 16);
  return h;
}
// Bottom of the full panel's cards, from the panel top.
static float fundoCompleto(void) {
  float y = 144;
  for (int i = 0; i < nTiles[EV_TAM_COMPLETO]; i += 2) y += linhaH(i) + EVU_GAP;
  return nTiles[EV_TAM_COMPLETO] ? y - EVU_GAP : y;
}

// COMPLETO: o cartao do mockup com os seis ladrilhos.
static void conteudoCompleto(float ox, float oy, float fase, float a) {
  GfxRect c = { ox, oy, EVU_W, EVU_H };
  float x = c.x + EVU_PAD, cw = c.w - 2 * EVU_PAD;
  char meta[512];
  caps("Estatísticas ao vivo", x, c.y + 32, cw, a);
#ifdef ESTATVIVO_TESTE
  if (tamanho == EV_TAM_COMPLETO)
    testeGeo.header = (GfxRect){ x, c.y + 32, cw, 104 };
  donoTeste = c;
#endif
  { TxtLinha t = txt_linha_corta(TXT_ILHA_PERGUNTA, tela.titulo[0] ? tela.titulo : i18n("Tocando agora"), EVU_TX, cw);
    txt_desenhar_alpha(t, x, c.y + 56, a); }
  // One meta line replaces the old stacked badges: subtitle, cadence, platform.
  // Too long, it takes the smaller size, then drops the cadence, then the platform.
  TxtEstilo me = TXT_AJ_TEXTO;
  for (int v = 0; v < 3; v++) {
    snprintf(meta, sizeof meta, "%s%s%s%s%s", tela.sub, tela.sub[0] && v < 2 ? " · " : "",
             v ? "" : i18n("atualiza a cada 1 s"), v ? "" : " · ", v < 2 ? nomePlataforma(tela.plataforma) : "");
    if (txt_largura(me, meta) <= cw || txt_largura(me = TXT_ILHA_SUB, meta) <= cw) break;
    me = TXT_AJ_TEXTO;
  }
  { TxtLinha m = txt_linha_corta(me, meta, 196, 196, 196, 255, cw);
    txt_desenhar_alpha(m, x, c.y + 104, a);
#ifdef ESTATVIVO_TESTE
    if (tamanho == EV_TAM_COMPLETO)
      testeGeo.meta = (GfxRect){ x, c.y + 104, (float)m.w, (float)m.h };
#endif
  }
  // As linhas de ladrilhos abaixo da linha do subtitulo (y + 104 + ~30).
  float y = c.y + 144;
  for (int i = 0; i < nTiles[EV_TAM_COMPLETO]; i += 2) {
    int id;
    float h = linhaH(i);
    for (int j = i; j < i + 2 && j < nTiles[EV_TAM_COMPLETO]; j++) {
      id = tiles[EV_TAM_COMPLETO][j];
      GfxRect r = { x + (j % 2) * (EVU_TILE_W + EVU_GAP), y, EVU_TILE_W, h };
#ifdef ESTATVIVO_TESTE
      if (tamanho == EV_TAM_COMPLETO) {
        testeTiles[testeNTiles++] = id;
        tileGeo(testeGeo.nTiles++, r);
      }
      donoTeste = r;
#endif
      float bottom = 0;
      switch (id) {
        case 0: bottom = tileVelocidade(r, fase, a); break;
        case 1: bottom = tileBuffer(r, a); break;
        case 2: bottom = tileFonte(r, a); break;
        case 3: bottom = tileServidor(r, a); break;
        case 4: bottom = tileErros(r, a); break;
        case 5: bottom = tileQuadros(r, a); break;
      }
      // Keep the largest measured height: raster rounding at the fitted scale
      // must not make adjacent frames alternate between two row heights.
      fundo[id] = fmaxf(fundo[id], bottom);
    }
    y += h + EVU_GAP;
  }
#ifdef ESTATVIVO_TESTE
  donoTeste = (GfxRect){ 0 };
#endif
}

// COMPACTO: o essencial em cinco linhas (resolucao, codec, velocidade, buffer,
// quadros perdidos).
static void conteudoCompacto(float ox, float oy, float a) {
  float x = ox + EVU_PAD, w = EVU_CW - 2 * EVU_PAD, y = oy + 60;
  char n[48], p[32], q[32], t[96];
#ifdef ESTATVIVO_TESTE
  donoTeste = (GfxRect){ ox, oy, EVU_CW, EVU_CH };
#endif
  caps("Estatísticas ao vivo", x, oy + 32, w, a);
  y += 50 + kv(i18n("Resolução"), tela.resolucao, x, y, w, 0, 0, a);
  y += 50 + kv(i18n("Codec · HDR"), tela.codecHdr, x, y, w, 1, 0, a);
  fmtVel(n, sizeof n);
  if (vis.kbps >= 0) snprintf(t, sizeof t, "%s Mbps", n);
  else t[0] = 0;
  y += 50 + kv(i18n("Velocidade"), t, x, y, w, 1, 0, a);
  if (vis.bufOrigem == EVO_MEDIDO) snprintf(t, sizeof t, "%d s", (int)(vis.bufSeg + 0.5));
  else if (vis.bufOrigem == EVO_SO_PCT) snprintf(t, sizeof t, "%d %%", vis.bufPct);
  else snprintf(t, sizeof t, "%s", i18n("Indisponível"));
  y += 50 + kv(i18n("Buffer à frente"), t, x, y, w, 1, 0, a);
  if (vis.quadrosOrigem == EVO_MEDIDO) {
    ev_fmt_milhar(q, sizeof q, vis.perdidos);
    ev_fmt_pct2(p, sizeof p, vis.perdidos, vis.quadros, sepDec());
    snprintf(t, sizeof t, "%s (%s %%)", q, p);
  } else snprintf(t, sizeof t, "%s", i18n("Indisponível"));
  kv(i18n("Quadros perdidos"), t, x, y, w, 1, 0, a);
#ifdef ESTATVIVO_TESTE
  donoTeste = (GfxRect){ 0 };
#endif
}

static int miniPadrao(void) { return nTiles[EV_TAM_MINIMO] == 2 && tiles[EV_TAM_MINIMO][0] == 0 && tiles[EV_TAM_MINIMO][1] == 1; }
// MINIMO: a linha fina, velocidade + buffer + a faisca, numa largura FIXA
// (EVU_MW): o numero tem casa reservada e a faisca fica no fim da linha.
static void conteudoMinimo(float ox, float oy, float fase, float a) {
  char n[24], b[160];
#ifdef ESTATVIVO_TESTE
  if (tamanho == EV_TAM_MINIMO) {
    testeNTiles = nTiles[EV_TAM_MINIMO];
    memcpy(testeTiles, tiles[EV_TAM_MINIMO], testeNTiles * sizeof(int));
  }
#endif
  if (!miniPadrao()) {
    for (int i = 0; i < nTiles[EV_TAM_MINIMO]; i++) {
      int id = tiles[EV_TAM_MINIMO][i];
      switch (id) {
        case 0: fmtVel(n, sizeof n); snprintf(b, sizeof b, "%s Mbps", n); break;
        case 1:
          if (vis.bufOrigem == EVO_MEDIDO) snprintf(b, sizeof b, "%d s", (int)(vis.bufSeg + 0.5));
          else if (vis.bufOrigem == EVO_SO_PCT) snprintf(b, sizeof b, "%d %%", vis.bufPct);
          else snprintf(b, sizeof b, "%s", i18n("Indisponível"));
          break;
        case 2: snprintf(b, sizeof b, "%s", tela.addon[0] ? tela.addon : tela.resolucao); break;
        case 3: snprintf(b, sizeof b, "%s", vis.host); break;
        case 4: snprintf(b, sizeof b, "%d", vis.erros); break;
        default:
          if (vis.quadrosOrigem == EVO_MEDIDO) ev_fmt_milhar(b, sizeof b, vis.perdidos);
          else snprintf(b, sizeof b, "%s", i18n("Indisponível"));
          break;
      }
      kv(i18n(tileNome[id]), b, ox + EVU_PAD, oy + 32 + i * 44, EVU_MW - 2 * EVU_PAD, i > 0, 1, a);
    }
    return;
  }
  TxtLinha tn, tb;
  float w = EVU_MW, nw;
  fmtVel(n, sizeof n);
  tn = txt_linha(TXT_G23B, n, EVU_TX);
  if (vis.bufOrigem == EVO_MEDIDO) snprintf(b, sizeof b, i18n("Mbps · buffer %d s"), (int)(vis.bufSeg + 0.5));
  else if (vis.bufOrigem == EVO_SO_PCT) snprintf(b, sizeof b, i18n("Mbps · buffer %d%%"), vis.bufPct);
  else snprintf(b, sizeof b, "Mbps");
  tb = txt_linha(TXT_AJ_TEXTO, b, 196, 196, 196, 255);
  nw = fmaxf(suaveLargura((float)tn.w), casa(TXT_G23B, n));
  gfx_icone((GfxRect){ ox + EVU_PAD, oy + (EVU_PH - 22) * 0.5f, 22, 22 }, "aj_gauge", 0.953f, 0.949f, 0.937f, a);
  txt_desenhar_alpha(tn, ox + EVU_PAD + 34, oy + (EVU_PH - (float)tn.h) * 0.5f, a);
  txt_desenhar_alpha(tb, ox + EVU_PAD + 34 + nw + 8, oy + (EVU_PH - (float)tb.h) * 0.5f, a);
  faisca(vis.hist, vis.nHist, (GfxRect){ ox + w - EVU_PAD - 84, oy + 10, 84, EVU_PH - 20 }, fase, a);
}

// O que se desenha agora: a amostra de verdade com os numeros na mola.
static void atualizarVis(void) {
  float v;
  vis = est;
  v = suave(&sm[0], (float)est.kbps);      if (v >= 0) vis.kbps = (int)(v + 0.5f);
  v = suave(&sm[1], (float)est.mediaKbps); if (v >= 0) vis.mediaKbps = (int)(v + 0.5f);
  v = suave(&sm[2], (float)est.bufSeg);    if (v >= 0) vis.bufSeg = v;
  v = suave(&sm[3], (float)est.bufMb);     if (v >= 0) vis.bufMb = v;
  v = suave(&sm[4], (float)est.bufPct);    if (v >= 0) vis.bufPct = (int)(v + 0.5f);
}

// Panel geometry shares the clock's top and screen inset, in the virtual screen
// of the scale the panel is drawn at (escalaPainel). Live values never change
// it; a card only grows when its text needs a second line.
#define EVU_HINT_H(tam) (EVU_KEY_H * ((tam) == EV_TAM_COMPLETO ? 1 : 2))
#define EVU_RODAPE(tam) (EVU_GAP + 10.0f + EVU_HINT_H(tam) + EVU_PAD)
void estatvivo_retangulo(int tam, int esquerda, float *g) {
  float w = tam == EV_TAM_COMPLETO ? EVU_W : tam == EV_TAM_COMPACTO ? EVU_CW : EVU_MW;
  carregarTiles();
  if (editando) w = EVU_MW;
  g[0] = esquerda ? EVU_MARG : NV_LAYOUT_REAL_W / gfx_escala() - EVU_MARG - w;
  g[1] = EVU_TOPO;
  g[2] = w;
  // Every focused size keeps an internal footer; an unfocused pinned panel
  // hugs its content.
  g[3] = tam == EV_TAM_COMPLETO ? fmaxf(EVU_H, fundoCompleto() + EVU_RODAPE(tam)) : tam == EV_TAM_COMPACTO ? EVU_CH + EVU_KEY_H :
    miniPadrao() ? EVU_PH : fmaxf(EVU_PH, nTiles[tam] * 44.0f + 32.0f);
  if (tam == EV_TAM_MINIMO && aberto) g[3] += EVU_RODAPE(tam);
  if (fixado && !aberto && tam != EV_TAM_MINIMO) g[3] -= EVU_HINT_H(tam) + EVU_PAD + 12.0f;
  if (editando) g[3] = 480;
}
void estatvivo_retangulo_atual(float *g) { estatvivo_retangulo(tamanho, ajustes_estat_lado(), g); }

// O conteudo mora no canto do lado escolhido do painel (que anda na mola).
static float ancora(const GfxRect *p, float w) { return ajustes_estat_lado() ? p->x : p->x + p->w - w; }

static void ptSoltar(int a, int b) { (void)a; (void)b; fixado = 0; }
static void ptLado(int a, int b) { (void)a; (void)b; ajustes_definir_estat_lado(!ajustes_estat_lado()); }

#define EVU_N_DICAS 5
// One row of body-sized hints (key bright, label muted, no keycaps),
// whenever the panel has focus, pinned or not.
// Narrow panels wrap hints onto two rows before dropping items.
// Items that do not fit are dropped, least needed first (Side, Size, Edit),
// never truncated. No Menu key: UP edits or unpins.
static void dicas(GfxRect p, float a) {
  const char *K[EVU_N_DICAS] = { "Ok", "← →", "↓", "Voltar", "↑" };
  const char *L[EVU_N_DICAS] = { "Tamanho", "Lado", "Fixar", "Fechar", fixado ? "Desafixar" : "Editar" };
  static const int sai[] = { 1, 0, 4, 2 };
  int n = tamanho == EV_TAM_COMPACTO && !fixado ? 4 : 5, i, shown = 0, fica[EVU_N_DICAS];
  float x = p.x + EVU_PAD, y = p.y + p.h - EVU_PAD - EVU_HINT_H(tamanho), avail = p.w - 2 * EVU_PAD;
  float rowW = 0;
  int rows = 1;
  TxtLinha k[EVU_N_DICAS], l[EVU_N_DICAS];
  float wi[EVU_N_DICAS];
  if (y < p.y + EVU_PH) return;
  for (i = 0; i < n; i++) {
    k[i] = txt_linha(TXT_AJ_TEXTO, i18n(K[i]), 243, 242, 239, 255);
    l[i] = txt_linha(TXT_AJ_TEXTO, i18n(L[i]), 180, 180, 182, 255);
    wi[i] = (float)(k[i].w + 6 + l[i].w);
    fica[i] = 1;
  }
  for (int drop = 0; drop <= 4; drop++) {
    rows = 1; rowW = 0;
    for (i = 0; i < n; i++) {
      if (!fica[i]) continue;
      if (rowW > 0 && rowW + 22 + wi[i] > avail) { rows++; rowW = 0; }
      rowW += (rowW > 0 ? 22 : 0) + wi[i];
    }
    if (rows * EVU_KEY_H <= EVU_HINT_H(tamanho) || drop == 4) break;
    if (sai[drop] < n) fica[sai[drop]] = 0;
  }
  GfxRect separator = { x, y - 10, avail, 1 };
#ifdef ESTATVIVO_TESTE
  testeGeo.separator = separator;
  memcpy(testeDicas, (float[]){ x, y, avail, EVU_HINT_H(tamanho) }, sizeof testeDicas);
  testeGeo.dicas[0] = 0;
  testeGeo.bodyTextH = (float)txt_linha(TXT_AJ_TEXTO, "Ag", 255, 255, 255, 255).h;
  testeGeo.hintTextH = fminf((float)k[0].h, (float)l[0].h);
#endif
  gfx_cor(separator, 0, 1, 1, 1, 0.07f * a);
  for (i = 0; i < n; i++) {
    if (!fica[i] || wi[i] > avail) continue;
    if (x > p.x + EVU_PAD && x + wi[i] > p.x + p.w - EVU_PAD) {
      x = p.x + EVU_PAD;
      y += EVU_KEY_H;
    }
#ifdef ESTATVIVO_TESTE
    testeGeo.keys[shown] = (GfxRect){ x, y, wi[i], EVU_KEY_H };
    snprintf(testeGeo.dicas + strlen(testeGeo.dicas), sizeof testeGeo.dicas - strlen(testeGeo.dicas),
             "%s %s|", i18n(K[i]), i18n(L[i]));
    testeGeo.nKeys = shown + 1;
    testeGeo.hintTextH = fminf(testeGeo.hintTextH, fminf((float)k[i].h, (float)l[i].h));
#endif
    shown++;
    txt_desenhar_alpha(k[i], x, y + (EVU_KEY_H - (float)k[i].h) * 0.5f, a);
    txt_desenhar_alpha(l[i], x + (float)k[i].w + 6, y + (EVU_KEY_H - (float)l[i].h) * 0.5f, a);
    if (ponteiro_ativo() && a > 0.5f)
      ponteiro_alvo(x, y, wi[i], EVU_KEY_H, NULL,
                    i == 0 ? ptTamanho : i == 1 ? ptLado : i == 2 ? ptFixar : i == 4 ? (fixado ? ptSoltar : ptEditar) : ptFora, 0, 0);
    x += wi[i] + 22;
  }
}

// Control Center checklist: selected entries first; OK toggles, arrows reorder.
static void ptEdFoco(int row, int action) { (void)action; focoEd = row; }
static void ptEdOk(int row, int action) { focoEd = row; editarTile(row, action); }
static void ptEdPronto(int x, int y) { (void)x; (void)y; editando = 0; }
static void desenhaEdicao(GfxRect p, float a) {
  float x = p.x + EVU_PAD, w = p.w - 2 * EVU_PAD;
  ajustes_ui_kicker(i18n("Editar"), x, p.y + 24, a);
  TxtLinha mode = txt_linha(TXT_ILHA_SUB, i18n(tamanho == EV_TAM_COMPLETO ? "Completo" : "Mínimo"), EVU_TX);
  txt_desenhar_alpha(mode, x + w - mode.w, p.y + 24, a);
  for (int row = 0; row < 6; row++) {
    int id = ofertaTile(row), tem = row < nTiles[tamanho];
    GfxRect r = { x, p.y + 64 + row * 50, w, 46 };
    if (row == focoEd) {
      if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, 0.17f * a);
      else gfx_cor(r, 0.5f, 0.195f, 0.20f, 0.223f, a);
    }
    gfx_icone((GfxRect){r.x + 14, r.y + 12, 22, 22}, tileIcone[id], 0.953f, 0.949f, 0.937f, a);
    TxtLinha label = txt_linha_corta(TXT_ILHA_SUB, i18n(tileNome[id]), EVU_TX, w - 110);
    txt_desenhar_alpha(label, r.x + 48, r.y + (46 - label.h) * 0.5f, a);
    float cr = 0.953f, cg = 0.949f, cb = 0.937f;
    if (tem) ajustes_acento_marca(&cr, &cg, &cb);
    gfx_icone((GfxRect){r.x + w - 36, r.y + 12, 22, 22}, tem ? "aj_circle-check" : "aj_plus", cr, cg, cb, a);
    if (ponteiro_ativo()) ponteiro_alvo(r.x, r.y, r.w, r.h, ptEdFoco, ptEdOk, row, 0);
  }
  const char *keys[] = { "Ok", "← →", "Voltar" };
  const char *labels[] = { "Pôr ou tirar", "Mover", "Pronto" };
  for (int i = 0; i < 3; i++) {
    float yy = p.y + 374 + i * 30;
    TxtLinha key = txt_linha(TXT_V2_KBD18, i18n(keys[i]), EVU_TX);
    TxtLinha label = txt_linha(TXT_ILHA_SUB, i18n(labels[i]), EVU_TX);
    txt_desenhar_alpha(key, x, yy, a);
    txt_desenhar_alpha(label, x + 86, yy, a);
    if (ponteiro_ativo()) {
      if (i == 1) {
        ponteiro_alvo(x, yy, w / 2, 30, NULL, ptEdOk, focoEd, -1);
        ponteiro_alvo(x + w / 2, yy, w / 2, 30, NULL, ptEdOk, focoEd, 1);
      } else ponteiro_alvo(x, yy, w, 30, NULL, i == 0 ? ptEdOk : ptEdPronto, focoEd, 0);
    }
  }
}

static void desenharVirtual(Uint32 agora) {
  int i, quer = aberto || fixado, desenho = tamanho;
  int inicial = entraA == 0;
  float fase, o;
  GfxRect p;
#ifndef ESTATVIVO_TESTE
  if (!player_aberto()) return;
#endif
#ifdef ESTATVIVO_TESTE
  dtq = 1.0f / 60.0f;   // quadros de captura sem relogio: a mola anda igual sempre
#else
  dtq = quadroEm ? anim_clamp((float)(Sint32)(agora - quadroEm) / 1000.0f, 0.0f, 0.05f) : 1.0f / 60.0f;
#endif
  quadroEm = agora;
  entraA = anim_mola(entraA, quer ? 1.0f : 0.0f, dtq, quer ? EVU_K_ENTRA : EVU_K_SAI);
  if (!quer && entraA < 0.01f) { entraA = 0; zerarAnim(); return; }
  o = entraA;
  if (!tela.titulo[0] && !tela.addon[0] && temSessao) montarTela(0);
  atualizarVis();
  fase = amostraEm ? anim_clamp((float)(Sint32)(agora - amostraEm) / 1000.0f, 0.0f, 1.0f) : 1.0f;
  // The panel never leaves the screen: it draws at the Interface size while it
  // fits the real screen, smaller otherwise (the full card is ~1000 px tall).
  {
    int dw, dh;
    gfx_tamanho_tela(&dw, &dh);
    float s = gfx_escala_ui(), alvo;
    float px = gfx_px_por_unidade();
    estatvivo_retangulo_atual(alvoRet);
    alvo = fminf(s, fminf(dw / (px * (alvoRet[2] + 2 * EVU_MARG)),
                         dh / (px * (alvoRet[3] + 2 * EVU_TOPO))));
    escP = inicial ? alvo : anim_mola(escP, alvo, dtq, EVU_K_CONT);
    gfx_escala_sair(escP);
  }
  estatvivo_retangulo_atual(alvoRet);
  // Reuse the player clock's origin and the shared rectangle spring.
  if (quer && inicial) {
    GfxRect clock = { NV_LAYOUT_REAL_W / gfx_escala_ui() - EVU_MARG - NV_ILHA_REP_W, EVU_TOPO, NV_ILHA_REP_W, EVU_PH };
    float k = gfx_escala_ui() / escP;   // the clock lays out at the Interface size
    plrilha_rect(&clock);
    ret[0] = clock.x * k; ret[1] = clock.y * k; ret[2] = clock.w * k; ret[3] = clock.h * k;
    for (i = 0; i < 4; i++) retV[i] = 0;
    for (i = 0; i < EV_TAM_N; i++) contA[i] = i == desenho ? 1.0f : 0.0f;
  }
  for (i = 0; i < 4; i++) ret[i] = anim_mola2(&retV[i], ret[i], alvoRet[i], dtq, EVU_W_RET);
  for (i = 0; i < EV_TAM_N; i++) contA[i] = anim_mola(contA[i], i == desenho ? 1.0f : 0.0f, dtq, EVU_K_CONT);
  p = (GfxRect){ ret[0], ret[1], ret[2], ret[3] };
  // So o painel de pe ocupa o canto da ilha. Fechado (lado padrao e a
  // direita) a pilula do relogio e as folhas (Audio, Legendas, Fontes)
  // continuam — esconder aqui as apagava no mesmo quadro.
  if ((aberto || fixado) && !ajustes_estat_lado()) plrilha_esconder();
  if (aberto && ponteiro_ativo() && o > 0.5f) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_LAYOUT_REAL_W / escP, NV_LAYOUT_REAL_H / escP, NULL, ptFora, 0, 0);
    if (!editando) ponteiro_alvo(alvoRet[0], alvoRet[1], alvoRet[2], alvoRet[3], NULL, ptTamanho, 0, 0);
  }
  if (fixado && !aberto && ponteiro_ativo() && o > 0.5f)
    ponteiro_alvo(p.x, p.y, p.w, p.h, NULL, ptAbrir, 0, 0);
#ifdef ESTATVIVO_TESTE
  memcpy(testePainel, (float[]){ p.x, p.y, p.w, p.h }, sizeof testePainel);
  memset(testeDicas, 0, sizeof testeDicas);
  memset(&testeGeo, 0, sizeof testeGeo);
  testeGeo.escala = gfx_escala();
  donoTeste = (GfxRect){ 0 };
  testeNTiles = 0;
#endif
  plrui_material(p, fminf(EVU_RAIO, p.h * 0.5f), 0, o);
  gfx_recorte(p.x, p.y, p.w, p.h);
  if (editando) desenhaEdicao(p, o);
  for (i = 0; !editando && i < EV_TAM_N; i++) {
    float a = o * contA[i];
    if (a < 0.01f) continue;
    larguraBase = i * 8; larguraI = 0;
    if (i == EV_TAM_COMPLETO) conteudoCompleto(ancora(&p, EVU_W), p.y, fase, a);
    else if (i == EV_TAM_COMPACTO) conteudoCompacto(ancora(&p, EVU_CW), p.y, a);
    else conteudoMinimo(ancora(&p, EVU_MW), p.y, fase, a);
  }
  if (aberto && !editando) dicas(p, o);
  gfx_sem_recorte();
}

// The panel lays out in the virtual screen like the clock island (Interface
// size), shrunk when that would not fit (desenharVirtual).
void estatvivo_desenhar(Uint32 agora) {
  ESCALA_INI();
  desenharVirtual(agora);
  ESCALA_FIM();
}

#ifdef ESTATVIVO_TESTE
void estatvivo_teste(const EvEstado *e, const EvTelaTeste *t) {
  teste = 1;
  temSessao = 1;
  est = *e;
  memset(&tela, 0, sizeof tela);
  snprintf(tela.titulo, sizeof tela.titulo, "%s", t->titulo);
  snprintf(tela.sub, sizeof tela.sub, "%s", t->sub);
  snprintf(tela.addon, sizeof tela.addon, "%s", t->addon);
  snprintf(tela.resolucao, sizeof tela.resolucao, "%s", t->resolucao);
  snprintf(tela.codecHdr, sizeof tela.codecHdr, "%s", t->codecHdr);
  snprintf(tela.taxa, sizeof tela.taxa, "%s", t->taxa);
  snprintf(tela.audio, sizeof tela.audio, "%s", t->audio);
  snprintf(tela.legenda, sizeof tela.legenda, "%s", t->legenda);
  tela.plataforma = t->plataforma;
  abriuEm = 0;
  amostraEm = 0;
  quadroEm = 0;
  entraA = 0;
  zerarAnim();
}
#endif
