// Drive deferred callbacks deterministically, without OAuth/account/network.
#include <assert.h>
#include "nvtmp.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void *(*job)(void *);
static void *jobArg;
static int fakeCreate(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
  (void)a; assert(!job); *t = pthread_self(); job = fn; jobArg = arg; return 0;
}
static int fakeDetach(pthread_t t) { (void)t; return 0; }
#define pthread_create fakeCreate
#define pthread_detach fakeDetach
#define NV_DISCORD_CLIENT_ID "123456"
#include "../src/discord.c"
#undef pthread_create
#undef pthread_detach
static int activeProfile = 1, httpStatus, calls, saves, deletes, video;
static const char *httpReply;
static char posted[1600], authHeader[300], savedName[40];
static CatItem item;
const char *i18n(const char *s) { return s; }
int perfis_ativo(void) { return activeProfile; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *b) {
  (void)b; saves++; snprintf(savedName, sizeof savedName, "%s", n); return 1;
}
char *dados_caminho(char *dst, unsigned n, const char *nome) {
  (void)nome; snprintf(dst,n,"%s/nuvio-discord-permissions-%ld", nvt_dir(),(long)getpid()); return dst;
}
int dados_apagar(const char *n) { (void)n; deletes++; return 1; }
char *rede_postar_seguro_st(const char *u, int sec, const char *const *cab, const char *b, int *st) {
  (void)u; (void)sec; calls++; *st = httpStatus;
  snprintf(posted, sizeof posted, "%s", b);
  authHeader[0] = 0;
  for (int i=0; cab && cab[i]; i++) if (!strncmp(cab[i], "Authorization:", 14))
    snprintf(authHeader, sizeof authHeader, "%s", cab[i]);
  return httpReply ? strdup(httpReply) : NULL;
}
struct DiscordWs { int dummy; };
DiscordWs *dws_abrir(const char *u) { (void)u; return NULL; }
int dws_estado(DiscordWs *w) { (void)w; return DWS_CAIU; }
int dws_codigo_fechamento(DiscordWs *w) { (void)w; return 0; }
int dws_enviar(DiscordWs *w, const char *t) { (void)w; (void)t; return 0; }
char *dws_receber(DiscordWs *w) { (void)w; return NULL; }
void dws_fechar(DiscordWs *w) { free(w); }
int player_com_video(void) { return video; }
int player_indice(void) { return 0; }
const CatItem *cat_item(int i) { (void)i; return &item; }
const char *player_linha_episodio(void) { return ""; }
int player_eh_canal(void) { return 0; }
int player_pausado(void) { return 0; }
float player_posicao_seg(void) { return 100; }
float player_duracao_seg(void) { return 7200; }
static void runJob(void) { void *(*fn)(void *) = job; void *arg = jobArg; assert(fn); job = NULL; jobArg = NULL; fn(arg); }
static void linked(void) {
  pthread_mutex_lock(&trava); estado = DIS_LIGADO;
  strcpy(token, "profile-token"); strcpy(refresh, "refresh+&opaque"); expiraEm = agoraSeg()+7200;
  pthread_mutex_unlock(&trava);
}
int main(void) {
  discord_passo(1); assert(perfil == 1 && discord_estado() == DIS_PARADO);
  // Cancelling while authorize is pending must ignore its later success.
  httpStatus = 200;
  httpReply = "{\"device_code\":\"opaque+&value\",\"user_code\":\"CODE\",\"verification_uri_complete\":\"https://discord.com/activate\",\"expires_in\":300,\"interval\":5}";
  discord_comecar(); assert(discord_estado() == DIS_PEDINDO && job);
  discord_cancelar(); runJob(); assert(discord_estado() == DIS_PARADO && !discord_codigo()[0]);
  discord_comecar(); runJob(); assert(discord_estado() == DIS_AGUARDANDO);
  const char *snapshot = discord_codigo(); assert(!strcmp(snapshot, "CODE"));
  discord_cancelar(); assert(!strcmp(snapshot, "CODE")); assert(!discord_codigo()[0]);
  // A token arriving after a profile switch may neither link nor persist.
  discord_comecar(); runJob(); discord_passo(100); assert(job);
  activeProfile = 2; discord_passo(101); assert(perfil == 2 && !discord_codigo()[0]);
  httpReply = "{\"access_token\":\"old-profile-token\",\"refresh_token\":\"old-refresh\",\"expires_in\":7200}";
  runJob(); assert(discord_estado() == DIS_PARADO && saves == 0);
  assert(strstr(posted, "opaque%2B%26value"));
  // Other 400 errors preserve the link; invalid_grant invalidates it.
  linked(); expiraEm = agoraSeg()+10; httpStatus = 400; httpReply = "{\"error\":\"invalid_request\"}";
  discord_passo(1000); assert(job); runJob(); assert(discord_estado() == DIS_LIGADO);
  assert(strstr(posted, "refresh%2B%26opaque"));
  httpReply = "{\"error\":\"invalid_grant\"}"; discord_passo(62000); runJob();
  assert(discord_estado() == DIS_INVALIDO && !token[0]);
  // An unlinked profile cannot be re-linked by a late refresh callback.
  linked(); expiraEm = agoraSeg()+10; discord_passo(123000); assert(job);
  discord_esquecer(); httpStatus = 200; httpReply = "{\"access_token\":\"late\",\"expires_in\":7200}";
  int saved = saves; runJob(); assert(discord_estado() == DIS_PARADO && saves == saved && deletes == 1);
  // Do not send a prior profile's artwork using the new profile's token.
  linked(); video = 1; strcpy(item.titulo, "Fixture"); strcpy(item.poster, "https://image.tmdb.org/t/p/w500/public.jpg");
  discord_passo(124000); assert(job && arteVivo);
  activeProfile = 3; discord_passo(124001); int before = calls;
  runJob(); assert(calls == before && !arteVivo && !arteMp[0]);
  // If title changes during artwork I/O, retry the new title after completion.
  linked(); discord_passo(125000); assert(job);
  strcpy(item.poster, "https://image.tmdb.org/t/p/w500/second.jpg"); discord_passo(125001);
  httpReply = "[{\"external_asset_path\":\"external/old\"}]"; runJob(); assert(!arteMp[0]);
  discord_passo(125002); assert(job && strstr(((Pedido *)jobArg)->a, "second.jpg"));
  httpReply = "[{\"external_asset_path\":\"external/new\"}]"; runJob(); assert(!strcmp(arteMp, "mp:external/new"));
  assert(!strcmp(authHeader, "Authorization: Bearer profile-token"));
  assert(posterPublico("https://images.metahub.space/poster/medium/tt1/img"));
  assert(!posterPublico("https://images.metahub.space.evil/image"));
  assert(!posterPublico("https://image.tmdb.org/image?api_key=private"));
  assert(!posterPublico("https://addon.example/private-key/poster"));
  // Shutdown invalidates pending authorization callbacks too.
  video = 0; discord_esquecer(); discord_comecar(); assert(job);
  discord_encerrar(); runJob(); assert(discord_estado() == DIS_PARADO && !discord_codigo()[0]);
  // Existing legacy token files are restricted before reading them.
  char caminho[600]; struct stat sb; dados_caminho(caminho,sizeof caminho,"fixture");
  FILE *f = fopen(caminho,"w"); assert(f); fclose(f); chmod(caminho,0644);
  restringirArquivo("fixture"); assert(stat(caminho,&sb)==0 && (sb.st_mode&0777)==0600);
  unlink(caminho);
  puts("discord_lifecycle: ok"); return 0;
}
