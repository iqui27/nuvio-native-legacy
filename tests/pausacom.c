// Comentarios do Trakt para o painel de pausa: parser (pausacom_interpretar) com
// corpos canned, e o ciclo pedir/obter/limpar com a rede dublada.
//
//   bash tests/pausacom.sh
//
// Compila so src/js.c e este arquivo. A REDE E AS CREDENCIAIS SAO DUBLES:
// trakt.h e pulado pela guarda NV_TRAKT_H (ele puxa SDL pelo perfil.h), e as
// funcoes de credencial sao definidas aqui. Os corpos JSON sao escritos a mao
// no formato que a api devolve (array de {comment, spoiler, review, user,
// user_rating}).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NV_TRAKT_H
int trakt_cabecalhos(const char **cab, char *aut, size_t nAut, char *chave,
                     size_t nChave);
int trakt_cabecalhos_publicos(const char **cab, char *chave, size_t nChave);
char *rede_baixar_com(const char *url, int segundos,
                      const char *const *cabecalhos);

#include "../src/pausacom.c"

// --- DUBLES ------------------------------------------------------------------

static int comChave = 1;     // 0 = sem nenhuma credencial do Trakt
static int nPedidos;
static char ultimaUrl[300];
static const char *respFilme;
static const char *respEp;

int trakt_cabecalhos(const char **cab, char *aut, size_t nAut, char *chave,
                     size_t nChave) {
  (void)cab; (void)aut; (void)nAut; (void)chave; (void)nChave;
  return 0;   // sem token: o teste usa o caminho publico
}

int trakt_cabecalhos_publicos(const char **cab, char *chave, size_t nChave) {
  if (!comChave) return 0;
  snprintf(chave, nChave, "trakt-api-key: teste");
  cab[0] = "trakt-api-version: 2";
  cab[1] = chave;
  cab[2] = NULL;
  return 1;
}

static char *dup(const char *s) {
  char *r = (char *)malloc(strlen(s) + 1);
  if (r) strcpy(r, s);
  return r;
}

// Segura os pedidos DENTRO da rede: cada `liberar` solta UM. Permite trocar de
// chave com um pedido em voo e olhar o estado entre o fim do velho e o novo.
static volatile int seguraRede, dentroDaRede, liberar;

char *rede_baixar_com(const char *url, int segundos, const char *const *cab) {
  const char *r;
  (void)segundos; (void)cab;
  nPedidos++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  r = strstr(url, "/movies/") ? respFilme : respEp;   // a resposta e a de QUANDO saiu
  if (seguraRede) {
    dentroDaRede = 1;
    while (seguraRede && !liberar) { struct timespec ts = {0, 2 * 1000 * 1000}; nanosleep(&ts, NULL); }
    if (liberar) liberar--;
    dentroDaRede = 0;
  }
  return r ? dup(r) : NULL;
}

// --- CONTAGEM ----------------------------------------------------------------

static int falhas, testes;

#define CHECA(cond, ...)                                  \
  do {                                                    \
    testes++;                                             \
    if (!(cond)) {                                        \
      falhas++;                                           \
      printf("FALHOU %s:%d: ", __FILE__, __LINE__);       \
      printf(__VA_ARGS__);                                \
      printf("\n");                                       \
    }                                                     \
  } while (0)

static int utf8Valido(const char *s, size_t n) {
  size_t i = 0;
  while (i < n) {
    unsigned char c = (unsigned char)s[i];
    size_t need, j;
    if (c < 0x80) need = 0;
    else if ((c >> 5) == 6) need = 1;
    else if ((c >> 4) == 14) need = 2;
    else if ((c >> 3) == 30) need = 3;
    else return 0;
    if (i + need >= n) return 0;
    for (j = 1; j <= need; j++)
      if (((unsigned char)s[i + j] & 0xC0) != 0x80) return 0;
    i += need + 1;
  }
  return 1;
}

