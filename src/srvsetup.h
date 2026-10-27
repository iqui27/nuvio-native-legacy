#ifndef NV_SRVSETUP_H
#define NV_SRVSETUP_H
#include <SDL2/SDL.h>
#include "jellyfin.h"
#define SRV_SETUP_MAX 12
// Protocol helpers block on workers only. All arrays are bounded.
typedef struct { char nome[96], url[512], id[64]; } SrvRede;
typedef struct { char nome[96], id[40], imagem[640]; int senha; } SrvUsuario;
typedef struct { char nome[96], id[64], local[512], remoto[512], chave[128]; } EmConnectServidor;
int srv_descobrir(int provider, RedeJob *job, SrvRede *out, int max);
int srv_ler_descoberta(int provider, const char *body, SrvRede *out);
int srv_usuarios(const JfConta *c, RedeJob *job, SrvUsuario *out, int max);
int em_connect_pedir(const char *path, const char *body, const char *token, RedeJob *job, RedeResposta *r);
int em_connect_ler_conta(const char *body, char *token, size_t nt, char *user, size_t nu);
int em_connect_servidores(const char *token, const char *user, RedeJob *job, EmConnectServidor *out, int max);
int em_connect_trocar(JfConta *c, const EmConnectServidor *server, const char *user, RedeJob *job);
void srvsetup_abrir(int provider); // -1 choose service, -2 manage
void srvsetup_abrir_codigo(int provider); // Explicit phone-code route from Settings.
void srvsetup_fechar(void);
int srvsetup_aberto(void);
void srvsetup_evento(const SDL_Event *e);
void srvsetup_atualizar(Uint32 agora);
void srvsetup_desenhar(Uint32 agora);
void srvsetup_ligar_biblioteca(void);
#endif
