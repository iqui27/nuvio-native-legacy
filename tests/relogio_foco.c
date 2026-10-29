// #495: executa os corpos reais de app/ilha e os pedidos de cada pagina.
// SDL e os servicos de conta/camada/desenho sao fronteiras tipadas em memoria;
// nao simula uma TV nem promete testar rede, GL ou o ciclo de login completo.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t SDL_Keycode;
typedef uint32_t Uint32;
typedef uint64_t NvPerfilSessao;
enum { SDL_KEYDOWN=1, SDL_KEYUP, SDL_MOUSEBUTTONDOWN, SDL_MOUSEWHEEL, SDL_FINGERDOWN, SDL_WINDOWEVENT, SDL_APP_WILLENTERBACKGROUND };
enum { SDL_WINDOWEVENT_FOCUS_LOST=30, SDL_WINDOWEVENT_HIDDEN };
enum { SDLK_RETURN=10, SDLK_KP_ENTER, SDLK_SPACE, SDLK_UP, SDLK_DOWN,
       SDLK_LEFT, SDLK_RIGHT, SDLK_ESCAPE, SDLK_AC_BACK, SDLK_BACKSPACE,
       SDLK_DELETE, SDLK_s, SDLK_F9 };
enum { NV_SCANCODE_BLUE=70, NV_SCANCODE_BACK };
typedef struct { int type; struct { int repeat; struct { SDL_Keycode sym; int scancode; } keysym; } key; struct { int event; } window; } SDL_Event;
#define SDL_zero(x) memset(&(x), 0, sizeof(x))
#include "../src/catalogo.h"
typedef struct { float x,y,w,h; } GfxRect;
#include "relogio_tela.inc"

// Cada booleano e uma fronteira independente: nenhuma regra de elegibilidade
// e reimplementada aqui; relogioCabe/FocoPermitido reais a combinam.
#define GETTERS(X) \
 X(ajustes_relogio_ligado) X(login_concluido) X(perfilsel_concluido) \
 X(player_aberto) X(player_mini_ativo) X(menu_aberto) X(spainel_aberto) \
 X(central_aberta) X(ilha_modal_aberto) X(registro_aberto) X(registro_envio_aberto) \
 X(estatvivo_aberto) X(spot_aberto) X(teclado_aberto) X(celb_aberto) X(avisos_aberto) \
 X(ctx_aberto) X(faixas_aberta) X(episodios_aberto) X(stream_folha_aberta) \
 X(guia_overlay_aberta) X(detail_aberto) X(detail_relogio_navegavel) \
 X(ajustes_relogio_navegavel) X(busca_relogio_navegavel) X(agendaui_menu_aberto) \
 X(vertudo_aberta) X(ajustes_relogio_cabe) X(detail_relogio_oculto) \
 X(sintro_aberto) X(novidades_aberto) X(novidades11_aberto) X(novidades12_aberto) \
 X(novidades13_aberto) X(novidades131_aberto) X(novidades132_aberto) \
 X(novidades133_aberto) X(novidades134_aberto) X(novidades139_aberto) \
 X(novidades1312_aberto) X(novidades142_aberto) X(novidades148_aberto) \
 X(novidades170_aberto) X(novidades180_aberto) X(novidades20_aberto) \
 X(novidades201_aberto) X(novidades202_aberto) X(novcartao_aberto) \
 X(telemetria_aberto) X(recintro_aberto) X(atualizacao_aberta) X(agendaviso_aberto) \
 X(avisos_cartao_aberto) X(glem_cartao_aberto) X(recenviar_aberto) X(pessoas_aberto) \
 X(recomenda_aberta) X(pipintro_aberto) X(diagnostico_intro_aberto) \
 X(menu_relogio_cabe)
