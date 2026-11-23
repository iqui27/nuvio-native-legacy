// FAROL 43 — aquecimento do Diario, backup e notas antigas do Trakt.
//
//   * o passe do Diario alimenta o modelo no primeiro uso, em fatias, so em
//     momentoBom (player fechado) e cobrindo os 400 titulos mais novos;
//   * modelo restaurado SEM o estado (o backup do aparelho carrega so o
//     modelo) nao repassa o Diario — o cursor-fundo impede o peso dobrado,
//     e um RECARREGADO posterior respeita o mesmo marcador;
//   * modelo restaurado NAO fica surdo: a marca `curRest` do diario separa
//     o que ja era dele do que chegou depois — o lote novo volta a pesar;
//   * farolsinal_recarregar le o arquivo recem-restaurado sem gravar o que
//     esta em RAM por cima dele; farolsinal_gravar desce o que a folga de
//     60 s ainda nao desceu;
//   * /sync/ratings vale UMA vez por conta: as notas viram sinais para
//     titulo que o modelo ainda nao conhece, a marca grava so com o perfil
//     certo residindo e o esquecer apaga modelo, cache e marca — de TODOS
//     os perfis da conta, nao so do ativo;
//   * nota e vez do mesmo titulo sao um sinal so: fica o maior dos dois,
//     em qualquer ordem de chegada — nunca a soma.
//
// Reusa os falsos do farol_sinal.c por inclusao, como o farol_varre — o que
// aqui e main la vira farol_suite2_main.
#define main farol_suite2_main
#include "farol_sinal.c"
#undef main
#include <dirent.h>

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

// Carrega o perfil no fio de tras: a consulta nao rouba mais a troca para a
// thread chamadora (43) — quem espera o modelo pronto bombeia os passos ate
// o trabalho acabar.
static void carregaPerfil(void) {
  FarolGosto tmp;
  farolsinal_modelo(&tmp);      // marca a troca e volta "ainda nao"
  drenaTrab();                  // troca + dia-frio + fatias rodam no fio
}