// --- CASOS DO PARSER ---------------------------------------------------------

static void teste_filtra(void) {
  static const char *json =
    "[\n"
    " {\"comment\":\"Este comentario tem mais de vinte letras de verdade.\","
    "\"spoiler\":false,\"review\":false,\"user\":{\"username\":\"maria\"},"
    "\"user_rating\":9},\n"
    " {\"comment\":\"Spoiler: o vilao era o pai o tempo todo mesmo.\","
    "\"spoiler\":true,\"review\":false,\"user\":{\"username\":\"spoilerman\"},"
    "\"user_rating\":null},\n"
    " {\"comment\":\"curto\",\"spoiler\":false,\"review\":false,"
    "\"user\":{\"username\":\"curto\"},\"user_rating\":null},\n"
    " {\"comment\":\"Outro comentario valido, bem legal e comprido.\","
    "\"spoiler\":false,\"review\":true,\"user\":{\"username\":\"joao\"}}\n"
    "]";
  PcComentario c[PC_MAX];
  int n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 2, "filtro: esperava 2, veio %d", n);
  if (n == 2) {
    CHECA(!strcmp(c[0].texto, "Este comentario tem mais de vinte letras de verdade."),
          "filtro: texto 0 = '%s'", c[0].texto);
    CHECA(!strcmp(c[0].autor, "maria"), "filtro: autor 0 = '%s'", c[0].autor);
    CHECA(c[0].nota == 9, "filtro: nota 0 = %d (esperava 9)", c[0].nota);
    CHECA(!strcmp(c[1].autor, "joao"), "filtro: autor 1 = '%s'", c[1].autor);
    CHECA(c[1].nota == 0, "filtro: nota ausente = %d (esperava 0)", c[1].nota);
  }
}

static void teste_notas(void) {
  static const char *json =
    "[{\"comment\":\"nota onze fora da escala aqui\",\"user\":{\"username\":\"a\"},"
    "\"user_rating\":11},"
    "{\"comment\":\"nota tres dentro da escala ok\",\"user\":{\"username\":\"b\"},"
    "\"user_rating\":3}]";
  PcComentario c[PC_MAX];
  int n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 2, "notas: esperava 2, veio %d", n);
  if (n == 2) {
    CHECA(c[0].nota == 0, "notas: 11 deve virar 0, veio %d", c[0].nota);
    CHECA(c[1].nota == 3, "notas: 3 veio %d", c[1].nota);
  }
}

static void teste_quebras(void) {
  static const char *json =
    "[{\"comment\":\"primeira linha\\n\\n   segunda\\tlinha\\r\\nterceira com texto longo o bastante\","
    "\"spoiler\":false,\"user\":{\"username\":\"ana\"}}]";
  PcComentario c[PC_MAX];
  int n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 1, "quebras: esperava 1, veio %d", n);
  if (n == 1)
    CHECA(!strcmp(c[0].texto,
                  "primeira linha segunda linha terceira com texto longo o bastante"),
          "quebras: texto = '%s'", c[0].texto);
}

static void teste_escapes(void) {
  // JSON: A \"quoted\" \u00e7 \\ e \/ fim \u00e9 bom mesmo  (\u00e7 = c-cedilha)
  static const char *json =
    "[{\"comment\":\"A \\\"quoted\\\" \\u00e7 \\\\ e \\/ fim \\u00e9 bom mesmo\","
    "\"user\":{\"username\":\"x\\u00e7\"}}]";
  PcComentario c[PC_MAX];
  int n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 1, "escapes: esperava 1, veio %d", n);
  if (n == 1) {
    CHECA(!strcmp(c[0].texto, "A \"quoted\" \xc3\xa7 \\ e / fim \xc3\xa9 bom mesmo"),
          "escapes: texto = '%s'", c[0].texto);
    CHECA(!strcmp(c[0].autor, "x\xc3\xa7"), "escapes: autor = '%s'", c[0].autor);
  }
}

