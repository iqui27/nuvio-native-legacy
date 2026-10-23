// O PAINEL DE ENVIO DO REGISTRO (mockup quadros 8-11, aprovado em 03/10): ilha
// modal sobre Ajustes > Sobre e ajuda. A esquerda o que vai (o fim do
// nuvio.log no mono do painel, com o tamanho real); a direita o andamento, o
// CODIGO de seis caracteres do recibo (regcodigo.h), ou o motivo da falha com
// a sua frase: servidor (com o HTTP), TV sem internet (redesaude.h: nem tenta,
// e "Tentar de novo" fica apagado ate a rede voltar), prazo, conexao. Sem
// porcentagem no "Enviando…": rede_postar_st nao da progresso, entao a barra e
// so o brilho correndo — nada de numero inventado. Incluido por registro.c.

static int envAberto, envFoco;
static long envTam;          // bytes que vao (o fim do arquivo, ate 200 KB)
static int envLinhasArq;
#ifdef NV_TOUCH_UI
static TelefoneCartao envTelefone;
static int envTelefoneGeracao;
#endif
#ifdef REGISTRO_TESTE
static int envTesteFixo;     // o harness fixa o estado (avisos_teste_envio)
#endif

int registro_envio_aberto(void) { return envAberto; }

// O tamanho e as linhas do que vai, lidos uma vez ao abrir.
static void envMedir(void) {
  const char *arq = registro_arquivo();
  FILE *f;
  envTam = 0; envLinhasArq = 0;
#ifdef REGISTRO_TESTE
  if (regTesteTexto) {
    const char *p;
    envTam = 186 * 1024; envLinhasArq = 2214;
    for (p = regTesteTexto; *p; p++) {}
    return;
  }
#endif
  if (!arq || (f = fopen(arq, "rb")) == NULL) return;
  { static char buf[200 * 1024];
    long tam, de;
    size_t n, k;
    fseek(f, 0, SEEK_END); tam = ftell(f);
    de = tam > (long)sizeof buf ? tam - (long)sizeof buf : 0;
    fseek(f, de, SEEK_SET);
    n = fread(buf, 1, sizeof buf, f);
    for (k = 0; k < n; k++) if (buf[k] == '\n') envLinhasArq++;
    envTam = (long)n; }
  fclose(f);
}

void registro_envio_abrir(void) {
#ifdef NV_TOUCH_UI
  telefonecartao_limpar(&envTelefone); envTelefoneGeracao++;
#endif
  envAberto = 1; envFoco = 0;
  fflush(stdout);
  envMedir();
  if (!nLin || SDL_GetTicks() - ultLeitura > 1000) carregar();
#ifdef REGISTRO_TESTE
  if (envTesteFixo) return;
#endif
  avisos_enviar_registro_atual();
}

int registro_envio_evento(const SDL_Event *e) {
  AvisosEnvio env;
  SDL_Keycode k;
  if (!envAberto) return 0;
  if (e->type == SDL_KEYUP &&
      ((soltarSym && e->key.keysym.sym == soltarSym) ||
       (soltarScan >= 0 && (int)e->key.keysym.scancode == soltarScan))) {
    soltarSym = 0; soltarScan = -1;
    return 1;
  }
  if (e->type != SDL_KEYDOWN) return e->type == SDL_KEYUP || e->type == SDL_TEXTINPUT;
  k = e->key.keysym.sym;
  avisos_envio_info(&env);
  if (nv_tecla_voltar(e)) { envAberto = 0; engolirSoltura(e); return 1; }   // o envio continua
  if (env.estado == 3) {
    int podeTentar = env.motivo != AVISOS_ENVIO_OFFLINE && env.motivo != AVISOS_ENVIO_INDISPONIVEL;
    // Erro: [Tentar de novo] [Fechar]. Sem internet: [Fechar] [Tentar apagado].
    if (k == SDLK_LEFT || k == SDLK_RIGHT) { if (podeTentar) envFoco = !envFoco; return 1; }
    if (ehOk(k)) {
      int tentar = env.motivo == AVISOS_ENVIO_OFFLINE ? 0 : (envFoco == 0);
      if (tentar && podeTentar) { envMedir(); avisos_enviar_registro_atual(); envFoco = 0; }
      else envAberto = 0;
      engolirSoltura(e);
    }
    return 1;
  }
  if (ehOk(k)) { envAberto = 0; engolirSoltura(e); }
  return 1;
}

