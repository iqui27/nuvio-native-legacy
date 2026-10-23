// Real Settings navigation, keyboard results and per-account persistence.
#include "text.h"
#include "gfx.h"
#include "guia.h"
static void recorteTeste(float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;}
static void semRecorteTeste(void){}
static int recargasTeste;
static void recarregarTeste(int limpar){(void)limpar;recargasTeste++;}
static GuiaCategoria categoriasTeste[2];
static int nCategoriasTeste, categoriaGravacoes;
static int categoriasLerTeste(GuiaCategoria *out,int max) {
  int n=out && max<nCategoriasTeste?max:nCategoriasTeste;
  if(out && n>0)memcpy(out,categoriasTeste,sizeof *out*(size_t)n);
  return n;
}
static int categoriaFalhar;
static int categoriasDefinirTeste(const GuiaCategoria *cs,int n) {
  if(categoriaFalhar)return 0;
  if(!n)return 1;
  for(int k=0;k<n;k++) {
    int achou=0;for(int i=0;i<nCategoriasTeste;i++)if(categoriasTeste[i].chave==cs[k].chave)achou=1;
    if(!achou)return 0;
  }
  for(int k=0;k<n;k++)for(int i=0;i<nCategoriasTeste;i++)if(categoriasTeste[i].chave==cs[k].chave)categoriasTeste[i].oculta=cs[k].oculta;
  categoriaGravacoes++;return 1;
}
#define guia_categorias categoriasLerTeste
#define guia_categorias_definir categoriasDefinirTeste
#define guia_recarregar recarregarTeste
#define gfx_recorte recorteTeste
#define gfx_sem_recorte semRecorteTeste
#include "../src/ajustes.c"
#undef guia_recarregar
#undef guia_categorias
#undef guia_categorias_definir
#undef gfx_recorte
#undef gfx_sem_recorte
#include <assert.h>
void teclado_teste_texto(const char *t);
void teclado_teste_foco(int f,int c);
static void key(SDL_Keycode k) {
  SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=k;ajustes_evento(&e);
}
static int linha(int op) {
  for(int i=0;i<AJ_N_TELA;i++)if(TELA[i].tipo==IT_OPC && TELA[i].op==op)return i;
  assert(!"Settings row missing");return -1;
}
static void tocar(int op) {
  int i=linha(op);assert(visivel(i) && focavel(i));
  ajPonteiroItem(i,0);assert(focoOp==op);key(SDLK_RETURN);
}
static void salvarTeclado(const char *t) {
  assert(teclado_aberto());teclado_teste_texto(t);teclado_teste_foco(0,0);
  SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;
  for(int i=0;i<20;i++)teclado_evento(&e);
  e.key.keysym.sym=SDLK_RIGHT;
  for(int i=0;i<(teclado_mascarado()?3:2);i++)teclado_evento(&e);
  e.key.keysym.sym=SDLK_RETURN;teclado_evento(&e);assert(!teclado_aberto());
  ajustes_atualizar(0.016f,SDL_GetTicks());
}
static void campo(int op,const char *t) {
  if(teclado_aberto())assert(stCampo==op);else tocar(op);
  salvarTeclado(t);
}
static IptvConta selecionada(void){IptvConta c;assert(iptv_conta_selecionada(&c));return c;}
static void resumoContas(int quantas) {
  char texto[160],esperado[160];int antes=iptv_conta_indice();
  aj2MenuValor(AJS_TVAOVIVO,texto,sizeof texto);
  if(quantas)snprintf(esperado,sizeof esperado,"%s: %d",i18n("Contas de TV ao vivo"),quantas);
  else snprintf(esperado,sizeof esperado,"%s",i18n("Não configurado"));
  assert(!strcmp(texto,esperado) && iptv_conta_indice()==antes);
}
static int listaOp(int tipo){return AJ_IPTV_LISTA_XTREAM+tipo-IPTV_XTREAM;}
static void raiz(void) {
  for(int i=0;i<6 && (uxEditor || iptvForm.tipo || teclado_aberto());i++) {
    key(SDLK_ESCAPE);ajustes_atualizar(0.016f,SDL_GetTicks());
  }
  assert(!uxEditor && !iptvForm.tipo);
}
static void lista(int tipo){raiz();tocar(listaOp(tipo));assert(uxEditor && uxOp==listaOp(tipo));}
static void nova(int tipo) {
  int antes=iptv_contas(NULL,0),selecionadaAntes=iptv_conta_indice();lista(tipo);
  ajIptvToqueEscolher(nValores(uxOp)-1,0);
  assert(!uxEditor && iptvForm.nova && iptvForm.tipo==tipo && teclado_aberto());
  assert(stCampo==ajIptvPrimeiroCampo(tipo) && !teclado_texto()[0]);
  assert(!iptvForm.nome[0] && !iptvForm.servidor[0] && !iptvForm.url[0] && !iptvForm.senha[0]);
  assert(iptv_contas(NULL,0)==antes && iptv_conta_indice()==selecionadaAntes);
}
static void adicionar(int tipo,const char *url,const char *usuario) {
  int antes=iptv_contas(NULL,0);nova(tipo);
  campo(ajIptvPrimeiroCampo(tipo),url);
  if(tipo==IPTV_XTREAM){campo(AJ_XTREAM_USUARIO,usuario);campo(AJ_XTREAM_SENHA,"fixture");}
  if(tipo==IPTV_STALKER)campo(AJ_STALKER_MAC,"00:1a:79:00:00:01");
  assert(iptv_contas(NULL,0)==antes);tocar(AJ_IPTV_SALVAR);
  assert(iptv_contas(NULL,0)==antes+1 && !iptvForm.tipo && uxEditor && uxOp==listaOp(tipo));
}
static void conta(int tipo,int pos) {
  lista(tipo);ajIptvToqueEscolher(pos,0);
  assert(uxEditor && uxOp==AJ_IPTV_ACOES && !iptvForm.tipo);
}
static void editar(int tipo,int pos) {
  conta(tipo,pos);ajIptvToqueEscolher(0,0);
  assert(!uxEditor && iptvForm.tipo==tipo && !iptvForm.nova);
}
static void excluir(int tipo,int pos) {
  conta(tipo,pos);ajIptvToqueEscolher(1,0);
  assert(uxEditor && uxOp==AJ_IPTV_EXCLUIR && uxPendente==0);
  ajIptvToqueEscolher(1,0);assert(uxEditor && uxOp==listaOp(tipo));
}
int main(int argc,char **argv) {
  setvbuf(stdout,NULL,_IONBF,0);
  assert(getenv("NUVIO_DADOS"));assert(SDL_Init(SDL_INIT_TIMER)==0);
  dados_iniciar(getenv("NUVIO_DADOS"));ajustes_dir(getenv("NUVIO_DADOS"));
  if(argc>1 && !strcmp(argv[1],"legacy")) {
    m3u_fonte_selecionar(0);
    assert(m3u_conta_definir("https://legacy.invalid/list","https://legacy.invalid/epg.xml"));
  }
  ajustes_iniciar();layout_modo_definir(1);layout_tela_definir(1080,1920);gfx_opacidade_grupo=0;
  assert(dados_gravar("aviso-legal.txt","1\n"));
  if(argc>1) {
    if(!strcmp(argv[1],"legacy")) {
      assert(iptv_contas(NULL,0)==1 && selecionada().id==0);resumoContas(1);
      adicionar(IPTV_M3U,"https://new.invalid/list",NULL);
      editar(IPTV_M3U,1);campo(AJ_M3U_EPG,"https://new.invalid/epg.xml");
      assert(definirValorDireto(AJ_GUIA_ORDEM,1));
      assert(definirValorDireto(AJ_EPG_MEMORIA,1));
    } else assert(!strcmp(argv[1],"reload"));
    assert(ajustes_guia_ordem()==1 && valor[AJ_EPG_MEMORIA]==1);
    assert(iptv_contas(NULL,0)==2 && selecionada().id!=0);resumoContas(2);
    editar(IPTV_M3U,1);assert(!strcmp(m3u_lista(),"https://new.invalid/list"));
    assert(!strcmp(m3u_epg(),"https://new.invalid/epg.xml"));
    editar(IPTV_M3U,0);assert(selecionada().id==0);
    assert(!strcmp(m3u_lista(),"https://legacy.invalid/list"));
    assert(!strcmp(m3u_epg(),"https://legacy.invalid/epg.xml"));
    editar(IPTV_M3U,1);
    puts("PASS: migrated account and two independent M3U/EPG pairs survive restart");
    ajustes_encerrar();SDL_Quit();return 0;
  }
  assert(!iptv_contas(NULL,0));resumoContas(0);
  focarOpcao(AJ_IPTV_LISTA_XTREAM);
  int n=0;for(int i=secIni[secAtual]+1;i<secFim(secAtual);i++)
    if(TELA[i].tipo==IT_OPC && visivel(i) && ajIptvGerenciador(TELA[i].op))n++;
  assert(n==3);
  for(int tipo=IPTV_XTREAM;tipo<=IPTV_STALKER;tipo++) {
    lista(tipo);char texto[160];assert(nValores(uxOp)==2 && uxPendente==1);
    uxValorTexto(uxOp,0,texto,sizeof texto);assert(!strcmp(texto,i18n("Nenhuma")));
    ajIptvToqueEscolher(0,0);assert(uxEditor && !iptvForm.tipo);
    nova(tipo);campo(ajIptvPrimeiroCampo(tipo),"https://cancel.invalid/list");
    tocar(AJ_IPTV_CANCELAR);assert(!iptvForm.tipo && !iptv_contas(NULL,0));
    nova(tipo);key(SDLK_ESCAPE);ajustes_atualizar(0.016f,SDL_GetTicks());
    assert(!iptvForm.tipo && !iptv_contas(NULL,0) && uxEditor && uxOp==listaOp(tipo));
  }
  puts("PASS: provider > Add immediately opens blank address input; initial Cancel returns to provider, partial setup and Back create no account");
  nova(IPTV_XTREAM);campo(AJ_XTREAM_SERVIDOR,"https://one.invalid");
  tocar(AJ_IPTV_SALVAR);assert(!iptv_contas(NULL,0) && iptvForm.nova);
  campo(AJ_XTREAM_USUARIO,"one");campo(AJ_XTREAM_SENHA,"fixture");
  assert(strcmp(textoLeitura(AJ_XTREAM_SENHA),"fixture"));
  campo(AJ_IPTV_NOME,"First provider");tocar(AJ_IPTV_ATIVA);
  tocar(AJ_IPTV_SALVAR);assert(iptv_contas(NULL,0)==1);
  resumoContas(1); // A disabled saved account is still set up.
  IptvConta um=selecionada();assert(!um.ativa && !strcmp(um.nome,"First provider"));
  adicionar(IPTV_XTREAM,"https://two.invalid","two");IptvConta dois=selecionada();assert(dois.id!=um.id);
  editar(IPTV_XTREAM,0);assert(!strcmp(xtream_usuario(),"one"));
  tocar(AJ_IPTV_ATIVA);assert(selecionada().ativa);
  editar(IPTV_XTREAM,1);assert(!strcmp(xtream_usuario(),"two"));
  puts("PASS: explicit Save validates all fields and saves name/enabled state; two Xtream accounts keep independent credentials");
  raiz();tocar(AJ_GUIA_ORDEM);key(SDLK_RIGHT);key(SDLK_RETURN);
  assert(ajustes_guia_ordem()==1 && perfilLocal(AJ_GUIA_ORDEM) && somenteDesteAparelho(AJ_GUIA_ORDEM));
  tocar(AJ_GUIA_ORDEM);key(SDLK_LEFT);key(SDLK_RETURN);assert(!ajustes_guia_ordem());
  tocar(AJ_IPTV_ORDEM);assert(frAberta==8 && faN==2);
  faContaFocar(0,0);assert(iptv_conta_indice()==1); // selecting a row does not move or switch accounts
  faPont(1,0);key(SDLK_RETURN);IptvConta cs[IPTV_MAX_CONTAS];assert(iptv_contas(cs,IPTV_MAX_CONTAS)==2);
  assert(cs[0].id==dois.id && selecionada().id==dois.id && !strcmp(xtream_usuario(),"two"));
  faPont(0,1);key(SDLK_RETURN);assert(iptv_contas(cs,IPTV_MAX_CONTAS)==2 && cs[0].id==um.id);
  assert(selecionada().id==dois.id);key(SDLK_ESCAPE);
  tocar(AJ_IPTV_ORDEM);perfis_invalidar_sessao();key(SDLK_RETURN);assert(!frAberta);
  puts("PASS: guide-order setting and account arrow controls preserve selection/credentials and reject stale profiles");
  raiz();tocar(AJ_EPG_MEMORIA);key(SDLK_RIGHT);key(SDLK_RETURN);
  assert(valor[AJ_EPG_MEMORIA]==1 && somenteDesteAparelho(AJ_EPG_MEMORIA) && !dePerfil(AJ_EPG_MEMORIA));
  tocar(AJ_EPG_MEMORIA);key(SDLK_RIGHT);key(SDLK_RETURN);assert(valor[AJ_EPG_MEMORIA]==2);
  tocar(AJ_GUIA_CATEGORIAS);assert(uxEditor && !uxNCategorias);key(SDLK_RETURN);assert(!categoriaGravacoes);ajToqueConfirmar(0,0);assert(!uxEditor && !categoriaGravacoes);
  nCategoriasTeste=2;
  categoriasTeste[0]=(GuiaCategoria){.chave=11,.sessao=nv_perfil_sessao_ler()};
  categoriasTeste[1]=(GuiaCategoria){.chave=22,.sessao=nv_perfil_sessao_ler()};
  strcpy(categoriasTeste[0].nome,"Sports");strcpy(categoriasTeste[0].conta,"Account A");
  strcpy(categoriasTeste[1].nome,"Sports");strcpy(categoriasTeste[1].conta,"Account B");
  tocar(AJ_GUIA_CATEGORIAS);assert(uxNCategorias==2 && nValores(uxOp)==2);
  // A row click is a draft checkbox, not a disk write. Confirm must not
  // toggle it a second time; Cancel and Back discard all staged choices.
  ajIptvToqueEscolher(0,0);assert(uxCategorias[0].oculta && !uxCategorias[1].oculta && !categoriaGravacoes);
  ajToqueConfirmar(0,0);assert(!uxEditor && categoriasTeste[0].oculta && !categoriasTeste[1].oculta && categoriaGravacoes==1);
  tocar(AJ_GUIA_CATEGORIAS);ajIptvToqueEscolher(1,0);ajToqueCancelar(0,0);
  assert(!uxEditor && !categoriasTeste[1].oculta && categoriaGravacoes==1);
  tocar(AJ_GUIA_CATEGORIAS);key(SDLK_DOWN);key(SDLK_RETURN);
  assert(uxCategorias[1].oculta && !categoriasTeste[1].oculta);key(SDLK_ESCAPE);
  assert(!uxEditor && !categoriasTeste[1].oculta && categoriaGravacoes==1);
  tocar(AJ_GUIA_CATEGORIAS);key(SDLK_RETURN);key(SDLK_DOWN);key(SDLK_RETURN);
  assert(!uxCategorias[0].oculta && uxCategorias[1].oculta && categoriaGravacoes==1);
  key(SDLK_RIGHT);assert(uxRodape==1);key(SDLK_RETURN);
  assert(!uxEditor && !categoriasTeste[0].oculta && categoriasTeste[1].oculta && categoriaGravacoes==2);
  tocar(AJ_GUIA_CATEGORIAS);key(SDLK_RETURN);key(SDLK_DOWN);key(SDLK_DOWN);assert(uxRodape==1);
  key(SDLK_LEFT);assert(uxRodape==2);key(SDLK_RETURN);
  assert(!uxEditor && !categoriasTeste[0].oculta && categoriaGravacoes==2);
  tocar(AJ_GUIA_CATEGORIAS);ajIptvToqueEscolher(0,0);categoriaFalhar=1;ajToqueConfirmar(0,0);
  assert(uxEditor && uxCategorias[0].oculta && !categoriasTeste[0].oculta && categoriaGravacoes==2);
  categoriaFalhar=0;ajToqueConfirmar(0,0);assert(!uxEditor && categoriasTeste[0].oculta && categoriaGravacoes==3);
  tocar(AJ_GUIA_CATEGORIAS);int gravacoes=categoriaGravacoes;perfis_invalidar_sessao();ajToqueConfirmar(0,0);
  assert(!uxEditor && !uxCategorias && categoriaGravacoes==gravacoes);
  puts("PASS: touch/remote draft checkboxes; Confirm saves once; Cancel/Back discard; failed save retains draft; stale profile cannot save");

  adicionar(IPTV_M3U,"https://three.invalid/list",NULL);IptvConta tres=selecionada();
  editar(IPTV_M3U,0);campo(AJ_M3U_EPG,"https://three.invalid/epg.xml");
  adicionar(IPTV_M3U,"https://four.invalid/list",NULL);IptvConta quatro=selecionada();
  assert(!m3u_epg()[0]);editar(IPTV_M3U,1);campo(AJ_M3U_EPG,"https://four.invalid/epg.xml");
  conta(IPTV_M3U,1);ajIptvToqueEscolher(1,0);key(SDLK_RETURN);
  assert(uxOp==AJ_IPTV_ACOES && iptv_contas(NULL,0)==4); // default Cancel
  ajIptvToqueEscolher(1,0);key(SDLK_ESCAPE);assert(uxOp==AJ_IPTV_ACOES && iptv_contas(NULL,0)==4);
  excluir(IPTV_M3U,1);resumoContas(3);assert(iptv_contas(NULL,0)==3 && !m3u_fonte_configurado(quatro.id));
  editar(IPTV_M3U,0);assert(selecionada().id==tres.id);
  assert(!strcmp(m3u_lista(),"https://three.invalid/list") && !strcmp(m3u_epg(),"https://three.invalid/epg.xml"));
  assert(xtream_fonte_configurado(um.id) && xtream_fonte_configurado(dois.id));
  puts("PASS: Edit/Delete actions, default Cancel and Back are safe; confirmed deletion preserves every other account");
  adicionar(IPTV_STALKER,"https://portal.invalid",NULL);assert(stalker_configurado());
  nova(IPTV_M3U);campo(AJ_M3U_URL,"ftp://invalid.example/list");tocar(AJ_IPTV_SALVAR);
  assert(iptv_contas(NULL,0)==4 && iptvForm.nova);
  raiz();nova(IPTV_M3U);perfis_invalidar_sessao();salvarTeclado("https://stale.invalid/list");
  assert(iptv_contas(NULL,0)==4 && !iptvForm.tipo);
  lista(IPTV_M3U);perfis_invalidar_sessao();key(SDLK_RETURN);assert(!uxEditor);
  conta(IPTV_M3U,0);ajIptvToqueEscolher(1,0);perfis_invalidar_sessao();ajIptvToqueEscolher(1,0);
  assert(iptv_contas(NULL,0)==4 && !uxEditor);
  while(ajIptvQuantos(IPTV_XTREAM))excluir(IPTV_XTREAM,0);
  while(ajIptvQuantos(IPTV_STALKER))excluir(IPTV_STALKER,0);
  resumoContas(1); // M3U-only setup, regardless of the previously selected type.
  excluir(IPTV_M3U,0);resumoContas(0);
  puts("PASS: Live TV summary counts saved accounts including M3U-only, disabled, migrated and reloaded accounts; deletion updates the count without switching accounts");
  // An explicit close must erase credentials from the unsaved form.
  nova(IPTV_XTREAM);salvarTeclado("https://draft.invalid");campo(AJ_XTREAM_SENHA,"do-not-retain");ajustes_encerrar();
  assert(!iptvForm.senha[0] && !iptvForm.tipo);
  puts("PASS: invalid URLs and stale-profile keyboard/list/delete results cannot create or remove accounts; closing wipes draft secrets");
  SDL_Quit();return 0;
}