// Monta "[{...ç...}]" com um comentario de `reps` cedilhas (2 bytes cada),
// depois do prefixo `pre`.
static int monta_cedilhas(char *dst, size_t tam, const char *pre, int reps) {
  size_t k = (size_t)snprintf(dst, tam, "[{\"comment\":\"%s", pre);
  int i;
  for (i = 0; i < reps && k + 3 < tam; i++) { dst[k++] = '\xc3'; dst[k++] = '\xa7'; }
  snprintf(dst + k, tam - k, "\",\"user\":{\"username\":\"u\"}}]");
  return (int)strlen(dst);
}

static void teste_utf8_corte(void) {
  static char json[2048];
  PcComentario c[PC_MAX];
  int n;
  size_t len;

  // 'a' + 200 cedilhas = 401 bytes. O byte 280 cai no meio de uma cedilha, entao
  // o corte recua para 279 bytes (1 + 139 caracteres) e entra "...".
  monta_cedilhas(json, sizeof json, "a", 200);
  n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 1, "corte: esperava 1, veio %d", n);
  if (n == 1) {
    len = strlen(c[0].texto);
    CHECA(len == 282, "corte: tamanho %zu (esperava 282)", len);
    CHECA(!strcmp(c[0].texto + len - 3, "..."), "corte: sem '...' no fim");
    CHECA(utf8Valido(c[0].texto, len), "corte: UTF-8 quebrado");
  }

  // 140 cedilhas = 280 bytes exatos: nao corta, nao tem "...".
  monta_cedilhas(json, sizeof json, "", 140);
  n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 1, "limite exato: esperava 1, veio %d", n);
  if (n == 1) {
    CHECA(strlen(c[0].texto) == 280 && !strstr(c[0].texto, "..."),
          "limite exato: tamanho %zu, com '...' = %d", strlen(c[0].texto),
          strstr(c[0].texto, "...") != NULL);
  }
}

static void teste_limite(void) {
  static char json[8192];
  PcComentario c[PC_MAX];
  size_t k = 0;
  int i, n;
  k += (size_t)snprintf(json + k, sizeof json - k, "[");
  for (i = 1; i <= 12; i++)
    k += (size_t)snprintf(json + k, sizeof json - k,
                          "%s{\"comment\":\"Comentario numero %d com texto suficiente\","
                          "\"user\":{\"username\":\"u%d\"}}",
                          i > 1 ? "," : "", i, i);
  snprintf(json + k, sizeof json - k, "]");
  n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == PC_MAX, "teto: esperava %d, veio %d", PC_MAX, n);
  if (n == PC_MAX)
    CHECA(!strcmp(c[PC_MAX - 1].texto, "Comentario numero 8 com texto suficiente"),
          "teto: ultimo = '%s'", c[PC_MAX - 1].texto);
  n = pausacom_interpretar(json, c, 3);
  CHECA(n == 3, "max=3: esperava 3, veio %d", n);
}

static void teste_malformado(void) {
  static const char *casos[] = {
    "",
    "isto nao e json",
    "{\"comment\":\"Comentario com texto suficiente aqui\"}",
    "[{\"comment\":\"Comentario com texto suficiente aqui\",\"user\":{\"username\":\"a\"}}",
    "[{\"comment\":\"Comentario com texto suficiente aqui\",\"user\":{\"username\":\"a\"}}]x",
    "[{\"comment\":\"Comentario com texto suficiente aqui",
    "[{\"comment\":\"Comentario com texto suficiente aqui\"}}]",
    "[1,2,3]",
    "[\"so string\"]",
    NULL,
  };
  PcComentario c[PC_MAX];
  size_t i;
  for (i = 0; i < sizeof casos / sizeof casos[0]; i++) {
    int n = pausacom_interpretar(casos[i], c, PC_MAX);
    CHECA(n == 0, "malformado #%zu: esperava 0, veio %d", i, n);
  }
  {
    // Aninhamento absurdo: 40 colchetes abertos (acima de PC_PROF).
    char fundo[64];
    int k;
    for (k = 0; k < 40; k++) fundo[k] = '[';
    fundo[40] = 0;
    CHECA(pausacom_interpretar(fundo, c, PC_MAX) == 0, "profundidade: aceitou 40 '['");
  }
  CHECA(pausacom_interpretar("[]", c, PC_MAX) == 0, "array vazio deve dar 0");
}

