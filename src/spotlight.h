#include "arteurl.h"
// SPOTLIGHT: a caixa de busca que o botao de MICROFONE do controle abre por
// cima de qualquer tela (pedido do dono, 01/10/2026), no lugar de levar a
// pessoa para a tela de Busca inteira.
//
// O QUE E: um painel de vidro centrado, com o campo no topo, o teclado de tela
// a esquerda (o D-pad continua precisando dele) e a lista de resultados a
// direita, que muda A CADA LETRA. Os resultados vem agrupados — melhor
// resultado em destaque, Titulos, Pessoas, Colecoes, Canais, Catalogos e
// Addons — e com o campo vazio a lista mostra as PESQUISAS RECENTES do perfil
// (as mesmas de buscasrec.h, que a tela de Busca usa) e "Em alta".
//
// QUEM ABRE (pesquisa de 01/10/2026):
//   - Android TV: KEYCODE_SEARCH (84), que e o que o botao de microfone da
//     maioria dos controles manda e que o Android ENTREGA ao app. O
//     NuvioActivity o troca por F6. KEYCODE_ASSIST (219) e
//     KEYCODE_VOICE_ASSIST (231) o sistema NAO entrega (documentado em
//     KeyEvent): controle cujo botao manda esses abre o Google Assistente e o
//     app nao fica sabendo. Amarela (PROG_YELLOW) vira F5.
//   - Samsung .tpk: "XF86BTVoice" CHEGA ao app (MEDIDO no D1: linhas
//     "[tecla] tpk sem mapa: XF86BTVoice" de um 6+ na 1.6.0). tpk.c o troca
//     por F6; "XF86Yellow" vira F5.
//   - Samsung .wgt: a tecla de microfone nao esta na lista registravel do
//     tizen.tvinputdevice. A casca registra "Search" (10225, controles antigos)
//     -> F6 e repassa a amarela (405) -> F5.
//   - LG webOS: o microfone do Magic Remote e do sistema (LG: "no APIs are
//     provided for system-level voice control"). Nenhuma tecla desconhecida do
//     D1 tem cara de microfone. Ficam a AMARELA (scancode 488, fora do Guia,
//     que ja a usa) e SEGURAR OK no item Buscar da barra lateral.
//   - Mac/teste: F6, F5 e "spotlight"/"voz" em /tmp/nuvio-key.
// F6 e "abrir pela voz": onde ha ditado (Android), o ditado ja comeca. F5 e
// "abrir": so a caixa.
//
// DITADO E TECLADO DO SISTEMA: no Android, por sistexto.h (SpeechRecognizer
// dentro do app com RECORD_AUDIO pedida no primeiro uso; reserva na tela de voz
// do sistema e no teclado do sistema, que tem o proprio microfone). O texto
// entra no campo enquanto a pessoa fala ou digita.
// Na Samsung (.tpk e .wgt) a UNICA via de voz e o botao de microfone ao lado de
// "Digitar pelo celular": ele abre o teclado da Samsung para o microfone do
// controle (entrada_texto.h, docs/plans/microfone-busca.md). A nota abaixo
// sobre webapis.voiceinteraction ficou como pesquisa: nao foi usada.
//
// LG E O TECLADO DO SISTEMA (pesquisa de 01/10/2026, NAO ligado):
//   O app usa o libSDL2 DO APARELHO (nenhum .so vai no .ipk). Na C9 (webOS
//   4.5, firmware W19) ele e o 2.0.4: a lista de simbolos que o webosbrew
//   extraiu (dev-toolbox-cli, common/data/05.40.20.01-HE_DTV_W19P_AFADATAA/
//   libSDL2-2.0.so.0.4.1.json) tem SDL_StartTextInput e
//   SDL_HasScreenKeyboardSupport, mas o Wayland e carregado por dlopen, entao
//   a lista nao diz se o backend da LG liga o teclado. O compositor oferece o
//   protocolo de texto: text_model_factory/text_model (show_input_panel,
//   commit_string) em webosbrew/wayland-protocols protocols/merged/tv-4.x/
//   text.xml, e text_model_interface em libwayland-webos-client de varios
//   firmwares. O port do webosbrew (webosbrew/SDL-webOS, ramo webOS-2.30.x,
//   src/video/wayland/SDL_waylandwebos_osk.c) implementa exatamente isso:
//   SDL_StartTextInput -> text_model_activate, commit_string ->
//   SDL_TEXTINPUT. SUSPEITO (nao medido) que o SDL de fabrica nao faca o
//   mesmo; o caminho que funciona com certeza e embarcar o SDL-webOS no .ipk,
//   uma troca grande (video exportado, teclas, cursor passam por ele).
//   spot_abrir loga uma linha "[spotlight] lg sdl X.Y.Z osk=N textinput=N"
//   por sessao: osk=1 diria que o SDL da TV afirma ter teclado de tela.
//   Nada foi testado na TV.
//
// SAMSUNG .wgt E A VOZ (pesquisa de 01/10/2026, NAO adicionado):
//   webapis.voiceinteraction exige o privilegio
//   http://developer.samsung.com/privilege/voicecontrol, nivel Public, desde
//   Tizen 6.0 (developer.samsung.com/smarttv/develop/api-references/
//   samsung-product-api-references/voiceinteraction-api.html). O .wgt normal
//   declara required_version 5.5 (tools/tizen-config.xml).
//   INSTALACAO EM 4/5, pelo codigo aberto do verificador (git.tizen.org,
//   platform/core/security/privilege-checker, capi/src/privilege_manager.c,
//   ramos tizen_4.0, tizen_5.0, tizen_5.5 e tizen_6.0): privilegio que nao esta
//   no banco da versao (ou emitido depois do required_version) volta como
//   PRVMGR_ERR_NO_EXIST_PRIVILEGE e o laco de privilege_manager_verify_privilege
//   SEGUE; so recusam a instalacao nome com "/internal/", privilegio da lista
//   negra e nivel acima do certificado (MISMATCHED_PRIVILEGE_LEVEL, que nao e o
//   caso: voicecontrol e Public). Ou seja: pelo Tizen aberto, NAO quebra. NAO
//   MEDIDO numa Samsung 2018-2020: o firmware de produto e fechado e pode
//   divergir. Mesmo instalando, em 4/5 webapis.voiceinteraction nao existe, e
//   quem usar tem de testar a presenca antes. Nada foi adicionado.
#ifndef NV_SPOTLIGHT_H
#define NV_SPOTLIGHT_H
#include <SDL2/SDL.h>

