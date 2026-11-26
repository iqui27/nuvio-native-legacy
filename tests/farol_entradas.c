// Teste dirigido da issue #23 do Farol (Entradas e batismo + FAROL27-r1):
// - Batismo ("Farol", nunca "Resolver um problema") em Ajustes, Guia e v2;
// - Busca de Ajustes ("farol", "problema", "erro", "registro", "beacon") e OK;
// - Navegacao real por D-pad + OK em Ajustes > Sobre e ajuda > Farol
//   nos layouts Palco (tri) e Lista, com Avisos do Farol = 0 e 1;
// - Capitulo "Farol" do Guia do app e abertura de resolver_abrir();
// - Comando "farol" via /tmp/nuvio-key (quando compilado com -DFAROL_TESTE_KEY).
#ifndef AJUSTES_TESTE
#define AJUSTES_TESTE 1
#endif
#ifndef RESOLVER_TESTE
#define RESOLVER_TESTE 1
#endif

#ifdef FAROL_TESTE_KEY
#define main nuvio_app_main
#include "../src/main.c"
#undef main
#include "../src/dados.h"
#include "../src/resolver.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
  char dir[512];
  struct stat st;
  FILE *f;
  assert(argc == 2);
  snprintf(dir, sizeof dir, "%s", argv[1]);
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  resolver_teste_esquecer();
  assert(resolver_aberto() == 0);

  f = fopen("/tmp/nuvio-key", "w");
  assert(f != NULL);
  fprintf(f, "farol\n");
  fclose(f);

  teclasInjetadas(app_evento);
  assert(resolver_aberto() == 1);
  assert(stat("/tmp/nuvio-key", &st) == 0 && st.st_size == 0);
  unlink("/tmp/nuvio-key");
  resolver_teste_esquecer();
  puts("farol_entradas_key: PASS");
  return 0;
}

#else

#include "../src/ajustes.c"
#include "../src/faroltexto.h"
#include "../src/resolver.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void tecla(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ajustes_evento(&e);
}

static void teclaGuia(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  guiaEvento(&e);
}

static int buscarFarol(const char *consulta, AjusteBuscaResultado *out, int cap) {
  int n = ajustes_buscar(consulta, out, cap);
  for (int i = 0; i < n; i++)
    if (out[i].op == AJ_FAROL) return i;
  return -1;
}

static void testarEntradaDpad(int lista, int avisos) {
  int steps = 0;
  ajustes_teste_layout(lista);
  if (lista) assert(ajustes_layout_lista() == 1 && triBladeLayout() == 0);
  else assert(ajustes_layout_lista() == 0 && triBladeLayout() == 1);

  ajustes_farol_avisos_definir(avisos);
  assert(ajustes_farol_avisos() == (avisos ? 1 : 0));

  resolver_teste_esquecer();
  assert(resolver_aberto() == 0);

  // Reinicia Ajustes na ilha de secoes e navega por D-pad ate Sobre e ajuda.
  secAtual = 0;
  ajustes_iniciar();
  assert(triIlha == TRI_SECOES && focoIndice == 1);
  triSecFoco = 0;
  focarSecao(0);
  while (triSecFoco != AJS_SOBRE && steps++ < AJ_MAX_SECOES) tecla(SDLK_DOWN);
  assert(triSecFoco == AJS_SOBRE);
  assert(strcmp(TELA[secIni[triSecFoco]].titulo, "Sobre e ajuda") == 0);

  // Entra em Sobre e ajuda e navega por D-pad ate a entrada Farol.
  tecla(SDLK_RETURN);
  assert(triIlha == TRI_LISTA && !focoIndice && secAtual == AJS_SOBRE);
  steps = 0;
  while (focoOp != AJ_FAROL && steps++ < 20) tecla(SDLK_DOWN);
  assert(focoOp == AJ_FAROL);
  assert(strcmp(rotuloOpcao(AJ_FAROL), FT_NOME) == 0);
  assert(strcmp(ajVisualIcone(AJ_FAROL), "aj_farol") == 0);

  // OK abre o Farol (resolver_abrir()) mesmo com Avisos do Farol desligado.
  tecla(SDLK_RETURN);
  assert(resolver_aberto() == 1);
  assert(resolver_fase() == RESOLVER_FASE_SINTOMA);
  resolver_teste_esquecer();
  assert(resolver_aberto() == 0);
}