// --- CICLO PEDIR / OBTER / LIMPAR -------------------------------------------

static void espera(int (*cond)(void)) {
  int i;
  struct timespec ts = {0, 10 * 1000 * 1000};
  for (i = 0; i < 500 && !cond(); i++) nanosleep(&ts, NULL);
}

static int tem3(void) { return pausacom_n() == 3; }
static int tem2(void) { return pausacom_n() == 2; }
static int vazio(void) { return pausacom_n() == 0; }

static void espera_fio(void);
static void teste_ciclo(void) {
  static const char *filme =
    "[{\"comment\":\"Filme bom com texto suficiente de sobra\",\"user\":{\"username\":\"f1\"},\"user_rating\":8},"
    "{\"comment\":\"Segundo comentario do filme tambem valido\",\"user\":{\"username\":\"f2\"}}]";
  static const char *episodio =
    "[{\"comment\":\"Episodio A com texto suficiente aqui\",\"user\":{\"username\":\"e1\"}},"
    "{\"comment\":\"Episodio B com texto suficiente aqui\",\"user\":{\"username\":\"e2\"}},"
    "{\"comment\":\"Episodio C com texto suficiente aqui\",\"user\":{\"username\":\"e3\"},\"user_rating\":5}]";
  PcComentario cm;
  int ok;

  respFilme = filme;
  respEp = episodio;

  // Filme: URL de filmes, dois comentarios, nota da primeira.
  pausacom_pedir("tt0111161", 0, 0);
  espera(tem2);
  CHECA(pausacom_n() == 2, "ciclo: filme esperava 2, veio %d", pausacom_n());
  CHECA(strstr(ultimaUrl, "/movies/tt0111161/comments/likes?limit=10") != NULL,
        "ciclo: URL de filme errada: %s", ultimaUrl);
  ok = pausacom_obter(0, &cm);
  CHECA(ok == 1 && !strcmp(cm.autor, "f1") && cm.nota == 8 &&
        !strcmp(cm.texto, "Filme bom com texto suficiente de sobra"),
        "ciclo: obter(0) = %d '%s' '%s' nota %d", ok, cm.autor, cm.texto, cm.nota);
  CHECA(pausacom_obter(2, &cm) == 0, "ciclo: obter fora da faixa devolveu 1");
  CHECA(cm.nota == 0 && cm.texto[0] == 0 && cm.autor[0] == 0 && cm.curtidas == 0 && cm.lingua[0] == 0,
        "ciclo: obter falho deve zerar a saida");

  // Mesma chave: nao refaz o pedido.
  {
    int antes = nPedidos;
    pausacom_pedir("tt0111161", 0, 0);
    { struct timespec ts = {0, 50 * 1000 * 1000}; nanosleep(&ts, NULL); }
    CHECA(nPedidos == antes, "ciclo: mesma chave refez o pedido (%d -> %d)", antes, nPedidos);
  }

  // Trocar para episodio logo em seguida: so o resultado do episodio fica.
  pausacom_pedir("tt0944947", 1, 2);
  espera(tem3);
  CHECA(pausacom_n() == 3, "ciclo: episodio esperava 3, veio %d", pausacom_n());
  CHECA(strstr(ultimaUrl, "/shows/tt0944947/seasons/1/episodes/2/comments/likes?limit=10") != NULL,
        "ciclo: URL de episodio errada: %s", ultimaUrl);
  ok = pausacom_obter(0, &cm);
  CHECA(ok == 1 && !strncmp(cm.texto, "Episodio A", 10), "ciclo: resultado antigo vazou: '%s'", cm.texto);
  CHECA(pausacom_obter(2, &cm) == 1 && cm.nota == 5,
        "ciclo: nota do episodio C = %d", cm.nota);

  // Limpar zera; obter nao devolve nada.
  pausacom_limpar();
  CHECA(pausacom_n() == 0, "ciclo: limpar deixou %d", pausacom_n());
  CHECA(pausacom_obter(0, &cm) == 0,
        "ciclo: obter depois de limpar devolveu 1");

  // Sem credencial: nenhum pedido sai, nada aparece.
  comChave = 0;
  nPedidos = 0;
  pausacom_pedir("tt0000001", 0, 0);
  espera(vazio);
  { struct timespec ts = {0, 50 * 1000 * 1000}; nanosleep(&ts, NULL); }
  CHECA(nPedidos == 0 && pausacom_n() == 0, "sem chave: pediu %d vezes", nPedidos);
  comChave = 1;

  // Id hostil: rejeitado antes da rede.
  nPedidos = 0;
  pausacom_pedir("tt1/../x", 0, 0);
  { struct timespec ts = {0, 50 * 1000 * 1000}; nanosleep(&ts, NULL); }
  CHECA(nPedidos == 0 && pausacom_n() == 0, "id hostil chegou a rede");
  pausacom_limpar();

  // Ids que nao sao do IMDb (addon, servidor pessoal) nunca vao ao Trakt.
  { const char *ids[] = { "kitsu:41370", "tmdb:603", "jf:abc.1", "em:9", "px:1", "", "tt", "ttabc", NULL };
    for (int i = 0; ids[i]; i++) {
      nPedidos = 0;
      pausacom_pedir(ids[i], 1, 1);
      { struct timespec ts = {0, 30 * 1000 * 1000}; nanosleep(&ts, NULL); }
      CHECA(nPedidos == 0 && pausacom_n() == 0, "id '%s' chegou a rede", ids[i]);
    } }

  // Especial (temporada 0) e episodio, nao filme.
  nPedidos = 0;
  pausacom_pedir("tt0944947", 0, 3);
  espera_fio();
  CHECA(nPedidos >= 1 && strstr(ultimaUrl, "/shows/tt0944947/seasons/0/episodes/3/comments/likes") != NULL,
        "especial: URL errada: %s", ultimaUrl);
  pausacom_limpar();
}