// --- O PACOTE: o fim do log em mono, numa caixa --------------------------------
static void rgLinhaPacote(const RegLinha *L, float x, float y, float w, float al) {
  RgCor cm = rgCorMsg(L, 0, 0), ca = rgCorArea(L, 0, 0);
  if (L->sev) {
    float r = L->sev == 2 ? 0.898f : 0.851f, g = L->sev == 2 ? 0.325f : 0.651f, b = L->sev == 2 ? 0.294f : 0.251f;
    gfx_cor((GfxRect){ x + 9, y + 7, 3, 10 }, 0.5f, r, g, b, al);
  }
  rgPedaco(TXT_MONO13, L->area, ca, x + 22, y + 4.5f, al, 92);
  if (L->fps && L->sev != 2) rgLinhaFpsE(L, x + 22 + 96, y + 3, al, TXT_MONO14, TXT_MONO14B, 7.7f, w - 22 - 96 - 16);
  else rgMensagem(TXT_MONO14, L->t + L->msg, cm, x + 22 + 96, y + 3, al, w - 22 - 96 - 16);
}
void registro_pacote(float x, float y, float w, float h, int n, const char *rotDir, float a) {
  int i, ini;
  float ly;
  if (!nLin || SDL_GetTicks() - ultLeitura > 1000) { int ar = area; carregar(); area = ar; filtrar(); }
  if (rgVid()) gfx_cor((GfxRect){ x, y, w, h }, 24.0f / h, 0, 0, 0, 0.28f * a);
  else gfx_cor((GfxRect){ x, y, w, h }, 24.0f / h, RG_CART_SOL, a);
  rgIcone("aj_file-text", x + 22, y + 22, 18, 0.6f * a);
  { TxtLinha t = txt_linha(TXT_MONO15, "nuvio.log", RG_TX, 255); txt_desenhar_alpha(t, x + 22 + 30, y + 31 - t.h * 0.5f, 0.6f * a); }
  if (rotDir && rotDir[0]) { TxtLinha t = rgT(TXT_AJ_MINI14, rotDir); txt_desenhar_alpha(t, x + w - 22 - t.w, y + 31 - t.h * 0.5f, 0.42f * a); }
  ly = y + 22 + 18 + 14;
  ini = nLin > n ? nLin - n : 0;
  gfx_recorte(x, ly, w, y + h - ly);
  for (i = ini; i < nLin; i++, ly += 25) {
    // a mascara do pacote: de 15% a 100% nos primeiros 110 px
    float t = (ly + 12 - (y + 22 + 18 + 14)) / 110.0f;
    float al = t >= 1 ? 1 : 0.15f + 0.85f * (t < 0 ? 0 : t);
    if (ly > y + h) break;
    rgLinhaPacote(&lin[i], x, ly, w, al * a);
  }
  gfx_sem_recorte();
}

