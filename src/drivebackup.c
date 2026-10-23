#include "drivebackup.h"
#include "drivelocal.h"
#include "drivetv.h"
#include "driveplatform.h"
#include "perfis.h"
#include "idioma.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "escala.h"
#include "text.h"
#include "js.h"
#include "teclado.h"
#include "backupcrypto.h"
#include "drivefontes.h"
#include "dados.h"
#include "ajustes.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef NV_ANDROID
#include "android.h"
#endif
enum { V_ENTRAR,V_OCUPADO,V_CODIGO,V_PRONTO,V_LISTA,V_CONFIRMAR,V_SENHA };
enum { O_CARREGAR=1,O_CODIGO,O_POLL,O_RENOVAR,O_LISTAR,O_ENVIAR,O_BAIXAR };
typedef struct {
  _Atomic int refs,pronto;
  int op,resultado,cifrado;BackupContexto contexto;
  char senha[1025],dir[4096];
  RedeJob *job;
  char acesso[4097],renovacao[4097],cliente[513],segredo[4097],id[200],pagina[4096];
  char *snapshot;size_t n;
  DriveTvCodigo codigo;DriveTvToken token;DrivePagina lista;
  BackupLocal *baixado;
} Trabalho;
static struct {
  int aberta,estado,foco,telefone,confirmado,repetir,reautorizou,senhaEtapa,cifrado,hardware;
  char senha[1025];
  BackupContexto contexto;
  RedeGrupo *grupo;Trabalho *trabalho;
  uint64_t pedido;
  char acesso[4097],renovacao[4097],cliente[513],segredo[4097],id[200],pagina[4096];
  DriveTvCodigo codigo;DrivePagina lista;
  BackupLocal *restauro;
  char *snapshot;size_t n;
  const char *mensagem;
  float offset;
#ifdef NV_TOUCH_UI
  ToqueRolagem toque;
#endif
} D;
#ifdef NV_ANDROID
static uint64_t pedidos;
#endif
#ifdef DRIVEBACKUP_TEST
static int (*testePedir)(void *,const RedePedido *,RedeResposta *);
static void *testePedirCtx;
#endif
static void apagarMemoria(void *p,size_t n){volatile unsigned char *v=p;while(n--)*v++=0;}
static void soltar(Trabalho *t) {
  if(!t || atomic_fetch_sub_explicit(&t->refs,1,memory_order_acq_rel)!=1)return;
  rede_job_soltar(t->job);apagarMemoria(t->snapshot,t->n);free(t->snapshot);backuplocal_liberar(t->baixado);
  apagarMemoria(t,sizeof *t);free(t);
}
static int contextoAtual(void) {
  BackupContexto c;
  return backupapp_contexto(&c) && c.sessao==D.contexto.sessao && c.perfil==D.contexto.perfil && !strcmp(c.usuario,D.contexto.usuario);
}
void drivebackup_fechar(void) {
#ifdef NV_ANDROID
  android_drive_cancelar();
#endif
  if(D.grupo){rede_grupo_cancelar(D.grupo);rede_grupo_soltar(D.grupo);}
  if(D.senhaEtapa) {
    if(teclado_aberto()){SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=SDLK_ESCAPE;teclado_evento(&e);}
    teclado_resultado();teclado_esquecer();
  }
  soltar(D.trabalho);apagarMemoria(D.snapshot,D.n);free(D.snapshot);backuplocal_liberar(D.restauro);
  apagarMemoria(&D,sizeof D);
}
int drivebackup_aberto(void){return D.aberta;}
static void *executar(void *arg) {
  Trabalho *t=arg;
  DriveLocal local={.token=t->acesso,.dono=t->contexto.usuario,.perfil=t->contexto.perfil,.job=t->job,.senha=t->senha,.exigirCriptografia=t->cifrado};
  DriveTv tv={.cliente=t->cliente,.segredo=t->segredo,.job=t->job};
#ifdef DRIVEBACKUP_TEST
  local.pedir=tv.pedir=testePedir;local.ctx=tv.ctx=testePedirCtx;
#endif
  uint64_t agora=SDL_GetTicks64();
  switch(t->op) {
    case O_CARREGAR:
      {char *s=driveplatform_vinculo_ler(t->contexto.usuario,t->contexto.perfil,t->cliente);
      if(s){snprintf(t->renovacao,sizeof t->renovacao,"%s",s);apagarMemoria(s,strlen(s));free(s);}}
      t->resultado=t->renovacao[0]?drivetv_renovar(&tv,t->renovacao,agora,&t->token):0;break;
    case O_CODIGO:t->resultado=drivetv_iniciar(&tv,agora,&t->codigo);break;
    case O_POLL:t->resultado=drivetv_consultar(&tv,&t->codigo,agora,&t->token);break;
    case O_RENOVAR:t->resultado=drivetv_renovar(&tv,t->renovacao,agora,&t->token);break;
    case O_LISTAR:t->resultado=drivelocal_listar(&local,t->pagina,&t->lista);break;
    case O_ENVIAR: {
      char *completo=NULL;size_t nc=0;
      t->resultado=drivefontes_enviar(&local,t->dir,t->snapshot,t->n,&completo,&nc);
      if(t->resultado==DRIVE_LOCAL_OK)t->resultado=drivelocal_enviar(&local,completo,nc,t->id);
      backupcrypto_limpar(completo,nc);free(completo);break;
    }
    case O_BAIXAR:
      t->resultado=drivelocal_baixar(&local,t->id,&t->baixado);
      if(t->resultado==DRIVE_LOCAL_OK)t->resultado=drivefontes_baixar(&local,t->dir,t->baixado);
      break;
  }
  atomic_store_explicit(&t->pronto,1,memory_order_release);soltar(t);return NULL;
}
static int iniciar(int op) {
  if(D.trabalho || !D.grupo || !contextoAtual())return 0;
  Trabalho *t=calloc(1,sizeof *t);if(!t)return 0;
  atomic_init(&t->refs,2);atomic_init(&t->pronto,0);t->op=op;t->contexto=D.contexto;t->cifrado=D.cifrado;
  if(op==O_ENVIAR || op==O_BAIXAR)snprintf(t->senha,sizeof t->senha,"%s",D.senha);
  snprintf(t->dir,sizeof t->dir,"%s",dados_dir());
  t->job=rede_job_criar(D.grupo);if(!t->job){free(t);return 0;}
  snprintf(t->acesso,sizeof t->acesso,"%s",D.acesso);snprintf(t->renovacao,sizeof t->renovacao,"%s",D.renovacao);
  snprintf(t->cliente,sizeof t->cliente,"%s",D.cliente);snprintf(t->segredo,sizeof t->segredo,"%s",D.segredo);
  snprintf(t->id,sizeof t->id,"%s",D.id);snprintf(t->pagina,sizeof t->pagina,"%s",D.pagina);t->codigo=D.codigo;
  if(op==O_ENVIAR) {
    t->snapshot=malloc(D.n+1);if(!t->snapshot){soltar(t);soltar(t);return 0;}
    memcpy(t->snapshot,D.snapshot,D.n);t->snapshot[D.n]=0;t->n=D.n;
  }
  pthread_t fio;
  if(pthread_create(&fio,NULL,executar,t)!=0){soltar(t);soltar(t);return 0;}
  pthread_detach(fio);D.trabalho=t;
  if(op!=O_POLL)D.estado=V_OCUPADO;
  return 1;
}
static void aviso(const char *mensagem) {
  if(!D.repetir) {
    apagarMemoria(D.senha,sizeof D.senha);apagarMemoria(D.snapshot,D.n);free(D.snapshot);D.snapshot=NULL;D.n=0;
  }
  D.mensagem=mensagem;D.estado=*D.acesso?V_PRONTO:V_ENTRAR;D.foco=0;
}
static void erroLocal(int r) {
  printf("[backup] Drive operation failed: result=%d\n",r);
  aviso(r==DRIVE_LOCAL_SENHA?"Senha incorreta ou backup danificado. Nenhuma configuração foi alterada.":
        r==DRIVE_LOCAL_NEGADO?"O Google Drive não autorizou esta operação.":
        r==DRIVE_LOCAL_AUSENTE?"Este backup não está mais disponível.":
        r==DRIVE_LOCAL_REPETIR?"O Google Drive está ocupado. Tente novamente mais tarde.":
        "Não foi possível concluir a operação no Google Drive.");
}
static void autorizar(int escolher) {
  D.estado=V_OCUPADO;D.foco=0;
  D.mensagem="Autorize o Nuvio na sua conta Google.";
#ifdef NV_ANDROID
  if(D.telefone) {
    D.pedido=++pedidos;if(!D.pedido)D.pedido=++pedidos;
    if(!android_drive_autorizar((long long)D.pedido,escolher,escolher?"":D.acesso)) {
      D.pedido=0;D.repetir=0;aviso("O login Google não está disponível neste aparelho.");
    }
    return;
  }
#endif
  if(!iniciar(escolher?O_CODIGO:O_RENOVAR)){D.repetir=0;aviso("Não foi possível iniciar o login Google.");}
}
void drivebackup_abrir(void) {
  drivebackup_fechar();D.aberta=1;D.mensagem="Faça backup das configurações locais deste perfil.";
  if(!backupapp_contexto(&D.contexto)){D.mensagem="Entre na conta Nuvio antes de usar o backup.";return;}
  D.grupo=rede_grupo_criar();
#ifdef NV_ANDROID
  D.telefone=android_interface_mobile_auto();
#endif
  if(!D.telefone) {
    char *config=driveplatform_configuracao_tv(),*p=config?strchr(config,'\n'):NULL;
    if(p && (size_t)(p-config)<sizeof D.cliente && strlen(p+1)<sizeof D.segredo) {
      *p=0;snprintf(D.cliente,sizeof D.cliente,"%s",config);snprintf(D.segredo,sizeof D.segredo,"%s",p+1);
      apagarMemoria(config,strlen(config)+strlen(p+1)+1);free(config);
      if(!iniciar(O_CARREGAR))aviso("Não foi possível iniciar o login Google.");
    } else {if(config){apagarMemoria(config,strlen(config));free(config);}D.mensagem="O login Google não está configurado nesta versão.";}
  }
}
static void novaLista(void) {D.foco=0;D.offset=0;}
static void pedirSenha(int etapa) {
  D.estado=V_SENHA;D.senhaEtapa=etapa;
  teclado_abrir_com(etapa==2?"Confirme a senha do backup":etapa==3?"Senha deste backup":"Crie uma senha para o backup",
    etapa==3?"Digite a senha que você usou ao criar este backup.":
    "Guarde esta senha. Ela será necessária para restaurar em outro aparelho e não pode ser recuperada.",1024,
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !@#$%^&*()-_=+[]{};:,.<>/?\\|`~\"\'",NULL);
  teclado_tipo(TECLADO_TIPO_SENHA);teclado_mascarar(1);teclado_aceitar_utf8(1);
}
static void enviarSnapshot(void) {
  BackupLocal *snapshot=NULL;apagarMemoria(D.snapshot,D.n);free(D.snapshot);D.snapshot=NULL;D.n=0;
  if(!backupapp_capturar(&D.contexto,(long long)time(NULL),&snapshot) ||
     !backuplocal_exportar(snapshot,&D.snapshot,&D.n)) {backuplocal_liberar(snapshot);aviso("Não foi possível preparar o backup.");return;}
  backuplocal_liberar(snapshot);D.mensagem="Criptografando e salvando backup no Google Drive…";
  if(!iniciar(O_ENVIAR))erroLocal(DRIVE_LOCAL_REDE);
}
static void senhaResultado(void) {
  int r=teclado_resultado();if(r==TECLADO_NADA)return;
  int etapa=D.senhaEtapa;D.senhaEtapa=0;
  if(r!=TECLADO_PRONTO){teclado_esquecer();aviso("Google Drive conectado.");return;}
  const char *s=teclado_texto();
  if(etapa==1) {
    if(SDL_utf8strlen(s)<8){teclado_esquecer();aviso("Use pelo menos 8 caracteres na senha do backup.");return;}
    snprintf(D.senha,sizeof D.senha,"%s",s);teclado_esquecer();pedirSenha(2);return;
  }
  if(etapa==2) {
    int igual=!strcmp(D.senha,s);teclado_esquecer();
    if(!igual){aviso("As senhas não coincidem. Tente novamente.");return;}
    enviarSnapshot();return;
  }
  if(etapa==3) {
    snprintf(D.senha,sizeof D.senha,"%s",s);teclado_esquecer();
    D.mensagem="Descriptografando backup do Google Drive…";if(!iniciar(O_BAIXAR))erroLocal(DRIVE_LOCAL_REDE);
  }
}
static void retomar(void) {
  if(D.repetir) {int op=D.repetir;D.repetir=0;if(!iniciar(op))erroLocal(DRIVE_LOCAL_REDE);}
}
void drivebackup_atualizar(void) {
  if(!D.aberta)return;
  if(!contextoAtual()){drivebackup_fechar();return;}
  if(D.senhaEtapa) {
    if(teclado_aberto())teclado_atualizar(1.0f/60.0f,SDL_GetTicks());
    senhaResultado();return;
  }
#ifdef NV_ANDROID
  if(D.pedido) {
    char *s=android_drive_evento();
    if(s) {
      char status[40];double pedido=js_num(s,NULL,"request",0);
      if(pedido==(double)D.pedido) {
        D.pedido=0;
        if(js_texto_raiz(s,"status",status,sizeof status) && !strcmp(status,"ok") &&
           js_texto_raiz(s,"token",D.acesso,sizeof D.acesso) && *D.acesso) {
          aviso("Google Drive conectado.");retomar();
        } else {D.repetir=0;apagarMemoria(D.acesso,sizeof D.acesso);aviso("O login Google não foi concluído. Tente novamente.");}
      }
      apagarMemoria(s,strlen(s));free(s);
    }
  }
#endif
  Trabalho *t=D.trabalho;
  if(t && atomic_load_explicit(&t->pronto,memory_order_acquire)) {
    D.trabalho=NULL;int r=t->resultado;
    if(t->op==O_CARREGAR && r==0) {D.estado=V_ENTRAR;D.foco=0;}
    else if(t->op==O_CODIGO || t->op==O_POLL || t->op==O_RENOVAR || t->op==O_CARREGAR) {
      if(r==DRIVE_TV_OK && t->op==O_CODIGO){D.codigo=t->codigo;D.estado=V_CODIGO;D.foco=0;}
      else if(r==DRIVE_TV_AGUARDAR || (t->op==O_POLL && (r==DRIVE_TV_REPETIR || r==DRIVE_TV_REDE)))D.codigo=t->codigo;
      else if(r==DRIVE_TV_OK) {
        snprintf(D.acesso,sizeof D.acesso,"%s",t->token.acesso);snprintf(D.renovacao,sizeof D.renovacao,"%s",t->token.renovacao);
        aviso("Google Drive conectado.");
        if(!driveplatform_vinculo_gravar(D.contexto.usuario,D.contexto.perfil,D.cliente,D.renovacao))D.mensagem="Conectado nesta sessão. Não foi possível salvar o login Google.";
        retomar();
      } else {
        D.repetir=0;apagarMemoria(D.acesso,sizeof D.acesso);apagarMemoria(D.renovacao,sizeof D.renovacao);
        if(r==DRIVE_TV_EXPIRADO)driveplatform_vinculo_gravar(D.contexto.usuario,D.contexto.perfil,D.cliente,"");
        aviso(r==DRIVE_TV_CONFIGURACAO?"O login Google não está configurado nesta versão.":
              r==DRIVE_TV_NEGADO?"O Google Drive não autorizou esta operação.":
              r==DRIVE_TV_EXPIRADO?"O login Google expirou. Conecte novamente.":"Não foi possível iniciar o login Google.");
      }
    } else if(r==DRIVE_LOCAL_AUTORIZAR && !D.reautorizou && (D.telefone || *D.renovacao)) {
      D.reautorizou=1;D.repetir=t->op;autorizar(0);
    } else if(r!=DRIVE_LOCAL_OK)erroLocal(r);
    else if(t->op==O_ENVIAR) {apagarMemoria(D.snapshot,D.n);free(D.snapshot);D.snapshot=NULL;D.n=0;aviso("Backup salvo no seu Google Drive.");}
    else if(t->op==O_LISTAR) {D.lista=t->lista;D.estado=V_LISTA;novaLista();D.mensagem=D.lista.n?"Escolha um backup deste perfil.":"Nenhum backup encontrado para este perfil.";}
    else if(t->op==O_BAIXAR) {
      apagarMemoria(D.senha,sizeof D.senha);
      BackupLocal *portavel=NULL;
      int selecionado=backupapp_selecionar(&D.contexto,t->baixado,0,&portavel);
      int valido=selecionado && backupapp_validar(&D.contexto,portavel);
      printf("[backup] download decrypted; selection=%d validation=%d\n",selecionado,valido);
      backuplocal_liberar(portavel);
      if(valido) {
        backuplocal_liberar(D.restauro);D.restauro=t->baixado;t->baixado=NULL;D.hardware=0;D.estado=V_CONFIRMAR;novaLista();
        D.mensagem="Restaure as configurações locais deste perfil. Contas e dados salvos pelo Nuvio serão mantidos.";
      } else aviso("Este backup não é compatível com este perfil ou esta versão.");
    }
    soltar(t);
  }
  if(D.estado==V_CODIGO && !D.trabalho && SDL_GetTicks64()>=D.codigo.proximaMs)
    if(!iniciar(O_POLL))aviso("Não foi possível iniciar o login Google.");
}
static int linhas(void) {
  if(D.estado==V_ENTRAR)return 2;
  if(D.estado==V_PRONTO)return 5;
  if(D.estado==V_LISTA)return D.lista.n+(*D.lista.proxima?1:0)+1;
  if(D.estado==V_CONFIRMAR)return 3;
  return 1;
}
static void voltar(void) {
  if(D.estado==V_LISTA || D.estado==V_CONFIRMAR) {
    backuplocal_liberar(D.restauro);D.restauro=NULL;aviso("Google Drive conectado.");
  } else drivebackup_fechar();
}
static void ativar(int idx,int b) {
  (void)b;if(!D.aberta || !contextoAtual() || idx<0 || idx>=linhas())return;
  D.foco=idx;
  if(D.estado==V_ENTRAR){if(idx==0)autorizar(1);else voltar();return;}
  if(D.estado==V_PRONTO) {
    D.reautorizou=0;D.repetir=0;
    if(idx==0) {
      if(!backupcrypto_disponivel()){aviso("O backup criptografado não está disponível nesta versão.");return;}
      pedirSenha(1);
    } else if(idx==1){*D.pagina=0;D.mensagem="Buscando backups no Google Drive…";if(!iniciar(O_LISTAR))erroLocal(DRIVE_LOCAL_REDE);}
    else if(idx==2){apagarMemoria(D.acesso,sizeof D.acesso);apagarMemoria(D.renovacao,sizeof D.renovacao);autorizar(1);}
    else if(idx==3) {
      if(!driveplatform_vinculo_gravar(D.contexto.usuario,D.contexto.perfil,D.cliente,"")){aviso("Não foi possível remover o login Google deste aparelho.");return;}
      apagarMemoria(D.acesso,sizeof D.acesso);apagarMemoria(D.renovacao,sizeof D.renovacao);aviso("Google Drive desconectado neste aparelho.");
    } else voltar();
  } else if(D.estado==V_LISTA) {
    if(idx<D.lista.n){
      snprintf(D.id,sizeof D.id,"%s",D.lista.arquivo[idx].id);D.cifrado=D.lista.arquivo[idx].criptografado;
      if(D.cifrado)pedirSenha(3);
      else {D.mensagem="Baixando backup do Google Drive…";if(!iniciar(O_BAIXAR))erroLocal(DRIVE_LOCAL_REDE);}
    }
    else if(*D.lista.proxima && idx==D.lista.n){snprintf(D.pagina,sizeof D.pagina,"%s",D.lista.proxima);if(!iniciar(O_LISTAR))erroLocal(DRIVE_LOCAL_REDE);}
    else voltar();
  } else if(D.estado==V_CONFIRMAR) {if(idx==0)D.hardware=!D.hardware;else if(idx==1)D.confirmado=1;else voltar();}
  else voltar();
}
int drivebackup_restauro(BackupContexto *c,BackupLocal **out) {
  if(!c || !out || !D.confirmado)return 0;
  D.confirmado=0;*out=NULL;
  BackupLocal *selecionado=NULL;
  if(!contextoAtual() || !backupapp_selecionar(&D.contexto,D.restauro,D.hardware,&selecionado) || !backupapp_validar(&D.contexto,selecionado)) {
    backuplocal_liberar(selecionado);aviso("Este backup não é compatível com este perfil ou esta versão.");return 0;
  }
  *c=D.contexto;*out=selecionado;backuplocal_liberar(D.restauro);D.restauro=NULL;D.estado=V_OCUPADO;return 1;
}
void drivebackup_evento(const SDL_Event *e) {
  if(D.senhaEtapa && teclado_aberto()){teclado_evento(e);return;}
  if(e->type!=SDL_KEYDOWN)return;
#ifdef NV_TOUCH_UI
  if(toquerol_navegacao(e))toquerol_limpar(&D.toque);
#endif
  switch(e->key.keysym.sym) {
    case SDLK_UP:if(D.foco>0)D.foco--;break;
    case SDLK_DOWN:if(D.foco+1<linhas())D.foco++;break;
    case SDLK_RETURN:case SDLK_KP_ENTER:if(!e->key.repeat)ativar(D.foco,0);break;
    case SDLK_ESCAPE:case SDLK_AC_BACK:case SDLK_BACKSPACE:voltar();break;
    default:break;
  }
}
static void foco(int i,int b){(void)b;D.foco=i;}
#ifdef NV_TOUCH_UI
static int rolar(const PonteiroRolagem *e){return toquerol_evento(&D.toque,e);}
#endif
void drivebackup_desenhar(void) {
  ESCALA_INI();float w=NV_VTELA_W,h=NV_VTELA_H,x=48,larg=w-96,y=320,base=h-40;
  gfx_cor((GfxRect){0,0,w,h},0,.055f,.06f,.075f,1);
  txt_desenhar(txt_linha_corta(TXT_TITULO2,i18n("Backup no Google Drive"),255,255,255,255,larg),x,48);
  const ContaPerfil *p=perfis_item_ativo();
  char perfil[160];snprintf(perfil,sizeof perfil,"%s · %s",i18n("Perfil"),p?p->nome:"");
  txt_desenhar(txt_linha_corta(TXT_BODY,perfil,190,195,205,255,larg),x,110);
  float textoAltura=txt_bloco_corta(TXT_BODY,i18n(D.mensagem?D.mensagem:""),205,210,220,x,164,larg,1.2f,1,3);
  y=164+textoAltura+40;
  if(D.estado==V_CODIGO) {
    txt_desenhar(txt_linha_corta(TXT_TITULO2,D.codigo.codigo,255,255,255,255,larg),x,y);
    txt_desenhar(txt_linha_corta(TXT_BODY,D.codigo.url,210,215,230,255,larg),x,y+70);y+=140;
  } else if(D.estado==V_CONFIRMAR) {
    y+=txt_bloco_corta(TXT_CAPTION,i18n("As conexões existentes serão preservadas. Ajustes de hardware só serão aplicados se você os selecionar."),175,180,195,x,y,larg,1.2f,1,3)+32;
  }
  int n=linhas();float altura=76,visivel=base-y;
  if(visivel<altura)visivel=altura;
  float max=n*altura-visivel;if(max<0)max=0;
  int livre=0;
#ifdef NV_TOUCH_UI
  livre=toquerol_livre(&D.toque);
#endif
  if(!livre){if(D.foco*altura<D.offset)D.offset=D.foco*altura;if((D.foco+1)*altura>D.offset+visivel)D.offset=(D.foco+1)*altura-visivel;}
  if(D.offset<0)D.offset=0;
  if(D.offset>max)D.offset=max;
  ponteiro_camada();
#ifdef NV_TOUCH_UI
  toquerol_vincular(&D.toque,(GfxRect){x,y,larg,visivel},gfx_escala(),0,max,1,&D.offset);ponteiro_rolagem(rolar);
#endif
  for(int i=0;i<n;i++) {
    float ly=y+i*altura-D.offset;if(ly<y || ly+altura>base)continue;
    const char *label="Cancelar";char data[256];
    if(D.estado==V_ENTRAR)label=i==0?"Conectar Google Drive":"Voltar";
    else if(D.estado==V_PRONTO){const char *botoes[]={"Fazer backup agora","Restaurar backup","Trocar conta Google","Desconectar","Voltar"};label=botoes[i];}
    else if(D.estado==V_CONFIRMAR)label=i==0?(D.hardware?"Incluir ajustes de hardware: Sim":"Incluir ajustes de hardware: Não"):i==1?"Restaurar configurações":"Cancelar";
    else if(D.estado==V_LISTA) {
      if(i<D.lista.n) {
        time_t t=(time_t)(D.lista.arquivo[i].criadoMs/1000);struct tm tm;
        if(localtime_r(&t,&tm))strftime(data,sizeof data,"%Y-%m-%d %H:%M",&tm);else snprintf(data,sizeof data,"%s",D.lista.arquivo[i].nome);
        label=data;
      } else label=(*D.lista.proxima && i==D.lista.n)?"Mais backups":"Voltar";
    }
    int tinta=255;
    if(i==D.foco) {
      float r,g,b;ajustes_acento(&r,&g,&b);
      gfx_cor((GfxRect){x,ly,larg,altura-8},14.0f/(altura-8),r,g,b,1);
      tinta=ajustes_tinta_foco();
    }
    txt_desenhar(txt_linha_corta(TXT_BODY,D.estado==V_LISTA && i<D.lista.n?label:i18n(label),tinta,tinta,tinta,255,larg-32),x+16,ly+18);
    ponteiro_alvo(x,ly,larg,altura-8,foco,ativar,i,0);
  }
  ESCALA_FIM();
  if(D.senhaEtapa && teclado_aberto())teclado_desenhar(SDL_GetTicks());
}
