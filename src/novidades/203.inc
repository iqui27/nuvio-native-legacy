// CONTEUDO DO CARTAO DE NOVIDADES DA 2.0.3 (motor: novidades_cartao.c).
//
// Incluido UMA vez, no meio de novidades_cartao.c: as pecas de la (vidro,
// novArte, novArteDesfocada, sai3, janela, lerp) e os tipos NovItem, NovCena e
// NovConteudo ja existem aqui. A PROXIMA VERSAO copia este arquivo para
// src/novidades/<versao>.inc, troca o texto, as cenas e a marca, e muda o
// #include do motor. Nada no app.c.
//
// REGRAS DO TEXTO (iguais as da 2.0.2): frases curtas, o tom do app, nada que
// os commits ou docs/releases/<versao>/NOTAS.md nao provem. Toda frase passa
// pela tabela (src/idioma_tab.h e as irmas). As linhas de cada frase sao
// RESERVADAS (`linhas`): a captura (tests/novidades_cartao_shot.sh) mede nos
// 30 idiomas com a fonte da TV e reprova reticencias.
//
// AS CENAS desenham dentro do retangulo da previa (760 x 848); os 230 px de
// baixo sao da legenda. `t` e o segundo da cena; com Animacoes reduzidas, o
// estado final.

// ------------------------------------------- cena 1: a tela do Dolby Vision
// A tela de VERDADE (dvtelaui.c, dvtela_previa_*), em miniatura: o mesmo
// desenho do player e a mesma maquina (dvt_passo) fed com os sinais que o
// backend manda, na ordem: lendo o arquivo, perfil achado, audio trocado,
// abrindo a fonte, preparando a imagem. Nada redesenhado aqui. Com Animacoes
// reduzidas, o estado final.
#define DV_DUR      8.0f
#define DV_ACHOU    1.3f
#define DV_AUDIO    2.4f
#define DV_ABRIR    3.4f
#define DV_IMAGEM   5.4f

static DvtelaEstado dvE;
static float dvT = -1.0f;

static void dvSinais(float t, DvtelaSinais *s) {
  memset(s, 0, sizeof *s);
  if (t >= DV_ACHOU) { s->sondado = 1; s->perfil = 8; }
  if (t >= DV_AUDIO) {
    s->audioTrocado = 1;
    snprintf(s->audioDe, sizeof s->audioDe, "A_TRUEHD");
    snprintf(s->audioPara, sizeof s->audioPara, "A_EAC3");
  }
  if (t >= DV_ABRIR) { s->caminho = 1; s->fonteAberta = 1; }
  if (t >= DV_IMAGEM) s->carregado = 1;
}

// Anda a maquina e as molas ate o segundo `t` em passos de 1/60 s (a captura
// salta no relogio: o estado tem de ser o mesmo de quem assistiu).
static void dvAte(float t) {
  DvtelaSinais s;
  if (dvT < 0.0f || t < dvT - 0.001f) {
    dvt_entrar(&dvE, 0);
    dvtela_previa_zerar();
    dvT = 0.0f;
  }
  while (dvT < t) {
    float passo = t - dvT > 1.0f / 60.0f ? 1.0f / 60.0f : t - dvT;
    dvT += passo;
    dvSinais(dvT, &s);
    dvt_passo(&dvE, &s, (Uint32)(dvT * 1000.0f));
    dvtela_previa_avancar(&dvE, passo);
  }
}

static void cenaDv(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas();
  float k = (p.w - 56.0f) / dvtela_previa_largura();
  float x = p.x + 28.0f, y = p.y + (p.h - 230.0f - dvtela_previa_altura() * k) * 0.5f;
  if (red) t = DV_DUR;
  dvAte(t);
  novArteDesfocada(24, p, PV_RAIO, a);
  gfx_cor(p, PV_RAIO / p.h, 0.016f, 0.017f, 0.021f, 0.62f * a);
  dvtela_previa_desenhar(x, y, k, &dvE, (Uint32)(t * 1000.0f), a);
}

// ------------------------------------------------- cena 2: Ajustes com previa
// Os Ajustes novos: cada opcao tem uma previa do que muda. Em cima, a Home em
// miniatura; embaixo, tres opcoes de verdade (Tela inicial e Cartazes). O foco
// desce pela lista e a previa mostra a mudanca daquela opcao: o relogio na
// tela, o selo de assistido nos cartazes, o trailer tocando no destaque.
#define AJ_DUR     5.4f
#define AJ_PASSO   1.8f