// Troca de chave com o pedido ainda em voo: so o resultado da chave NOVA fica.
static void teste_resultado_velho(void) {
  static const char *velho =
    "[{\"comment\":\"Resposta da chave antiga, nao pode aparecer\",\"user\":{\"username\":\"velho\"}}]";
  static const char *novo =
    "[{\"comment\":\"Resposta da chave nova, esta pode aparecer\",\"user\":{\"username\":\"novo\"}}]";
  PcComentario cm;
  int k, ok;
  pausacom_relogio_teste(NULL);
  comChave = 1;
  pausacom_limpar();
  respFilme = velho;
  seguraRede = 1; dentroDaRede = 0; liberar = 0;
  nPedidos = 0;
  pausacom_pedir("tt0000801", 0, 0);
  for (k = 0; k < 1000 && !dentroDaRede; k++) { struct timespec ts = {0, 2 * 1000 * 1000}; nanosleep(&ts, NULL); }
  CHECA(dentroDaRede, "velho: o pedido A nao chegou a rede");
  respFilme = novo;
  pausacom_pedir("tt0000802", 0, 0);   // A ainda em voo
  liberar = 1;                         // solta A; o fio vai buscar B e para na rede
  for (k = 0; k < 1000 && !(nPedidos == 2 && dentroDaRede); k++) { struct timespec ts = {0, 2 * 1000 * 1000}; nanosleep(&ts, NULL); }
  CHECA(nPedidos == 2 && dentroDaRede, "velho: o pedido B nao chegou a rede (%d)", nPedidos);
  // A terminou e B ainda nao: a resposta da chave antiga NAO pode estar na tela.
  CHECA(pausacom_n() == 0, "velho: resultado da chave antiga vazou (n=%d)", pausacom_n());
  liberar = 1; seguraRede = 0;
  espera_fio();
  ok = pausacom_n() == 1 && pausacom_obter(0, &cm);
  CHECA(ok && !strcmp(cm.autor, "novo"), "velho: devia ficar so o da chave nova (n=%d)", pausacom_n());
  pausacom_limpar();
}

