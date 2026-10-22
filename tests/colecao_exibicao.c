// AS EXIBICOES DA PAGINA DE COLECAO (vertudo.c), sem janela e sem rede.
//
// Pedido do dono (06/10): cada colecao escolhe como se mostra — Grade, Linha do
// tempo, Ranking, Saga, Decadas, Mural. O que este teste cobra:
//   1. o Automatico pelo conteudo (Diretores, ranking, franquia, o resto);
//   2. toda exibicao cabe na largura util da grade e nenhuma celula encosta
//      noutra (a tabela de celulas e o unico lugar que posiciona);
//   3. as linhas sao contiguas e crescentes (a navegacao depende disso);
//   4. a ordem cronologica de Saga/Decadas/Linha (sem ano vai para o fim) e os
//      cabecalhos de decada antes dos titulos deles;
//   5. o D-pad: cima/baixo pelo centro mais proximo no Mural, a esquerda no
//      primeiro da linha pede a barra, cima na primeira linha vai ao botao;
//   6. OK no botao troca a exibicao e a escolha fica gravada POR PERFIL.
//
// Inclui src/vertudo.c (o estado e estatico) e troca a lista que a descoberta
// leu por uma semeada aqui.
#define desc_vertudo_n shot_vt_n
#define desc_vertudo_item shot_vt_item
#include "../src/vertudo.c"
#undef desc_vertudo_n
#undef desc_vertudo_item
#include <assert.h>

static CatItem semente[24];
static int nSemente;
int shot_vt_n(void) { return nSemente; }
int shot_vt_item(int i, CatItem *dst) {
  if (i < 0 || i >= nSemente) return 0;
  if (dst) *dst = semente[i];
  return 1;
}

static const int ANOS[] = { 2005, 1977, 1999, 2015, 1980, 0, 2002, 1983,
                            2017, 2019, 2008, 1997, 2016, 1977 };
#define NANOS (int)(sizeof ANOS / sizeof *ANOS)

