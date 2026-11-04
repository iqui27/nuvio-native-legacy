// feat/mkv-chapters-all-platforms: a fileira de capitulos com PONTEIRO (Magic
// Remote da LG) e DEDO (Android Mobile). Player real, desenho real; o toque e
// injetado por ponteiro_teste_toque e os alvos lidos de ponteiro_teste_lista —
// chamar o `ativar` de um alvo e exatamente o que o clique faz.
// O ajuste "Capitulos" e por TV e nasce desligado: o teste o liga.
static int capitulosLigados(void) { return 1; }
#define ajustes_capitulos capitulosLigados
#include "../src/player.c"
#undef ajustes_capitulos
#include <assert.h>

static int falhas;
static void ok(int c, const char *o) { printf("  %-64s %s\n", o, c ? "ok" : "FALHOU"); if (!c) falhas++; }

static void quadro(Uint32 t) {
  ponteiro_quadro(t);
  desenharOsdCorpo(t, 1, 1, NULL);
  ponteiro_desenhar();
}
static void quadros(Uint32 *t, int n) { while (n-- > 0) { *t += 16; quadro(*t); } }

// Alvos da fileira no ultimo quadro fechado.
typedef struct { int n, nome, cortados; const PonteiroAlvo *chip[MKV_MAX_CAPS]; } Alvos;
static Alvos alvos(void) {
  const PonteiroAlvo *v;
  int n = ponteiro_teste_lista(&v), i;
  Alvos a; memset(&a, 0, sizeof a);
  for (i = 0; i < n; i++) {
    if (v[i].ativar == ponteiroAbrirCapitulos) a.nome++;
    if (v[i].ativar == ponteiroCapitulo && v[i].a >= 0 && v[i].a < MKV_MAX_CAPS) {
      a.chip[v[i].a] = &v[i]; a.n++;
      if (!v[i].b) a.cortados++;
    }
  }
  return a;
}

