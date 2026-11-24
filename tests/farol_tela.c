// FAROL — a tela (issue 19, PLANO 3.2/3.4, ADENDO C, ALMA 2/4): Inicio, Cadeia
// e Veredito em src/resolver.c, dirigidos por SEQUENCIAS REAIS DE TECLA
// (D-pad, OK, Voltar) sobre a fachada de verdade (src/farol.c) e os mesmos
// espelhos de producao de tests/farol_resolver.c (player.c/app.c extraidos).
//
// Cobre:
//   - Inicio: kicker "Ajustes › Ajuda", titulo "Farol", os 6 sintomas mais
//     "Cores estranhas", a faixa so com falha < 6 h, "Achar o que ver"
//     reservado (desenhado, fora do foco), "Registros enviados · N";
//     varredura do D-pad: todo foco alcanca todo foco, sem beco; Voltar sai.
//   - Cadeia: com animacoes a luz anda em etapas, nunca passa de onde quebrou
//     e entrega o Veredito; OK adianta; setas nao mexem; reduzidas = direto.
//   - Sonda ativa no ar: a linha fica em "testando…" ate 5 s, OK nao fura,
//     Voltar cancela a sonda NA HORA e volta ao Inicio; sonda que termina
//     re-julga; 5 s sem resposta cancela e segue.
//   - Veredito: selo "Achamos o motivo", foco NASCE na acao, varredura sem
//     beco, "Mandar registro" so com escala JUNTO ou SE_FALHAR depois de a
//     acao falhar; FE_FAROL_ACAO / FE_FAROL_RESULTADO no anel; o despacho
//     reinicia a sessao (farol_veredito() == NULL) e preserva a tentativa do
//     player externo; "Cores estranhas" chega ao IMAGEM_DV.
//   - Modo solido x vidro, RESOLVER_TESTE (nada sai para a rede).
#define FAROL_RESOLVER_SEM_MAIN
#include "farol_resolver.c"
#include "layout.h"     // NV_SCANCODE_BACK

static int falhas;
#define CONFERE(c) do { if (!(c)) { fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

static time_t c_agoraTeste;
time_t time(time_t *out) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  time_t t = c_agoraTeste ? c_agoraTeste : ts.tv_sec;
  if (out) *out = t;
  return t;
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void tecla_app(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  app_resolver_despachar(&e);
}

static void quadros(int n, float dt) {
  for (int i = 0; i < n; i++) resolver_atualizar(dt, 0);
}

// Um quadro desenhado: zera o anel de textos e desenha a tela.
static void desenhar(void) {
  c_txtN = 0;
  resolver_atualizar(0.016f, 0);
  resolver_desenhar(0);
}

static int escreveu(const char *s) {
  for (int i = 0; i < c_txtN; i++) if (strstr(c_txt[i], s)) return 1;
  return 0;
}

static int anel_tem(uint16_t tipo, const char *tok, int b) {
  FarolEvento ev[FAROL_ANEL_N];
  int n = farolanel_ler(ev, FAROL_ANEL_N, 0), achou = 0;
  for (int i = 0; i < n; i++)
    if (ev[i].ev == tipo && (!tok || !strcmp(ev[i].tok, tok)) && (b < 0 || ev[i].b == b)) achou++;
  return achou;
}

