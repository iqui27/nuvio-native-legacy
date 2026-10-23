#ifndef NV_DRIVEVINCULO_H
#define NV_DRIVEVINCULO_H
// Installation-private refresh credential, never a settings-backup section.
// NULL means absent/invalid; the caller wipes and frees a returned token.
char *drivevinculo_ler(const char *dir,const char *dono,int perfil,const char *cliente);
int drivevinculo_gravar(const char *dir,const char *dono,int perfil,const char *cliente,const char *token);
#endif