#define DECLARE_GET(name) static int state_##name; static int name(void) { return state_##name; }
GETTERS(DECLARE_GET)
static Tela tela;
static int homePronta, mobile, detailIdx, pedidos[12];
static NvPerfilSessao sessao;
static CatItem item;
static NvPerfilSessao nv_perfil_sessao_ler(void) { return sessao; }
static int detail_indice(void) { return detailIdx; }
const CatItem *cat_item(int i) { return i == detailIdx ? &item : NULL; }
#ifdef NV_TOUCH_UI
static int layout_modo_mobile(void) { return mobile; }
#endif
static int relogioQuer, relogioFocado;
static float modalT, corpoT;
static int anelN;
static GfxRect anelRect;
static void gfx_anel_fora(GfxRect r, float raio, float folga, float esp,
                          float cr, float cg, float cb, float alpha) {
  (void)raio; (void)folga; (void)esp; (void)cr; (void)cg; (void)cb; (void)alpha;
  anelN++; anelRect = r;
}
#include "relogio_ilha.inc"

// Os despachos conservam o caminho real de CH+; aqui so seus donos externos
// informam quem consumiu e registram se Salvos foi acionado.
static int ilhaConsome, avisoConsome, ilhaN, avisoN, salvosN, modalPedido;
static int cartao, atividade, retangulo, salvosOrigem, modalPode;
static Uint32 ticks;
static Uint32 SDL_GetTicks(void) { return ticks; }
static int ilha_cartao_na_tela(void) { return cartao; }
static int ilha_atividade_expansivel(void) { return atividade; }
static int ilha_modal_abrir(void) {
  modalPedido++;
  if (modalPode) state_ilha_modal_aberto=1;
  return modalPode;
}
static int ilha_rect(float *x,float *y,float *w,float *h) {
  *x=100; *y=20; *w=140; *h=52; return retangulo;
}
static void spainel_fechar(void) { state_spainel_aberto=0; }
static void spainel_abrir(void) { state_spainel_aberto=1; salvosN++; }
static void spainel_abrir_de(float x,float y,float w,float h) {
  (void)x; (void)y; (void)w; (void)h; salvosOrigem++; spainel_abrir();
}
#include "relogio_salvos.inc"
static int ilha_evento(const SDL_Event *e) {
  if (e->type != SDL_KEYDOWN || e->key.keysym.scancode != NV_SCANCODE_BLUE || e->key.repeat) abort();
  ilhaN++; return ilhaConsome;
}
static int avisos_evento(const SDL_Event *e) {
  if (e->type != SDL_KEYDOWN || e->key.keysym.scancode != NV_SCANCODE_BLUE) abort();
  avisoN++; return avisoConsome;
}
#include "relogio_pedidos.inc"
#include "relogio_app.inc"

static int checks;
#define CHECK(test, nome) do { if (!(test)) { fprintf(stderr,"FAIL: %s (linha %d)\n",nome,__LINE__); exit(1); } checks++; printf("ok: %s\n",nome); } while (0)
static SDL_Event tecla(int tipo, SDL_Keycode k, int sc, int repeat) {
  SDL_Event e; SDL_zero(e); e.type=tipo; e.key.keysym.sym=k; e.key.keysym.scancode=sc; e.key.repeat=repeat; return e;
}
static int enviar(int tipo, SDL_Keycode k, int sc, int repeat) {
  SDL_Event e=tecla(tipo,k,sc,repeat); return relogioFocoEvento(&e);
}
static void limpar(void) {
#define CLEAR_GET(name) state_##name=0;
  GETTERS(CLEAR_GET)
  memset(&relogioFoco,0,sizeof relogioFoco); memset(pedidos,0,sizeof pedidos);
  state_ajustes_relogio_ligado=state_login_concluido=state_perfilsel_concluido=1;
  state_detail_relogio_navegavel=state_ajustes_relogio_navegavel=state_busca_relogio_navegavel=1;
  state_ajustes_relogio_cabe=1; state_menu_relogio_cabe=1;
  tela=TELA_HOME; homePronta=1; sessao=1; mobile=detailIdx=0;
  snprintf(item.imdb,sizeof item.imdb,"tt1"); snprintf(item.tipo,sizeof item.tipo,"movie");
  ilhaConsome=avisoConsome=ilhaN=avisoN=salvosN=modalPedido=0;
  cartao=atividade=salvosOrigem=0; retangulo=modalPode=1; ticks+=1000;
  relogioQuer=1; relogioFocado=0; modalT=corpoT=0; anelN=0;
}
static void focar(void) { if (!relogioFocoPedir(1)) abort(); }