int main(void) {
  static const MkvCap caps[] = {
    { 0, "Prologue" }, { 85, "Opening Theme Song" }, { 175, "Part A - The Long Road Home" },
    { 420, "Eyecatch" }, { 430, "Part B - Somewhere Beyond the Hills" }, { 700, "Part C - Return" },
    { 1050, "Ending Theme Song" }, { 1140, "Post-Credits Scene" }, { 1200, "Next Episode Preview" },
    { 1260, "Extra - Studio Commentary" }, { 1320, "Extra - Behind the Scenes" },
  };
  MkvCap c[MKV_MAX_CAPS];
  int n, i, cortado = -1;
  Uint32 t = 1000;
  float rol;
  Alvos a;

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_Window *w = SDL_CreateWindow("caps", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  gfx_opacidade_grupo = 0;
  assert(txt_iniciar("deploy/app", 1));   // largura real dos nomes: a fileira passa da barra
  ponteiro_iniciar();
  ponteiro_teste_toque(1);   // Android Mobile: alvos existem sem cursor

  aberto = visivel = 1; entrada = anim = cheio = fileira = 1;
  comVideo = 0; canalSessao = 0; duracaoSeg = 1440; duracaoReal = 1; posSeg = 100;
  barraFoco = 1; ultimoInput = t;
  capmkv_aplicar(caps, (int)(sizeof caps / sizeof *caps));
  n = capitulosDoArquivo(c, MKV_MAX_CAPS);
  assert(n == (int)(sizeof caps / sizeof *caps));

  puts("capitulos/ponteiro: fileira fechada");
  quadros(&t, 4);
  a = alvos();
  ok(a.nome == 1, "nome do capitulo acima da barra e um alvo");
  ok(a.n == 0, "fileira fechada: nenhum chip recebe toque");
  { // O nome e registrado depois da barra (ganha na sobreposicao): tocar no
    // trilho debaixo dele tem de continuar buscando, nao abrir a fileira.
    const PonteiroAlvo *v; int nv = ponteiro_teste_lista(&v), j, barra = -1, perdeu = 0;
    for (j = 0; j < nv; j++) if (v[j].ativar == ponteiroBuscar) barra = j;
    assert(barra >= 0);
    for (j = 0; j < nv; j++)
      if (v[j].ativar == ponteiroAbrirCapitulos) {
        float x, yTrilho = v[barra].y + v[barra].h * 0.5f - 3.0f;   // topo do trilho de 6 px
        for (x = v[j].x; x < v[j].x + v[j].w; x += 8.0f) {
          if (x < v[barra].x || x >= v[barra].x + v[barra].w) continue;   // folga alem da ponta da barra
          int k = ponteiro_achar(v, nv, x, yTrilho), k2 = ponteiro_achar(v, nv, x, yTrilho + 5.0f);
          if (k != barra || k2 != barra) perdeu = 1;
        }
      }
    ok(!perdeu, "tocar no trilho debaixo do nome busca (o nome nao cobre a barra)");
  }

  puts("capitulos/ponteiro: tocar no nome abre a fileira");
  ponteiroAbrirCapitulos(0, 0);
  ok(capFoco == 1 && capSel == 1, "abriu no capitulo que toca (Opening em 100 s)");
  quadros(&t, 60);   // a mola chega
  a = alvos();
  ok(a.nome == 0, "aberta: o nome deixa de ser alvo");
  ok(a.n >= 3, "chips visiveis sao alvos");
  ok(a.cortados >= 1, "a fileira passa da barra: ha chip cortado na borda");
  for (i = 0; i < n; i++)
    if (a.chip[i]) {
      ok(a.chip[i]->x >= PLR_MARGEM - 0.5f && a.chip[i]->x + a.chip[i]->w <= NV_VTELA_W - PLR_MARGEM + 0.5f,
         "alvo do chip dentro da largura da barra");
      ok((a.chip[i]->focar != NULL) == (a.chip[i]->b != 0), "so chip inteiro foca por cima");
      if (!a.chip[i]->b && cortado < 0 && i > capSel) cortado = i;
    }
  assert(cortado > 0);

  puts("capitulos/ponteiro: chip cortado so rola, nao busca");
  a.chip[cortado]->ativar(cortado, 0);
  ok(posSeg == 100.0f && capFoco == 1 && capSel == cortado, "toque no cortado: seleciona, sem busca");
  quadros(&t, 2);
  a = alvos();
  ok(a.chip[cortado] && a.chip[cortado]->b == 1, "e no quadro seguinte ele esta inteiro");
  rol = capRol;
  ok(rol > 0.0f, "a fileira rolou");

  puts("capitulos/ponteiro: passar por um chip visivel nao faz a fileira pular");
  for (i = cortado - 1; i >= 0 && !(a.chip[i] && a.chip[i]->b); i--) {}
  if (i >= 0) {
    for (; i - 1 >= 0 && a.chip[i - 1] && a.chip[i - 1]->b; i--) {}   // o mais a esquerda inteiro
    a.chip[i]->focar(i, 1);
    quadros(&t, 2);
    ok(capSel == i && capRol == rol, "foco anda para a esquerda, rolagem fica");
  } else ok(0, "achar um chip inteiro a esquerda");

  puts("capitulos/ponteiro: tocar num chip inteiro busca o comeco e fecha");
  a = alvos();
  for (i = 0; i < n && !(a.chip[i] && a.chip[i]->b); i++) {}
  assert(i < n);
  a.chip[i]->ativar(i, 1);
  ok(posSeg == (float)c[i].inicio && capFoco == 0, "busca o capitulo e fecha a fileira");

  puts("capitulos/teclas: OK continua buscando (mesmo caminho)");
  ponteiroAbrirCapitulos(0, 0);
  capSel = 6;
  { SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    player_evento(&e); }
  ok(posSeg == 1050.0f && capFoco == 0, "OK no Ending: 1050 s");

  puts("capitulos/teclas: rolagem para a DIREITA igual a de antes");
  ponteiroAbrirCapitulos(0, 0);
  for (i = 0; i < 6; i++) {
    SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RIGHT;
    player_evento(&e); quadros(&t, 1);
  }
  { float fimBarra = NV_VTELA_W - PLR_MARGEM;
    a = alvos();
    ok(capSel == n - 1 && a.chip[capSel] && a.chip[capSel]->b == 1, "o chip em foco esta inteiro");
    // Antes: com rolagem, o chip em foco encostava na borda direita da barra.
    ok(capRol > 0.0f && a.chip[capSel] &&
       a.chip[capSel]->x + a.chip[capSel]->w > fimBarra - 1.0f, "rolou e o foco encosta na borda direita, como antes"); }
  { SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN; player_evento(&e); }

  puts("capitulos/ponteiro: arquivo sem capitulos depois de um com");
  capmkv_zerar();
  quadros(&t, 4);
  a = alvos();
  ok(a.nome == 0 && a.n == 0 && capFoco == 0, "nenhum alvo, fileira fechada");

  puts("capitulos/fileira: lista encolhe com a fileira aberta");
  { static const MkvCap tres[] = { { 0, "Intro" }, { 90, "Main" }, { 1300, "Credits" } };
    capmkv_aplicar(caps, (int)(sizeof caps / sizeof *caps));
    quadros(&t, 2);
    ponteiroAbrirCapitulos(0, 0);
    capSel = n - 1;
    quadros(&t, 60);   // a mola chega: chips viram alvos
    capmkv_zerar(); capmkv_aplicar(tres, 3);   // outro arquivo, 3 capitulos
    quadros(&t, 2);
    a = alvos();
    ok(capSel >= 0 && capSel < 3, "o indice volta para dentro da lista nova");
    ok(a.n > 0 && a.n <= 3, "so chips da lista nova sao alvos"); }

  SDL_Quit();
  printf("%s\n", falhas ? "FALHOU" : "player_capitulos_ponteiro: ok");
  return falhas ? 1 : 0;
}
