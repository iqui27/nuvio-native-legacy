// STUB provisorio: compila e responde "vazio". Substituido pela implementacao.
#include "diario.h"

void diario_iniciar(void) {}
void diario_passo(void) {}
void diario_esquecer(void) {}
void diario_gravar_agora(void) {}
int diario_adicionar(int perfil, const DiarioEntrada *e) { return 0; }
int diario_adicionar_lote(int perfil, const DiarioEntrada *v, int n) { return 0; }
int diario_definir_nota(int perfil, const char *id, int t, int e, long long quando, int nota10) { return 0; }
int diario_definir_flags(int perfil, const char *id, int t, int e, long long quando, unsigned ligar, unsigned desligar) { return 0; }
int diario_definir_tags(int perfil, const char *id, int t, int e, long long quando, unsigned tags) { return 0; }
int diario_definir_data(int perfil, const char *id, int t, int e, long long quando, long long novo) { return 0; }
int diario_apagar(int perfil, const char *id, int t, int e, long long quando) { return 0; }
int diario_avaliar(int perfil, const char *id, char tipo, int nota10) { return 0; }
int diario_nota_da_reacao(int reacao) { return 0; }
int diario_n(int perfil) { return 0; }
int diario_n_titulos(int perfil) { return 0; }
int diario_entrada(int perfil, int i, DiarioEntrada *saida) { return 0; }
int diario_titulo(int perfil, const char *id, DiarioTitulo *saida) { return 0; }
int diario_titulo_i(int perfil, int i, DiarioTitulo *saida) { return 0; }
int diario_episodio(int perfil, const char *id, int t, int e, long long *quando) { return 0; }
int diario_ultimo_episodio(int perfil, const char *id, int *t, int *e, long long *quando) { return 0; }
int diario_por_titulo(int perfil, const char *id, DiarioEntrada *saida, int max) { return 0; }
int diario_por_periodo(int perfil, long long de, long long ate, DiarioEntrada *saida, int max) { return 0; }
int diario_por_lista(int perfil, const char *const *ids, int n, DiarioTitulo *saida) { return 0; }
int diario_percorrer(int perfil, const DiarioFiltro *f, DiarioVisita cb, void *ctx) { return 0; }
unsigned diario_geracao(int perfil) { return 0; }
int diario_ouvir(DiarioOuvinte fn, void *ctx) { return 0; }
void diario_parar_de_ouvir(DiarioOuvinte fn, void *ctx) {}
int diario_nuvem_estado(int perfil) { return 0; }
void diario_nuvem_definir(int perfil, int estado) {}
int diario_nuvem_pendentes(int perfil) { return 0; }
int diario_nuvem_ativa(int perfil) { return 0; }
int diario_regra_mesma_vez(const DiarioEntrada *a, const DiarioEntrada *b) { return 0; }
int diario_regra_confianca(char origem) { return 0; }
size_t diario_linha(const DiarioEntrada *e, char *dst, size_t tam) { return 0; }
int diario_ler_linha(const char *linha, DiarioEntrada *e) { return 0; }
void diario_separar_id(const char *src, char *id, size_t tam, int *t, int *e) {}
size_t diario_csv_letterboxd(const DiarioEntrada *e, const char *titulo, int ano, int cabecalho, char *dst, size_t tam) { return 0; }
