#ifndef NV_M3U_H
#define NV_M3U_H
#include <stdint.h>
typedef struct {
  char id[80], nome[140], logo[480], categoria[64], epgId[64];
  char url[4096], cabecalhos[512];
} M3uCanal;
// Extended playlist parser; modifies its buffer, no network or preferences.
int m3u_processar(char *texto, const char *origem, M3uCanal *saida, int max,
                  char *epg, unsigned nEpg, int *total);
int m3u_configurado(void);
const char *m3u_lista(void);
const char *m3u_epg(void);
int m3u_definir_lista(const char *url);
int m3u_definir_epg(const char *url);
int m3u_conta_definir(const char *lista,const char *epg);
int m3u_url_valida(const char *url);
void m3u_esquecer(void);
void m3u_esquecer_todos(void);
int m3u_url_epg(char *saida,unsigned n);
int m3u_canais_sessao(M3uCanal *saida,int max,uint64_t sessao);
int m3u_ultimo_total(void);
int m3u_e_id(const char *id);
int m3u_resolver_sessao(const char *id,char *url,unsigned n,uint64_t sessao,int renovar);
int m3u_cabecalhos(const char *id,char *saida,unsigned n);
void m3u_recarregar(void);
void m3u_fonte_selecionar(unsigned id);
int m3u_fonte_configurado(unsigned id);
int m3u_fonte_canais(unsigned id,M3uCanal *out,int max,uint64_t sessao);
int m3u_fonte_urls(unsigned id,char *lista,unsigned nLista,char *epg,unsigned nEpg);
int m3u_fonte_epg(unsigned id,char *out,unsigned n);
int m3u_fonte_total(unsigned id);
void m3u_fonte_recarregar(unsigned id);
void m3u_fonte_esquecer(unsigned id);
void m3u_fontes_invalidar(void);
#endif