// --- O MODAL ---------------------------------------------------------------------
// Centrado na tela virtual (escala.h) e no maximo da largura dela menos 80:
// 300, 250, 1320 e 580 em 100%.
#define RE_W (NV_TELA_W - 80.0f < 1320.0f ? NV_TELA_W - 80.0f : 1320.0f)
#define RE_H 580.0f
#define RE_X ((NV_TELA_W - RE_W) * 0.5f)
#define RE_Y ((NV_TELA_H - RE_H) * 0.5f)
static void rgKbdTexto(const char *k, const char *l, float xDir, float yc) {
  TxtLinha kt = rgT(TXT_AJ_KBD, k), lt = rgT(TXT_ILHA_APOIO, l);
  float kw = kt.w + 18 < 34 ? 34 : kt.w + 18, x = xDir - lt.w - 9 - kw;
  rgNeutro((GfxRect){ x, yc - 15, kw, 30 }, 15, 0.09f, RG_CHIP_SOL, 1);
  txt_desenhar_alpha(kt, x + (kw - kt.w) * 0.5f, yc - kt.h * 0.5f, 0.82f);
  txt_desenhar_alpha(lt, x + kw + 9, yc - lt.h * 0.5f, 0.5f);
}
// Botao pilula 56 (21/600): focado cheio no acento, o outro neutro 8%.
static float rgBotao(const char *rot, const char *icone, float x, float y, int foco, float a) {
  TxtLinha t = foco ? rgTT(TXT_ILHA_ITEM, rot) : rgT(TXT_ILHA_ITEM, rot);
  float w = 28 + (icone ? 20 + 10 : 0) + t.w + 28;
  if (foco) rgPilulaAcento((GfxRect){ x, y, w, 56 }, 28, a);
  else rgNeutro((GfxRect){ x, y, w, 56 }, 28, 0.08f, RG_CHIP_SOL, a);
  if (icone) { if (foco) rgIconeT(icone, x + 28, y + 18, 20, a); else rgIcone(icone, x + 28, y + 18, 20, 0.88f * a); }
  txt_desenhar_alpha(t, x + 28 + (icone ? 30 : 0), y + (56 - t.h) * 0.5f, (foco ? 1.0f : 0.88f) * a);
  return w;
}
// PONTEIRO (#99): no erro com "Tentar de novo" possivel, foco pela mesma
// variavel das setas (0 = Tentar de novo, 1 = Fechar); o OK do clique segue
// por registro_envio_evento. Os outros estados tem um botao so (camada sem
// alvos: o clique e o OK).
static void ponteiroEnvio(int i, int b) {
  AvisosEnvio env;
  (void)b;
  if (!envAberto || (i != 0 && i != 1) || envFoco == i) return;
  if (avisos_envio_info(&env) != 3 || env.motivo == AVISOS_ENVIO_OFFLINE ||
      env.motivo == AVISOS_ENVIO_INDISPONIVEL) return;
  envFoco = i;
}
int registro_envio_teste_foco(void) { return envFoco; }

static void rgTamTexto(char *d, size_t tam, long bytes) {
  long kb = (bytes + 512) / 1024;
  snprintf(d, tam, "%ld KB", kb);
}
static void rgMilhar(char *d, size_t tam, long n) {
  char b[32];
  int len, i, j = 0;
  char sep = rgSep() == ',' ? '.' : ',';
  snprintf(b, sizeof b, "%ld", n);
  len = (int)strlen(b);
  for (i = 0; i < len && j < (int)tam - 2; i++) {
    d[j++] = b[i];
    if ((len - i - 1) % 3 == 0 && i < len - 1) d[j++] = sep;
  }
  d[j] = 0;
}

