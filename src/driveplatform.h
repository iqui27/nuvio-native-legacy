#ifndef NV_DRIVEPLATFORM_H
#define NV_DRIVEPLATFORM_H

// Phones use Google's Android authorization API. Limited-input TVs use the
// same device-code protocol regardless of their operating system.
#if defined(NV_ANDROID) || defined(NV_WEBOS) || defined(NV_TPK) || (defined(__EMSCRIPTEN__) && !defined(NV_VIDAA))
#define NV_DRIVE_UI 1
#endif

char *driveplatform_configuracao_tv(void);
char *driveplatform_vinculo_ler(const char *dono,int perfil,const char *cliente);
int driveplatform_vinculo_gravar(const char *dono,int perfil,const char *cliente,const char *token);
#endif
