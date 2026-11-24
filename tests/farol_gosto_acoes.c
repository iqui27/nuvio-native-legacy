// tests/farol_gosto_acoes.c — issue #51 ("Mais assim", "Nao e para mim",
// "Comparar com...", 1 linha P6 no ctxinfo, Ajustes > Sobre e ajuda > Gosto,
// farolgostoui e trava parental).
//
// Sem armazenamento por thread. Inclui ../src/ajustes.c diretamente (como
// tests/farol_entradas.c) para validar indices/enum/busca/tri/lista junto ao
// restante do app linkado (ctxmenu, ctxinfo, farolgostoui, farolcompara, farol,
// farolsinal, ilha, ilhaacao, perfilinfantil).
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NV_LINUX_DESKTOP
#include <GLES2/gl2.h>
#endif

#include "../src/ajustes.c"
#include "catalogo.h"
#include "ctxinfo.h"
#include "ctxmenu.h"
#include "dados.h"
#include "farol.h"
#include "farolcompara.h"
#include "farolgosto.h"
#include "farolgostoui.h"
#include "farolinfo.h"
#include "farolsinal.h"
#include "faroltexto.h"
#include "gfx.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "ondever.h"
#include "perfis.h"
#include "perfilinfantil.h"
#include "text.h"
#include "tex_cache.h"

void testar(int s);
void testar(int s) { (void)s; }

// Gancho forte simulando #53 (farolconsent) para provar que o ajuste aciona o
// gancho e respeita perfil infantil/trava parental.
static int gConsentEstado;
static int gConsentChamadas;
static int gConsentAbrir;
int farolconsent_ativo(void) { return gConsentEstado; }
int farolconsent_definir(int ligado) {
  gConsentChamadas++;
  gConsentEstado = ligado ? 1 : 0;
  return 1;
}
int farolconsent_abrir(void) { gConsentAbrir++; return 1; }

static long long gEpochS = 1775077200LL; // 2026-04-01 UTC
static uint32_t gMs = 10000u;
static long long relogioS(void) { return gEpochS; }
static uint32_t relogioMs(void) { return gMs; }

static SDL_Event mkKey(SDL_Keycode sym, int repeat) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = sym;
  e.key.repeat = (Uint8)repeat;
  return e;
}

static void teclaAj(SDL_Keycode sym) {
  SDL_Event e = mkKey(sym, 0);
  ajustes_evento(&e);
  ajustes_atualizar(0.016f, gMs);
}

static void abrirCtx(int idx) {
  SDL_Event up;
  ctx_abrir(idx);
  assert(ctx_aberto());
  memset(&up, 0, sizeof up);
  up.type = SDL_KEYUP;
  up.key.keysym.sym = SDLK_RETURN;
  ctx_evento(&up);
}

static void teclaCtx(SDL_Keycode sym, int repeat) {
  SDL_Event e = mkKey(sym, repeat);
  ctx_evento(&e);
  ctx_atualizar(0.016f, gMs);
}

static void teclaFgui(SDL_Keycode sym, int repeat) {
  SDL_Event e = mkKey(sym, repeat);
  farolgostoui_evento(&e);
  farolgostoui_atualizar(0.016f, gMs);
}

static void avancarIlha(Uint32 dtMs) {
  Uint32 alvo = gMs + dtMs;
  while (gMs < alvo) {
    Uint32 passo = alvo - gMs > 200u ? 200u : (alvo - gMs);
    gMs += passo;
    txt_novo_quadro();
    gfx_novo_quadro();
    ilha_desenhar(gMs);
  }
}

static CatItem mkItem(const char *imdb, const char *tit, const char *tipo,
                      const char *genero, const char *dir, const char *meta) {
  CatItem ci;
  memset(&ci, 0, sizeof ci);
  snprintf(ci.imdb, sizeof ci.imdb, "%s", imdb);
  snprintf(ci.titulo, sizeof ci.titulo, "%s", tit);
  snprintf(ci.tipo, sizeof ci.tipo, "%s", tipo);
  snprintf(ci.genero, sizeof ci.genero, "%s", genero);
  snprintf(ci.direcao, sizeof ci.direcao, "%s", dir);
  snprintf(ci.meta, sizeof ci.meta, "%s", meta);
  ci.tmdb = atoi(imdb + 2);
  return ci;
}

