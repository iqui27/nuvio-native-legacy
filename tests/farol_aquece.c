// FAROL 43 — aquecimento do Diario, backup e notas antigas do Trakt.
//
//   * o passe do Diario alimenta o modelo no primeiro uso, em fatias, so em
//     momentoBom (player fechado) e cobrindo os 400 titulos mais novos;
//   * modelo restaurado SEM o estado (o backup do aparelho carrega so o
//     modelo) nao repassa o Diario — o cursor-fundo impede o peso dobrado,
//     e um RECARREGADO posterior respeita o mesmo marcador;
//   * farolsinal_recarregar le o arquivo recem-restaurado sem gravar o que
//     esta em RAM por cima dele; farolsinal_gravar desce o que a folga de
//     60 s ainda nao desceu;
//   * /sync/ratings vale UMA vez por conta: as notas viram sinais para
//     titulo que o modelo ainda nao conhece, a marca grava so com o perfil
//     certo residindo e o esquecer apaga modelo, cache e marca.
//
// Reusa os falsos do farol_sinal.c por inclusao, como o farol_varre — o que
// aqui e main la vira farol_suite2_main.
#define main farol_suite2_main
#include "farol_sinal.c"
#undef main

// Nome do marcador de notas: farol-notas-<fnv32(sessao_usuario)>.txt.
static void notasNome(char *dst, size_t cap) {
  unsigned long long h = 1469598103934665603ULL;
  const char *p;
  for (p = sessao_usuario(); *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211ULL; }
  snprintf(dst, cap, "farol-notas-%08llx.txt", h & 0xffffffffULL);
}
static int notasExiste(void) {
  char cam[640], nome[64];
  struct stat st;
  notasNome(nome, sizeof nome);
  snprintf(cam, sizeof cam, "%s/%s", T_dir, nome);
  return stat(cam, &st) == 0;
}
static void notasApaga(void) {
  char cam[640], nome[64];
  notasNome(nome, sizeof nome);
  snprintf(cam, sizeof cam, "%s/%s", T_dir, nome);
  unlink(cam);
}

// Semente do passe: `n` filmes terminados, ids ttNNN prefixados, genero e
// quando escolhidos pelo chamador. dPoe (sem evento) + dRecarrega armam a
// varredura; os passos seguintes a correm em fatias.
static void semeia(int n, const char *prefixo, const char *genero,
                   long long quando) {
  DiarioEntrada e;
  int i;
  char id[64];
  for (i = 0; i < n; i++) {
    snprintf(id, sizeof id, "%s%03d", prefixo, i);
    cat_add(id, "movie", 0, genero, id);
    entrada(&e, id, DIARIO_FILME, 0, 0, 0, 0);
    e.quando = quando + i;
    dPoe(&e);
  }
  dRecarrega();
}

