// PECAS DO PLAYER NO GLASS UI (mockup aprovado em 03/10, player-mockup.html).
//
// O player tinha tres folhas com tres desenhos, tres barras de progresso, duas
// formulas de acento e dois sistemas de veu (inventario do mockup, secao 10).
// Estas pecas sao as UNICAS que as telas do player usam para o material da
// ilha, o foco, as dicas de tecla, o kicker, o segmentado, a barra e os veus —
// uma conta so para cada coisa, nos dois materiais (ajustes_vidro()).
#ifndef NV_PLRUI_H
#define NV_PLRUI_H
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <stddef.h>

// MATERIAL DA ILHA. Vidro: rgba(14,15,18,.80) + luz larga no canto de cima;
// solido: #15161A opaco. Sombra curta (GFX_SOMBRA) nos dois, nunca contorno.
// `raioPx` em pixels; `modal` = 1 deixa o miolo um degrau mais denso (.86),
// como a ilha que vira modal.
void plrui_material(GfxRect r, float raioPx, int modal, float a);
// Same material; semSombra = 1 drops the drop shadow (Sources sheet).
void plrui_material_s(GfxRect r, float raioPx, int modal, float a, int semSombra);
// The title-page card material (shared glass, GFX_VIDRO_DENS): what the
// "..." more menu draws, so it matches cast / trailers / comments cards.
void plrui_cartao(GfxRect r, float raioPx, float a);
// Superficie de FOCO de uma linha/celula: vidro branco 12%, solido #2B2D34
// (com o filete claro de cima e a sombra do mockup).
void plrui_linha_foco(GfxRect r, float raioPx, float a);
// FOCO DE BOTAO: pilula cheia no acento com a luz colorida por baixo (o
// box-shadow 0 8 28 acento 35% do mockup), nos dois materiais.
void plrui_pilula_foco(GfxRect r, float a);
// Video variant: live frame keeps shining through the focused accent pill.
void plrui_pilula_foco_video(GfxRect r, float a);
// Island over live video (post-play / error cards): the shared material where
// glass exists; glass off keeps the opacity-adjustable tint, never #15161A.
void plrui_material_video(GfxRect r, float raioPx, float a);
// Botao em repouso dentro de uma ilha: branco 8% (solido #24262C).
void plrui_botao_repouso(GfxRect r, float a);
// Video variant: glass off keeps the live frame visible through the fill.
void plrui_botao_repouso_video(GfxRect r, float a);
// OSD disc: shared glass at 52%; glass off uses an opacity-dependent tint.
void plrui_disco_osd(GfxRect r, float a);

// Botao de ilha (.btn do mockup: 60 de altura, 21/600, icone 22): devolve a
// largura. `foco` 0..1 (mola). Desenha em (x, y).
float plrui_botao_largura(const char *rotulo, const char *icone);
float plrui_botao(float x, float y, const char *rotulo, const char *icone, float foco, float a);
float plrui_botao_video(float x, float y, const char *rotulo, const char *icone, float foco, float a);

// Kicker: 15/700 em caixa alta espacada, `cinza` 0..255 (115 = .45, 158 = .62).
// Devolve a largura. x < 0 so mede.
float plrui_kicker(const char *s, float x, float y, int r, int g, int b, float a);

// DICAS DE TECLA (kbd + rotulo), lado a lado com vao de 22. `teclas` e
// `rotulos` ja traduzidos pelo chamador? Nao: os dois passam por i18n aqui.
// `alinhaDir` = 1 faz x ser a borda DIREITA. Devolve a largura total.
// x < 0 measures only, using the same translated text and spacing.
float plrui_dicas(const char *const *teclas, const char *const *rotulos, int n,
                  float x, float yc, int alinhaDir, float a);
float plrui_dicas_video(const char *const *teclas, const char *const *rotulos, int n,
                        float x, float yc, int alinhaDir, float a);

// SEGMENTADO (.seg): shared frosted sheet track (solid #1D1E23), item
// selecionado 14% (solido #34363E); `ed` = selecionado E em foco (cheio no
// acento). `contagem[i]` < 0 = sem numero. Devolve a largura. x < 0 so mede.
// `sel` e `ed` aceitam FRACOES: quem chama anima com mola e a pilula de foco
// desliza entre os chips (sel) e esmaece ao entrar/sair do segmentado (ed).
float plrui_seg(const char *const *rotulos, const int *contagem, int n, float sel, float ed,
                float x, float y, float a);

// A BARRA DE PROGRESSO, a mesma em todo lugar (OSD, pausa, ao vivo, trailer):
// trilho branco 20% com cantos redondos, o buffer a 30%, o andado no acento.
// `foco` engorda 6 -> 10 e poe a cabeca branca. `caps` sao os cortes da
// abertura/creditos (fracoes), `nCaps` quantos.
void plrui_barra(float x, float y, float w, float frac, float buf, int foco,
                 const float *caps, int nCaps, float a);

// TRILHO FINO (4 px, .trilho): carregando, Seekr carregando, lembrete.
// `cr/cg/cb` < 0 = acento.
void plrui_trilho(GfxRect r, float frac, float cr, float cg, float cb, float a);

// Anel de 12 pontos do carregamento, centrado em (cx, cy), diametro `d`.
// `cinza` = 1 pinta em branco 70% (o Seekr esperando), senao no acento.
void plrui_anel_solto(float cx, float cy, float d, Uint32 agora, float a);
void plrui_anel(float cx, float cy, float d, int cinza, Uint32 agora, float a);

// O ponto que respira (atividade): 10 px no acento com o halo de 6 a 22%.
void plrui_respira(float cx, float cy, float d, Uint32 agora, float a);

// Separador vertical da pilula (1 x 22, branco 18%).
void plrui_sep(float x, float yc, float a);

// Tempo "1:12:40" / "32:28" (sem hora quando menor que uma hora).
void plrui_tempo(char *b, size_t n, double seg);

// Troca o ponto decimal pela virgula nas linguas que escrevem com virgula
// (idioma_ponto_decimal): "18.2 GB" -> "18,2 GB", "+0.25 s" -> "+0,25 s".
void plrui_decimal(char *s);

// Tira separadores " · " soltos nas pontas (e espacos), e junta os
// repetidos: um campo vazio no meio de "a · b · c" nunca deixa "a ·" ou
// "· b". Edita no lugar.
void plrui_limpar_sep(char *s);

// Tinta do texto sobre a pilula de foco (0..255).
int plrui_tinta(void);

#endif
