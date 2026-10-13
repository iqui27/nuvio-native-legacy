#!/usr/bin/env python3
import pathlib
import subprocess
import tempfile
src = pathlib.Path('src/biblioteca.c').read_text()
a = src.index('static void pollEscrita(void) {')
b = src.index('\n}', a) + 2
code = r'''
#include "listas.h"
#include "listasescrita.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int escrevendo, temAberta = 1, modo, fonte;
static LstLista aberta;
static char abertaMidia[8], descNova[192] = "Description A";
enum { MODO_LISTAS, FONTE_TRAKT };
static int result = LSTESC_OK, operation = LSTESC_DESCRICAO, reloads;
int lstesc_estado(void) { return result; }
int lstesc_qual(void) { return operation; }
long lstesc_lista_id(void) { return 77; }
int lstesc_consumir(void) { return result; }
static void abrirStub(const LstLista *l, const char *m) { (void)l; (void)m; reloads++; }
static void minhasStub(void) {}
#define lst_abrir abrirStub
#define lst_minhas_pedir minhasStub
static void pedirFonte(void) {}
static void remapear(int x) { (void)x; }
static void dizer(const char *s) { (void)s; }
'''
code += src[a:b]
a = src.index('static void abrirLista(int i) {')
b = src.index('\n}', a) + 2
code += '\nstatic int abertaPropria, edModo, ordemPega, ordemMudou, acaoSel;\n'
code += 'enum { ED_NADA }; static char recado[160]; static struct { int fileira, coluna; } foco;\n'
code += src[a:b]
code += r'''
int main(void) {
  aberta.fonte = LST_TRAKT; aberta.traktId = 88; strcpy(aberta.descricao, "Description B");
  escrevendo = 1; pollEscrita(); assert(!strcmp(aberta.descricao, "Description B"));
  operation = LSTESC_ORDENAR; escrevendo = 1; pollEscrita(); assert(!reloads);
  result = LSTESC_FALHA; escrevendo = 1; pollEscrita(); assert(!reloads);
  assert(lst_ler_trakt("[{\"name\":\"A\",\"ids\":{\"trakt\":77},\"description\":\"Old\"},"
                       "{\"name\":\"B\",\"ids\":{\"trakt\":88},\"description\":\"Description B\"}]", 0) == 2);
  lst_minhas_teste("[{\"name\":\"A\",\"ids\":{\"trakt\":77},\"description\":\"Old\"}]");
  result = LSTESC_OK; operation = LSTESC_DESCRICAO; abrirLista(0);
  escrevendo = 1; pollEscrita(); assert(!strcmp(aberta.descricao, "Description A"));
  LstLista own; assert(lst_minha(0, &own) && !strcmp(own.descricao, "Description A"));
  temAberta = 0; abrirLista(0); assert(!strcmp(aberta.descricao, "Description A"));
  abrirLista(1); assert(!strcmp(aberta.descricao, "Description B"));
  escrevendo = 1; pollEscrita();
  abrirLista(0); assert(!strcmp(aberta.descricao, "Description A"));
  result = LSTESC_FALHA; strcpy(descNova, "Rejected"); escrevendo = 1; pollEscrita();
  abrirLista(0); assert(!strcmp(aberta.descricao, "Description A"));
  result = LSTESC_OK; descNova[0] = 0; escrevendo = 1; pollEscrita();
  abrirLista(0); assert(!aberta.descricao[0]);
}
'''
# A delayed real PUT must retain A when the editor's second attempt B is refused.
race = r'''
#include "listas.h"
#include "listasescrita.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int escrevendo, temAberta = 1, modo, fonte;
static LstLista aberta;
static char abertaMidia[8], descNova[192];
static long notaItemId;
enum { MODO_LISTAS, FONTE_TRAKT, TEC_BUSCA, TEC_NOVA, TEC_DESC, TEC_NOTA };
static void dizer(const char *s) { (void)s; }
static void remapear(int x) { (void)x; }
static void pedirFonte(void) {}
static void abrirStub(const LstLista *l, const char *m) { (void)l; (void)m; }
static void minhasStub(void) {}
#define lst_abrir abrirStub
#define lst_minhas_pedir minhasStub
void desc_vertudo_invalidar_lista(long id) { assert(id == 77); }
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int entered, released;
static char serverBody[900];
static int sendFixture(const char *method, const char *url, const char *body, char **resp) {
  assert(!strcmp(method, "PUT")); assert(!strcmp(url, "https://api.trakt.tv/users/me/lists/77"));
  pthread_mutex_lock(&gate);
  snprintf(serverBody, sizeof serverBody, "%s", body);
  entered = 1; pthread_cond_signal(&changed);
  while (!released) pthread_cond_wait(&changed, &gate);
  pthread_mutex_unlock(&gate);
  *resp = strdup(""); return 200;
}
'''
for name in ('tecladoPronto', 'pollEscrita'):
    a = src.index('static void ' + name + '(')
    b = src.index('\n}', a) + 2
    race += src[a:b] + '\n'
race += r'''
int main(void) {
  const char *list = "[{\"name\":\"Own\",\"ids\":{\"trakt\":77},\"description\":\"Old\"}]";
  assert(lst_ler_trakt(list, 0) == 1); lst_minhas_teste(list); aberta = *lst_lista(0);
  lstesc_teste_envio(sendFixture);
  tecladoPronto(TEC_DESC, "Accepted A"); assert(escrevendo);
  pthread_mutex_lock(&gate);
  while (!entered) pthread_cond_wait(&changed, &gate);
  pthread_mutex_unlock(&gate);
  pollEscrita(); assert(!strcmp(aberta.descricao, "Old"));
  tecladoPronto(TEC_DESC, "Rejected B");
  assert(lstesc_estado() == LSTESC_PENDENTE);
  assert(!strcmp(serverBody, "{\"description\":\"Accepted A\"}"));
  assert(!strcmp(descNova, "Accepted A"));
  pthread_mutex_lock(&gate); released = 1; pthread_cond_signal(&changed); pthread_mutex_unlock(&gate);
  lstesc_teste_esperar(); pollEscrita();
  assert(!escrevendo && !strcmp(aberta.descricao, "Accepted A"));
  assert(!strcmp(lst_lista(0)->descricao, "Accepted A"));
  LstLista own; assert(lst_minha(0, &own) && !strcmp(own.descricao, "Accepted A"));
}
'''
with tempfile.TemporaryDirectory(prefix='biblioteca-escrita-') as tmp:
    p = pathlib.Path(tmp)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                    '-ffunction-sections', '-fdata-sections', 'src/listas.c', 'src/js.c',
                    str(p / 'test.c'), '-Wl,-dead_strip', '-Wl,-undefined,dynamic_lookup',
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
    (p / 'race.c').write_text(race)
    subprocess.run(['cc', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                    '-ffunction-sections', '-fdata-sections', 'src/listas.c', 'src/listasescrita.c', 'src/js.c',
                    str(p / 'race.c'), '-Wl,-dead_strip', '-Wl,-undefined,dynamic_lookup',
                    '-o', str(p / 'race')], check=True)
    subprocess.run([str(p / 'race')], check=True)
print('biblioteca late response and rejected description: PASS')