static void montarCatalogoComFarol(void) {
  static CatItem lote[16];
  static CatFileira fils[2];
  int i;
  memset(lote, 0, sizeof lote);
  memset(fils, 0, sizeof fils);
  lote[0] = mkItem("tt0000101", "Filme Um", "movie", "Drama \xc2\xb7 Suspense",
                   "Diretor A", "2024 \xc2\xb7 1 h 45 min");
  lote[1] = mkItem("tt0000102", "Filme Dois", "movie", "Ação \xc2\xb7 Crime",
                   "Diretor B", "2023 \xc2\xb7 110 min");
  lote[2] = mkItem("tt0000103", "Serie Tres", "series", "Drama \xc2\xb7 Mistério",
                   "Criador C", "2022 \xc2\xb7 2 temporadas");
  for (i = 3; i < 10; i++) {
    char id[24], tit[40];
    snprintf(id, sizeof id, "tt0000%03d", 101 + i);
    snprintf(tit, sizeof tit, "Item %d", i);
    lote[i] = mkItem(id, tit, "movie", "Drama", "Diretor A", "2021 · 95 min");
  }
  for (i = 0; i < 8; i++) lote[8 + i] = lote[i]; // farol_gosto1 copia separada
  snprintf(fils[0].chave, sizeof fils[0].chave, "populares");
  snprintf(fils[0].titulo, sizeof fils[0].titulo, "Populares");
  fils[0].ini = 0; fils[0].n = 8;
  snprintf(fils[1].chave, sizeof fils[1].chave, "farol_gosto1");
  snprintf(fils[1].titulo, sizeof fils[1].titulo, "Para você");
  fils[1].ini = 8; fils[1].n = 8;
  fils[1].farolSelo = farol_selo();
  cat_definir_tudo(lote, 16, fils, 2);
}

static int farolContem(const char *imdb) {
  int r, i;
  for (r = 0; r < cat_n_fileiras(); r++) {
    const CatFileira *cf = cat_fileira(r);
    if (!cf || strncmp(cf->chave, "farol_", 6)) continue;
    for (i = cf->ini; i < cf->ini + cf->n; i++) {
      CatItem ci;
      if (cat_copiar_item(i, &ci) && !strcmp(ci.imdb, imdb)) return 1;
    }
  }
  return 0;
}

static void esperarFiosSinal(void) {
  int t = 0;
  while (farolsinal_teste_trabalho() && t++ < 200) SDL_Delay(5);
}

static void esperarComparaFios(void) {
  assert(farolcompara_teste_esperar(3000));
}

// Seleciona uma opcao pelo rotulo dentro do menu de contexto aberto.
static void selecionarCtxPorRotulo(const char *rot) {
  int i, achou = -1;
  for (i = 0; i < 16; i++) {
    // Move para item i olhando ops[]? Usa navegacao D-pad e valida foco.
    // Reabre no topo sempre que chama; ctxmenu nasce no foco 0.
    break;
  }
  (void)rot; (void)achou;
}
#undef selecionarCtxPorRotulo