// Passeio pseudoaleatorio fixo pelas 4 direcoes (como tests/resolver_shot.c):
// todo foco visitado tem saida e alcanca todos os outros; `esperados` focos.
static void varredura(int esperados, int faseEsperada, int proibido) {
  enum { MAXF = 64, PASSOS = 1200 };
  static const SDL_Keycode DIR[4] = { SDLK_RIGHT, SDLK_LEFT, SDLK_DOWN, SDLK_UP };
  int vis[MAXF], nvis = 0, saida[MAXF] = { 0 };
  static unsigned char aresta[MAXF][MAXF];
  int i, j, k;
  unsigned seed = 4242u;
  memset(aresta, 0, sizeof aresta);
  vis[nvis++] = resolver_foco();
  for (int p = 0; p < PASSOS; p++) {
    int antes = resolver_foco(), depois, a, b;
    seed = seed * 1103515245u + 12345u;
    tecla(DIR[(seed >> 16) & 3]);
    if (!resolver_aberto() || resolver_fase() != faseEsperada) { CONFERE(!"a seta mudou de fase"); return; }
    depois = resolver_foco();
    CONFERE(depois >= 0 && depois != proibido);
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (b == nvis && nvis < MAXF) vis[nvis++] = depois;
    for (a = 0; a < nvis && vis[a] != antes; a++) {}
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (antes != depois && a < nvis && b < nvis) { saida[a] = 1; aresta[a][b] = 1; }
  }
  if (nvis != esperados) {
    fprintf(stderr, "FALHA: fase %d com %d focos, esperava %d:", faseEsperada, nvis, esperados);
    for (i = 0; i < nvis; i++) fprintf(stderr, " %d", vis[i]);
    fprintf(stderr, "\n");
    falhas++;
  }
  for (i = 0; i < nvis; i++) CONFERE(saida[i]);
  for (k = 0; k < nvis; k++)
    for (i = 0; i < nvis; i++)
      for (j = 0; j < nvis; j++)
        if (aresta[i][k] && aresta[k][j]) aresta[i][j] = 1;
  for (i = 0; i < nvis; i++)
    for (j = 0; j < nvis; j++)
      if (i != j && !aresta[i][j]) {
        fprintf(stderr, "FALHA: fase %d: foco %d nao alcanca %d\n", faseEsperada, vis[i], vis[j]);
        falhas++;
      }
}

// Do Inicio (sem faixa, foco no primeiro cartao) ate o cartao `alvo` pela
// grade 2 x 4, so com setas.
static void ir_sintoma(int alvo) {
  int idx = -1;
  for (int i = 0; i < RES_S_FAROL_N; i++) if (resolverregra_sintoma_farol(i) == alvo) idx = i;
  CONFERE(idx >= 0);
  for (int i = 0; i < idx / 2; i++) tecla(SDLK_DOWN);
  if (idx % 2) tecla(SDLK_RIGHT);
  CONFERE(resolver_foco() == alvo);
}

static FarolFatos fatos_base(void) {
  FarolFatos f;
  farolregra_fatos_iniciar(&f);
  f.plat = FP_ANDROID;
  f.addonsQtd = 3;
  return f;
}