// --- REPETIR PEDIDO QUE FALHOU (relogio falso, sem dormir) -------------------

static long long agoraFalso;
static long long relogioFalso(void) { return agoraFalso; }

// 1 quando o fio ja terminou (sem fio vivo nem pedido marcado). Le sob a trava.
static int fioParado(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = !fioVivo;
  pthread_mutex_unlock(&trava);
  return v;
}

static void espera_fio(void) { espera(fioParado); }
static void passa(long long ms) { agoraFalso += ms; }

// Corpo cortado no meio (ou pagina de erro) e falha, nao "sem comentarios".
static void teste_retry_malformado(void) {
  static const char *ok =
    "[{\"comment\":\"Filme bom com texto suficiente de sobra\",\"user\":{\"username\":\"m1\"}}]";
  pausacom_relogio_teste(relogioFalso);
  agoraFalso = 5000000;
  comChave = 1;
  respFilme = "[{\"comment\":\"cortado no meio";
  nPedidos = 0;
  pausacom_pedir("tt0000606", 0, 0);
  espera_fio();
  CHECA(nPedidos == 1 && pausacom_n() == 0, "malformado-retry: 1o pedido (%d)", nPedidos);
  respFilme = ok;
  passa(30000);
  pausacom_pedir("tt0000606", 0, 0);
  espera_fio();
  CHECA(nPedidos == 2 && pausacom_n() == 1, "malformado-retry: apos 30 s esperava 2 pedidos e 1 comentario (%d, %d)",
        nPedidos, pausacom_n());
  pausacom_limpar();
}