int main(int argc, char **argv) {
  char dir[512];
  assert(argc == 2 && strlen(argv[1]) < sizeof dirAjustes);
  snprintf(dir, sizeof dir, "%s", argv[1]);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  player_dir(dir);
  fontefalha_dir(dir);
  ajustes_iniciar();
  assert(gravar());

  // 1. Contrato basico de AJ_FAROL e ausencia de "Resolver um problema" nos titulos.
  assert(AJ_FAROL >= 0 && AJ_FAROL < AJ_N);
  assert(strcmp(CHAVE[AJ_FAROL], "-farol") == 0);
  assert(OPCOES[AJ_FAROL].tipo == OP_ACAO);
  assert(strcmp(OPCOES[AJ_FAROL].rotulo, FT_NOME) == 0);
  assert(escopoDe(AJ_FAROL) == ESC_NENHUM);
  assert(familiaPreviaOpcao(AJ_FAROL) == AJPV_ABOUT);
  assert(ajCenaSecao(AJ_FAROL) == AJC_SOB_GUIA);
  assert(strcmp(ajVisualIcone(AJ_FAROL), "aj_farol") == 0);
  assert(strcmp(aj2DescobertaTit(AJ2_T_RESOLVER), FT_NOME) == 0);
  assert(strcmp(aj2TopoIcone(AJ2_T_RESOLVER), "aj_farol") == 0);
  {
    int vezes = 0;
    const char *sec = "";
    for (int k = 0; k < AJ_N_TELA; k++) {
      if (TELA[k].tipo == IT_SEC) sec = TELA[k].titulo;
      if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_FAROL) {
        vezes++;
        assert(strcmp(sec, "Sobre e ajuda") == 0);
      }
    }
    assert(vezes == 1);
  }

  // 2. Busca dos Ajustes: "farol", "problema", "erro", "registro" e "beacon".
  {
    AjusteBuscaResultado res[AJ_N];
    static const char *const termos[] = {
      "farol", "problema", "erro", "registro", "beacon"
    };
    valor[AJ_IDIOMA] = IDIOMA_PT + 1;
    for (size_t i = 0; i < sizeof termos / sizeof *termos; i++) {
      int idx = buscarFarol(termos[i], res, AJ_N);
      assert(idx >= 0);
      assert(strcmp(res[idx].titulo, FT_NOME) == 0);
      assert(strstr(res[idx].caminho, "Sobre e ajuda") != NULL);
      assert(strcmp(res[idx].icone, "aj_info") == 0);
    }
    // Em ingles tambem traduz para "Beacon" sem editar idioma_*.h.
    valor[AJ_IDIOMA] = IDIOMA_EN + 1;
    {
      int idx = buscarFarol("beacon", res, AJ_N);
      assert(idx >= 0);
      assert(strcmp(res[idx].titulo, "Beacon") == 0);
    }
    valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  }

  // Selecionar resultado da busca (ajustes_abrir_opcao) + OK abre resolver_abrir().
  resolver_teste_esquecer();
  ajustes_abrir_opcao(AJ_FAROL);
  ajustes_iniciar();
  assert(triIlha == TRI_LISTA && focoOp == AJ_FAROL);
  tecla(SDLK_RETURN);
  assert(resolver_aberto() == 1);
  resolver_teste_esquecer();

  // 3. Navegacao real D-pad + OK nos dois layouts (Palco/tri e Lista), com
  //    Avisos do Farol = 0 ("So quando eu abrir") e 1 ("Ligado").
  testarEntradaDpad(0, 0);
  testarEntradaDpad(0, 1);
  testarEntradaDpad(1, 0);
  testarEntradaDpad(1, 1);

  // 4. Capitulo do Guia atualizado para "Farol" e abertura funcional de resolver_abrir().
  assert(strcmp(GUIA_CAPS[11].titulo, FT_NOME) == 0);
  assert(strcmp(GUIA_CAPS[11].icone, "aj_farol") == 0);
  {
    int entFarol = -1;
    for (int i = 0; i < GUIA_NENT; i++) {
      if (!strcmp(GUIA_ENT[i].id, "p-farol")) { entFarol = i; break; }
    }
    assert(entFarol >= 0);
    assert(GUIA_ENT[entFarol].cap == 11);
    assert(GUIA_ENT[entFarol].alvo == AJ_FAROL);
    assert(GUIA_ENT[entFarol].tela == GT_FAROL);

    guiaAbrir(0);
    assert(ajustes_guia_aberto() == 1);
    while (gv.idx < GI_CAP0 + 11) teclaGuia(SDLK_DOWN);
    assert(gv.idx == GI_CAP0 + 11);
    teclaGuia(SDLK_RIGHT);
    assert(gv.col == GC_LISTA && gv.ent == entFarol);
    // Entra nos botoes do inspetor: GB_AJUSTES (0), GB_TELA (1 -> GT_FAROL).
    teclaGuia(SDLK_RIGHT);
    assert(gv.col == GC_BOTOES && gv.botao == 0);
    teclaGuia(SDLK_RIGHT);
    assert(gv.col == GC_BOTOES && gv.botao == 1);
    teclaGuia(SDLK_RETURN);
    assert(resolver_aberto() == 1);
    assert(ajustes_guia_aberto() == 0);
    assert(focoOp == AJ_FAROL);
    resolver_teste_esquecer();
  }

  puts("farol_entradas: PASS");
  return 0;
}
#endif