// Como tests/ctxmenu_contract exige ops[] privado a ctxmenu.c, abrimos no
// topo e contamos linhas conhecidas do nosso fixture ou navegamos verificando
// o efeito; mas podemos checar se OP esta na ordem conhecida:
// 0: Salvar na biblioteca, 1: Marcar como assistido, 2: Mais assim,
// 3: Nao e para mim, 4: Comparar com... (quando Recomendar esta ligado!).
// Quando Recomendar esta desligado:
// 0: Salvar na biblioteca, 1: Marcar como assistido,
// 2: Nao e para mim, 3: Comparar com...
static void testarCtxMenuGostoECompara(void) {
  FarolGosto g;
  int32_t pesoAntes, pesoDepois;
  int32_t genDrama;
  const char *v;

  montarCatalogoComFarol();
  farol_recomendar_definir(1, NULL);
  farol_gosto_zerar(NULL);
  esperarFiosSinal();

  // 1) "Mais assim": aplica +1.5 e anuncia "Anotado." 4s sem foco.
  assert(farolsinal_modelo(&g));
  genDrama = mapa_hash_nome("Drama");
  pesoAntes = farolgosto_peso(&g, FG_DIM_GENERO, genDrama, farolsinal_dia_atual());
  assert(pesoAntes == 0);

  abrirCtx(8); // item 0 da fileira farol_gosto1 ("Filme Um", Drama)
  teclaCtx(SDLK_DOWN, 0); // 1: Marcar como assistido
  teclaCtx(SDLK_DOWN, 0); // 2: Mais assim
  teclaCtx(SDLK_RETURN, 0);
  assert(!ctx_aberto());

  esperarFiosSinal();
  assert(farolsinal_modelo(&g));
  pesoDepois = farolgosto_peso(&g, FG_DIM_GENERO, genDrama, farolsinal_dia_atual());
  assert(pesoDepois > 0);
  assert(ilha_tem("farol-gosto"));
  v = ilha_aviso_vez();
  assert(strcmp(v, "farol-gosto") == 0);
  // 4s, sem foco: apos ~100ms ancora sem tecla central; aos 4500ms ja sumiu.
  avancarIlha(100);
  assert(!ilha_tecla_central());
  assert(ilha_tem("farol-gosto"));
  avancarIlha(4200);
  assert(!ilha_tem("farol-gosto"));

  // Quando Recomendar esta desligado, "Mais assim" sai do menu e sinal nao entra;
  // menu passa a ter "Nao e para mim" na pos 2 e "Comparar com..." na pos 3.
  farol_recomendar_definir(0, NULL);
  assert(!farolsinal_recomendar());
  assert(!farolContem("tt0000101")); // desligar removeu farol_gosto1 na hora!
  montarCatalogoComFarol();
  abrirCtx(0);
  teclaCtx(SDLK_DOWN, 0); // 1: Assistido
  teclaCtx(SDLK_DOWN, 0); // 2: Nao e para mim (pois Mais assim sumiu!)
  teclaCtx(SDLK_DOWN, 0); // 3: Comparar com...
  teclaCtx(SDLK_RETURN, 0);
  assert(ctx_comparar_contar() == 1);
  ctx_comparar_limpar();

  // 2) "Nao e para mim" + remocao imediata + Desfazer via ilhaacao_feita.
  farol_recomendar_definir(1, NULL);
  montarCatalogoComFarol();
  assert(farolContem("tt0000102"));
  abrirCtx(9); // "Filme Dois" em farol_gosto1
  teclaCtx(SDLK_DOWN, 0); // 1: Assistido
  teclaCtx(SDLK_DOWN, 0); // 2: Mais assim
  teclaCtx(SDLK_DOWN, 0); // 3: Nao e para mim
  teclaCtx(SDLK_RETURN, 0);
  assert(!ctx_aberto());
  assert(!farolContem("tt0000102")); // saiu de farol_gosto1 NA HORA!
  assert(farolContem("tt0000101"));  // os outros titulos da fileira ficaram!
  assert(farolsinal_modelo(&g));
  assert(farolgosto_recusado(&g, mapa_hash_titulo("tt0000102")) == 1);
  assert(ilhaacao_tem_desfazer() == 1);
  assert(ilha_tem(ILHA_ACAO_CHAVE));

  // Desfazer: restaura tt0000102 na fileira e remove a recusa do modelo!
  ilhaacao_botao(1);
  assert(farolContem("tt0000102"));
  assert(farolsinal_modelo(&g));
  assert(farolgosto_recusado(&g, mapa_hash_titulo("tt0000102")) == 0);
  avancarIlha(7000);

  // 3) "Comparar com...": ate 3 distintos, limpa em troca de perfil/logout.
  ctx_comparar_limpar();
  abrirCtx(0); // tt0000101
  teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0); // 4: Comparar com...
  teclaCtx(SDLK_RETURN, 0);
  assert(ctx_comparar_contar() == 1);
  assert(!farolcompara_aberto());

  // Repetido nao conta nem abre.
  abrirCtx(0);
  teclaCtx(SDLK_DOWN, 0); teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0); teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_RETURN, 0);
  assert(ctx_comparar_contar() == 1);
  assert(!farolcompara_aberto());

  // 2o distinto -> abre P4 com 2 colunas.
  abrirCtx(1); // tt0000102
  teclaCtx(SDLK_DOWN, 0); teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0); teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_RETURN, 0);
  assert(ctx_comparar_contar() == 2);
  assert(farolcompara_aberto());
  assert(farolcompara_n_colunas() == 2);
  farolcompara_fechar();
  esperarComparaFios();

  // 3o distinto (serie tem OP_REWATCH em 2 -> Comparar com... em 5) -> abre P4 com 3 colunas.
  abrirCtx(2); // tt0000103
  teclaCtx(SDLK_DOWN, 0); teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0); teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_RETURN, 0);
  assert(ctx_comparar_contar() == 3);
  assert(farolcompara_aberto());
  assert(farolcompara_n_colunas() == 3);
  farolcompara_fechar();
  esperarComparaFios();

  // Troca de perfil / logout limpa a selecao.
  ctx_abrir_sessao();
  assert(ctx_comparar_contar() == 0);
  puts("  ctxmenu (Mais assim / Nao e para mim + Undo / Comparar com...): ok");
}