#ifdef NV_TOUCH_UI
static int envTelefoneChave(const AvisosEnvio *e) { return envTelefoneGeracao * 1000 + e->estado * 100 + e->motivo; }
static int envTelefoneRolar(const PonteiroRolagem *e) {
  if (!envAberto || !telefoneui_ativo()) return 0;
  return toquerol_evento(&envTelefone.rolagem, e);
}
static void envTelefoneEscolher(int i, int chave) {
  AvisosEnvio env; SDL_Event e;
  if (!envAberto || !telefoneui_ativo()) return;
  avisos_envio_info(&env);
  if (chave != envTelefoneChave(&env)) return;
  int retry = env.estado == 3 && env.motivo != AVISOS_ENVIO_OFFLINE && env.motivo != AVISOS_ENVIO_INDISPONIVEL;
  if (i < 0 || i > retry) return;
  envFoco = retry ? i : 1;
  memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  registro_envio_evento(&e);
}
static float envTelefoneStat(const char *nome, const char *valor, float x, float y, float w, float a) {
  float h = telefonecartao_texto(TXT_AJ_ESTADO, nome, x, y, w, 28, .45f * a);
  return h + 4 + telefonecartao_texto(TXT_AJ_ESTADO, valor, x, y + h + 4, w, 28, .8f * a) + 18;
}
static float envTelefoneConteudo(const AvisosEnvio *env, float x, float y, float w, float al) {
  float inicio = y;
  char kb[32], v[200], a[96], s[160];
  const char *tit, *txt;
  rgTamTexto(kb, sizeof kb, env->bytes ? env->bytes : envTam);
  rgAparelho(a, sizeof a, s, sizeof s, NULL, 0);
  if (env->estado == 2) {
    y += telefonecartao_texto(TXT_AJ_ESTADO, "Registro enviado", x, y, w, 28, .68f * al) + 12;
    tit = env->codigo[0] ? "Informe este código a quem faz o app" : "Registro enviado";
    txt = env->codigo[0] ? "No grupo ou na issue do GitHub. Com ele, dá para achar este registro entre os outros."
                         : "O servidor guardou o registro, mas não devolveu um código para ele.";
    y += telefonecartao_titulo(tit, x, y, w, al);
    y += telefonecartao_texto(TXT_AJ_TEXTO, txt, x, y, w, 30, .62f * al) + 24;
    if (env->codigo[0]) {
      float bw = 90;
      for (int i = 0; i < 6 && env->codigo[i]; i++) {
        char c[2] = {env->codigo[i], 0};
        bw = fmaxf(bw, txt_largura(TXT_LOG_COD, c) + 24);
      }
      int cols = w >= 6 * bw + 5 * 14 ? 6 : 3, n = 0;
      for (int i = 0; i < 6 && env->codigo[i]; i++, n++) {
        char c[2] = {env->codigo[i], 0};
        GfxRect r = {x + (i % cols) * (bw + 14), y + (i / cols) * 144, bw, 130};
        if (al > 0) {
          TxtLinha l = rgT(TXT_LOG_COD, c);
          rgNeutro(r, 20, .07f, RG_CHIP_SOL, al);
          txt_desenhar_alpha(l, r.x + (r.w - l.w) * .5f, r.y + (r.h - l.h) * .5f, al);
        }
      }
      y += ((n + cols - 1) / cols) * 144 + 12;
    }
    { struct tm tmv; time_t q = env->quando ? env->quando : time(NULL); localtime_r(&q, &tmv);
      snprintf(v, sizeof v, "%02d:%02d · %s · HTTP %d", tmv.tm_hour, tmv.tm_min, kb, env->http); }
    y += telefonecartao_texto(TXT_AJ_ESTADO, v, x, y, w, 28, .55f * al) + 24;
  } else if (env->estado == 3) {
    int off = env->motivo == AVISOS_ENVIO_OFFLINE;
    y += telefonecartao_texto(TXT_AJ_ESTADO, off ? "Sem internet" : "Não foi enviado", x, y, w, 28, .68f * al) + 12;
    if (off) {
      tit = "A TV está sem conexão";
      txt = "Nenhum dos últimos endereços respondeu. O registro fica guardado e sai assim que a rede voltar.";
    } else switch (env->motivo) {
      case AVISOS_ENVIO_PRAZO:
        tit = "O servidor de registros demorou demais";
        txt = "Passaram 30 s sem resposta. A rede da TV funciona; o servidor estava lento. Tente de novo daqui a pouco."; break;
      case AVISOS_ENVIO_CONEXAO:
        tit = "A conexão caiu no meio do envio";
        txt = "O registro continua aqui. Tente de novo; se repetir, confira a rede da TV."; break;
      case AVISOS_ENVIO_CONTA:
        tit = "Entre na sua conta para enviar";
        txt = "O servidor de registros só aceita envio de quem entrou com a conta Nuvio ou o Trakt."; break;
      case AVISOS_ENVIO_INDISPONIVEL:
        tit = "Esta versão não tem servidor de registros";
        txt = "O registro só pode ser lido nesta tela (botão vermelho ou Ajustes › Sobre e ajuda › Ver o registro na tela)."; break;
      default:
        tit = "O servidor de registros não aceitou agora";
        txt = ajustes_envio_auto() ? "O problema é do lado de lá, não da sua TV. O registro continua aqui; com “Enviar registros sozinho” ligado, ele vai no próximo envio automático."
                                  : "O problema é do lado de lá, não da sua TV. O registro continua aqui; tente de novo daqui a pouco."; break;
    }
    y += telefonecartao_titulo(tit, x, y, w, al);
    y += telefonecartao_texto(TXT_AJ_TEXTO, txt, x, y, w, 30, .62f * al) + 24;
    if (off) {
      int fal = 0, hs = 0; char host[64] = "", ha[48]; unsigned ms = 0;
      if (rede_saude_resumo(&fal, &hs, host, sizeof host, &ms)) { rgHa(ha, sizeof ha, (long)(ms / 1000)); snprintf(v, sizeof v, "%s · %s", ha, host); }
      else snprintf(v, sizeof v, "%s", i18n("nenhuma nesta sessão"));
      y += envTelefoneStat("Última resposta", v, x, y, w, al);
      snprintf(v, sizeof v, i18n("%d em %d hosts"), fal, hs);
      y += envTelefoneStat("Falhas seguidas", v, x, y, w, al);
      y += envTelefoneStat("Ao voltar", i18n("envia sozinho"), x, y, w, al);
      y += telefonecartao_texto(TXT_AJ_ESTADO, "esperando a rede", x, y, w, 28, .55f * al) + 24;
    } else {
      if (env->motivo == AVISOS_ENVIO_SERVIDOR) { snprintf(v, sizeof v, "HTTP %d", env->http); y += envTelefoneStat("Resposta", v, x, y, w, al); }
      { struct tm tmv; time_t q = env->quando ? env->quando : time(NULL); localtime_r(&q, &tmv);
        snprintf(v, sizeof v, "%02d:%02d · %s", tmv.tm_hour, tmv.tm_min, kb); }
      y += envTelefoneStat("Tentativa", v, x, y, w, al);
      if (ajustes_envio_auto() && env->autoProximoMs > SDL_GetTicks()) {
        long seg = (long)(env->autoProximoMs - SDL_GetTicks()) / 1000;
        if (seg < 60) snprintf(v, sizeof v, i18n("ligado · próximo em %ld s"), seg);
        else snprintf(v, sizeof v, i18n("ligado · próximo em %ld min"), (seg + 59) / 60);
      } else snprintf(v, sizeof v, "%s", ajustes_envio_auto() ? i18n("ligado") : i18n("desligado"));
      y += envTelefoneStat("Envio automático", v, x, y, w, al);
    }
  } else {
    y += telefonecartao_texto(TXT_AJ_ESTADO, "Sobre e ajuda", x, y, w, 28, .68f * al) + 12;
    y += telefonecartao_titulo("Enviando o registro", x, y, w, al);
    y += telefonecartao_texto(TXT_AJ_TEXTO, "Os últimos 200 KB desta sessão, sem senhas nem chaves, para quem faz o app. Endereços vão cortados no host.", x, y, w, 30, .62f * al) + 24;
    { char ln[32]; rgMilhar(ln, sizeof ln, env->linhas ? env->linhas : envLinhasArq); snprintf(v, sizeof v, i18n("%s · %s linhas"), kb, ln); }
    y += envTelefoneStat("Tamanho", v, x, y, w, al);
    { long sg = (long)(SDL_GetTicks() / 1000); char t[48];
      if (sg < 60) snprintf(t, sizeof t, i18n("%ld s"), sg); else snprintf(t, sizeof t, i18n("%ld min"), sg / 60);
      snprintf(v, sizeof v, i18n("aberta há %s"), t); }
    y += envTelefoneStat("Desta sessão", v, x, y, w, al);
    { char *p = strstr(s, " · "); if (p) *p = 0; }
    snprintf(v, sizeof v, "%s · %s", a, s);
    y += envTelefoneStat("Aparelho", v, x, y, w, al);
    y += telefonecartao_texto(TXT_AJ_SUB, "Enviando…", x, y, w, 30, .7f * al) + 24;
  }
  /* The same stored tail, drawn inside the body's current clip. No second
   * clipping surface can expose it below the fixed decision footer. */
  if (al > 0) {
    gfx_cor((GfxRect){x, y, w, 304}, 24.0f / 304, RG_CART_SOL, .7f * al);
    TxtLinha t = txt_linha(TXT_MONO15, "nuvio.log", RG_TX, 255);
    txt_desenhar_alpha(t, x + 22, y + 22, .6f * al);
    TxtLinha k = rgTC(TXT_AJ_MINI14, kb, w - 180);
    txt_desenhar_alpha(k, x + w - 22 - k.w, y + 22, .42f * al);
    int ini = nLin > 9 ? nLin - 9 : 0;
    for (int i = ini; i < nLin; i++) rgLinhaPacote(&lin[i], x, y + 64 + (i - ini) * 25, w, al);
  }
  return y - inicio + 304 + 16;
}
static void envTelefoneDesenhar(const AvisosEnvio *env) {
  int retry = env->estado == 3 && env->motivo != AVISOS_ENVIO_OFFLINE && env->motivo != AVISOS_ENVIO_INDISPONIVEL;
  telefonecartao_medir(&envTelefone, NV_TELA_W, NV_TELA_H, retry ? 2 : 1);
  float total = envTelefoneConteudo(env, 0, 0, envTelefone.corpo.w, 0);
  int chave = envTelefoneChave(env);
  telefonecartao_comecar(&envTelefone, total, chave, 1, envTelefoneRolar, 1);
  envTelefoneConteudo(env, envTelefone.corpo.x, envTelefone.corpo.y - envTelefone.offset, envTelefone.corpo.w, 1);
  gfx_sem_recorte();
  if (retry) telefonecartao_botao(&envTelefone, 0, "Tentar de novo", envFoco == 0, NULL, envTelefoneEscolher, chave, 1);
  telefonecartao_botao(&envTelefone, retry, env->estado == 1 ? "Fechar e continuar enviando" : "Fechar", !retry || envFoco == 1, NULL, envTelefoneEscolher, chave, 1);
}
#endif

