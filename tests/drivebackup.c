// Real session/profile/snapshot owners; only Google HTTP is simulated. No
// graphics context is needed for modal actions, workers or stale-visit guards.
#define DRIVEBACKUP_TEST 1
#include "../src/drivebackup.c"
#include "dados.h"
#include "sessao.h"
#include "ajustes.h"
#include "player.h"
#include <assert.h>
static const char sessaoFixture[]="eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ1c2VyLTEiLCJleHAiOjQxMDI0NDQ4MDB9.test-signature\nfixture-refresh\n0\n";
typedef struct {
  const char *snapshot;size_t ns;int status;_Atomic int entrou,saiu;
  int esperar;
} Mock;
static int pedir(void *ctx,const RedePedido *p,RedeResposta *r) {
  Mock *m=ctx;
  assert(!p->seguir && p->job);
  if(m->esperar) {
    atomic_store(&m->entrou,1);
    for(int i=0;i<500 && !atomic_load(&m->saiu);i++)SDL_Delay(1);
    assert(atomic_load(&m->saiu));
  }
  const char *body;
  if(strstr(p->url,"alt=media"))body=m->snapshot;
  else {
    assert(strstr(p->url,"spaces=drive") && strstr(p->url,"nuvioOwner"));
    body="{\"files\":[{\"id\":\"Backup_1\",\"name\":\"Nuvio backup\",\"mimeType\":\"application/vnd.nuvio.local-settings\",\"size\":\"256\",\"createdTime\":\"2026-10-18T10:30:00Z\",\"appProperties\":{\"nuvioBackupVersion\":\"2\",\"nuvioOwner\":\"user-1\",\"nuvioProfile\":\"7\"}}]}";
  }
  r->status=m->status;r->n_corpo=strstr(p->url,"alt=media")?m->ns:strlen(body);
  r->corpo=malloc(r->n_corpo+1);assert(r->corpo);memcpy(r->corpo,body,r->n_corpo);r->corpo[r->n_corpo]=0;return 1;
}
static void pronto(void) {
  for(int i=0;i<2000 && D.trabalho;i++){drivebackup_atualizar();SDL_Delay(1);}
  assert(!D.trabalho);
}
void teclado_teste_texto(const char *);
void teclado_teste_foco(int,int);
static void digitarSenha(const char *s) {
  teclado_teste_texto(s);
  // Native IME submits through the same keyboard result contract.
  teclado_teste_foco(0,0);
  SDL_Event down={.type=SDL_KEYDOWN};down.key.keysym.sym=SDLK_DOWN;
  for(int i=0;i<20;i++)drivebackup_evento(&down);
  SDL_Event right={.type=SDL_KEYDOWN};right.key.keysym.sym=SDLK_RIGHT;
  for(int i=0;i<8;i++)drivebackup_evento(&right);
  SDL_Event enter={.type=SDL_KEYDOWN};enter.key.keysym.sym=SDLK_RETURN;
  drivebackup_evento(&enter);drivebackup_atualizar();
}
static void baixarComSenha(void) {
  ativar(0,0);assert(D.senhaEtapa==3);
  digitarSenha("backup-password");pronto();
}
int main(void) {
  dados_iniciar(getenv("NUVIO_DADOS"));assert(dados_gravar("sessao.txt",sessaoFixture));sessao_iniciar();
  assert(dados_gravar("perfil.txt","7\n"));perfis_carregar_ativo();ajustes_dir(dados_dir());player_dir(dados_dir());
  BackupContexto c;assert(backupapp_contexto(&c));BackupLocal *b=NULL;
  assert(backupapp_capturar(&c,1792330000,&b));char *snapshot=NULL;size_t n;
  assert(backuplocal_exportar(b,&snapshot,&n));backuplocal_liberar(b);
  unsigned char *sealed=NULL;size_t sealedN=0;
  assert(backupcrypto_selar(snapshot,n,"backup-password",15,"user-1:p7:settings",&sealed,&sealedN));
  Mock m={.snapshot=(const char *)sealed,.ns=sealedN,.status=200};testePedir=pedir;testePedirCtx=&m;
  drivebackup_abrir();assert(drivebackup_aberto() && D.contexto.perfil==7);
  D.estado=V_PRONTO;snprintf(D.acesso,sizeof D.acesso,"test-token");
  ativar(0,0);assert(D.senhaEtapa==1 && teclado_mascarado());
  digitarSenha("a");assert(D.estado==V_PRONTO && !*D.senha && !*teclado_texto());
  ativar(0,0);digitarSenha("backup-password");assert(D.senhaEtapa==2);
  digitarSenha("different-password");assert(D.estado==V_PRONTO && !*D.senha && !*teclado_texto() && !D.snapshot);
  ativar(1,0);assert(D.trabalho && D.estado==V_OCUPADO);pronto();
  // Cancelling keyboard input never leaves a backup password behind.
  assert(D.estado==V_LISTA && D.lista.n==1 && !strcmp(D.lista.arquivo[0].id,"Backup_1"));
  BackupLocal *restore=NULL;assert(!drivebackup_restauro(&c,&restore));
  ativar(0,0);assert(D.estado==V_SENHA && D.senhaEtapa==3 && teclado_mascarado());
  // Cancelling the password prompt must clear it and never download anything.
  SDL_Event escape={.type=SDL_KEYDOWN};escape.key.keysym.sym=SDLK_ESCAPE;drivebackup_evento(&escape);drivebackup_atualizar();
  assert(D.estado==V_PRONTO && !D.senhaEtapa && !*D.senha && !*teclado_texto());
  ativar(1,0);pronto();baixarComSenha();assert(D.estado==V_CONFIRMAR && D.restauro && !D.hardware && !*D.senha);
  assert(!drivebackup_restauro(&c,&restore));
  // Cancelling confirmation makes no file changes and discards the snapshot.
  ativar(2,0);assert(D.estado==V_PRONTO && !D.restauro);
  ativar(1,0);pronto();baixarComSenha();ativar(1,0);
  assert(drivebackup_restauro(&c,&restore) && restore && !backuplocal_parte(restore,BACKUP_AJUSTES_HARDWARE,NULL) && backupapp_validar(&c,restore));
  assert(!drivebackup_restauro(&c,&b));backuplocal_liberar(restore);drivebackup_fechar();
  // A queued confirmation cannot survive even a reopening of the same profile.
  drivebackup_abrir();D.estado=V_PRONTO;snprintf(D.acesso,sizeof D.acesso,"test-token");
  ativar(1,0);pronto();baixarComSenha();ativar(1,0);
  perfis_invalidar_sessao();assert(!drivebackup_restauro(&c,&restore));drivebackup_atualizar();assert(!drivebackup_aberto());
  // Closing during HTTP never joins/block-waits. The old worker owns its
  // buffers; a new profile/view cannot receive its files or credentials.
  drivebackup_abrir();D.estado=V_PRONTO;snprintf(D.acesso,sizeof D.acesso,"test-token");
  m.esperar=1;ativar(1,0);
  for(int i=0;i<1000 && !atomic_load(&m.entrou);i++)SDL_Delay(1);
  assert(atomic_load(&m.entrou));Trabalho *antigo=D.trabalho;atomic_fetch_add(&antigo->refs,1);drivebackup_fechar();
  assert(dados_gravar("perfil.txt","8\n"));perfis_carregar_ativo();perfis_invalidar_sessao();
  drivebackup_abrir();assert(D.contexto.perfil==8 && !*D.acesso && D.estado==V_ENTRAR);
  atomic_store(&m.saiu,1);
  for(int i=0;i<2000 && !atomic_load_explicit(&antigo->pronto,memory_order_acquire);i++){drivebackup_atualizar();SDL_Delay(1);}
  assert(atomic_load_explicit(&antigo->pronto,memory_order_acquire));soltar(antigo);
  assert(D.contexto.perfil==8 && D.estado==V_ENTRAR && !D.lista.n && !*D.acesso && !D.restauro);
  drivebackup_fechar();free(snapshot);free(sealed);
  puts("PASS: Drive modal worker/list/download/confirmation ownership and profile cancellation (mock Google HTTP, real native owners)");
}