static void testarCtxInfoP6(void) {
  CatItem semDado = mkItem("tt0000201", "Sem Dado", "movie", "", "", "");
  CatItem comDado = mkItem("tt0000202", "Com Duracao", "movie", "Drama", "Dir X", "2024 · 105 min");
  char txt[1024];
  float hSem, hCom;

  ctxinfo_teste_relogio(relogioS);

  ctxinfo_texto(&semDado, NULL, txt, sizeof txt);
  assert(strstr(txt, "info:") == NULL);
  hSem = ctxinfo_altura(&semDado, NULL, CTXI_W);

  ctxinfo_texto(&comDado, NULL, txt, sizeof txt);
  assert(strstr(txt, "info: ") != NULL);
  assert(strstr(txt, "Termina ") != NULL || strstr(txt, "min") != NULL);
  hCom = ctxinfo_altura(&comDado, NULL, CTXI_W);
  assert(hCom > hSem);
  ctxinfo_teste_relogio(NULL);
  puts("  ctxinfo (1 linha P6 com dado / nada sem dado): ok");
}

static void alimentarModeloTop5(void) {
  static const char *const GEN[6] = {
    "Drama", "Suspense", "Crime", "Aventura", "Ação", "Comédia"
  };
  static const char *const DIR[6] = {
    "Ana Lima", "Bruno Reis", "Clara Paz", "Diego Luz", "Eva Sol", "Fabio Mar"
  };
  static const char *const KW[6] = {
    "time travel", "detective", "heist", "survival", "mystery", "family"
  };
  int i, r;
  farol_recomendar_definir(1, NULL);
  farol_gosto_zerar(NULL);
  esperarFiosSinal();
  // Registra 6 temas no cache de farolsinal (como se tivessem vindo do TMDB).
  for (i = 0; i < 6; i++)
    farolsinal_registrar_nome(FG_DIM_TEMA, 5000 + i, KW[i]);

  for (i = 0; i < 6; i++) {
    int pesos = 6 - i; // 0 mais forte -> 5 mais fraco
    for (r = 0; r < pesos; r++) {
      FarolFicha f;
      char id[24];
      memset(&f, 0, sizeof f);
      f.tmdb = 90000 + i * 10 + r;
      f.tipo = FF_FILME;
      f.ano = 2023;
      f.minutos = 110;
      f.nGenero = 1;
      f.genero[0] = mapa_hash_nome(GEN[i]);
      f.nPessoa = 1;
      f.pessoa[0] = mapa_hash_nome(DIR[i]);
      f.nTema = 1;
      f.tema[0] = 5000 + i;
      farolsinal_registrar_nome(FG_DIM_PESSOA, f.pessoa[0], DIR[i]);
      farolsinal_ficha_ok(farolficha_chave(&f), &f);
      snprintf(id, sizeof id, "tmdb:%d", (int)f.tmdb);
      farolsinal_saida(id, 'm', 0, 0, f.tmdb, 700, 3600, 0); // -0.8 -> nao queremos negativo!
      // Melhor: simula CatItem via farolsinal_interesse/mais_assim positivo!
    }
  }
  // Vamos reiniciar limpo e aplicar apenas sinais positivos via CatItem / mais_assim
  farol_gosto_zerar(NULL);
  esperarFiosSinal();
  for (i = 0; i < 6; i++)
    farolsinal_registrar_nome(FG_DIM_TEMA, 6000 + i, KW[i]);
  for (i = 0; i < 6; i++) {
    int rep = (6 - i) * 2;
    for (r = 0; r < rep; r++) {
      FarolFicha f;
      CatItem ci;
      memset(&f, 0, sizeof f);
      f.tmdb = 80000 + i * 10 + r;
      f.tipo = FF_FILME;
      f.ano = 2022;
      f.minutos = 100;
      f.nGenero = 1;
      f.genero[0] = mapa_hash_nome(GEN[i]);
      f.nPessoa = 1;
      f.pessoa[0] = mapa_hash_nome(DIR[i]);
      f.nTema = 1;
      f.tema[0] = 6000 + i;
      farolsinal_registrar_nome(FG_DIM_PESSOA, f.pessoa[0], DIR[i]);
      farolsinal_ficha_ok(farolficha_chave(&f), &f);

      memset(&ci, 0, sizeof ci);
      snprintf(ci.imdb, sizeof ci.imdb, "tt%07d", 700000 + i * 10 + r);
      snprintf(ci.tipo, sizeof ci.tipo, "movie");
      snprintf(ci.genero, sizeof ci.genero, "%s", GEN[i]);
      snprintf(ci.direcao, sizeof ci.direcao, "%s", DIR[i]);
      ci.tmdb = f.tmdb;
      farolsinal_mais_assim(&ci);
    }
  }
  esperarFiosSinal();
}

