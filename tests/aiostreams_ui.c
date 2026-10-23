// Exercise real setup transitions, keyboard result consumption and account exit.
#include "../src/aiostreamsui.c"
#include <assert.h>
static int keyboardResult, keyboardMasked, p2p, activeProfile=1;
static const char *keyboardText="";
static Stream traceStream;
int stream_n(void) { return 1; }
int stream_lista_do_alvo(const char *id) { return !strcmp(id,"tt1"); }
const Stream *stream_item(int i) { (void)i; return &traceStream; }
void stream_inspecao_ordem(int *r,int n) { for(int i=0;i<n;i++) r[i]=i+1; }
int celular_abrir_link(const char *t,const char *d) { (void)t;return d[0]!=0; }
int addons_n(void) { return 0; }
int addons_exportar(AddonRemoto *v,int n) { (void)v;(void)n;return 0; }
int perfis_ativo_addons(void) { return activeProfile; }
int contacache_gravar(const char *s,int p,const char *u,const char *c) { (void)s;(void)p;(void)u;(void)c;return 1; }
unsigned addons_versao(void) { return 0; }
const char *addons_nome(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *addons_base(int i) { (void)i; return ""; }
int addons_adicionar(const char *n,const char *u) { (void)n;(void)u;return 1; }
int perfis_ativo(void) { return activeProfile; }
const char *sessao_usuario(void) { return "offline-user"; }
const char *i18n(const char *s) { return s; }
void teclado_abrir_com(const char *t,const char *d,int m,const char *a,const char *i) { (void)t;(void)d;(void)m;(void)a;(void)i; }
void teclado_mascarar(int v) { keyboardMasked=v; }
int teclado_aberto(void) { return 0; }
void teclado_evento(const SDL_Event *e) { (void)e; }
void teclado_atualizar(float d,Uint32 a) { (void)d;(void)a; }
int teclado_resultado(void) { int r=keyboardResult; keyboardResult=0; return r; }
const char *teclado_texto(void) { return keyboardText; }
void teclado_esquecer(void) { keyboardText=""; }
int celular_disponivel(void) { return 0; }
int celular_abrir(const char *t) { (void)t;return 0; }
void celular_fechar(void) { }
int celular_pegar(char *d,size_t n) { (void)d;(void)n;return 0; }
int ajustes_p2p_ligado(void) { return p2p; }
void ajustes_definir_p2p_ligado(int v) { p2p=v; }
int fil_n(void) { return 0; }
const char *fil_chave(int i) { (void)i;return ""; }
void fil_alternar(int i) { (void)i; }
int fil_mover(int i,int d) { (void)i;(void)d;return 0; }
void desc_remontar_fileiras(void) { }
void desc_repetir(void) { }
int rede_pedir(const RedePedido *p,RedeResposta *r) { (void)p; memset(r,0,sizeof *r); return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
int main(void) {
  aioui_abrir(-1); assert(pagina==SEM);
  foco=1; acao(); assert(pagina==INSTANCIA && entrada==1);
  keyboardText="https://self.test"; keyboardResult=TECLADO_PRONTO;
  aioui_atualizar(0,100); assert(pagina==SERVICOS && !strcmp(host,"https://self.test"));
  foco=0; acao(); assert(pagina==IMPORTAR);
  foco=1; acao(); assert(entrada==3 && keyboardMasked);
  keyboardText="https://self.test/manifest.json/evil"; keyboardResult=TECLADO_PRONTO;
  aioui_atualizar(0,100); assert(!pendente && mensagem[0]);
  pagina=FONTES; foco=0; acao(); assert(p2p);
  lateral=1; foco=5; acao(); assert(pagina==FILTROS);
  strcpy(alvo,"tt1"); strcpy(traceStream.url,"https://media.test/v");
  aio_resultados(&snap,"{\"streams\":[{\"url\":\"https://media.test/v\"},{\"externalUrl\":\"https://outside.test/private\"}]}");
  trace(); assert(snap.resultado[0].rank==1 && snap.resultado[0].decisao==4);
  traceStream.foraCache=1; trace(); assert(snap.resultado[0].decisao==3);
  pagina=LINKS; lateral=0; foco=0; acao(); assert(linkSelecionado==1);
  pagina=FILTROS; regraSelecionada=-1; foco=2; acao(); assert(regraSelecionada==2);
  activeProfile=2; aioui_atualizar(0,100); assert(!aioui_aberto() && !base[0]);
  puts("aiostreams UI: setup, masked import, P2P, navigation and profile isolation passed");
  return 0;
}