int main(void) {
  DiarioEntrada e;
  FarolGosto g;
  int i, pAntes;
  char cam[640], *buf;
  long nBuf;
  FILE *fp;
  static int32_t genDrama, genCrime, genFamilia, genFantasia;
  const char *tmpdir = getenv("TMPDIR");

  // O rascunho respeita TMPDIR (a roda.sh manda um /tmp privado); sem ele,
  // fica em /tmp como sempre — o fim da regua apaga a pasta de qualquer jeito.
  snprintf(T_dir, sizeof T_dir, "%s/nuvio-farol-aquece-XXXXXX",
           tmpdir && *tmpdir ? tmpdir : "/tmp");
  OK(mkdtemp(T_dir) != NULL, "mkdtemp");
  T_now = 1767225600LL + 40LL * 86400LL;
  T_ms = 10u * 1000u * 1000u;
  genAcao = (int32_t)mapa_hash_nome("Acao");
  genDrama = (int32_t)mapa_hash_nome("Drama");
  genCrime = (int32_t)mapa_hash_nome("Crime");
  genFamilia = (int32_t)mapa_hash_nome("Familia");
  genFantasia = (int32_t)mapa_hash_nome("Fantasia");
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
  // Um RECARREGADO arma uma varredura NOVA: os titulos ja contados dela sao
  // outros — os 5 mais novos entram mesmo com os 400 antigos no anel.
  semeia(5, "tt_dd", "Drama", T_now - 100);
  drenaTrab();
  OK(peso(FG_DIM_GENERO, genDrama) > 0,
     "varredura rearmada conta os titulos novos");

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

  // ---------- nota e vez do mesmo titulo sao um sinal so (o maior) ---------
  zeraTudo(); notasApaga();
  cat_add("tt_mx1", "movie", 0, "Acao", "MX1");
  cat_add("tt_mx2", "movie", 0, "Drama", "MX2");
  cat_add("tt_mx3", "movie", 0, "Crime", "MX3");
  // Nota primeiro: o "terminou" que chega depois nao soma — fica o maior.
  farolsinal_trakt_notas(1,
    "[{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":8,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_mx1\",\"tmdb\":201}}}]");
  entrada(&e, "tt_mx1", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == esp(0, 0, FG_D_GOSTOU),
     "nota 8 + terminou = o maior, nao a soma");
  // Vez primeiro: a nota que chega depois so completa ate o maior.
  entrada(&e, "tt_mx2", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genDrama) == esp(0, 0, FG_D_TERMINOU),
     "terminou sozinho pesa +1.0");
  farolsinal_trakt_notas(1,
    "[{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":8,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_mx2\",\"tmdb\":202}}}]");
  OK(peso(FG_DIM_GENERO, genDrama) ==
     esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_GOSTOU - FG_D_TERMINOU),
     "terminou + nota 8 completa ate o maior");
  // E alterar a vez para "gostou" continua teto-do-maior, nunca soma.
  entrada(&e, "tt_mx3", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  farolsinal_trakt_notas(1,
    "[{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":8,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_mx3\",\"tmdb\":203}}}]");
  e.flags = DIARIO_F_CORACAO;
  dEv(DIARIO_EV_ALTERADA, &e);
  OK(peso(FG_DIM_GENERO, genCrime) ==
     esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_GOSTOU - FG_D_TERMINOU),
     "alterada para gostou fica no maior com a nota");

  // ---------- os cortes de nota sao secos: 3/4 pesam, 6/7 tambem -----------
  zeraTudo(); notasApaga();
  cat_add("tt_n3", "movie", 0, "Acao", "N3");
  cat_add("tt_n4", "movie", 0, "Drama", "N4");
  cat_add("tt_n6", "movie", 0, "Familia", "N6");
  cat_add("tt_n7", "movie", 0, "Fantasia", "N7");
  farolsinal_trakt_notas(1,
    "[{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":3,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_n3\",\"tmdb\":301}}},"
    "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":4,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_n4\",\"tmdb\":302}}},"
    "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":6,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_n6\",\"tmdb\":303}}},"
    "{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":7,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_n7\",\"tmdb\":304}}}]");
  OK(peso(FG_DIM_GENERO, genAcao) == esp(0, 0, FG_D_REPROVOU),
     "nota 3 vira -2.0 (o corte e <=3)");
  OK(peso(FG_DIM_GENERO, genDrama) == 0, "nota 4 nao pesa");
  OK(peso(FG_DIM_GENERO, genFamilia) == 0, "nota 6 nao pesa");
  OK(peso(FG_DIM_GENERO, genFantasia) == esp(0, 0, FG_D_GOSTOU),
     "nota 7 vira +2.0 (o corte e >=7)");

  // ---------- a pendencia de nota atravessa a gravacao do estado -----------
  zeraTudo(); notasApaga();
  // Sem busca a ficha nunca chega: a pendencia K_NOTA fica viva para a
  // gravacao do estado — e a volta dela confere o round-trip (a linha 'p'
  // do estado aceita kind de nota tambem).
  farolsinal_busca(NULL);
  farolsinal_trakt_notas(1,
    "[{\"rated_at\":\"2026-02-09T16:00:00.000Z\",\"rating\":9,"
    "\"type\":\"movie\",\"movie\":{\"ids\":{\"imdb\":\"tt_nocat\",\"tmdb\":999}}}]");
  OK(farolsinal_pendencias() >= 1, "nota sem ficha virou pendencia");
  farolsinal_sair();
  farolsinal_teste_zera();
  carregaPerfil();
  OK(farolsinal_pendencias() >= 1,
     "pendencia de nota sobreviveu ao sair/recarregar");
  farolsinal_busca(fakeBusca);

  // ---------- esquecer apaga modelo, cache de fichas e a marca -------------
  {
    char camF[640];
    FILE *w;
    // O logout esquece a CONTA: os arquivos dos outros perfis tambem saem.
    snprintf(cam, sizeof cam, "%s/farol-gosto-p2.txt", T_dir);
    OK((w = fopen(cam, "wb")) != NULL, "modelo do perfil 2 no disco");
    if (w) { fputs("FG 2 2 1 320 0 0\n", w); fclose(w); }
    snprintf(cam, sizeof cam, "%s/farol-sinal-p2.txt", T_dir);
    OK((w = fopen(cam, "wb")) != NULL, "estado do perfil 2 no disco");
    if (w) { fputs("FS 1\n", w); fclose(w); }
    farolsinal_esquecer();
    snprintf(cam, sizeof cam, "%s/farol-gosto-p1.txt", T_dir);
    OK(stat(cam, &(struct stat){0}) != 0, "esquecer apaga o modelo");
    snprintf(cam, sizeof cam, "%s/farol-sinal-p1.txt", T_dir);
    OK(stat(cam, &(struct stat){0}) != 0, "esquecer apaga o estado");
    snprintf(cam, sizeof cam, "%s/farol-gosto-p2.txt", T_dir);
    OK(stat(cam, &(struct stat){0}) != 0,
       "esquecer apaga o modelo do perfil 2");
    snprintf(cam, sizeof cam, "%s/farol-sinal-p2.txt", T_dir);
    OK(stat(cam, &(struct stat){0}) != 0,
       "esquecer apaga o estado do perfil 2");
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
  // Recarrega o perfil do disco — a consulta nao rouba a troca: ela roda no
  // fio de tras e o ralo bombeia ate o modelo residir.
  carregaPerfil();
  OK(farolsinal_modelo(&g), "modelo recarrega do disco");
  OK(g.titulos == 1, "modelo restaurado tem o titulo");
  // As MESMAS entradas voltam ao Diario e um RECARREGADO chega: a varredura
  // nao pode rearmar — o cursor-fundo segura o peso restaurado.
  dRecarrega();
  drenaTrab();
  OK(peso(FG_DIM_GENERO, genAcao) == pAntes,
     "modelo de backup nao repassa o diario (sem dobro)");
  OK(titulosModelo() == 1, "modelo de backup nao conta de novo");
  // Mas o que chega DEPOIS da restauracao tem de pesar (ressalva R1): a
  // marca do diario separa "ja era do modelo" de "novo desde entao".
  avanca(120);
  semeia(3, "tt_n", "Drama", T_now);
  drenaTrab();
  OK(peso(FG_DIM_GENERO, genDrama) > 0,
     "lote novo pesa no modelo restaurado");
  OK(peso(FG_DIM_GENERO, genAcao) == pAntes,
     "o diario antigo continua sem dobrar");

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

  // ---------- a marca do restore segura o gravar na janela ----------
  zeraTudo(); notasApaga();
  carregaPerfil();
  farolsinal_definir_recomendar(0);  // sujo sem fio (rec vira 0)
  snprintf(cam, sizeof cam, "%s/farol-gosto-p1.txt", T_dir);
  unlink(cam);                   // o restore veio SEM o arquivo do modelo
  farolsinal_restaura_marcar();  // a janela abre antes do arquivo pousar
  farolsinal_gravar();           // nem o gravar explicito regrava (ressalva R5)
  OK(stat(cam, &(struct stat){0}) != 0,
     "gravar sob a marca do restore nao regrava o modelo");
  farolsinal_recarregar();
  drenaTrab();
  OK(farolsinal_modelo(&g) && g.titulos == 0,
     "sem arquivo restaurado, o modelo zera");

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
    // O genero E do clima tambem soma: Drama so entra no "Sobreviver
    // juntos" pelo genE — sem o loop do i=-1 o clima ficava em zero.
    memset(&mg, 0, sizeof mg); farolgosto_iniciar(&mg, 1);
    memset(&mf, 0, sizeof mf);
    mf.tmdb = 2; mf.tipo = FG_TIPO_FILME; mf.ano = 2010; mf.minutos = 100;
    mf.genero[0] = 18;                    // Drama
    farolgosto_aplicar(&mg, &mf, FG_D_GOSTOU, (uint16_t)diaNow(), 0, 1);
    OK(mapa_clima_gosto(&mg, 3, (uint16_t)diaNow()) >= FG_D_GOSTOU,
       "clima conta o genero E (Drama) do modelo");
    OK(mapa_clima_gosto(&mg, 7, (uint16_t)diaNow()) == 0,
       "clima sem nenhum genero do modelo fica em zero");
  }

  // A pasta de rascunho sai com a regua (a roda.sh tem /tmp privado, mas o
  // teste tambem roda a mao — nao deixa farol-* para tras).
  {
    DIR *d; struct dirent *de;
    if ((d = opendir(T_dir)) != NULL) {
      while ((de = readdir(d)) != NULL)
        if (de->d_name[0] != '.') {
          snprintf(cam, sizeof cam, "%s/%s", T_dir, de->d_name);
          unlink(cam);
        }
      closedir(d);
    }
    rmdir(T_dir);
  }

  printf("farol_aquece: tudo ok\n");
  return 0;
}
