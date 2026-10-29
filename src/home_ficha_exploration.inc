// Temporary native render studies. Included only by NV_FICHA_EXPLORATION.
// Existing focus springs own every dimension; no animation is disabled.
static float heroiMeta(const CatItem *ci, float x, float y, float w);
static void fichaStudyArt(const CatItem *ci, GfxRect b, int landscape, float alpha) {
  const char *path = landscape ? ci->backdrop : ci->poster;
  GLuint t = tex_obter_larg(path, landscape ? 960 : 360);
  gfx_tex_aspect_atual = tex_aspecto(path);
  gfx_rect(b, t, GFX_CARD, 0, 0, 0, 22.0f / b.h, 1, 1, 1, alpha);
  gfx_tex_aspect_atual = 0;
}
static void fichaStudyLogo(const CatItem *ci, float x, float y, float maxW, float maxH, float alpha) {
  GLuint t = tex_obter_logo_larg(ci->logo, 460);
  float aspect = tex_aspecto(ci->logo);
  if (t && aspect > 0) {
    float w = fminf(maxW, maxH * aspect), h = w / aspect;
    gfx_rect((GfxRect){x, y + maxH - h, w, h}, t, GFX_TEXTO,
             0, 0, 0, 0, 1, 1, 1, alpha);
  } else {
    TxtLinha title = txt_linha_corta(TXT_TITULO3, ci->titulo, 243, 242, 239, 255, maxW);
    txt_desenhar_alpha(title, x, y, alpha);
  }
}
static void desenhaFichaExploration(int r, float cardY, float h) {
  float x = homeConteudoX() - scrollX[r] + bordaX(r);
  float posterW = h * 2.0f / 3.0f;
  int active = !focoHero && foco.fileira == r;
  // Paint raised cards last so their shadows never cover a neighbour.
  for (int pass = 0; pass < 2; pass++) {
    float cursor = x;
    for (int c = 0; c < fileiras[r].n; c++) {
      const CatItem *ci = cat_item_exato(fileiraItemIndice(&fileiras[r], c));
      float f = anim_clamp(animFoco[r][c], 0, 1);
      float width = fichaExploration == 3 ? 390.0f + 250.0f * f
                  : posterW + (fichaExploration == 1 ? 470.0f * f : 0);
      float height = fichaExploration == 3 ? 219.375f + (h - 219.375f) * f : h;
      GfxRect b = {cursor, cardY + (h - height) * .5f - 10.0f * f, width, height};
      if (fichaExploration == 2) {
        b.w *= 1 + .08f * f; b.h *= 1 + .08f * f;
        b.y = cardY - h * .04f * f;
      }
      cursor += width + 32.0f;
      if ((f > .01f) != pass || !ci || b.x + b.w < 0 || b.x > NV_TELA_W) continue;
      float oldAlpha = gfx_opacidade_grupo;
      gfx_opacidade_grupo *= .80f + .20f * f;
      if (f > .01f) {
        GfxRect shadow = {b.x - 12, b.y - 6, b.w + 24, b.h + 28};
        gfx_rect(shadow, 0, GFX_SOMBRA, f, 0, 0, .07f, 0, 0, 0, .55f);
        // Focus is scale/lift and short shadow, never a decorative outline.
      }
      if (fichaExploration == 1 && f > .001f) {
        gfx_cor(b, 22.0f / b.h, .071f, .075f, .086f, 1);
        fichaStudyArt(ci, b, 1, .45f);
        gfx_rect(b, 0, GFX_VEU_CARD, 0, 0, 0, 22.0f / b.h, .02f, .02f, .03f, .98f);
        // Portrait remains intact as the card opens from its left edge.
        fichaStudyArt(ci, (GfxRect){b.x, b.y, posterW, b.h}, 0, 1);
        float a = anim_clamp((f - .35f) / .65f, 0, 1);
        if (a > .001f && active && c == foco.coluna) {
          // Fixed text measure; reveal by clipping, never rewrap during motion.
          gfx_recorte(b.x + posterW, b.y, fmaxf(0, b.w - posterW), b.h);
          float tx = b.x + posterW + 28, tw = 400;
          TxtLinha title = txt_linha_corta(TXT_ILHA_NOME_L, ci->titulo, 243, 242, 239, 255, tw);
          txt_desenhar_alpha(title, tx, b.y + 28, a);
          txt_desenhar_alpha(txt_linha_corta(TXT_CW_META, ci->meta, 214, 216, 224, 255, tw), tx, b.y + 76, a);
          txt_desenhar_alpha(txt_linha_corta(TXT_CW_META, ci->genero, 184, 189, 200, 255, tw), tx, b.y + 110, a);
          badge_imdb(tx, b.y + 152, ci->nota, 0, a);
          sinopse_bloco_corta(TXT_ILHA_TEXTO, ci->sinopse, 214, 216, 224,
                              tx, b.y + 199, tw, 28, a, 3);
          gfx_recorte(0, corteFileiras(), NV_TELA_W, NV_TELA_H - corteFileiras());
        }
      } else {
        fichaStudyArt(ci, b, fichaExploration == 3, 1);
        if (fichaExploration == 3) {
          gfx_veu_base(b, 22.0f / b.h, .65f, .94f);
          fichaStudyLogo(ci, b.x + 28, b.y + b.h - 112, b.w - 56, 78, 1);
        }
      }
      if (fichaExploration == 3 && f > .01f && active && c == foco.coluna) {
        float old = gfx_opacidade_grupo;
        gfx_opacidade_grupo *= f;
        heroiMeta(ci, b.x, cardY + h + 18, 640);
        sinopse_bloco_corta(TXT_ILHA_TEXTO, ci->sinopse, 190, 194, 204,
                            b.x, cardY + h + 58, 640, 28, 1, 2);
        gfx_opacidade_grupo = old;
      }
      gfx_opacidade_grupo = oldAlpha;
      if (active && c == foco.coluna) {
        alvoCard(b.x, b.y, b.w, b.h, r, c);
        itemFoco.indice = fileiraItemIndice(&fileiras[r], c);
        itemFoco.rect = b; itemFoco.arte = ci->poster;
        itemFoco.titulo = ci->titulo; itemFoco.genero = ci->genero; itemFoco.meta = ci->meta;
        publicarItemFoco();
      }
    }
  }
}
