#include "driveplatform.h"
#include "drivevinculo.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NV_ANDROID
#include "android.h"
#endif
#ifndef NV_GOOGLE_TV_CLIENT_ID
#define NV_GOOGLE_TV_CLIENT_ID ""
#endif
#ifndef NV_GOOGLE_TV_CLIENT_SECRET
#define NV_GOOGLE_TV_CLIENT_SECRET ""
#endif

char *driveplatform_configuracao_tv(void) {
#ifdef NV_ANDROID
  return android_drive_configuracao_tv();
#elif defined(NV_DRIVE_UI)
  const char *id=NV_GOOGLE_TV_CLIENT_ID,*secret=NV_GOOGLE_TV_CLIENT_SECRET;
  if(!*id || !*secret || strlen(id)>512 || strlen(secret)>4096 || strchr(id,'\n') || strchr(secret,'\n'))return NULL;
  size_t n=strlen(id)+strlen(secret)+2;char *out=malloc(n);
  if(out)snprintf(out,n,"%s\n%s",id,secret);
  return out;
#else
  return NULL;
#endif
}
char *driveplatform_vinculo_ler(const char *dono,int perfil,const char *cliente) {
#ifdef NV_ANDROID
  (void)cliente;return android_drive_vinculo_ler(dono,perfil);
#else
  return drivevinculo_ler(dados_dir(),dono,perfil,cliente);
#endif
}
int driveplatform_vinculo_gravar(const char *dono,int perfil,const char *cliente,const char *token) {
#ifdef NV_ANDROID
  (void)cliente;return android_drive_vinculo_gravar(dono,perfil,token);
#else
  dados_fs_travar();int ok=drivevinculo_gravar(dados_dir(),dono,perfil,cliente,token);dados_fs_liberar();
  if(ok){dados_marcar_sujo(0);dados_sincronizar_logo();}
  return ok;
#endif
}