static void testarAjustesEGostoUI(void) {
  AjusteBuscaResultado res[AJ_N];
  int n, i, achouRec = 0, achouApr = 0, achouZer = 0, achouLlm = 0;
  FarolGostoResumo r;

  // Garante indices existentes preservados + opcoes novas no fim de OpcaoId
  assert(AJ_FAROL_AVISOS < AJ_FAROL);
  assert(AJ_FAROL_RECOMENDAR == AJ_FAROL + 1);
  assert(AJ_FAROL_APRENDEU == AJ_FAROL + 2);
  assert(AJ_FAROL_ZERAR == AJ_FAROL + 3);
  assert(AJ_FAROL_LLM_SO_ESTA_TV == AJ_FAROL + 4);
  assert(AJ_FAROL_LLM_SO_ESTA_TV + 1 == AJ_N);
  assert(escopoDe(AJ_FAROL_RECOMENDAR) == ESC_NENHUM);
  assert(escopoDe(AJ_FAROL_APRENDEU) == ESC_NENHUM);
  assert(escopoDe(AJ_FAROL_ZERAR) == ESC_NENHUM);
  assert(escopoDe(AJ_FAROL_LLM_SO_ESTA_TV) == ESC_NENHUM);

  // Busca em Ajustes encontra todas as opcoes do Farol > Gosto
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  n = ajustes_buscar("farol gosto", res, AJ_N);
  for (i = 0; i < n; i++) {
    if (res[i].op == AJ_FAROL_RECOMENDAR) achouRec = 1;
    if (res[i].op == AJ_FAROL_APRENDEU) achouApr = 1;
    if (res[i].op == AJ_FAROL_ZERAR) achouZer = 1;
    if (res[i].op == AJ_FAROL_LLM_SO_ESTA_TV) achouLlm = 1;
  }
  assert(achouRec && achouApr && achouZer && achouLlm);

  // Em Palco/tri (0) e Lista (1), navegar ate AJ_FAROL_APRENDEU e abrir folha
  alimentarModeloTop5();
  farolgostoui_resumo(&r);
  assert(!r.vazio);
  assert(r.nGeneros == 5);
  assert(r.nPessoas == 5);
  assert(r.nTemas == 5);
  assert(strcmp(r.generos[0], "Drama") == 0);
  assert(strcmp(r.pessoas[0], "Ana Lima") == 0);
  // "time travel" -> mapa_tema_nome -> "viagem no tempo"
  assert(strcmp(r.temas[0], "viagem no tempo") == 0);

  // Palco/tri: vai ate Sobre e ajuda e abre AJ_FAROL_APRENDEU
  valor[AJ_LAYOUT_AJUSTES] = 0;
  ajustes_iniciar();
  assert(triIlha == TRI_SECOES);
  triSecFoco = 0; focarSecao(0);
  {
    int steps = 0;
    while (triSecFoco != AJS_SOBRE && steps++ < AJ_MAX_SECOES) teclaAj(SDLK_DOWN);
    assert(triSecFoco == AJS_SOBRE);
    teclaAj(SDLK_RETURN);
    assert(triIlha == TRI_LISTA);
    steps = 0;
    while (focoOp != AJ_FAROL_APRENDEU && steps++ < 25) teclaAj(SDLK_DOWN);
    assert(focoOp == AJ_FAROL_APRENDEU);
    teclaAj(SDLK_RETURN);
    assert(farolgostoui_aberto() && farolgostoui_modo() == 1);
  }
  // Key repeat no OK NAO pode fechar a folha; D-pad e Voltar fecham sem beco!
  teclaFgui(SDLK_DOWN, 0);
  teclaFgui(SDLK_UP, 0);
  teclaFgui(SDLK_RETURN, 1); // repeat == 1 -> ignorado!
  assert(farolgostoui_aberto());
  teclaFgui(SDLK_ESCAPE, 0); // Voltar fecha
  assert(farolgostoui_modo() == 0);
  farolgostoui_fechar();

  // Lista (1): AJ_FAROL_ZERAR abre confirmacao -> Voltar cancela preservando tudo!
  valor[AJ_LAYOUT_AJUSTES] = 1;
  ajustes_abrir_opcao(AJ_FAROL_ZERAR);
  ajustes_iniciar();
  assert(focoOp == AJ_FAROL_ZERAR);
  teclaAj(SDLK_RETURN);
  assert(farolgostoui_aberto() && farolgostoui_modo() == 2);
  teclaFgui(SDLK_ESCAPE, 0); // Cancela
  farolgostoui_fechar();
  farolgostoui_resumo(&r);
  assert(!r.vazio); // preservado!

  // Confirma Zerar (ignorando repeat) -> zera modelo + pendencias + anuncia na pilula
  montarCatalogoComFarol();
  teclaAj(SDLK_RETURN);
  assert(farolgostoui_modo() == 2);
  teclaFgui(SDLK_RETURN, 1); // repeat ignorado
  assert(farolgostoui_modo() == 2);
  teclaFgui(SDLK_RETURN, 0); // confirma!
  farolgostoui_atualizar(0.5f, gMs);
  assert(!farolgostoui_aberto());
  assert(!farolContem("tt0000101"));
  assert(ilha_tem("farol-gosto"));
  farolgostoui_resumo(&r);
  assert(r.vazio == 1);
  avancarIlha(4200);

  // LLM / "So nesta TV": seguro desligado por padrao; liga quando desbloqueado;
  // bloqueado em perfil infantil!
  gConsentEstado = 0; gConsentChamadas = 0; gConsentAbrir = 0;
  assert(!ajustes_farol_llm_ativo());
  assert(valor[AJ_FAROL_LLM_SO_ESTA_TV] == 1); // Desligado (padrao)
  ajustes_abrir_opcao(AJ_FAROL_LLM_SO_ESTA_TV);
  ajustes_iniciar();
  teclaAj(SDLK_RETURN);
  assert(ajustes_farol_llm_ativo() == 1);
  assert(gConsentChamadas == 1 && gConsentAbrir == 1);
  teclaAj(SDLK_RETURN);
  assert(ajustes_farol_llm_ativo() == 0);

  // Perfil infantil bloqueia LLM even when settings are unlocked!
  assert(perfil_infantil_definir(1, 1));
  assert(perfil_infantil_ativo() == 1);
  assert(!perfil_ajustes_travados(1));
  farolGostoEspelhar();
  assert(inativa(AJ_FAROL_LLM_SO_ESTA_TV));
  teclaAj(SDLK_RETURN);
  assert(ajustes_farol_llm_ativo() == 0);
  definirValorDireto(AJ_FAROL_LLM_SO_ESTA_TV, 0);
  assert(ajustes_farol_llm_ativo() == 0);
  puts("  ajustes + farolgostoui + child LLM block: ok");
}