static void teste_retry(void) {
  static const char *filme_ok =
    "[{\"comment\":\"Filme bom com texto suficiente de sobra\",\"user\":{\"username\":\"r1\"}},"
    "{\"comment\":\"Segundo comentario do filme tambem valido\",\"user\":{\"username\":\"r2\"}}]";
  int i;

  pausacom_relogio_teste(relogioFalso);
  agoraFalso = 1000000;
  comChave = 1;
  respFilme = NULL;   // rede cai: rede_baixar_com devolve NULL
  respEp = NULL;

  // 1. Falha; mesma chave antes de 30 s: nenhum pedido novo.
  nPedidos = 0;
  pausacom_pedir("tt0000101", 0, 0);
  espera_fio();
  CHECA(nPedidos == 1, "retry: 1o pedido devia sair, saiu %d", nPedidos);
  passa(29999);
  pausacom_pedir("tt0000101", 0, 0);
  espera_fio();
  CHECA(nPedidos == 1, "retry: repetiu antes de 30 s (%d pedidos)", nPedidos);
  CHECA(pausacom_n() == 0, "retry: n != 0 durante a falha");

  // 2. Passados 30 s: exatamente um pedido novo; a janela seguinte fica quieta.
  passa(1);
  pausacom_pedir("tt0000101", 0, 0);
  espera_fio();
  CHECA(nPedidos == 2, "retry: 30 s depois esperava 2 pedidos, veio %d", nPedidos);
  CHECA(pausacom_n() == 0, "retry: n != 0 durante o retry");
  pausacom_pedir("tt0000101", 0, 0);
  espera_fio();
  CHECA(nPedidos == 2, "retry: segundo pedido na mesma janela (%d)", nPedidos);

  // 3. Terceira falha: a chave nunca mais sai da maquina.
  passa(30000);
  pausacom_pedir("tt0000101", 0, 0);
  espera_fio();
  CHECA(nPedidos == 3, "retry: 3o pedido esperava 3, veio %d", nPedidos);
  passa(10LL * 60 * 1000);
  for (i = 0; i < 5; i++) {
    pausacom_pedir("tt0000101", 0, 0);
    espera_fio();
  }
  CHECA(nPedidos == 3, "retry: 4o pedido da mesma chave (%d)", nPedidos);

  // 4. Chave nova sai na hora e tem orcamento proprio (3 redes).
  pausacom_pedir("tt0000202", 0, 0);
  espera_fio();
  CHECA(nPedidos == 4, "retry: chave nova nao saiu na hora (%d)", nPedidos);
  passa(30000);
  pausacom_pedir("tt0000202", 0, 0);
  espera_fio();
  CHECA(nPedidos == 5, "retry: chave nova, 30 s depois, esperava 5 e veio %d", nPedidos);
  passa(30000);
  pausacom_pedir("tt0000202", 0, 0);
  espera_fio();
  CHECA(nPedidos == 6, "retry: chave nova, 2o retry, esperava 6 e veio %d", nPedidos);
  passa(10LL * 60 * 1000);
  pausacom_pedir("tt0000202", 0, 0);
  espera_fio();
  CHECA(nPedidos == 6, "retry: chave nova passou do limite (%d)", nPedidos);

  // 5. Resposta valida sem comentario nao e falha: nunca repete.
  respFilme = "[]";
  nPedidos = 0;
  pausacom_pedir("tt0000303", 0, 0);
  espera_fio();
  CHECA(nPedidos == 1 && pausacom_n() == 0, "vazio: pedidos %d, n %d", nPedidos, pausacom_n());
  passa(600000);
  pausacom_pedir("tt0000303", 0, 0);
  espera_fio();
  CHECA(nPedidos == 1, "vazio: repetiu resposta valida (%d)", nPedidos);
  respFilme = NULL;

  // 6. Sem credencial, depois credencial: so repete 30 s depois, e ja com rede.
  comChave = 0;
  respFilme = filme_ok;
  nPedidos = 0;
  pausacom_pedir("tt0000404", 0, 0);
  espera_fio();
  CHECA(nPedidos == 0 && pausacom_n() == 0, "sem chave: pediu %d vezes", nPedidos);
  passa(10000);
  comChave = 1;
  pausacom_pedir("tt0000404", 0, 0);
  espera_fio();
  CHECA(nPedidos == 0, "sem chave: repetiu antes de 30 s (%d)", nPedidos);
  passa(20000);
  pausacom_pedir("tt0000404", 0, 0);
  espera_fio();
  CHECA(nPedidos == 1 && pausacom_n() == 2,
        "credencial chegou: pedidos %d, n %d", nPedidos, pausacom_n());
  respFilme = NULL;

  // 7. Falta de credencial nao gasta o limite: 5 tentativas sem rede, depois 3 redes.
  comChave = 0;
  nPedidos = 0;
  pausacom_pedir("tt0000505", 0, 0);
  espera_fio();
  for (i = 0; i < 4; i++) {
    passa(30000);
    pausacom_pedir("tt0000505", 0, 0);
    espera_fio();
  }
  CHECA(nPedidos == 0, "sem chave: saiu rede %d vezes", nPedidos);
  comChave = 1;
  for (i = 0; i < 5; i++) {
    passa(30000);
    pausacom_pedir("tt0000505", 0, 0);
    espera_fio();
  }
  CHECA(nPedidos == 3, "sem chave: esperava 3 redes depois, veio %d", nPedidos);

  pausacom_relogio_teste(NULL);
  pausacom_limpar();
}