// Teclas sinteticas (ver o topo): quem traduz o controle manda estas.
#define SPOT_TECLA_VOZ   SDLK_F6
#define SPOT_TECLA_ABRIR SDLK_F5

// `voz` = 1 quando quem abriu foi o botao de microfone: com ditado disponivel
// ele comeca na hora.
void spot_abrir(int voz);
// Abre sem pedir o teclado do sistema (demonstracao das Dicas do controle).
void spot_abrir_demo(void);
// Busca somente preferencias locais, sem consultas de catalogo/pessoas nem historico.
void spot_abrir_ajustes(int voz);
// "Buscar no guia" (Guia de uso): os recursos do guia e os ajustes que casam.
void spot_abrir_guia(void);
// Reabre a busca local após abrir um resultado, restaurando consulta e foco.
void spot_reabrir_ajustes(void);
void spot_fechar(void);
int  spot_aberto(void);
// Ainda na tela (aberto ou saindo na animacao).
int  spot_visivel(void);
// Entrada assentada: o fundo de tras pode ser congelado (ver app.c).
int  spot_cheio(void);
void spot_evento(const SDL_Event *e);
void spot_atualizar(float dt, Uint32 agora);
// `veuPronto` = 1 quando o veu ja foi pintado na copia congelada do fundo.
void spot_desenhar(Uint32 agora, int veuPronto);
// O veu de tela inteira, para quem congela o fundo pinta-lo dentro da copia.
void spot_veu(void);

// O que a pessoa escolheu. Consumido na leitura, como busca_pediu_abrir.
enum {
  SPOT_NADA = 0,
  SPOT_TITULO,      // indice = indice no catalogo
  SPOT_PESSOA,      // indice = titulo de onde a pessoa veio (-1 = veio do
                    // TMDB: abre por tituloTmdb/tituloTipo); tmdb/nome/arte
  SPOT_COLECAO,     // indice = col_folder(indice)
  SPOT_CANAL,       // id/nome/base do canal (guia_item_do_canal)
  SPOT_CATALOGO,    // indice = cat_fileira(indice)
  SPOT_ADDONS,      // abre a tela de Addons
  SPOT_AJUSTE,      // indice = OpcaoId estavel da preferencia
  SPOT_GUIA         // indice = recurso do Guia de uso (ajustes_guia_ir)
};
typedef struct {
  int  tipo;
  int  indice;
  long tmdb;
  long tituloTmdb;
  char tituloTipo[8];
  char id[80];
  char nome[140];
  char arte[NV_ARTE_URL_MAX];
  char base[600];
} SpotPedido;
int  spot_pediu(SpotPedido *p);

// Texto que veio de fora (ditado, testes): substitui o campo.
void spot_texto_externo(const char *t);
const char *spot_consulta(void);
// Para testes: quantas linhas a lista tem, o tipo e o texto da linha i e a
// linha focada (-1 com o foco no teclado).
int  spot_n_linhas(void);
int  spot_linha_tipo(int i);
const char *spot_linha_texto(int i);
int  spot_linha_focada(void);
// Para testes: o teclado do app esta aberto? Foco na barra (1 campo, 2
// microfone, 0 fora dela)? Altura atual do corpo (0 = so a barra).
int  spot_teclado_app_aberto(void);
int  spot_foco_campo(void);
float spot_altura_corpo(void);
#endif
