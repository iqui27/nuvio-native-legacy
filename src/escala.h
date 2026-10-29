// TAMANHO DA INTERFACE: A TELA VIRTUAL DAS CAMADAS AMPLIADAS (gfx.h).
//
// Uma camada ampliada (Ajustes, player, folhas, ilhas, menus, modais) faz o
// layout numa tela de NV_VTELA_W x NV_VTELA_H — 1920/s x 1080/s — e desenha
// entre ESCALA_INI e ESCALA_FIM: o gfx multiplica tudo por s e a tela virtual
// cobre a real de borda a borda. Quem ancora na borda direita ou de baixo usa a
// tela VIRTUAL, e o layout REFLUI (menos linhas, folha mais estreita) em vez de
// vazar. Com s = 1 os dois tamanhos sao os de sempre, bit a bit.
//
// REGRA: o modulo que mede pela tela virtual e o mesmo que liga a escala no
// proprio desenho publico. Assim o layout e o desenho nunca discordam, venha a
// chamada do app, de outra tela ou de um teste de captura.
//
// Um arquivo inteiro na tela virtual define NV_ESCALA_TELA antes do include:
// NV_TELA_W/H passam a ser os virtuais ali dentro. NV_ESCALA_TELA_ATIVA: a
// tela do fator ATIVO no momento (para ESCALA_SE_COUBER).
#ifndef NV_ESCALA_H
#define NV_ESCALA_H
#include "layout.h"
#include "gfx.h"

#ifdef NV_TOUCH_UI
#define NV_LAYOUT_REAL_W nv_layout_w
#define NV_LAYOUT_REAL_H nv_layout_h
#else
#define NV_LAYOUT_REAL_W NV_TELA_BASE_W
#define NV_LAYOUT_REAL_H NV_TELA_BASE_H
#endif
#define NV_VTELA_W (NV_LAYOUT_REAL_W / gfx_escala_ui())
#define NV_VTELA_H (NV_LAYOUT_REAL_H / gfx_escala_ui())

// Liga a escala configurada ate o fim do bloco. Aninhar e seguro.
#define ESCALA_INI() float escalaAnt_ = gfx_escala_entrar()
#define ESCALA_FIM() gfx_escala_sair(escalaAnt_)
// O contrario: desenha em 1080p mesmo chamado de dentro de uma camada ampliada
// (o teclado, que ja ocupa a largura da tela em 100%).
#define ESCALA_REAL_INI() float escalaAnt_ = gfx_escala(); gfx_escala_sair(1.0f)
#define ESCALA_REAL_FIM() gfx_escala_sair(escalaAnt_)
// ESCALA COM PISO: telas que nascem maiores que 100% (Agenda 120%, topo da
// Biblioteca 120%, Spotlight 130%). O fator e max(piso, Tamanho da interface):
// acompanha o ajuste acima do piso. escala_min(m) devolve o fator e
// ESCALA_MIN_INI(m)/FIM() ligam e devolvem a escala, como ESCALA_INI/FIM.
static inline float escala_min(float m) {
  // O piso tambem engorda com a escala de texto: m * txt, nao m puro.
  float u = gfx_escala_ui_base();
  return gfx_escala_txt() * (u < m ? m : u);
}
#define ESCALA_MIN_INI(m) float escalaMinAnt_ = gfx_escala(); gfx_escala_sair(escala_min(m))
#define ESCALA_MIN_FIM() gfx_escala_sair(escalaMinAnt_)
// So amplia se um cartao de w x h (em px de 1080p) ainda couber na tela real
// com 16 px de folga em cada borda; senao desenha em 1080p. Para os cartoes
// grandes de tela quase cheia (novidades, explicadores), que ja foram
// desenhados para ocupar a tela em 100%. Quem usa mede pela tela ATIVA
// (NV_ESCALA_TELA_ATIVA abaixo), porque o fator depende do cartao.
#define ESCALA_SE_COUBER_INI(w, h) \
  float escalaAnt_ = gfx_escala(); \
  gfx_escala_sair(((w) * gfx_escala_ui() <= NV_LAYOUT_REAL_W - 32.0f && (h) * gfx_escala_ui() <= NV_LAYOUT_REAL_H - 32.0f) \
                  ? gfx_escala_ui() : 1.0f)
#define ESCALA_SE_COUBER_FIM() gfx_escala_sair(escalaAnt_)


// ESCALA DE TEXTO E COMPONENTES: home, detalhe e biblioteca ligam so ela (nao
// o Tamanho da interface). TXT_INI/FIM no desenho, na atualizacao e nos eventos
// publicos do modulo; NV_TXT_TELA no topo do arquivo faz NV_TELA_W/H medirem a
// tela virtual real / txt (1920x1080 quando a escala e 100%).
#define TXT_INI() float txtAnt_ = gfx_escala_txt_entrar()
#define TXT_FIM() gfx_escala_sair(txtAnt_)

#endif

#ifdef NV_TXT_TELA
#undef NV_TELA_W
#undef NV_TELA_H
#define NV_TELA_W (NV_LAYOUT_REAL_W / gfx_escala_tela_txt())
#define NV_TELA_H (NV_LAYOUT_REAL_H / gfx_escala_tela_txt())
// ANC: an anchor (margin, row top, hero base) in REAL px -> layer px. The text
// scale grows text and components from it, never moves it (#205).
#define ANC(v) ((v) / gfx_escala_tela_txt())
#endif

#ifdef NV_ESCALA_TELA_ATIVA
#undef NV_TELA_W
#undef NV_TELA_H
#define NV_TELA_W (NV_LAYOUT_REAL_W / gfx_escala())
#define NV_TELA_H (NV_LAYOUT_REAL_H / gfx_escala())
#endif

#ifdef NV_ESCALA_TELA
#undef NV_TELA_W
#undef NV_TELA_H
#define NV_TELA_W NV_VTELA_W
#define NV_TELA_H NV_VTELA_H
#endif