static void teste_curtidas_idioma(void) {
  const char *json =
    "[{\"comment\":\"This one is written in English, quite long.\",\"likes\":17,\"language\":\"en\","
    "\"user\":{\"username\":\"en1\"},\"user_rating\":9},"
    "{\"comment\":\"Este foi escrito em portugues, bem comprido.\",\"likes\":3,\"language\":\"pt\","
    "\"user\":{\"username\":\"pt1\"}},"
    "{\"comment\":\"Sem idioma declarado mas com texto suficiente.\",\"likes\":-4,"
    "\"user\":{\"username\":\"sem\"}}]";
  PcComentario c[PC_MAX];
  int n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 3, "idioma: esperava 3, veio %d", n);
  CHECA(c[0].curtidas == 17 && !strcmp(c[0].lingua, "en") && c[0].nota == 9,
        "idioma: primeiro %d '%s'", c[0].curtidas, c[0].lingua);
  CHECA(c[1].curtidas == 3 && !strcmp(c[1].lingua, "pt"), "idioma: segundo");
  CHECA(c[2].curtidas == 0 && c[2].lingua[0] == 0, "idioma: curtida negativa/sem idioma = 0/''");

  // Ordem: com a interface em portugues, "pt" e o sem idioma sobem; o ingles
  // desce, sem perder a ordem relativa. Pelo ciclo completo (pedir -> obter).
  respFilme = json;
  pausacom_limpar();
  pausacom_definir_idioma("pt");
  pausacom_pedir("tt0000777", 0, 0);
  espera(tem3);
  { PcComentario o[3]; int i, ok = pausacom_n() == 3;
    for (i = 0; ok && i < 3; i++) ok = pausacom_obter(i, &o[i]);
    CHECA(ok && !strcmp(o[0].autor, "pt1") && !strcmp(o[1].autor, "sem") && !strcmp(o[2].autor, "en1"),
          "idioma: ordem com interface pt errada (%s %s %s)", o[0].autor, o[1].autor, o[2].autor); }
  { PcComentario ordered[3] = {0};
    strcpy(ordered[0].lingua, "en");
    strcpy(ordered[1].lingua, "pt");
    ordenarIdioma(ordered, 3, "");
    CHECA(!ordered[0].lingua[0] && !strcmp(ordered[1].lingua, "en") &&
          !strcmp(ordered[2].lingua, "pt"), "unknown UI language: shared ordering rule"); }
  pausacom_definir_idioma("");
  pausacom_limpar();
}

static void teste_spoiler_embutido(void) {
  const char *json =
    "[{\"comment\":\"Gostei muito, mas [SPoiler]o vilao e o pai[/spoiler] foi demais.\","
    "\"spoiler\":false,\"user\":{\"username\":\"a\"}},"
    "{\"comment\":\"Um comentario sem nenhuma marca escondida, tudo certo.\","
    "\"spoiler\":false,\"user\":{\"username\":\"b\"}}]";
  PcComentario c[PC_MAX];
  int n = pausacom_interpretar(json, c, PC_MAX);
  CHECA(n == 1 && !strcmp(c[0].autor, "b"), "spoiler embutido: esperava so 'b', veio n=%d", n);
}

int main(void) {
  teste_filtra();
  teste_notas();
  teste_quebras();
  teste_escapes();
  teste_utf8_corte();
  teste_limite();
  teste_malformado();
  teste_ciclo();
  teste_resultado_velho();
  teste_retry();
  teste_retry_malformado();
  teste_spoiler_embutido();
  teste_curtidas_idioma();
  if (falhas) {
    printf("%d de %d falharam\n", falhas, testes);
    return 1;
  }
  printf("ok: %d testes\n", testes);
  return 0;
}
