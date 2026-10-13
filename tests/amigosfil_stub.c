// A FILEIRA DE AMIGOS (src/amigosfil.c) DE MENTIRA, para os testes que
// compilam home.c com uma lista curta de fontes (home.sh, homepos.sh,
// cwordem.sh...). Sem ela o link falha: home.c chama amigosfil_* desde o
// redesenho do social (02/10/2026). Zero amigos = a fileira e o convite.
#include "amigosfil.h"
#include <SDL2/SDL.h>
// Account row projection belongs to separate integration tests. A strong
// implementation overrides this double in tests/colfileiras_home.c.
__attribute__((weak)) void colfileiras_sincronizar(void) {}
__attribute__((weak)) void selospacote_conta_do_blob(const char *blob) { (void)blob; }
__attribute__((weak)) int selospacote_n(void) { return 0; }
// homefil.c (hide-watched, pinned collections) persists through dados_* and keys
// per profile; these tests have no disk, so the doubles read nothing and write nowhere.
__attribute__((weak)) char *dados_ler(const char *nome) { (void)nome; return NULL; }
__attribute__((weak)) int dados_gravar(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 1; }
__attribute__((weak)) int perfis_ativo(void) { return 1; }
// home.c opens the row menu (hold OK on a row); ctxmenu.c is not linked into these tests.
__attribute__((weak)) void ctx_abrir_filmenu(const char *chave, const char *titulo, const char *pastaId, int comVistos) {
  (void)chave; (void)titulo; (void)pastaId; (void)comVistos;
}
// home.c reads the touch long-press flag; ponteiro.c is not linked into these tests.
__attribute__((weak)) int ponteiro_ok_longo(void) { return 0; }
int  amigosfil_n_colunas(void) { return 1; }
int  amigosfil_convite(void) { return 1; }
int  amigosfil_indice_cat(int coluna) { (void)coluna; return -1; }
int  amigosfil_tecla(SDL_Keycode k, int *coluna) { (void)k; (void)coluna; return 0; }
void amigosfil_ok(int coluna) { (void)coluna; }
void amigosfil_atualizar(float dt, int focada, int *coluna) { (void)dt; (void)focada; (void)coluna; }
void amigosfil_desenhar(float x0, float y, float alt, float corte, int focada, int coluna,
                        Uint32 agora) {
  (void)x0; (void)y; (void)alt; (void)corte; (void)focada; (void)coluna; (void)agora;
}
int  amigosfil_pediu_titulo(char *imdb, size_t tam) { (void)imdb; (void)tam; return 0; }
int  amigosfil_pediu_perfil(char *id, size_t tam) { (void)id; (void)tam; return 0; }
int  amigosfil_pediu_ajustes(void) { return 0; }
int  amigosfil_dentro(void) { return -1; }