static void cenaAjustes(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas(), i;
  static const char *const ROT[3] = { "Relógio na tela", "Selo de assistido nos pôsteres", "Trailer no destaque" };
  float ar, ag, ab, fpos, peso[3];
  GfxRect win = { p.x + 40.0f, p.y + 40.0f, p.w - 80.0f, 300.0f };
  ajustes_acento(&ar, &ag, &ab);
  // A posicao do foco: anda para a linha seguinte em 0,3 s a cada passo.
  if (red) fpos = 0.0f;
  else {
    int k = (int)(t / AJ_PASSO);
    float dentro = t - (float)k * AJ_PASSO;
    if (k > 2) k = 2;
    fpos = (float)k;
    if (k > 0 && dentro < 0.3f) fpos = (float)(k - 1) + sai3(dentro / 0.3f);
  }
  for (i = 0; i < 3; i++) peso[i] = 1.0f - anim_clamp(fabsf(fpos - (float)i), 0.0f, 1.0f);

  novArte(15, p, PV_RAIO, a);
  gfx_cor(p, PV_RAIO / p.h, 0.02f, 0.02f, 0.025f, 0.70f * a);

  // A previa: a Home em miniatura.
  gfx_rect((GfxRect){ win.x - 24.0f, win.y - 8.0f, win.w + 48.0f, win.h + 44.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.12f,
           0, 0, 0, 0.55f * a);
  novArte(18, win, 22.0f, a);
  { GfxRect v = win;
    gfx_rect(v, 0, GFX_VEU_CARD, 0, 0, 0, 22.0f / v.h, 0.03f, 0.03f, 0.035f, 0.80f * a); }
  // Titulo e sinopse do destaque (tracos, como as outras cenas).
  gfx_cor((GfxRect){ win.x + 28.0f, win.y + 44.0f, 220.0f, 18.0f }, 0.5f, 1, 1, 1, 0.85f * a);
  gfx_cor((GfxRect){ win.x + 28.0f, win.y + 74.0f, 300.0f, 9.0f }, 0.5f, 1, 1, 1, 0.38f * a);
  gfx_cor((GfxRect){ win.x + 28.0f, win.y + 92.0f, 250.0f, 9.0f }, 0.5f, 1, 1, 1, 0.24f * a);
  // O trailer tocando: a barra no acento enche e o som aparece.
  if (peso[2] > 0.01f) {
    float pr = red ? 0.4f : anim_clamp((t - 2.0f * AJ_PASSO) / 3.0f, 0.0f, 1.0f) * 0.6f + 0.1f;
    GfxRect trilho = { win.x + 28.0f, win.y + 118.0f, 220.0f, 4.0f };
    gfx_cor(trilho, 0.5f, 1, 1, 1, 0.20f * peso[2] * a);
    trilho.w *= pr;
    gfx_cor(trilho, 0.5f, ar, ag, ab, 0.95f * peso[2] * a);
    gfx_cor((GfxRect){ win.x + win.w - 64.0f, win.y + 24.0f, 40.0f, 40.0f }, 0.5f, 0, 0, 0, 0.45f * peso[2] * a);
    gfx_icone((GfxRect){ win.x + win.w - 54.0f, win.y + 34.0f, 20.0f, 20.0f }, "aj_volume-2", 1, 1, 1, peso[2] * a);
  }
  // O relogio na tela: a ilha no canto de cima.
  if (peso[0] > 0.01f) {
    char hora[16];
    time_t ag0 = time(NULL);
    struct tm tmv;
    TxtLinha hl;
    GfxRect il;
    localtime_r(&ag0, &tmv);
    hora_tela(hora, sizeof hora, &tmv);
    hl = txt_linha(TXT_V2_LN_B, hora, 243, 242, 239, 255);
    il = (GfxRect){ win.x + win.w - 24.0f - ((float)hl.w + 40.0f), win.y + 22.0f - (1.0f - peso[0]) * 6.0f,
                    (float)hl.w + 40.0f, 44.0f };
    vidro(il, 22.0f, peso[0] * a);
    txt_desenhar_alpha(hl, il.x + 20.0f, il.y + (il.h - (float)hl.h) * 0.5f, peso[0] * a);
  }
  // A fileira de cartazes (o selo de assistido em dois).
  { static const int ARTE[4] = { 12, 24, 15, 18 };
    float pw = 92.0f, ph = 132.0f, gap = 16.0f, x0 = win.x + 28.0f, y0 = win.y + win.h - ph - 22.0f;
    for (i = 0; i < 4; i++) {
      GfxRect c = { x0 + (float)i * (pw + gap), y0, pw, ph };
      novArte(ARTE[i], c, 12.0f, a);
      gfx_anel(c, 12.0f / c.h, 1.0f, 1, 1, 1, 0.10f * a);
      if (i < 2 && peso[1] > 0.01f) {
        float d = 26.0f * (0.7f + 0.3f * peso[1]);
        GfxRect s = { c.x + c.w - 8.0f - d, c.y + 8.0f, d, d };
        // O visto contra o acento: escuro quando o acento e claro (tema branco).
        float ic = 0.299f * ar + 0.587f * ag + 0.114f * ab > 0.6f ? 0.08f : 1.0f;
        gfx_cor(s, 0.5f, ar, ag, ab, 0.95f * peso[1] * a);
        gfx_icone((GfxRect){ s.x + d * 0.22f, s.y + d * 0.22f, d * 0.56f, d * 0.56f }, "aj_check", ic, ic, ic,
                  peso[1] * a);
      }
    } }
  gfx_anel(win, 22.0f / win.h, 1.0f, 1, 1, 1, 0.10f * a);

  // As tres opcoes, no vidro, com o foco andando.
  { GfxRect g = { win.x, win.y + win.h + 24.0f, win.w, 3.0f * 64.0f + 24.0f };
    vidro(g, 24.0f, a);
    plrui_linha_foco((GfxRect){ g.x + 12.0f, g.y + 12.0f + fpos * 64.0f, g.w - 24.0f, 64.0f }, 20.0f, a);
    for (i = 0; i < 3; i++) {
      GfxRect r = { g.x + 12.0f, g.y + 12.0f + (float)i * 64.0f, g.w - 24.0f, 64.0f };
      int foc = peso[i] > 0.5f;
      const char *val = i18n("Ligado");
      TxtLinha v = txt_linha(TXT_V2_26, val, 243, 242, 239, 255);
      TxtLinha l = txt_linha_corta(foc ? TXT_V2_LN_B : TXT_V2_26, i18n(ROT[i]), 243, 242, 239, 255,
                                   r.w - 48.0f - (float)v.w - 28.0f);
      txt_desenhar_alpha(l, r.x + 24.0f, r.y + (r.h - (float)l.h) * 0.5f, (foc ? 1.0f : 0.78f) * a);
      txt_desenhar_alpha(v, r.x + r.w - 24.0f - (float)v.w, r.y + (r.h - (float)v.h) * 0.5f, (foc ? 0.95f : 0.55f) * a);
    } }
}

// ------------------------------------------------------------------ a tabela
static const NovCena CENAS_203[] = {
  { "Dolby Vision em MKV", "Etapa por etapa, enquanto abre.", cenaDv, DV_DUR, NOV_LG | NOV_OUTRAS },
  { "Ajustes", "Veja antes de mudar.", cenaAjustes, AJ_DUR, 0 },
};

// Dolby Vision em MKV e o ponteiro do Magic Remote sao so da LG: nas outras
// plataformas o grupo/item nao aparece (o Mac de desenvolvimento mostra tudo).
static const NovItem ITENS_203[] = {
  { .grupo = "Som e imagem", .icone = "aj_monitor-play", .nome = "Dolby Vision em MKV nas TVs LG",
    .frase = "Uma tela mostra cada etapa enquanto abre; o áudio TrueHD troca para E-AC-3 ou AC-3 no mesmo idioma para o Dolby Vision ficar ligado.",
    .linhas = 3,
    // Duas linhas fixas (o \n e da chave, e as traducoes o repetem): o caminho
    // inteiro na segunda, sem sobrar uma palavra sozinha.
    .frase2 = "Continua desligado por padrão.\nAjustes › Reprodução › Mais opções.", .linhas2 = 2,
    .plataformas = NOV_LG | NOV_OUTRAS },
  { .grupo = "Correções", .icone = "aj_circle-play", .nome = "Reprodução automática",
    .frase = "Sem plugins no modo só add-ons." },
  { .icone = "aj_image", .nome = "Cartazes com endereço longo",
    .frase = "Agora aparecem no título e em Salvos." },
  { .icone = "aj_tv-minimal-play", .nome = "TV ao vivo",
    .frase = "O canal reconecta sozinho após uma queda." },
  { .icone = "aj_list-video", .nome = "Continuar assistindo e Trakt",
    .frase = "Episódio certo, sem plays repetidos no Trakt." },
  { .grupo = "Otimização", .icone = "aj_gauge", .nome = "Início mais leve em GPUs fracas",
    .frase = "Sem trailer automático em Mali-400, 450 e 470." },
  { .icone = "aj_clock", .nome = "Android abre no ponto salvo",
    .frase = "Medido na TCL: caiu de 23,3 s para 14,8 s.",
    .plataformas = NOV_ANDROID | NOV_OUTRAS },
  { .grupo = "Também", .icone = "aj_sliders-horizontal", .nome = "Ajustes redesenhados",
    .frase = "Cada opção mostra uma prévia do que muda." },
  { .icone = "aj_mouse-pointer", .nome = "Ponteiro do Magic Remote",
    .frase = "Funciona em quase todas as telas.", .plataformas = NOV_LG | NOV_OUTRAS },
  { .icone = "aj_languages", .nome = "Tailandês com fonte própria",
    .frase = "Na interface e nas legendas, com a Noto Sans Thai." },
};

static const int ARTES_203[] = { 24, 15, 18, 12 };

static const NovConteudo NOV_ATUAL = {
  .versao = "2.0.3",
  .arquivo = "novidades-203.txt",
  .subtitulo = "Uma versão focada em correções e otimização.",
  .itens = ITENS_203, .nItens = (int)(sizeof ITENS_203 / sizeof *ITENS_203),
  .cenas = CENAS_203, .nCenas = (int)(sizeof CENAS_203 / sizeof *CENAS_203),
  .artes = ARTES_203, .nArtes = (int)(sizeof ARTES_203 / sizeof *ARTES_203),
};