int main(void) {
  DiarioEntrada e;
  FarolGosto g;
  int i, pAcao, pDrama, pAntes;
  char cam[640], *buf;
  long nBuf;
  FILE *fp;
  static int32_t genDrama, genCrime;

  strcpy(T_dir, "/tmp/nuvio-farol-aquece-XXXXXX");
  OK(mkdtemp(T_dir) != NULL, "mkdtemp");
  T_now = 1767225600LL + 40LL * 86400LL;
  T_ms = 10u * 1000u * 1000u;
  genAcao = (int32_t)mapa_hash_nome("Acao");
  genDrama = (int32_t)mapa_hash_nome("Drama");
  genCrime = (int32_t)mapa_hash_nome("Crime");
  farolsinal_relogio(fakeS, fakeMs);
  farolanel_relogio(fakeMs);
  farolsinal_pasta(T_dir);
  farolsinal_iniciar();
  farolsinal_busca(fakeBusca);

  // ---------- o passe do Diario alimenta o modelo no primeiro uso ----------
  zeraTudo(); notasApaga();
  semeia(30, "tt_w", "Acao", T_now - 3600);
  drenaTrab();                       // varredura inteira em fatias no passo
  OK(titulosModelo() == 30, "aquecimento: 30 titulos do diario no modelo");
  OK(peso(FG_DIM_GENERO, genAcao) > 0, "aquecimento: genero ganhou peso");
  {
    // RECARREGADO de novo: a varredura rearmada repassa as MESMAS entradas —
    // a dedup por eh segurava o peso antes da 43 e continua segurando.
    int p1 = peso(FG_DIM_GENERO, genAcao);
    dRecarrega();
    drenaTrab();
    OK(peso(FG_DIM_GENERO, genAcao) == p1, "repasse nao dobra o peso");
    OK(titulosModelo() == 30, "repasse nao dobra titulos");
  }

  // ---------- o teto e de 400 titulos, os mais novos primeiro --------------
  zeraTudo(); notasApaga();
  semeia(50, "tt_d", "Drama", T_now - 20000);        // as mais antigas
  semeia(400, "tt_w", "Acao", T_now - 5000);         // as 400 mais novas
  drenaTrab();
  OK(titulosModelo() == 400, "aquecimento para nos 400 titulos mais novos");
  OK(peso(FG_DIM_GENERO, genAcao) > 0, "as 400 novas pesaram");
  OK(peso(FG_DIM_GENERO, genDrama) == 0,
     "a 451a em diante nao pesa (cap de 400)");

  // ---------- o passe so anda em momentoBom (player fechado) ---------------
  zeraTudo(); notasApaga();
  semeia(8, "tt_w", "Acao", T_now - 3600);
  D_visita = 0;
  farolsinal_passo(0); farolsinal_passo(0); farolsinal_passo(0);
  OK(D_visita == 0, "passo com player aberto nao varre o diario");
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "passo com player aberto nao pesa");
  drenaTrab();
  OK(D_visita > 0, "momentoBom liberou a varredura");
  OK(peso(FG_DIM_GENERO, genAcao) > 0, "momentoBom liberou o peso");

  // A fila lenta de fichas tambem obedece: pendencia com player aberto nao
  // gera pedido; liberar o momento deixa a busca sair (e sempre uma de cada
  // vez — o fio e o mesmo da 38).
  zeraTudo(); notasApaga();
  entrada(&e, "tt_sem_ficha", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);                          // sem catalogo: pendencia + fila lenta
  OK(farolsinal_pendencias() == 1, "pendencia armada");
  B_chamadas = 0;
  farolsinal_passo(0);               // acorda o fio, mas momento ruim
  usleep(400000);
  OK(B_chamadas == 0, "fila lenta nao pede com player aberto");
  farolsinal_passo(1);
  for (i = 0; i < 4000 && B_chamadas == 0; i++) usleep(2000);
  OK(B_chamadas > 0, "momentoBom liberou a fila lenta");
  drenaTrab();

  // ---------- /sync/ratings: uma vez por conta, fora do fio da UI -----------
  zeraTudo(); notasApaga();
  cat_add("tt_r8", "movie", 0, "Acao", "R8");
  cat_add("tt_r2", "movie", 0, "Drama", "R2");
  cat_add("tt_r5", "movie", 0, "Crime", "R5");
  cat_add("tt_rs", "series", 0, "Acao", "RS");
  // O titulo com sinal vivo ignora a nota velha: a verdade mais fresca e a
  // do aparelho.
  cat_add("tt_vivo", "movie", 0, "Crime", "VIVO");
  entrada(&e, "tt_vivo", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genCrime) == FG_D_TERMINOU, "vivo creditou antes");
  OK(farolsinal_trakt_quer() == 1, "sem marca: a leitura e devida");
  {
    // rated_at no dia do relogio (dia 40 do modelo): sem decaimento, os
    // pesos ficam exatos — nota velha pesa decaida na vida real.
    static const char json[] =
      "[{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":8,"
      "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_r8\",\"tmdb\":101}}},"
      "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":2,"
      "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_r2\",\"tmdb\":102}}},"
      "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":5,"
      "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_r5\",\"tmdb\":103}}},"
      "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":9,"
      "\"type\":\"episode\",\"episode\":{\"ids\":{\"imdb\":\"tt_re\",\"tmdb\":104}}},"
      "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":10,"
      "\"type\":\"show\",\"show\":{\"ids\":{\"imdb\":\"tt_rs\",\"tmdb\":105}}},"
      "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":1,"
      "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_vivo\",\"tmdb\":106}}}]";
    // Perfil errado: nada aplica e a marca NAO grava — a volta seguinte
    // tenta de novo.
    farolsinal_trakt_notas(2, json);
    OK(peso(FG_DIM_GENERO, genAcao) == 0, "perfil errado nao aplica");
    OK(farolsinal_trakt_quer() == 1, "perfil errado nao grava a marca");
    farolsinal_trakt_notas(1, json);
    OK(farolsinal_trakt_quer() == 0, "marca gravada: nunca mais le");
    OK(peso(FG_DIM_GENERO, genDrama) == FG_D_REPROVOU, "nota 2 vira -2.0");
    OK(peso(FG_DIM_GENERO, genAcao) ==
       esp(esp(0, 0, FG_D_GOSTOU), 1, FG_D_GOSTOU),
       "nota 8 de filme + nota 10 de serie = dois +2.0");
    OK(peso(FG_DIM_GENERO, genCrime) == FG_D_TERMINOU,
       "nota 5 nao pesa e o titulo vivo ignora a nota velha");
    // Segunda entrega do mesmo corpo: a hb por titulo deduplica.
    farolsinal_trakt_notas(1, json);
    OK(peso(FG_DIM_GENERO, genDrama) == FG_D_REPROVOU,
       "reentrega das notas nao dobra");
    OK(titulosModelo() == 4, "so os titulos com peso contam");
  }

  // ---------- esquecer apaga modelo, cache de fichas e a marca -------------
  {
    char camF[640];
    farolsinal_esquecer();
    snprintf(cam, sizeof cam, "%s/farol-gosto-p1.txt", T_dir);
    OK(stat(cam, &(struct stat){0}) != 0, "esquecer apaga o modelo");
    OK(!notasExiste(), "esquecer apaga a marca das notas");
    snprintf(camF, sizeof camF, "%s/farol-fichas.bin", T_dir);
    OK(stat(camF, &(struct stat){0}) != 0, "esquecer apaga o cache de fichas");
  }

  // ---------- modelo sem estado nao repassa o Diario (restore do backup) ----
  zeraTudo(); notasApaga();
  cat_add("tt_b1", "movie", 0, "Acao", "B1");
  entrada(&e, "tt_b1", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  pAntes = peso(FG_DIM_GENERO, genAcao);
  OK(pAntes == FG_D_TERMINOU, "sinal ao vivo creditou");
  farolsinal_sair();                 // grava modelo + estado
  {
    // O backup do aparelho so carrega o MODELO: some o arquivo de estado e
    // o cursor vira "o diario inteiro ja esta dentro" (issue 43).
    char camS[640];
    snprintf(camS, sizeof camS, "%s/farol-sinal-p1.txt", T_dir);
    OK(unlink(camS) == 0, "estado removido (restaurou so o modelo)");
  }
  farolsinal_teste_zera();
  // Recarrega o perfil do disco — travaPronta rouba a troca e a executa aqui.
  OK(farolsinal_modelo(&g), "modelo recarrega do disco");
  OK(g.titulos == 1, "modelo restaurado tem o titulo");
  // As MESMAS entradas voltam ao Diario e um RECARREGADO chega: a varredura
  // nao pode rearmar — o cursor-fundo segura o peso restaurado.
  dRecarrega();
  drenaTrab();
  OK(peso(FG_DIM_GENERO, genAcao) == pAntes,
     "modelo de backup nao repassa o diario (sem dobro)");
  OK(titulosModelo() == 1, "modelo de backup nao conta de novo");

  // ---------- recarregar le o restaurado sem pisar nele; gravar desce ja ----
  zeraTudo(); notasApaga();
  cat_add("tt_b2", "movie", 0, "Acao", "B2");
  entrada(&e, "tt_b2", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  farolsinal_gravar();               // desce sem esperar a folga de 60 s
  snprintf(cam, sizeof cam, "%s/farol-gosto-p1.txt", T_dir);
  fp = fopen(cam, "rb");
  OK(fp != NULL, "gravar escreveu o modelo na hora");
  OK(fp && !fseek(fp, 0, SEEK_END) && (nBuf = ftell(fp)) > 0, "le o arquivo");
  buf = malloc((size_t)nBuf + 1); assert(buf);
  rewind(fp);
  OK(fread(buf, 1, (size_t)nBuf, fp) == (size_t)nBuf, "le o arquivo todo");
  buf[nBuf] = 0;
  fclose(fp);
  // A restauracao regrava o arquivo por baixo com uma versao mais velha
  // (simula o instantaneo vindo de outra TV).
  entrada(&e, "tt_b3", DIARIO_FILME, 0, 0, 0, 0);
  cat_add("tt_b3", "movie", 0, "Acao", "B3");
  dAdd(&e);
  farolsinal_gravar();
  OK(peso(FG_DIM_GENERO, genAcao) > pAntes, "RAM evoluiu depois do arquivo");
  {
    FILE *w = fopen(cam, "wb");
    OK(w != NULL, "regrava o modelo restaurado");
    fwrite(buf, 1, (size_t)nBuf, w);
    fclose(w);
  }
  free(buf);
  farolsinal_recarregar();
  drenaTrab();
  OK(farolsinal_modelo(&g), "modelo depois do recarregar");
  OK(g.titulos == 1, "o restaurado venceu: o que era RAM nao pisou");
  OK(peso(FG_DIM_GENERO, genAcao) == pAntes, "peso do arquivo restaurado");

  // ---------- mapa_clima_gosto: o modelo informa a ordem dos climas ---------
  {
    // Modelo com afinidade em Acao (tmdb 28 filme / 10759 serie): o clima
    // "Sobreviver juntos" (id 3, Acao+Aventura E Drama) ganha peso; o clima
    // "Crime e culpa" (id 7, Crime) fica em zero.
    FarolGosto mg;
    FarolGostoFicha mf;
    memset(&mg, 0, sizeof mg);
    farolgosto_iniciar(&mg, 1);
    memset(&mf, 0, sizeof mf);
    mf.tmdb = 1; mf.tipo = FG_TIPO_FILME; mf.ano = 2010; mf.minutos = 100;
    mf.genero[0] = 28;
    farolgosto_aplicar(&mg, &mf, FG_D_GOSTOU, (uint16_t)diaNow(), 0, 1);
    OK(mapa_clima_gosto(&mg, 3, (uint16_t)diaNow()) >= FG_D_GOSTOU,
       "clima com o genero amado tem peso do modelo");
    OK(mapa_clima_gosto(&mg, 7, (uint16_t)diaNow()) == 0,
       "clima sem o genero nao tem peso");
    OK(mapa_clima_gosto(NULL, 3, (uint16_t)diaNow()) == 0, "sem modelo = 0");
    OK(mapa_clima_gosto(&mg, -1, (uint16_t)diaNow()) == 0, "clima ruim = 0");
  }

  printf("farol_aquece: tudo ok\n");
  return 0;
}
