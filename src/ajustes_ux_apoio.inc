// APOIAR O PROJETO (Sobre e ajuda): o painel com os QRs de verdade (apoio.h).
// Era a previa da linha na 2.0.2; no redesenho a previa virou uma cena sem
// palavras (ajcT_apoiar) e os codigos sumiram, entao o OK da linha abre este
// painel. Mesmos enderecos de sempre (apoio.c). O QR vai num cartao branco de
// cantos arredondados (apoio_qr), grande o bastante para a camera a 3 m.
// OK e Voltar fecham e a pessoa continua na linha "Apoiar o projeto".
static void apoioAbrir(void) { apoioAberto = 1; }
static void apoioFechar(void) { apoioAberto = 0; focarOpcao(AJ_APOIAR); }
static void apoioEvento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || guiaVoltar(k, e)) apoioFechar();
}
#ifdef NV_TOUCH_UI
static void apoioPontFechar(int a, int b) { (void)a; (void)b; if (apoioAberto) apoioFechar(); }
typedef struct { GfxRect painel; float lado, gap, pad; int cols, linhas; } ApoioTelefone;
static ApoioTelefone apoioTelefoneMedir(int n, float ajudaAltura) {
  ApoioTelefone v = { .gap = 24, .pad = 32 };
  float margem = 48 / ajustes_tamanho_ajustes();
  float w = fminf(908, NV_VTELA_W - 2 * margem), util = w - 2 * v.pad;
  v.cols = (int)((util + v.gap) / (300 + v.gap));
  if (v.cols < 1) v.cols = 1;
  if (n > 0 && v.cols > n) v.cols = n;
  v.linhas = n > 0 ? (n + v.cols - 1) / v.cols : 0;
  v.lado = fminf(300, (util - (v.cols - 1) * v.gap) / v.cols);
  // A descricao tem ate tres linhas; o rodape de cada QR cabe no seu cartao.
  float fixo = 84 + ajudaAltura + 34 + v.pad;
  if (v.linhas > 0) v.lado = fminf(v.lado, (NV_VTELA_H - 48 - fixo) / v.linhas - 22 - 64 - 12 - 30 - v.gap);
  float h = fixo + v.linhas * (v.lado + 22 + 64 + 12 + 30 + v.gap);
  v.painel = (GfxRect){(NV_VTELA_W - w) * .5f, (NV_VTELA_H - h) * .5f, w, h};
  return v;
}
static void apoioDesenharTelefone(void) {
  int n = apoio_n();
  ApoioTelefone v = apoioTelefoneMedir(n, 3 * 28.5f);
  float ajudaAltura = txt_bloco_corta(TXT_AJ_SUB, ajudaOpcao(AJ_APOIAR), 243, 242, 239,
                                     0, 0, v.painel.w - 2 * v.pad, 28.5f, 0, 3);
  v = apoioTelefoneMedir(n, ajudaAltura);
  GfxRect m = v.painel, fechar = {m.x + m.w - v.pad - 56, m.y + 24, 56, 56};
  float y = m.y + 84, passo = v.lado + 22 + 64 + 12 + 30 + v.gap;
  float total = v.cols * v.lado + (v.cols - 1) * v.gap, x0 = m.x + (m.w - total) * .5f;
  ajVeuModal(); ajIlha(m, 36, 1, 1);
  ajIcone("aj_heart", m.x + v.pad, m.y + 36, 22, .8f);
  { TxtLinha t = ajTxtC(TXT_AJ_SEG, "Apoiar o projeto", m.w - 2 * v.pad - 110);
    txt_desenhar(t, m.x + v.pad + 34, m.y + 36); }
  ajNeutro(fechar, 28, .08f, AJ_CHIP_SOL, 1);
  ajIcone("aj_x", fechar.x + 17, fechar.y + 17, 22, 1);
  ponteiro_camada(); ajToqueCamada();
  ponteiro_alvo(0, 0, NV_VTELA_W, NV_VTELA_H, NULL, apoioPontFechar, 0, 0);
  ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
  ponteiro_alvo(fechar.x, fechar.y, fechar.w, fechar.h, NULL, apoioPontFechar, 0, 0);
  txt_bloco_corta(TXT_AJ_SUB, ajudaOpcao(AJ_APOIAR), 243, 242, 239,
                  m.x + v.pad, y, m.w - 2 * v.pad, 28.5f, .62f, 3);
  y += ajudaAltura + 34;
  for (int i = 0; i < n; i++) {
    int q = apoio_qual(i);
    float qx = x0 + (i % v.cols) * (v.lado + v.gap), qy = y + (i / v.cols) * passo;
    TxtLinha nome = txt_linha_corta(TXT_W20_24B, apoio_nome(q), 20, 21, 26, 255, v.lado - 32);
    TxtLinha u = txt_linha_corta(TXT_V2_18, apoio_url_curta(q), 243, 242, 239, 255, v.lado);
    apoio_qr(q, qx, qy, v.lado, 1);
    GfxRect rot = {qx, qy + v.lado + 22, v.lado, 64};
    gfx_cor(rot, .11f, .957f, .961f, .980f, 1);
    txt_desenhar(nome, qx + (v.lado - nome.w) * .5f, rot.y + (64 - nome.h) * .5f);
    txt_desenhar_alpha(u, qx + (v.lado - u.w) * .5f, rot.y + 76, .62f);
  }
}
#endif
static void apoioDesenhar(void) {
#ifdef NV_TOUCH_UI
  if (ajTelaCheia()) { apoioDesenharTelefone(); return; }
#endif
  const float lado = 300.0f, gap = 100.0f, pad = 44.0f;
  int n = apoio_n(), i;
  float total = n > 0 ? (float)n * lado + (float)(n - 1) * gap : 0.0f;
  float w = total + 2 * pad + 120.0f, h, y, x0;
  GfxRect m;
  AjDica d[1] = { { "Voltar", "Fechar" } };
  float dw = ajDicasLargura(d, 1);
  if (w < 760.0f) w = 760.0f;
  h = 84.0f + 80.0f + 34.0f + lado + 22.0f + 64.0f + 12.0f + 30.0f + pad;
  m = (GfxRect){ (NV_VTELA_W - w) * 0.5f, (NV_VTELA_H - h) * 0.5f, w, h };
  ajVeuModal();
  ajIlha(m, 36, 1, 1);
  ajIcone("aj_heart", m.x + pad, m.y + 36, 22, 0.8f);
  ajKicker("Apoiar o projeto", m.x + pad + 34, m.y + 38, 1);
  ajDicasLinha(d, 1, m.x + m.w - pad - dw, m.y + 40, 1);
  y = m.y + 84;
  y += ajBloco(TXT_AJ_SUB, ajudaOpcao(AJ_APOIAR), m.x + pad, y, m.w - 2 * pad, 28.5f, 0.62f, 3);
  y += 34;
  x0 = m.x + (m.w - total) * 0.5f;
  for (i = 0; i < n; i++) {
    int q = apoio_qual(i);
    float qx = x0 + (float)i * (lado + gap);
    TxtLinha u = txt_linha_corta(TXT_V2_18, apoio_url_curta(q), 243, 242, 239, 255, lado + gap * 0.8f);
    apoio_qr(q, qx, y, lado, 1.0f);
    apoio_rotulo(q, qx + lado * 0.5f, y + lado + 22, 64.0f, 1, 1.0f);
    txt_desenhar_alpha(u, qx + (lado - (float)u.w) * 0.5f, y + lado + 22 + 64 + 12, 0.62f);
  }
}

