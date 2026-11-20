#include "catalogo.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "rewatch.h"
#define NV_DUBLE_SESSAO
#include "perfisessao.h"
__attribute__((weak)) int rewatch_n_ativas(void) { return 0; }
__attribute__((weak)) int rewatch_ativa_n(int i, char *id, size_t n) { (void)i; if(n) *id=0; return 0; }
__attribute__((weak)) int rewatch_alvo(const char *id, const CatEp *eps, int n, int *t, int *e, int *pct) { (void)id;(void)eps;(void)n;(void)t;(void)e;(void)pct;return 0; }
__attribute__((weak)) int rewatch_parcial_de(const char *id, int *t, int *e, int *pct) { (void)id;(void)t;(void)e;(void)pct;return 0; }
// Fileira da caca (caca.h): descoberta.c chama para fontes "caca"; os testes
// isolados nao tem caca.c, entao nenhuma fonte responde e a fileira fica vazia.
__attribute__((weak)) int caca_por_chave(const char *chave) { (void)chave; return -1; }
__attribute__((weak)) int caca_itens(int i, int soFaltam, void *dst, int max) { (void)i;(void)soFaltam;(void)dst;(void)max; return 0; }
__attribute__((weak)) int cat_indice_titulo(const char *id, int i) { (void)id;(void)i;return -1; }
__attribute__((weak)) const CatEp *cat_episodio(int i, int e) { (void)i;(void)e;return NULL; }
__attribute__((weak)) int ajustes_fonte_biblioteca(void) { return 0; }
__attribute__((weak)) int ajustes_nota_episodio(int f) { (void)f;return 1; }
__attribute__((weak)) int ajustes_ocultar_nao_lancados(void) { return 0; }
// homeestado.c tem fil_n/fil_chave; aqui o fixture nao monta a home inteira.
__attribute__((weak)) int fil_n(void) { return 0; }
__attribute__((weak)) const char *fil_chave(int i) { (void)i; return ""; }
__attribute__((weak)) char *dados_caminho(char *dst, unsigned n, const char *nome) { snprintf(dst,n,"/tmp/nuvio-fixture-%s",nome); return dst; }
__attribute__((weak)) int metaep_tvdb_video(const char *video, const char *fim) { (void)video;(void)fim;return 0; }
__attribute__((weak)) char *rede_baixar_st(const char *u,int t,const char *const *c,int *st) { (void)u;(void)t;(void)c;if(st)*st=0;return NULL; }
// MDBList (#762): sem extras.c no fixture, a chave nunca existe — desc_colecao_inicial
// trata como lista nao pronta, como num aparelho sem a chave configurada.
__attribute__((weak)) int extras_mdblist_chave(char *dst, unsigned n) { (void)dst;(void)n;return 0; }