static void testarTravaParental(void) {
  FarolGosto g;
  int32_t antes;
  farol_recomendar_definir(1, NULL);
  farol_gosto_zerar(NULL);
  esperarFiosSinal();
  montarCatalogoComFarol();

  assert(perfil_infantil_definir(1, 1));
  assert(perfil_ajustes_travar(1, 1));
  assert(perfil_ajustes_travados(1) == 1);

  assert(farolsinal_modelo(&g));
  antes = farolgosto_peso(&g, FG_DIM_GENERO, mapa_hash_nome("Drama"), farolsinal_dia_atual());

  // Nenhuma mutacao funciona em ctxmenu
  abrirCtx(8); // "Filme Um"
  teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0); // Mais assim
  teclaCtx(SDLK_RETURN, 0);
  esperarFiosSinal();
  assert(farolsinal_modelo(&g));
  assert(farolgosto_peso(&g, FG_DIM_GENERO, mapa_hash_nome("Drama"), farolsinal_dia_atual()) == antes);

  abrirCtx(9); // "Filme Dois"
  teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0);
  teclaCtx(SDLK_DOWN, 0); // Nao e para mim
  teclaCtx(SDLK_RETURN, 0);
  esperarFiosSinal();
  assert(farolContem("tt0000102")); // nao removeu!
  assert(farolsinal_modelo(&g));
  assert(farolgosto_recusado(&g, mapa_hash_titulo("tt0000102")) == 0);

  // Nenhuma mutacao funciona em Ajustes nem API direta
  ajustes_farol_recomendar_definir(0);
  assert(farolsinal_recomendar() == 1);
  definirValorDireto(AJ_FAROL_RECOMENDAR, 1);
  assert(farolsinal_recomendar() == 1);

  definirValorDireto(AJ_FAROL_LLM_SO_ESTA_TV, 0);
  assert(ajustes_farol_llm_ativo() == 0);

  farol_gosto_zerar(NULL);
  assert(farolContem("tt0000101"));
  farolgostoui_abrir_zerar();
  assert(!farolgostoui_aberto());

  // Limpa trava para encerramento limpo
  assert(perfil_ajustes_travar(1, 0));
  assert(perfil_infantil_definir(1, 0));
  puts("  trava parental (bloqueia todas mutacoes): ok");
}