int main(void) {
  limpar(); focar();
  CHECK(ilha_relogio_focado(),"topo concede foco sem acionar a ilha");
  for(int i=0;i<4;i++) enviar(SDL_KEYDOWN,SDLK_UP,0,i&1);
  enviar(SDL_KEYUP,SDLK_UP,0,0);
  CHECK(!ilhaN && !salvosN && relogioFoco.ativo,"Cima repetido nunca abre nem confirma");
  enviar(SDL_KEYDOWN,SDLK_RETURN,0,0); enviar(SDL_KEYDOWN,SDLK_RETURN,0,0); enviar(SDL_KEYDOWN,SDLK_RETURN,0,1);
  CHECK(!ilhaN && relogioFoco.okArmado,"OK segurado aguarda soltura inclusive repeat zero");
  enviar(SDL_KEYUP,SDLK_KP_ENTER,0,0);
  CHECK(!ilhaN && relogioFoco.okArmado,"soltura de outro OK nao confirma");
  enviar(SDL_KEYUP,SDLK_RETURN,0,0);
  CHECK(ilhaN==1 && avisoN==1 && salvosN==1 && salvosOrigem==1 && !modalPedido && !relogioFoco.ativo,"OK solto usa caminho curto CH+ uma vez");
  CHECK(!enviar(SDL_KEYUP,SDLK_RETURN,0,0) && salvosN==1,"soltura duplicada nao reabre");

  limpar(); cartao=1; focar();
  enviar(SDL_KEYDOWN,SDLK_RETURN,0,0);
  CHECK(!modalPedido && !state_ilha_modal_aberto,"cartao retido aguarda OK solto sem executar Retomar");
  enviar(SDL_KEYUP,SDLK_RETURN,0,0);
  CHECK(modalPedido==1 && state_ilha_modal_aberto && !salvosN,"cartao retido abre o modal existente, sem reproduzir");
  limpar(); atividade=1; focar();
  enviar(SDL_KEYDOWN,SDLK_RETURN,0,0); enviar(SDL_KEYUP,SDLK_RETURN,0,0);
  CHECK(modalPedido==1 && !salvosN,"atividade expansivel usa o mesmo modal de CH+");
  limpar(); cartao=1; modalPode=0; retangulo=0; focar();
  enviar(SDL_KEYDOWN,SDLK_RETURN,0,0); enviar(SDL_KEYUP,SDLK_RETURN,0,0);
  CHECK(modalPedido==1 && salvosN==1 && !salvosOrigem,"cartao indisponivel preserva fallback Salvos sem geometria antiga");

  limpar(); focar(); ilhaConsome=1;
  enviar(SDL_KEYDOWN,SDLK_SPACE,0,0); enviar(SDL_KEYUP,SDLK_SPACE,0,0);
  CHECK(ilhaN==1 && !avisoN && !salvosN,"Espaco respeita aviso/enquete da ilha antes de Salvos");
  limpar(); focar(); avisoConsome=1;
  enviar(SDL_KEYDOWN,SDLK_KP_ENTER,0,0); enviar(SDL_KEYUP,SDLK_KP_ENTER,0,0);
  CHECK(ilhaN==1 && avisoN==1 && !salvosN,"central de avisos conserva prioridade");
  limpar(); focar();
  enviar(SDL_KEYDOWN,SDLK_RETURN,0,0); state_ilha_modal_aberto=1;
  CHECK(enviar(SDL_KEYDOWN,SDLK_RETURN,0,0),"novo modal nao recebe repeticao do OK armado");
  CHECK(enviar(SDL_KEYUP,SDLK_RETURN,0,0) && !salvosN && !relogioFoco.okArmado,"novo modal nao recebe soltura nem ativacao tardia");
  const int janelas[]={SDL_WINDOWEVENT_FOCUS_LOST,SDL_WINDOWEVENT_HIDDEN};
  for (unsigned i=0;i<sizeof janelas/sizeof janelas[0];i++) {
    limpar(); focar(); enviar(SDL_KEYDOWN,SDLK_RETURN,0,0);
    SDL_Event janela; SDL_zero(janela); janela.type=SDL_WINDOWEVENT; janela.window.event=janelas[i];
    CHECK(relogioFocoEvento(&janela) && !relogioFoco.ativo && !ilha_relogio_focado(),"perder foco/ocultar janela cancela selecao do relogio");
    CHECK(enviar(SDL_KEYUP,SDLK_RETURN,0,0) && !ilhaN && !salvosN,"soltura apos restaurar janela e consumida sem abrir ilha");
  }
  limpar(); focar(); enviar(SDL_KEYDOWN,SDLK_DOWN,0,0);
  SDL_Event perdeu; SDL_zero(perdeu); perdeu.type=SDL_WINDOWEVENT; perdeu.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
  relogioFocoEvento(&perdeu);
  CHECK(!relogioFoco.devolverTecla && !enviar(SDL_KEYDOWN,SDLK_DOWN,0,0),"perda da soltura ao ocultar janela nao prende proxima navegacao");
  limpar(); focar(); enviar(SDL_KEYDOWN,SDLK_RETURN,0,0);
  SDL_Event fundo; SDL_zero(fundo); fundo.type=SDL_APP_WILLENTERBACKGROUND;
  CHECK(relogioFocoEvento(&fundo) && !relogioFoco.ativo && enviar(SDL_KEYUP,SDLK_RETURN,0,0) && !ilhaN,"segundo plano Android cancela foco e nao confirma ao voltar");
  limpar(); focar(); state_spainel_aberto=1; relogioFocoValidar();
  CHECK(!relogioFoco.ativo && !ilha_relogio_focado(),"camada cancela foco antes do proximo quadro");

  const SDL_Keycode backs[]={SDLK_ESCAPE,SDLK_AC_BACK,SDLK_BACKSPACE,SDLK_DELETE};
  for (unsigned i=0;i<sizeof backs/sizeof backs[0];i++) {
    limpar(); focar();
    int a=enviar(SDL_KEYDOWN,backs[i],0,0), b=enviar(SDL_KEYDOWN,backs[i],0,0);
    int c=enviar(SDL_KEYUP,0,NV_SCANCODE_BACK,0);
    CHECK(a && b && c && !relogioFoco.ativo && !relogioFoco.devolverTecla && !enviar(SDL_KEYDOWN,backs[i],0,0),"Voltar restaura pagina, consome gesto e libera proximo Voltar");
  }
  limpar(); focar();
  CHECK(enviar(SDL_KEYDOWN,0,NV_SCANCODE_BACK,0) && enviar(SDL_KEYUP,SDLK_AC_BACK,0,0) && !relogioFoco.devolverTecla,"Back vendor sem simbolo e soltura remapeada combinam");
  limpar(); focar();
  CHECK(enviar(SDL_KEYDOWN,SDLK_DOWN,0,0) && enviar(SDL_KEYUP,SDLK_DOWN,0,0) && !relogioFoco.ativo,"Baixo devolve foco sem navegar a pagina");
  limpar(); focar();
  CHECK(!enviar(SDL_KEYDOWN,SDLK_s,NV_SCANCODE_BLUE,0) && !relogioFoco.ativo,"atalho Channel/AZUL permanece com seu dono");
  limpar(); focar(); SDL_Event toque; SDL_zero(toque); toque.type=SDL_FINGERDOWN;
  CHECK(!relogioFocoEvento(&toque) && !relogioFoco.ativo,"toque continua pelo caminho existente");

  limpar(); focar(); sessao++; relogioFocoValidar();
  CHECK(!relogioFoco.ativo,"troca de perfil invalida mesmo na mesma tela");
  limpar(); focar(); tela=TELA_BUSCA; relogioFocoValidar();
  CHECK(!relogioFoco.ativo,"troca de pagina invalida dono");
  limpar(); state_detail_aberto=1; focar(); strcpy(item.imdb,"tt2"); relogioFocoValidar();
  CHECK(!relogioFoco.ativo,"troca de titulo no mesmo indice invalida dono");
  limpar(); focar(); state_vertudo_aberta=1; relogioFocoValidar();
  CHECK(!relogioFoco.ativo,"abrir colecao invalida dono anterior");
#ifdef NV_TOUCH_UI
  limpar(); focar(); mobile=1; relogioFocoValidar();
  CHECK(!relogioFoco.ativo,"mudar TV/Mobile invalida dono");
#endif
  limpar(); state_ajustes_relogio_ligado=0;
  CHECK(!relogioFocoPedir(1),"relogio desligado mantem navegacao antiga");
  limpar(); homePronta=0;
  CHECK(!relogioFocoPedir(1),"Home ainda indisponivel nao captura foco");
  limpar(); state_detail_aberto=1; state_detail_relogio_oculto=1;
  CHECK(!relogioFocoPedir(1),"cartao de titulo recolhido sem relogio nao captura");

  const Tela permitidas[]={TELA_HOME,TELA_EXPLORAR,TELA_BUSCA,TELA_BIBLIOTECA,TELA_AGENDA,TELA_PERFIL,TELA_SOCIAL,TELA_ADDONS,TELA_PLUGINS,TELA_AJUSTES};
  const int pedidoIndice[]={0,1,2,3,4,5,6,7,8,9};
  for (unsigned i=0;i<sizeof permitidas/sizeof permitidas[0];i++) {
    limpar(); tela=permitidas[i]; pedidos[pedidoIndice[i]]=1;
    CHECK(relogioFocoPedir(relogioPedidoPagina()) && !relogioPedidoPagina(),"pagina visivel consome seu pedido uma unica vez");
  }
  limpar(); pedidos[10]=pedidos[11]=1;
  CHECK(vertudo_pediu_relogio() && detail_pediu_relogio() && !vertudo_pediu_relogio() && !detail_pediu_relogio(),"colecao e detalhe consomem pedidos separados");
  limpar(); state_teclado_aberto=1; pedidos[0]=1;
  CHECK(!relogioFocoPedir(relogioPedidoPagina()),"pedido negado por teclado e consumido");
  state_teclado_aberto=0;
  CHECK(!relogioPedidoPagina() && !relogioFoco.ativo,"fechar teclado nao reproduz pedido antigo");
  limpar(); tela=TELA_BUSCA; state_busca_relogio_navegavel=0;
  CHECK(!relogioFocoPedir(1),"IME ou voz pertencem a Busca");
  limpar(); tela=TELA_AJUSTES; state_ajustes_relogio_navegavel=0;
  CHECK(!relogioFocoPedir(1),"folha de Ajustes nao transfere foco");
  limpar(); tela=TELA_AGENDA; state_agendaui_menu_aberto=1;
  CHECK(!relogioFocoPedir(1),"menu da Agenda conserva teclado");
  limpar(); state_detail_aberto=1; state_detail_relogio_navegavel=0;
  CHECK(!relogioFocoPedir(1),"modal privado do detalhe conserva teclado");

  const Tela proibidas[]={TELA_LOGIN,TELA_ESCOLHA_PERFIL,TELA_GUIA,TELA_DIAGNOSTICO,TELA_PLAYER,TELA_LIVETV_DIAG};
  for(unsigned i=0;i<sizeof proibidas/sizeof proibidas[0];i++) {
    limpar(); tela=proibidas[i];
    CHECK(!relogioFocoPedir(1),"tela sem relogio navegavel nunca captura");
  }
  limpar(); state_player_aberto=1;
  CHECK(!relogioFocoPedir(1),"player de tela cheia excluido");
  limpar(); state_player_mini_ativo=1;
  CHECK(!relogioFocoPedir(1),"PiP conserva controles");
  limpar(); // Retido nao e player_aberto/mini: o cartao deve continuar acessivel.
  CHECK(relogioFocoPedir(1),"sessao retida nao e bloqueio de navegacao");

  limpar(); focar(); GfxRect r={100,20,140,52}; desenharFocoRelogio(r,26,1);
  CHECK(anelN==1 && anelRect.x==100 && anelRect.w==140,"foco desenha contorno na pilula real");
  modalT=.1f; desenharFocoRelogio(r,26,1); modalT=0; corpoT=.1f; desenharFocoRelogio(r,26,1);
  CHECK(anelN==1,"contorno nao aparece sobre modal ou corpo expandindo");
  corpoT=0; ilha_relogio_visivel(0); desenharFocoRelogio(r,26,1);
  CHECK(!ilha_relogio_focado() && anelN==1,"ocultar relogio limpa foco e contorno");
  printf("relogio_foco: %d checks passed\n",checks);
  return 0;
}
