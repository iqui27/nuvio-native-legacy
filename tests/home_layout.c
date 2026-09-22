// Sem janela, rede ou TV: valida composição editorial e foco sobre dados reais
// de catálogo (fixtures), usando a implementação da Home e do catálogo.
#include <assert.h>
#include "../src/home.c"

void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso;
}
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso;
}
int tex_falhou(const char *url) { (void)url; return 0; }
int tex_largura_fonte(const char *url) { (void)url; return 0; }
Uint32 SDL_GetTicks(void) { return 0; }

// catalogo.c agora le o progresso de progresso.c, que fala com dados.c e
// perfis.c. Aqui nao ha disco nem conta: dublês vazios bastam.
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
// NULL de proposito: fileiras.c sai cedo em carregar() e a lista nasce vazia,
// que e o estado de quem nunca abriu o app. O teste de layout nao tem opiniao
// sobre escolha local de fileiras.
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
void  dados_marcar_sujo(int leve) { (void)leve; }
void  sync_proteger_ajustes_locais(void) {}
int   perfis_ativo(void) { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }

int main(void) {
  // O TETO DE FILEIRAS NO MAXIMO, porque este teste e sobre COMPOSICAO e FOCO.
  //
  // O limite legado é 16 linhas visíveis. O fixture valida a composição dentro
  // desse teto, incluindo as linhas fixas.
  fil_definir_limite(FIL_LIMITE_MAX);
  assert(MAX_FIL <= FOCUS_MAX_FILEIRAS);
  assert(perfilCatalogo("Oscars 2026 - Filme") == FILEIRA_COLECAO);
  assert(perfilCatalogo("NETFLIX - Série") == FILEIRA_SERVICO);
  assert(perfilCatalogo("For You - Filme") == FILEIRA_NORMAL);
  assert(fabsf(larguraDe(FILEIRA_DESTAQUE) /
               alturaDe(FILEIRA_DESTAQUE) - 16.0f / 9.0f) < 0.01f);
  assert(alturaDe(FILEIRA_DESTAQUE) > alturaDe(FILEIRA_COLECAO));
  assert(fabsf(larguraDe(FILEIRA_DESTAQUE_QUADRADO) /
               alturaDe(FILEIRA_DESTAQUE_QUADRADO) - 4.0f / 3.0f) < 0.01f);
  assert(alturaDe(FILEIRA_DESTAQUE_QUADRADO) > alturaDe(FILEIRA_DESTAQUE));
  assert(larguraDe(FILEIRA_DESTAQUE_QUADRADO) * alturaDe(FILEIRA_DESTAQUE_QUADRADO) >
         larguraDe(FILEIRA_DESTAQUE) * alturaDe(FILEIRA_DESTAQUE));
  assert(tipoDaEscolha(FIL_TIPO_DESTAQUE) == FILEIRA_DESTAQUE);
  assert(tipoDaEscolha(FIL_TIPO_DESTAQUE_QUADRADO) == FILEIRA_DESTAQUE_QUADRADO);
  assert(!strcmp(fil_tipo_rotulo(FIL_TIPO_DESTAQUE_QUADRADO), "Destaque 4:3"));
  assert(gapDe(FILEIRA_DESTAQUE) == NV_CARD_GAP_GRANDE);
  assert(gapDe(FILEIRA_DESTAQUE_QUADRADO) == NV_CARD_GAP_GRANDE);
  assert(gapDe(FILEIRA_NORMAL) == NV_CARD_GAP);
  assert(larguraDe(FILEIRA_COLECAO) > larguraDe(FILEIRA_SERVICO));
  assert(!temRotulo(FILEIRA_DESTAQUE));
  assert(!temRotulo(FILEIRA_CATALOGOS));

  CatItem *itensTeste = calloc(48, sizeof *itensTeste);
  CatFileira fils[16] = {0};
  assert(itensTeste);
  for (int i = 0; i < 16; i++) {
    snprintf(fils[i].chave, sizeof fils[i].chave, "catalogo_%d", i);
    snprintf(fils[i].titulo, sizeof fils[i].titulo, "Lista %d", i);
    snprintf(fils[i].base, sizeof fils[i].base, "https://example.invalid/addon");
    snprintf(fils[i].tipo, sizeof fils[i].tipo, "movie");
    snprintf(fils[i].catId, sizeof fils[i].catId, "id%d", i);
    fils[i].ini = i*3; fils[i].n = 3;
  }
  snprintf(fils[0].chave, sizeof fils[0].chave, "continue_watching");
  fils[0].base[0] = fils[0].catId[0] = 0;
  snprintf(fils[14].titulo, sizeof fils[14].titulo, "Netflix - Filme");
  snprintf(fils[15].titulo, sizeof fils[15].titulo, "Oscar - Filme");
  cat_definir_tudo(itensTeste, 48, fils, 16);
  sincronizarFileiras();
  assert(nFileiras == 16);
  assert(foco.nFileiras == 16);
  assert(fileiras[1].tipo == FILEIRA_SOCIAL && fileiras[1].ini == -1 && fileiras[1].n == 1);
  assert(fileiras[0].tipo == FILEIRA_DESTAQUE);
  for (int i = 1; i < 16; i++) {
    int achou = 0;
    for (int r = 0; r < nFileiras; r++)
      if (!strcmp(fileiras[r].chave, fils[i].chave)) achou++;
    assert(achou == 1); // todas as fileiras de catálogo visíveis são únicas
  }
  foco.fileira = 0; foco.coluna = 0;
  for (int i = 1; i < nFileiras; i++) assert(focus_mover(&foco, 0, 1));
  assert(foco.fileira == nFileiras - 1);
  assert(!focus_mover(&foco, 0, 1));
  // Mesma contagem, ordem diferente: manter chave, coluna e scroll.
  foco.fileira = 5; foco.coluna = 2; scrollX[5] = 123;
  char chave[192]; snprintf(chave, sizeof chave, "%s", fileiras[5].chave);
  CatFileira troca = fils[4]; fils[4] = fils[8]; fils[8] = troca;
  cat_definir_tudo(itensTeste, 48, fils, 16);
  sincronizarFileiras();
  assert(!strcmp(fileiras[foco.fileira].chave, chave));
  assert(foco.coluna == 2);
  assert(scrollX[foco.fileira] == 123);
  assert(col_carregar("tests/fixtures/collections") == 2);
  assert(col_folder(0)->nSources==2);
  assert(col_folder(0)->frames==0);
  assert(col_folder(-1)==NULL);
  const char *ids[]={"", "now_playing_movies","trending_movies","trending_series",
    "ai_movies_for_you","ai_series_for_you","snoak_top100_movies","snoak_top100_series"};
  for(int i=1;i<8;i++)snprintf(fils[i].catId,sizeof fils[i].catId,"%s",ids[i]);
  // TOP10 sai do TITULO que o catalogo trouxe (perfilCatalogo), nao do catId —
  // o id e do addon e pode ser qualquer coisa.
  snprintf(fils[6].titulo,sizeof fils[6].titulo,"Top 100 - Filme");
  snprintf(fils[7].titulo,sizeof fils[7].titulo,"Top 100 - Serie");
  cat_definir_tudo(itensTeste,48,fils,16);filsAplicadas=-1;sincronizarFileiras();
  // SEM TABELA DE CURADORIA: a ordem e a da conta (catordem) ou a local
  // (fil_unir) — a montagem nao renomeia nem reordena por cima da escolha.
  // O que a home acrescenta sozinha entra nos lugares fixos dela: "Entre
  // amigos" em segundo, grupos de colecao no FIM, na ordem declarada.
  assert(nFileiras>=11);
  assert(fileiras[1].tipo==FILEIRA_SOCIAL);
  // O primeiro catalogo com conteudo vira o destaque; o resto segue a ordem.
  assert(fileiras[0].tipo==FILEIRA_DESTAQUE);
  assert(!strcmp(fileiras[0].chave,"catalogo_1"));
  assert(fileiras[nFileiras-2].tipo==FILEIRA_CATALOGOS);
  assert(!strcmp(fileiras[nFileiras-2].titulo,"Streaming"));
  assert(!strcmp(col_folder(fileiras[nFileiras-2].folders[0])->title,"Netflix"));
  assert(!strcmp(fileiras[nFileiras-1].titulo,"Themes"));
  { int tops=0;
    for (int r=0; r<nFileiras; r++)
      if (fileiras[r].tipo==FILEIRA_TOP10) {
        tops++;
        assert(fileiras[r].stackN==3 && fileiras[r].n==1);
      }
    assert(tops==2); }
  for (int i=8; i<16; i++) {
    int encontrado=0;
    for (int r=0; r<nFileiras; r++)
      if (!strcmp(fileiras[r].chave, fils[i].chave)) encontrado=1;
    assert(encontrado);
  }
  fileiras[9].n=3;fileiras[9].stackN=0;fileiras[9].verTudo=1;
  for(int i=0;i<nFileiras;i++)assert(strcmp(fileiras[i].titulo,"Seus catálogos"));
  snprintf(fils[15].chave,sizeof fils[15].chave,"social_activity");
  fils[15].base[0]=fils[15].catId[0]=0;
  cat_definir_tudo(itensTeste,48,fils,16);sincronizarFileiras();
  int sociais=0;
  for(int i=0;i<nFileiras;i++)if(fileiras[i].tipo==FILEIRA_SOCIAL){
    sociais++;assert(fileiras[i].ini==45 && fileiras[i].n==3);
  }
  assert(sociais==1); // dados reais substituem vazio, nunca duplicam a fileira
  assert(fileiras[9].stackN==0 && fileiras[9].n==3 && fileiras[9].verTudo);

  // O destaque continua mostrando uma terceira alternativa quando a primeira
  // fonte entrega só dois títulos: o item extra vem de um catálogo já carregado
  // e mantém seu índice real, sem fallback de arte de outro título.
  { CatFileira curtas[2] = {0};
    snprintf(curtas[0].chave, sizeof curtas[0].chave, "curated_movies");
    snprintf(curtas[0].titulo, sizeof curtas[0].titulo, "Destaques");
    snprintf(curtas[0].base, sizeof curtas[0].base, "https://example.invalid/addon");
    snprintf(curtas[0].tipo, sizeof curtas[0].tipo, "movie");
    snprintf(curtas[0].catId, sizeof curtas[0].catId, "curated");
    curtas[0].ini = 0; curtas[0].n = 2;
    snprintf(curtas[1].chave, sizeof curtas[1].chave, "more_movies");
    snprintf(curtas[1].titulo, sizeof curtas[1].titulo, "Mais filmes");
    snprintf(curtas[1].base, sizeof curtas[1].base, "https://example.invalid/addon");
    snprintf(curtas[1].tipo, sizeof curtas[1].tipo, "movie");
    snprintf(curtas[1].catId, sizeof curtas[1].catId, "more");
    curtas[1].ini = 2; curtas[1].n = 2;
    cat_definir_tudo(itensTeste, 4, curtas, 2);
    sincronizarFileiras();
    { int destaque = -1;
      for (int r = 0; r < nFileiras; r++)
        if (fileiras[r].tipo == FILEIRA_DESTAQUE) { destaque = r; break; }
      assert(destaque >= 0);
      assert(fileiras[destaque].n == 3);
      assert(fileiraItemIndice(&fileiras[destaque], 0) == 0);
      assert(fileiraItemIndice(&fileiras[destaque], 1) == 1);
      assert(fileiraItemIndice(&fileiras[destaque], 2) == 2);
    }
  }

  // Arte de outro titulo nunca e fallback silencioso, mesmo quando o indice
  // esta alem do acervo local. Sem catalogo, os vetores locais continuam
  // disponiveis apenas na mesma posicao.
  snprintf(itensTeste[0].poster,sizeof itensTeste[0].poster,"own-poster.jpg");
  snprintf(itensTeste[0].backdrop,sizeof itensTeste[0].backdrop,"own-backdrop.jpg");
  snprintf(itensTeste[1].poster,sizeof itensTeste[1].poster,"other-poster.jpg");
  nBd=2; nPst=1;
  snprintf(bd[0],sizeof bd[0],"fallback-0.jpg");
  snprintf(bd[1],sizeof bd[1],"fallback-1.jpg");
  snprintf(pst[0],sizeof pst[0],"fallback-poster-0.jpg");
  cat_definir_tudo(itensTeste,48,NULL,0);
  assert(!strcmp(arte_por_identidade(0,0),"own-poster.jpg"));
  assert(!strcmp(arte_por_identidade(0,1),"own-backdrop.jpg"));
  assert(!strcmp(arte_por_identidade(1,0),"other-poster.jpg"));
  assert(arte_por_identidade(999,0)==NULL);

  // A integracao de segunda ordem retargeta sem overshoot, e reduced motion
  // pode saltar ao destino sem deixar velocidade residual.
  { float x=0, v=0;
    for (int i=0; i<60; i++) {
      x=anim_mola2(&v,x,1.0f,0.016f,NV_MOLA2_SCROLL);
      assert(x>=0.0f && x<=1.0f);
    }
    x=anim_mola2(&v,x,0.0f,0.016f,NV_MOLA2_SCROLL);
    assert(x>=0.0f && x<=1.0f);
    x=anim_mola2_reduzida(&v,x,0.35f,0.016f,NV_MOLA2_SCROLL,1);
    assert(x==0.35f && v==0.0f);
  }
  assert(NV_HERO_FADE_MS>=180.0f && NV_HERO_FADE_MS<=250.0f);

  // --- #103: o cartaz EM FOCO cabe inteiro, tambem no fim da fileira -------
  //
  // O relato veio com foto: fileira de 12 titulos, foco no 11o, e o cartao
  // aberto passando por baixo da borda direita da tela ("part of the focused
  // tile is cut off"). A causa estava no alvo da rolagem — ele media o cartao
  // EM REPOUSO e somava so metade da escala de foco, entao nem a abertura 16:9
  // nem o anel de 4 px entravam na conta.
  //
  // A asserção refaz, a partir do alvo, a MESMA conta que o desenho faz para
  // achar a borda direita do cartao focado; e a unica forma de o teste falhar
  // pelo motivo certo se alguem mexer num dos dois lados sozinho.
  {
    fileiras[0].tipo = FILEIRA_NORMAL;
    fileiras[0].n = 12;
    fileiras[0].stackN = 0;
    fileiras[0].verTudo = 0;
    fileiras[0].escala = 1.0f;
    if (nFileiras < 1) nFileiras = 1;

    const float lw    = larguraFil(0);
    const float passo = passoFil(0);
    const float esc   = 1.0f + escalaDe(FILEIRA_NORMAL);
    const float anel  = ajustes_borda_foco() ? NV_ANEL_FOCO : 0.0f;
    const float limite = NV_TELA_W - NV_HOME_SAFE_RIGHT;

    // As duas maneiras de o cartao ficar maior que a caixa em repouso: so a
    // escala de foco (abre 0) e a abertura em repouso ja completa (abre 1).
    for (int k = 0; k < 2; k++) {
      float abre = (float)k;
      for (int col = 0; col < fileiras[0].n; col++) {
        float sx = alvoScrollFil(0, col, 0.0f, abre);
        float w  = lw * esc
                 + (alturaFil(0) * esc * NV_EXP_ASPECTO - lw * esc) * abre;
        float cx = ajustes_conteudo_x() + (float)col * passo - sx
                 + lw * 0.5f + (w - lw * esc) * 0.5f;
        // Meio pixel de tolerancia: o alvo e float e o corte e visual.
        assert(cx + w * 0.5f + anel <= limite + 0.5f);
        // E nada some pela esquerda: a caixa em repouso do cartao focado nunca
        // comeca antes da margem de conteudo.
        assert(ajustes_conteudo_x() + (float)col * passo - sx >= -0.5f);
      }
    }
    // A primeira coluna nao rola: a fileira comeca onde sempre comecou.
    assert(alvoScrollFil(0, 0, 0.0f, 0.0f) == 0.0f);
    assert(alvoScrollFil(0, 0, 0.0f, 1.0f) == 0.0f);
    // E o ULTIMO cartaz, que e o da foto, exige rolagem MAIOR que a que a
    // conta antiga (so meia escala de foco) produzia.
    { int ult = fileiras[0].n - 1;
      float antigo = (float)ult * passo + lw
                   + lw * escalaDe(FILEIRA_NORMAL) * 0.5f
                   - (NV_TELA_W - ajustes_conteudo_x() - NV_HOME_SAFE_RIGHT);
      assert(alvoScrollFil(0, ult, 0.0f, 1.0f) > antigo + 1.0f); }

    // AS TRES PARCELAS, cada uma cobrada sozinha — sem isto, uma delas pode
    // sumir e o teste acima continuar verde porque as outras duas sobram.
    //
    // A parcela da ABERTURA e a que mais cresce e a que menos aparece nesta
    // execucao: com os ajustes de fabrica "Pôsteres horizontais" esta LIGADO,
    // o cartaz ja mede 16:9 (318x183 medidos aqui) e abrir quase nao o
    // aumenta. Quem relatou o #103 tem a opcao desligada — cartaz 2:3 — e ai
    // o aberto mede altura*16/9 contra a largura em pe: o pulo e de centenas
    // de pixels, e era ele que faltava. A asserção compara com a formula do
    // desenho para valer nas duas configuracoes.
    assert(sobraDireitaFoco(0, 1.0f) > sobraDireitaFoco(0, 0.0f));
    { float e = 1.0f + escalaDe(FILEIRA_NORMAL);
      float abertura = alturaFil(0) * e * NV_EXP_ASPECTO - lw * e;
      assert(fabsf(sobraDireitaFoco(0, 1.0f) - sobraDireitaFoco(0, 0.0f)
                   - abertura) < 0.01f); }
    // O anel so entra quando existe, e a escala so quando o card cresce: as
    // duas nunca valem juntas (ver escalaDe).
    assert(fabsf(sobraDireitaFoco(0, 0.0f)
                 - (lw * escalaDe(FILEIRA_NORMAL) * 0.5f
                    + (ajustes_borda_foco() ? NV_ANEL_FOCO : 0.0f))) < 0.01f);
  }

  free(itensTeste);
  puts("home layout: PASS (fallback, colecoes, ordem, ranks, foco, rolagem #103)");
  return 0;
}