static void semear(void) {
  int i;
  nSemente = NANOS;
  for (i = 0; i < nSemente; i++) {
    CatItem *c = &semente[i];
    memset(c, 0, sizeof *c);
    snprintf(c->imdb, sizeof c->imdb, "tt70%05d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Capítulo %d", i);
    if (ANOS[i]) snprintf(c->meta, sizeof c->meta, "%d · 2 h", ANOS[i]);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  vertudo_evento(&e);
}

static int sobrepoe(int a, int b) {
  return celX[a] < celX[b] + celW[b] - 0.5f && celX[b] < celX[a] + celW[a] - 0.5f &&
         celY[a] < celY[b] + celH[b] - 0.5f && celY[b] < celY[a] + celH[a] - 0.5f;
}

static ColFolder pasta;
static ColSource fontes[2];

int main(void) {
  int L, i, j;
  char dir[600];
  snprintf(dir, sizeof dir, "%s/nv-colexib.XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar("deploy/app/art");
  semear();
  pasta.sources = fontes;
  pasta.nSources = 1;
  snprintf(pasta.id, sizeof pasta.id, "starwars");
  snprintf(pasta.groupId, sizeof pasta.groupId, "franquias");
  snprintf(fontes[0].title, sizeof fontes[0].title, "Star Wars");
  collection = &pasta;
  aberta = 1;

  // 1. AUTOMATICO = exatamente o que a 2.0.3 mostra: Grade para tudo, so o
  // Diretor na Linha do tempo (a 2.0.3 ja fazia). Ranking, Saga, Decadas, Mural
  // e Vitrine existem, mas SO pelo botao "Exibição" (opt-in).
  { static const char *GRUPOS[] = { "Film Collections", "TV Collections", "Awards",
                                    "Studios", "Genres", "" };
    static const char *TITULOS[] = { "Star Wars", "Trilogia do Anel", "Saga Marvel",
                                     "Franquia X", "Top 100", "Franchise" };
    int g, t, ab, r, sv;
    for (g = 0; g < 6; g++) for (t = 0; t < 6; t++)
      for (ab = 1; ab <= 3; ab++) for (r = 0; r < 2; r++) for (sv = 0; sv < 2; sv++) {
        snprintf(pasta.group, sizeof pasta.group, "%s", GRUPOS[g]);
        snprintf(pasta.title, sizeof pasta.title, "%s", TITULOS[t]);
        pasta.nSources = ab; ranked = r; semVitrineAuto = sv;
        assert(layAuto() == VT_LAY_GRADE);
        snprintf(pasta.group, sizeof pasta.group, "Directors");
        assert(layAuto() == VT_LAY_LINHA);
      } }
  ranked = 0; semVitrineAuto = 0; pasta.nSources = 1;
  snprintf(pasta.title, sizeof pasta.title, "Star Wars");
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  collection = NULL; assert(layAuto() == VT_LAY_GRADE);
  collection = &pasta;

  // 2-4. GEOMETRIA de cada exibicao
  for (L = VT_LAY_GRADE; L < VT_LAY_VITRINE; L++) {
    lay = L; celLay = -1; orderN = -1; foco = 0;
    montar(nSemente);
    assert(celN == nSemente);
    for (i = 0; i < nSemente; i++) {
      assert(celX[i] >= 0.0f && celY[i] >= 0.0f);
      assert(celX[i] + celW[i] <= VT_CW + 30.0f);   // ranking passa 16 px no maximo
      if (i) assert(celLin[i] >= celLin[i - 1] && celLin[i] <= celLin[i - 1] + 1);
      for (j = 0; j < i; j++) if (sobrepoe(i, j)) {
        fprintf(stderr, "exibicao %d: celulas %d e %d se sobrepoem\n", L, i, j);
        return 1;
      }
    }
    if (ordenaPorAno()) {
      int ant = 0;
      CatItem it;
      for (i = 0; i < nSemente; i++) {
        int y; assert(viewItem(i, &it)); y = yearOf(&it);
        assert(y >= ant); ant = y;
      }
      assert(viewItem(nSemente - 1, &it) && !it.meta[0]);   // sem ano: no fim
    }
    printf("exibicao %-15s ok: %d celulas, %d linhas, altura %.0f\n",
           LAY_NOME[L], celN, celLin[celN - 1] + 1, celFim);
  }
  // Decadas: 1970, 1980, 1990, 2000, 2010 e "sem data" = 6 secoes, na ordem,
  // cada cabecalho acima do primeiro titulo dela.
  lay = VT_LAY_DECADAS; celLay = -1; orderN = -1; montar(nSemente);
  assert(decN == 6);
  assert(decAno[0] == 1970 && decAno[4] == 2010 && decAno[5] == 0);
  assert(decQtd[0] == 2 && decQtd[1] == 2 && decQtd[5] == 1);
  for (i = 1; i < decN; i++) assert(decY[i] > decY[i - 1]);
  assert(celY[0] == decY[0] + VT_DEC_H);

  // 5. D-PAD no Mural: o grande (0) desce para o pequeno de baixo mais perto.
  lay = VT_LAY_MURAL; celLay = -1; orderN = -1; montar(nSemente);
  tabFocus = 0; foco = 0;
  tecla(SDLK_DOWN); assert(foco == 3);           // primeiro pequeno de baixo
  tecla(SDLK_UP);   assert(foco == 1);           // o pequeno de cima, mesmo x
  tecla(SDLK_LEFT); assert(foco == 0);
  pedMenu = 0;
  tecla(SDLK_LEFT); assert(pedMenu == 1 && foco == 0);   // primeiro da linha
  foco = 2; tecla(SDLK_DOWN); assert(foco == 4);
  tecla(SDLK_DOWN); assert(celLin[foco] == 2);   // bloco seguinte, espelhado
  // Cima na primeira linha: a faixa do botao "Exibição".
  foco = 1; tecla(SDLK_UP); assert(tabFocus == 1 && tabCursor == nAbas());

  // 6. OK NO BOTAO troca e grava; outro perfil nao herda.
  salvos_perfil(1);
  layEscolha = VT_LAY_AUTO; lay = VT_LAY_GRADE;
  tecla(SDLK_RETURN);
  assert(lay == VT_LAY_LINHA && layEscolha == VT_LAY_LINHA);
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  assert(lay == VT_LAY_SAGA);
  assert(escolhaGravada(&pasta) == VT_LAY_SAGA);
  { ColFolder outra = pasta; snprintf(outra.id, sizeof outra.id, "outra");
    assert(escolhaGravada(&outra) == VT_LAY_AUTO); }
  salvos_perfil(2);
  assert(escolhaGravada(&pasta) == VT_LAY_AUTO);
  gravarEscolha(&pasta, VT_LAY_MURAL);
  assert(escolhaGravada(&pasta) == VT_LAY_MURAL);
  salvos_perfil(1);
  assert(escolhaGravada(&pasta) == VT_LAY_SAGA);
  // Depois do Mural vem a Vitrine, e depois dela volta ao comeco (Grade).
  lay = VT_LAY_MURAL; tabFocus = 1; tabCursor = nAbas();
  tecla(SDLK_RETURN); assert(lay == VT_LAY_VITRINE && tabFocus == 1);
  assert(nAbas() == 0 && tabCursor == 0);
  tecla(SDLK_RETURN); assert(lay == VT_LAY_GRADE);

  // 7. VITRINE: fileiras semeadas direto (a fila de abas precisa de rede).
  pasta.nSources = 2;
  lay = VT_LAY_VITRINE; layEscolha = VT_LAY_VITRINE; tabFocus = 0;
  vitIniciar();
  assert(vitBuf && vitMulti && vitN == 2);
  vitLin[0].n = 5; vitLin[0].estado = VIT_PRONTA;
  vitLin[1].n = 0; vitLin[1].estado = VIT_VAZIA;
  for (i = 0; i < 5; i++) vitBuf[i] = semente[i];
  vitCarga = vitN;
  // A fileira vazia e pulada; cima na primeira vai ao botao; esquerda na
  // primeira coluna pede a barra; direita anda e para na ultima.
  vitFR = 0; vitFC = 0; vitMexeu = 1;
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); assert(vitFC == 2);
  for (i = 0; i < 6; i++) tecla(SDLK_RIGHT);
  assert(vitFC == 4);
  tecla(SDLK_DOWN); assert(vitFR == 0 && vitFC == 4);
  pedMenu = 0; vitFC = 0; tecla(SDLK_LEFT); assert(pedMenu == 1);
  tecla(SDLK_UP); assert(tabFocus == 1 && tabCursor == nAbas());
  tecla(SDLK_DOWN); assert(tabFocus == 0);
  // Segunda fileira com itens: baixo desce lembrando a coluna de cada uma.
  vitLin[1].n = 3; vitLin[1].estado = VIT_PRONTA;
  for (i = 0; i < 3; i++) vitBuf[vitCap + i] = semente[7 + i];
  vitFR = 0; vitFC = 4;
  tecla(SDLK_DOWN); assert(vitFR == 1 && vitFC == 0);   // cada fileira lembra a sua coluna
  tecla(SDLK_RIGHT);
  tecla(SDLK_UP); assert(vitFR == 0 && vitFC == 4);
  // O destaque segue o foco depois do repouso.
  vitAtualizar(0.016f, 1000); vitAtualizar(0.016f, 1300);
  assert(vitHeroAtual == 0 * vitCap + 4);
  tecla(SDLK_RETURN);   // sem catalogo global neste teste: so nao pode cair
  // Sem abas (uma fonte so) as fileiras saem do catalogo: tipos misturados
  // viram Filmes e Series; so filmes, decadas da mais nova a mais velha.
  pasta.nSources = 1; vitIniciar();
  assert(!vitMulti);
  vitAgrupar(nSemente);
  assert(vitN == 6 && vitLin[0].n >= 1 && strstr(vitLin[0].nome, "2010"));
  assert(!strcmp(vitLin[vitN - 1].nome, i18n("Sem data")));
  snprintf(semente[1].tipo, sizeof semente[1].tipo, "series");
  vitAgrupar(nSemente);
  assert(vitN == 2 && !strcmp(vitLin[0].nome, i18n("Filmes")) && !strcmp(vitLin[1].nome, i18n("Séries")));

  printf("colecao_exibicao: ok\n");
  return 0;
}