int main(void) {
  FarolFatos fx;

  // ------------------------------------------------------------- Inicio
  zerar();
  c_reduzidas = 1;
  resolver_abrir();
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
  CONFERE(resolver_foco() == resolverregra_sintoma_farol(0));   // sem faixa: o 1o cartao
  desenhar();
  CONFERE(escreveu(FT_AJUDA_KICKER));
  CONFERE(escreveu(FT_NOME));
  CONFERE(escreveu(FT_SINTOMA_CORES));
  CONFERE(escreveu(FT_ACHAR_O_QUE_VER));
  CONFERE(escreveu("Registros enviados · 0"));
  CONFERE(!escreveu(FT_BANNER_FALHA));                       // sem falha, sem faixa
  CONFERE(!escreveu("Resolver um problema"));                // o nome antigo saiu da tela
  // 7 cartoes + "Enviar um registro" + "Registros enviados"; "Achar o que ver" fora.
  varredura(9, RESOLVER_FASE_INICIO, RESOLVER_B_ACHAR);
  // Voltar sai.
  tecla(SDLK_ESCAPE);
  CONFERE(!resolver_aberto());

  // Faixa de cima: so com falha < 6 h, e nasce com o foco.
  zerar();
  falhar_fonte("A última fonte falhou.", "", FC_NENHUMA, 0, "ums_outro");
  resolver_abrir();
  CONFERE(resolver_foco() == RESOLVER_B_BANNER);
  desenhar();
  CONFERE(escreveu(FT_BANNER_FALHA));
  varredura(10, RESOLVER_FASE_INICIO, RESOLVER_B_ACHAR);
  tecla(SDLK_BACKSPACE);
  CONFERE(!resolver_aberto());
  zerar();
  resolverregra_falha_registrar(RES_T_FONTE, "velha", "", "", (long)time(NULL) - RES_REJEITA_S - 60);
  resolver_abrir();
  CONFERE(resolver_foco() != RESOLVER_B_BANNER);
  desenhar();
  CONFERE(!escreveu(FT_BANNER_FALHA));                       // mais de 6 h: some

  // O Voltar do controle que so manda o scancode tambem sai.
  { SDL_Event e;
    memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UNKNOWN;
    e.key.keysym.scancode = NV_SCANCODE_BACK;
    resolver_evento(&e); }
  CONFERE(!resolver_aberto());

  // P2: se o erro passar de 6 h com o Inicio aberto, o banner some sem
  // precisar fechar e reabrir a tela (e o foco sai de B_BANNER).
  zerar();
  c_agoraTeste = 1700000000L;
  resolverregra_falha_registrar(RES_T_FONTE, "falha limite", "", "", c_agoraTeste - RES_REJEITA_S + 2);
  resolver_abrir();
  CONFERE(resolver_foco() == RESOLVER_B_BANNER);
  desenhar();
  CONFERE(escreveu(FT_BANNER_FALHA));
  c_agoraTeste += 5;
  desenhar();
  CONFERE(!escreveu(FT_BANNER_FALHA));
  CONFERE(resolver_foco() == resolverregra_sintoma_farol(0));
  c_agoraTeste = 0;
  tecla(SDLK_ESCAPE);
  CONFERE(!resolver_aberto());

  // Cada cartao, pelas setas, abre o seu sintoma; Voltar do Veredito volta ao Inicio.
  for (int i = 0; i < RES_S_FAROL_N; i++) {
    int s = resolverregra_sintoma_farol(i);
    zerar();
    resolver_abrir();
    ir_sintoma(s);
    tecla(SDLK_RETURN);
    CONFERE(resolver_sintoma() == s);
    CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO);       // reduzidas: sem varrer
    CONFERE(resolver_foco() == RESOLVER_B_PRINC);             // foco nasce na acao
    tecla(SDLK_ESCAPE);
    CONFERE(resolver_fase() == RESOLVER_FASE_INICIO && resolver_aberto());
  }

  // ----------------------------------------------------- Cores estranhas
  zerar();
  fx = fatos_base();
  fx.plat = FP_WEBOS;
  fx.fonteDv = 1;
  fx.tvDv = 0;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  ir_sintoma(RES_S_CORES);
  tecla(SDLK_RETURN);
  CONFERE(resolver_sintoma() == RES_S_CORES);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO && resolver_foco() == RESOLVER_B_PRINC);
  CONFERE(resolver_teste_veredito() == FAROL_C_IMAGEM_DV);
  CONFERE(!resolver_mandar_registro());                      // IMAGEM_DV escala NUNCA
  desenhar();
  CONFERE(escreveu(FT_A_CAUSA_ENCONTRADA));
  CONFERE(escreveu(FT_IMAGEM_DV_L1));
  CONFERE(escreveu(FT_SINTOMA_CORES));                       // o kicker da Cadeia
  // O mesmo fato fora do sintoma nao vira DV (RES_S_GERAL nao colide).
  tecla(SDLK_ESCAPE);
  ir_sintoma(RES_S_NAO_COMECA);
  tecla(SDLK_RETURN);
  CONFERE(resolver_teste_veredito() != FAROL_C_IMAGEM_DV);

  // ---------------------------------------------- Cadeia com animacao
  zerar();
  c_reduzidas = 0;
  c_offline = 1;                                               // REDE_FORA: para na 1a
  resolver_abrir();
  quadros(30, 0.016f);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA);
  CONFERE(resolver_luz() < 1.0f);
  CONFERE(resolver_teste_veredito() == FAROL_C_REDE_FORA);
  desenhar();
  CONFERE(escreveu(FT_SONDANDO));                              // o painel ainda testa
  CONFERE(escreveu(FT_A_CAUSA_ENCONTRADA));                    // o veredito passivo ja esta na tela
  CONFERE(escreveu(FT_REDE_FORA_L1));
  CONFERE(escreveu(FT_NAO_TESTADO));                           // etapas seguintes ja exibem "nao testado"
  tecla(SDLK_LEFT); tecla(SDLK_DOWN);                          // setas nao mexem
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA);
  { float antes = resolver_luz(); int passos = 0;
    while (resolver_fase() == RESOLVER_FASE_CADEIA && passos < 200) {
      quadros(1, 0.016f);
      CONFERE(resolver_luz() >= antes && resolver_luz() <= 1.0f + 1e-4f);   // nunca passa de onde quebrou
      antes = resolver_luz();
      passos++;
    }
    CONFERE(passos > 3); }                                     // andou em quadros, nao saltou
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO && resolver_foco() == RESOLVER_B_PRINC);
  CONFERE(resolver_luz() == 1.0f);
  { const int *est = resolver_teste_estados();
    CONFERE(est[RES_T_INTERNET] == RES_FALHOU);
    for (int t = RES_T_ADDONS; t < RES_T_N; t++) CONFERE(est[t] == RES_NAO_TESTADO); }
  desenhar();
  CONFERE(escreveu(FT_A_CAUSA_ENCONTRADA));
  CONFERE(escreveu(FT_NAO_TESTADO));                           // o que vem depois fica apagado
  varredura(2, RESOLVER_FASE_VEREDITO, -1);                    // Tentar de novo + Testar outro
  // OK adianta a luz.
  tecla(SDLK_ESCAPE);
  c_offline = 0;
  falhar_fonte("A última fonte falhou.", "", FC_NENHUMA, 0, "ums_outro");
  tecla(SDLK_RETURN);   // a faixa nao existe nesta abertura; o 1o cartao
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO && resolver_foco() == RESOLVER_B_PRINC);
  // Voltar no meio da Cadeia volta ao Inicio.
  tecla(SDLK_ESCAPE);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA);
  tecla(SDLK_ESCAPE);
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO && resolver_aberto());

  // ------------------------------------------------- sonda ativa no ar
  zerar();
  c_reduzidas = 0;
  falhar_fonte("A última fonte falhou.", "", FC_NENHUMA, 0, "ums_outro");
  resolver_abrir();
  c_sondaPronta = 0; c_sondaCanceladaN = 0;
  tecla(SDLK_RETURN);                                          // faixa -> O video nao comeca
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA && resolver_testando());
  quadros(120, 0.016f);                                        // ~2 s
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA && resolver_testando());
  CONFERE(resolver_luz() <= (float)RES_T_FONTE + 1.0f);
  desenhar();
  CONFERE(escreveu(FT_TESTANDO));
  CONFERE(escreveu(FT_SONDANDO_FONTE));
  CONFERE(escreveu(FT_A_CAUSA_ENCONTRADA));
  tecla(SDLK_RETURN);                                          // OK nao fura a sonda
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA);
  tecla(SDLK_ESCAPE);                                          // Voltar cancela NA HORA
  CONFERE(c_sondaCanceladaN == 1);
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO && !resolver_testando());
  // A sonda que termina sozinha re-julga e entrega o Veredito.
  c_sondaPronta = 0;
  tecla(SDLK_RETURN);
  CONFERE(resolver_testando());
  quadros(30, 0.016f);
  c_sondaPronta = 1;
  quadros(200, 0.016f);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO && resolver_foco() == RESOLVER_B_PRINC);
  // 5 s sem resposta: cancela e segue.
  tecla(SDLK_ESCAPE);
  c_sondaPronta = 0; c_sondaCanceladaN = 0;
  tecla(SDLK_RETURN);
  quadros(319, 0.016f);                                        // 5,104 s
  CONFERE(c_sondaCanceladaN == 1);
  quadros(200, 0.016f);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO);
  // Reduzidas: sem varrer, mas a sonda no ar segura igual (sem respiro).
  tecla(SDLK_ESCAPE);
  c_reduzidas = 1;
  c_sondaPronta = 0; c_sondaCanceladaN = 0;
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA && resolver_testando());
  tecla(SDLK_ESCAPE);
  CONFERE(c_sondaCanceladaN == 1 && resolver_fase() == RESOLVER_FASE_INICIO);
  c_sondaPronta = 1;

  // ---------------------------------- Veredito: Mandar registro e escala
  // TV_FORMATO_REPETE (Android, externo): escala JUNTO -> "Mandar registro".
  zerar();
  fx = fatos_base();
  fx.haFalha = 1; fx.erroIdadeS = 30; fx.erro.classe = FC_FORMATO;
  fx.falhasSeguidas = 2; fx.externoDisponivel = 1;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  tecla(SDLK_RETURN);
  CONFERE(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  CONFERE(resolver_foco() == RESOLVER_B_PRINC && resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  CONFERE(resolver_mandar_registro() && resolver_teste_sec() == FAROL_A_ENVIAR_REGISTRO);
  CONFERE(!strcmp(farolregra_acao_texto(resolver_teste_sec()), FT_ACAO_MANDAR_REGISTRO));
  desenhar();
  CONFERE(escreveu(FT_ACAO_MANDAR_REGISTRO));                  // o rotulo do 2o botao
  CONFERE(escreveu(FT_ACAO_PLAYER_EXTERNO));                   // e a acao concreta
  varredura(3, RESOLVER_FASE_VEREDITO, -1);                    // acao + Mandar + Testar outro
  // "Mandar registro" vai ao envio (gancho #22) com FE_FAROL_ACAO saneado.
  tecla(SDLK_ESCAPE);
  tecla(SDLK_RETURN);
  CONFERE(resolver_foco() == RESOLVER_B_PRINC);
  tecla(SDLK_RIGHT);
  CONFERE(resolver_foco() == RESOLVER_B_SEC);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_ENVIO);
  CONFERE(anel_tem(FE_FAROL_ACAO, "enviar_registro", -1) == 1);
  c_envio.estado = 2; c_envio.quando = time(NULL) + 1; snprintf(c_envio.codigo, sizeof c_envio.codigo, "%s", "4F7K92");
  quadros(1, 0.016f);
  CONFERE(anel_tem(FE_FAROL_RESULTADO, "enviar_registro", 1) == 1);
  CONFERE(resolver_enviados_n() == 1);
  tecla(SDLK_RETURN);                                          // Pronto volta ao Veredito
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO && resolver_foco() == RESOLVER_B_PRINC);

  // TV_FORMATO (Android, 1 falha): SE_FALHAR. Sem falha anterior, sem "Mandar".
  // OK despacha o player externo: FE_FAROL_ACAO, sessao reiniciada, fecha.
  zerar();
  fx = fatos_base();
  fx.haFalha = 1; fx.erroIdadeS = 30; fx.erro.classe = FC_FORMATO;
  fx.falhasSeguidas = 1; fx.externoDisponivel = 1;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  tecla(SDLK_RETURN);
  CONFERE(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  CONFERE(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  CONFERE(!resolver_mandar_registro());
  CONFERE(farol_veredito() != NULL);
  tecla(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(resolver_pedido() == RESOLVER_PEDIU_PLAYER_EXT);
  CONFERE(anel_tem(FE_FAROL_ACAO, "player_externo", -1) == 1);
  CONFERE(farol_veredito() == NULL);                           // farol_sessao_reiniciar no despacho
  CONFERE(resolver_teste_acao_pendente() == FAROL_A_PLAYER_EXTERNO);
  // Volta com o mesmo problema: a acao nao consertou -> resultado 0 e agora
  // a escala SE_FALHAR libera "Mandar registro".
  resolver_abrir();
  tecla(SDLK_RETURN);
  CONFERE(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  CONFERE(anel_tem(FE_FAROL_RESULTADO, "player_externo", 0) == 1);
  CONFERE(resolver_teste_acao_pendente() == 0);
  CONFERE(resolver_mandar_registro());
  CONFERE(resolver_foco() == RESOLVER_B_PRINC);                // ainda nasce na acao
  // Depois de outra tentativa, o problema sumiu: resultado 1.
  tecla(SDLK_RETURN);
  CONFERE(!resolver_aberto() && resolver_pedido() == RESOLVER_PEDIU_PLAYER_EXT);
  fx = fatos_base();
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  tecla(SDLK_RETURN);
  CONFERE(resolver_teste_veredito() != FAROL_C_TV_FORMATO);
  CONFERE(anel_tem(FE_FAROL_RESULTADO, "player_externo", 1) == 1);

  // REDE_FORA: "Tentar de novo" re-julga na hora; mesmo veredito = falhou.
  zerar();
  c_offline = 1;
  resolver_abrir();
  tecla(SDLK_RETURN);
  CONFERE(resolver_teste_veredito() == FAROL_C_REDE_FORA && resolver_teste_prim() == FAROL_A_TENTAR_DE_NOVO);
  CONFERE(!resolver_mandar_registro());                        // escala NUNCA
  tecla(SDLK_RETURN);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_VEREDITO && resolver_foco() == RESOLVER_B_PRINC);
  CONFERE(anel_tem(FE_FAROL_ACAO, "reconexao", -1) == 1);
  CONFERE(anel_tem(FE_FAROL_RESULTADO, "reconexao", 0) == 1);
  CONFERE(!resolver_mandar_registro());                        // NUNCA segue NUNCA
  c_offline = 0;
  tecla(SDLK_RETURN);
  CONFERE(anel_tem(FE_FAROL_RESULTADO, "reconexao", 1) == 1);
  CONFERE(resolver_teste_veredito() != FAROL_C_REDE_FORA);

  // Caminho real do player externo (player fechado, Android): o despacho
  // reinicia a sessao e a tentativa diagnosticada segue elegivel para app.c.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  sim_abrir_tentativa("tt1111111", "series", "Episodio A", 1, 3,
                      "https://cdn.example/epA.mkv", 0);
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  aberto = 0;
  resolver_abrir();
  CONFERE(resolver_foco() == RESOLVER_B_BANNER);
  tecla_app(SDLK_RETURN);                                      // faixa -> Cadeia -> Veredito
  CONFERE(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  CONFERE(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO && resolver_foco() == RESOLVER_B_PRINC);
  { uint64_t t = ultimoFarolTentativa;
    CONFERE(t != 0);
    tecla_app(SDLK_RETURN);                                    // app.c despacha
    CONFERE(!resolver_aberto());
    CONFERE(resolver_tentativa_externo() == t);
    CONFERE(c_androidAbrirN == 1 && !strcmp(c_androidAbrirUrl, "https://cdn.example/epA.mkv"));
    CONFERE(farol_veredito() == NULL); }
  CONFERE(c_tocarInternoN == 0);

  // Nada achado cai no cartao antigo e o atalho do registro segue ali.
  zerar();
  resolver_abrir();
  tecla(SDLK_RETURN);
  CONFERE(resolver_teste_veredito() == FAROL_C_NADA_ACHADO);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO && resolver_foco() == RESOLVER_B_PRINC);
  varredura(3, RESOLVER_FASE_VEREDITO, -1);                    // Enviar + Tentar + Outro
  tecla(SDLK_ESCAPE);
  // Legenda: passo a passo, sem cadeia, sem beco.
  ir_sintoma(RES_S_LEGENDA);
  c_reduzidas = 0;
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO);          // sem cadeia, nem com animacao
  varredura(2, RESOLVER_FASE_VEREDITO, -1);
  c_reduzidas = 1;

  // ----------------------------------------------- modo solido x vidro
  zerar();
  resolver_abrir();
  c_vidro = 0; c_vidroPaineis = 0;
  desenhar();
  CONFERE(c_vidroPaineis == 0);
  c_vidro = 1;
  desenhar();
  CONFERE(c_vidroPaineis > 0);
  c_vidro = 0;

  // RESOLVER_TESTE: nada saiu para a rede (envio so planta estado).
  CONFERE(c_androidAbrirN <= 1);

  if (falhas) { fprintf(stderr, "farol_tela: %d falha(s)\n", falhas); return 1; }
  puts("farol_tela: inicio, cadeia e veredito ok");
  return 0;
}