#ifdef NV_LINUX_DESKTOP
static void testarGlRender(const char *dir) {
  const int W = 1920, H = 1080;
  unsigned char *px = malloc((size_t)W * (size_t)H * 4u);
  int i, claros = 0;
  assert(px);

  alimentarModeloTop5();
  farolgostoui_abrir_aprendeu();
  for (i = 0; i < 30; i++) {
    farolgostoui_atualizar(0.016f, gMs + (Uint32)i * 16u);
    txt_novo_quadro();
    gfx_novo_quadro();
    glViewport(0, 0, W, H);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    farolgostoui_desenhar(gMs + (Uint32)i * 16u);
    glFinish();
  }
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (i = 0; i < W * H * 4; i += 4)
    if (px[i] > 180 && px[i + 1] > 180 && px[i + 2] > 180) claros++;
  assert(claros > 1500);
  farolgostoui_fechar();
  (void)dir;
  free(px);
  puts("  gl render (farolgostoui): ok");
}
#endif

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
#ifdef NV_LINUX_DESKTOP
  const int W = 1920, H = 1080;
  SDL_Window *win;
  SDL_GLContext gl;
#endif
  assert(dir && dir[0]);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  perfis_definir_ativo(1);
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  farolsinal_pasta(dir);
  farolsinal_relogio(relogioS, relogioMs);
  farol_teste_relogio(relogioS);
  ilha_abrir_sessao();
  ajustes_iniciar();

#ifdef NV_LINUX_DESKTOP
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  win = SDL_CreateWindow("farol-51", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  glViewport(0, 0, W, H);
  gfx_tamanho_alvo(W, H);
  assert(gfx_iniciar());
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
#endif

  testarCtxMenuGostoECompara();
  testarCtxInfoP6();
  testarAjustesEGostoUI();
  testarTravaParental();
#ifdef NV_LINUX_DESKTOP
  testarGlRender(argc > 1 ? argv[1] : dir);
  tex_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
#else
  (void)argc; (void)argv;
#endif

  farol_recomendar_definir(1, NULL);
  farol_gosto_zerar(NULL);
  esperarFiosSinal();
  SDL_Quit();
  puts("farol_gosto_acoes: PASS");
  return 0;
}