static void registro_envio_desenharCorpo_(void);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void registro_envio_desenhar(void) {
  ESCALA_INI();
  registro_envio_desenharCorpo_();
  ESCALA_FIM();
}
static void registro_envio_desenharCorpo_(void) {
  AvisosEnvio env;
  float x, y, w, base = RE_Y + RE_H - 56;
  char kb[32], v[200], a[96], s[160];
  int st;
  if (!envAberto) return;
  // Modal sobre os Ajustes (ajustes_ux_ilha.inc): os alvos de tras deixam de valer.
  ponteiro_camada();
  st = avisos_envio_info(&env);
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { envTelefoneDesenhar(&env); return; }
#endif
  ajustes_ui_ilha((GfxRect){ RE_X, RE_Y, RE_W, RE_H }, 36, 1);
  rgTamTexto(kb, sizeof kb, env.bytes ? env.bytes : envTam);
  registro_pacote(RE_X + 60, RE_Y + 56, 400, 452, 15, kb, st == 1 ? 1.0f : st == 2 ? 0.55f : 0.4f);
  if (st == 1) {   // o brilho correndo embaixo do pacote
    float ar, ag, ab, px = RE_X + 60, pw = 400, by = RE_Y + 56 + 452 - 4;
    float t = ajustes_animacoes_reduzidas() ? 0.3f : fmodf((float)SDL_GetTicks() / 1600.0f, 1.0f);
    ajustes_acento(&ar, &ag, &ab);
    gfx_cor((GfxRect){ px, by, pw, 4 }, 0, 1, 1, 1, 0.06f);
    gfx_cor((GfxRect){ px + (pw - 120) * t, by, 120, 4 }, 0.5f, ar, ag, ab, 0.9f);
  }
  x = RE_X + 60 + 400 + 60; w = RE_X + RE_W - 60 - x; y = RE_Y + 56;
  rgAparelho(a, sizeof a, s, sizeof s, NULL, 0);
  if (st == 2) {
    rgIconeCor("aj_check", x, y - 1, 18, RG_VERDE, 1);
    rgCaps(TXT_MINI, "Registro enviado", 2.1f, x + 28, y, 76, 195, 138, 1);
    y += 18 + 8;
    y += rgBloco(TXT_ILHA_TITULO, env.codigo[0] ? "Informe este código a quem faz o app" : "Registro enviado",
                 x, y, w, 48, 1, 2) + 14;
    y += rgBloco(TXT_AJ_TEXTO, env.codigo[0] ? "No grupo ou na issue do GitHub. Com ele, dá para achar este registro entre os outros."
                                             : "O servidor guardou o registro, mas não devolveu um código para ele.",
                 x, y, w, 30, 0.62f, 3) + 34;
    if (env.codigo[0]) {
      int i;
      for (i = 0; i < 6 && env.codigo[i]; i++) {
        char c[2] = { env.codigo[i], 0 };
        TxtLinha l = rgT(TXT_LOG_COD, c);
        GfxRect r = { x + i * 104, y, 90, 130 };
        rgNeutro(r, 20, 0.07f, RG_CHIP_SOL, 1);
        txt_desenhar(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f);
      }
    }
    { struct tm tmv; time_t q = env.quando ? env.quando : time(NULL);
      localtime_r(&q, &tmv);
      snprintf(v, sizeof v, "%02d:%02d · %s · HTTP %d", tmv.tm_hour, tmv.tm_min, kb, env.http);
      { TxtLinha l = rgT(TXT_AJ_ESTADO, v); txt_desenhar_alpha(l, x, base - 28 - l.h * 0.5f, 0.55f); } }
    { TxtLinha l = rgTT(TXT_ILHA_ITEM, "Fechar"); float bw = 28 + l.w + 28; rgBotao("Fechar", NULL, x + w - bw, base - 56, 1, 1); }
    return;
  }
  if (st == 3) {
    int off = env.motivo == AVISOS_ENVIO_OFFLINE;
    const char *kick, *icone, *tit, *txt;
    int kr, kg, kbb;
    if (off) {
      kick = "Sem internet"; icone = "aj_wifi-off"; kr = 233; kg = 194; kbb = 122;
      tit = "A TV está sem conexão";
      txt = "Nenhum dos últimos endereços respondeu. O registro fica guardado e sai assim que a rede voltar.";
    } else {
      kick = "Não foi enviado"; icone = "aj_server-crash"; kr = 242; kg = 161; kbb = 150;
      switch (env.motivo) {
        case AVISOS_ENVIO_PRAZO:
          icone = "aj_clock";
          tit = "O servidor de registros demorou demais";
          txt = "Passaram 30 s sem resposta. A rede da TV funciona; o servidor estava lento. Tente de novo daqui a pouco.";
          break;
        case AVISOS_ENVIO_CONEXAO:
          icone = "aj_wifi-off";
          tit = "A conexão caiu no meio do envio";
          txt = "O registro continua aqui. Tente de novo; se repetir, confira a rede da TV.";
          break;
        case AVISOS_ENVIO_CONTA:
          icone = "aj_user-round";
          tit = "Entre na sua conta para enviar";
          txt = "O servidor de registros só aceita envio de quem entrou com a conta Nuvio ou o Trakt.";
          break;
        case AVISOS_ENVIO_INDISPONIVEL:
          tit = "Esta versão não tem servidor de registros";
          txt = "O registro só pode ser lido nesta tela (botão vermelho ou Ajustes › Sobre e ajuda › Ver o registro na tela).";
          break;
        default:
          tit = "O servidor de registros não aceitou agora";
          txt = ajustes_envio_auto() ? "O problema é do lado de lá, não da sua TV. O registro continua aqui; com “Enviar registros sozinho” ligado, ele vai no próximo envio automático."
                                     : "O problema é do lado de lá, não da sua TV. O registro continua aqui; tente de novo daqui a pouco.";
          break;
      }
    }
    rgIconeCor(icone, x, y - 1, 18, kr / 255.0f, kg / 255.0f, kbb / 255.0f, 1);
    rgCaps(TXT_MINI, kick, 2.1f, x + 28, y, kr, kg, kbb, 1);
    y += 18 + 8;
    y += rgBloco(TXT_ILHA_TITULO, tit, x, y, w, 48, 1, 2) + 14;
    y += rgBloco(TXT_AJ_TEXTO, txt, x, y, w, 30, 0.62f, 3) + 30;
    if (off) {
      int fal = 0, hs = 0;
      char host[64] = "", ha[48];
      unsigned ms = 0;
      if (rede_saude_resumo(&fal, &hs, host, sizeof host, &ms)) {
        rgHa(ha, sizeof ha, (long)(ms / 1000));
        snprintf(v, sizeof v, "%s · %s", ha, host);
      } else snprintf(v, sizeof v, "%s", i18n("nenhuma nesta sessão"));
      y += rgStat(i18n("Última resposta"), v, x, y, w);
      snprintf(v, sizeof v, i18n("%d em %d hosts"), fal, hs);
      y += rgStat(i18n("Falhas seguidas"), v, x, y, w);
      rgStat(i18n("Ao voltar"), i18n("envia sozinho"), x, y, w);
      { float bx = x;
        bx += rgBotao("Fechar", NULL, bx, base - 56, 1, 1) + 12;
        rgBotao("Tentar de novo", "aj_rotate-cw", bx, base - 56, 0, 0.45f);
        { TxtLinha l = rgT(TXT_AJ_ESTADO, "esperando a rede");
          txt_desenhar_alpha(l, x + w - l.w, base - 28 - l.h * 0.5f, 0.55f);
          rgRespira(x + w - l.w - 10 - 10, base - 28, 0.914f, 0.761f, 0.478f); } }
      return;
    }
    if (env.motivo == AVISOS_ENVIO_SERVIDOR) {
      snprintf(v, sizeof v, "HTTP %d", env.http);
      { TxtLinha lk = rgT(TXT_AJ_ESTADO, "Resposta"), lv = txt_linha(TXT_AJ_ESTADO, v, 242, 161, 150, 255);
        gfx_cor((GfxRect){ x, y, w, 1 }, 0, 1, 1, 1, 0.07f);
        txt_desenhar_alpha(lk, x, y + 12, 0.45f);
        txt_desenhar(lv, x + w - lv.w, y + 12);
        y += 1 + 11 + 20.6f + 11; }
    }
    { struct tm tmv; time_t q = env.quando ? env.quando : time(NULL);
      localtime_r(&q, &tmv);
      snprintf(v, sizeof v, "%02d:%02d · %s", tmv.tm_hour, tmv.tm_min, kb);
      y += rgStat(i18n("Tentativa"), v, x, y, w); }
    if (ajustes_envio_auto() && env.autoProximoMs > SDL_GetTicks()) {
      long seg = (long)(env.autoProximoMs - SDL_GetTicks()) / 1000;
      if (seg < 60) snprintf(v, sizeof v, i18n("ligado · próximo em %ld s"), seg);
      else snprintf(v, sizeof v, i18n("ligado · próximo em %ld min"), (seg + 59) / 60);
    } else snprintf(v, sizeof v, "%s", ajustes_envio_auto() ? i18n("ligado") : i18n("desligado"));
    rgStat(i18n("Envio automático"), v, x, y, w);
    { float bx = x;
      int pode = env.motivo != AVISOS_ENVIO_INDISPONIVEL;
      if (pode) {
        float w0 = rgBotao("Tentar de novo", "aj_rotate-cw", bx, base - 56, envFoco == 0, 1);
        TxtLinha lf = rgT(TXT_ILHA_ITEM, "Fechar");
        ponteiro_alvo(bx, base - 56, w0, 56, ponteiroEnvio, NULL, 0, 0);
        ponteiro_alvo(bx + w0 + 12, base - 56, 28 + lf.w + 28, 56, ponteiroEnvio, NULL, 1, 0);
        bx += w0 + 12;
      }
      rgBotao("Fechar", NULL, bx, base - 56, !pode || envFoco == 1, 1); }
    return;
  }
  // Enviando.
  ajustes_ui_kicker("Sobre e ajuda", x, y, 1);
  y += 18 + 8;
  y += rgBloco(TXT_ILHA_TITULO, "Enviando o registro", x, y, w, 48, 1, 2) + 14;
  y += rgBloco(TXT_AJ_TEXTO, "Os últimos 200 KB desta sessão, sem senhas nem chaves, para quem faz o app. Endereços vão cortados no host.",
               x, y, 640, 30, 0.62f, 3) + 34;
  { char ln[32];
    rgMilhar(ln, sizeof ln, env.linhas ? env.linhas : envLinhasArq);
    snprintf(v, sizeof v, i18n("%s · %s linhas"), kb, ln);
    y += rgStat(i18n("Tamanho"), v, x, y, w); }
  { long sg = (long)(SDL_GetTicks() / 1000);
    char t[48];
    if (sg < 60) snprintf(t, sizeof t, i18n("%ld s"), sg); else snprintf(t, sizeof t, i18n("%ld min"), sg / 60);
    snprintf(v, sizeof v, i18n("aberta há %s"), t);
    y += rgStat(i18n("Desta sessão"), v, x, y, w); }
  { char *p = strstr(s, " · "); if (p) *p = 0; }
  snprintf(v, sizeof v, "%s · %s", a, s);
  rgStat(i18n("Aparelho"), v, x, y, w);
  { float ar, ag, ab;
    TxtLinha l = rgT(TXT_AJ_SUB, "Enviando…");
    ajustes_acento(&ar, &ag, &ab);
    rgRespira(x, base - 14, ar, ag, ab);
    txt_desenhar_alpha(l, x + 22, base - 14 - l.h * 0.5f, 0.7f);
    rgKbdTexto("Voltar", "Fechar e continuar enviando", x + w, base - 14); }
}

#ifdef REGISTRO_TESTE
void registro_teste_envio(int abrir) { envTesteFixo = 1; if (abrir) registro_envio_abrir(); else envAberto = 0; }
void registro_teste_envio_foco(int f) { envFoco = f; }
#endif
