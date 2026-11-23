// CORRECAO-FAROL6B: o caminho de PRODUCAO do Resolver com o motor de
// playbooks. Amarra o src/resolver.c real — o mesmo arquivo que a TV compila —
// com um cenario mutavel nos stubs (o "mini app" dos testes de UI). O que a
// revisao reprovou era julgar so no teste: aqui quem falha e o testar() de
// producao. Cobre:
//   - add-on instalado que voltou sem fonte (addonsMotivo) -> Add-ons RES_FALHOU
//   - falha recente de Fonte com erro desconhecido (FC_NENHUMA/"ums_outro")
//   - IMAGEM_DV nao dispara sem o sintoma "cores estranhas" (mascara propria)
//   - cartao/veredito Farol: texto promete a acao que o botao executa
//   - despacho: PLAYER_EXTERNO -> RESOLVER_PEDIU_PLAYER_EXT,
//               RECONECTAR_DEBRID -> RESOLVER_PEDIU_DEBRID
// Mutantes que tem de quebrar este teste: tirar addonsMotivo da ponte
// farol_fatos, voltar a filtrar FC_NENHUMA como "sem falha", ou devolver a
// linha de resolver.c para resolverregra_testar so.

#include "resolver.h"
#include "resolverregra.h"
#include "farolregra.h"
#include "farolerro.h"
#include "farolanel.h"
#include "faroltexto.h"
#include "fontesonda.h"
#include "streams.h"
#include "avisos.h"
#include "gfx.h"
#include "text.h"
#include "ponteiro.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------- cenario
static int c_offline, c_addons = 3, c_ocupado, c_semPlano, c_recusa, c_debrid = 1,
           c_tvNegou, c_temErro, c_erroDebrid, c_kb, c_streamAtual = -1;
static long c_mem = -1;
static char c_motivo[200], c_recusaTxt[64];
static FarolErro c_erro;
static Stream c_stream;
static AvisosEnvio c_envio;
static int c_envioChamadas;

// ------------------------------------------------------------------- stubs
const char *i18n(const char *s) { return s; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *v) { (void)n; (void)v; return 1; }
int rede_saude_offline(void) { return c_offline; }
int addons_n(void) { return c_addons; }
int addons_ocupado(void) { return c_ocupado; }
int addons_motivo_vazio(char *d, unsigned n) {
  if (!c_motivo[0]) return 0;
  snprintf(d, n, "%s", c_motivo);
  return 1;
}
int debrid_sem_plano(void) { return c_semPlano; }
const char *debrid_sem_plano_frase(int m) { (void)m; return c_semPlano ? "Sua conta de debrid está sem plano." : NULL; }
int debrid_recusa(char *d, unsigned n) {
  if (!c_recusa) return 0;
  snprintf(d, n, "%s", c_recusaTxt[0] ? c_recusaTxt : "TorBox 403");
  return 1;
}
int debrid_ativo(void) { return c_debrid; }
int video_registro_negado(void) { return c_tvNegou; }
int player_ultimo_erro_farol(FarolErro *o, int *foiDebrid) {
  if (!c_temErro) return 0;
  *o = c_erro;
  if (foiDebrid) *foiDebrid = c_erroDebrid;
  return 1;
}
int stream_atual(void) { return c_streamAtual; }
const Stream *stream_item(int i) { (void)i; return &c_stream; }
int stream_kbps_atual(void) { return c_kb; }
long memlog_livre_mb(void) { return c_mem; }
int avisos_envio_info(AvisosEnvio *o) { *o = c_envio; c_envioChamadas++; return 1; }
int ajustes_animacoes_reduzidas(void) { return 1; }
void ajustes_acento(float *r, float *g, float *b) { *r = 0.4f; *g = 0.7f; *b = 1.0f; }
float ajustes_acento_tinta(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
int ajustes_tinta_foco(void) { return 255; }
int ajustes_tinta_foco2(void) { return 200; }
float botao_largura(const char *rot, const char *ic, int pri) { (void)rot; (void)ic; (void)pri; return 100.0f; }
void botao_pilula(GfxRect r, const char *rot, const char *ic, float f, int p, int al, float a) { (void)r; (void)rot; (void)ic; (void)f; (void)p; (void)al; (void)a; }
float botao_cor_foco(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
void botao_luz(GfxRect r, float f, float a) { (void)r; (void)f; (void)a; }
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) { (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) { (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) { (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca; }
float gfx_escala_ui(void) { return 1.0f; }
float gfx_escala(void) { return 1.0f; }
float gfx_escala_entrar(void) { return 1.0f; }
void gfx_escala_sair(float a) { (void)a; }
int ponteiro_ativo(void) { return 0; }
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn fo, PonteiroFn at, int a, int b) { (void)x; (void)y; (void)w; (void)h; (void)fo; (void)at; (void)a; (void)b; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) { TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; return l; }
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float mw) { TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; (void)mw; return l; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) { (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml; return 0.0f; }
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float t) { (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)t; return 0.0f; }

// ------------------------------------------------------------- cenario-base
static void zerar(void) {
  memset(&c_stream, 0, sizeof c_stream);
  memset(&c_erro, 0, sizeof c_erro);
  memset(&c_envio, 0, sizeof c_envio);
  c_offline = 0; c_addons = 3; c_ocupado = 0; c_semPlano = 0; c_recusa = 0;
  c_debrid = 1; c_tvNegou = 0; c_temErro = 0; c_erroDebrid = 0; c_kb = 0;
  c_streamAtual = -1; c_mem = -1; c_motivo[0] = 0; c_recusaTxt[0] = 0;
  c_envioChamadas = 0;
  c_envio.disponivel = 0;
  resolverregra_falha_limpar();
  resolver_teste_fatos(NULL);
  resolver_teste_fatos_farol(NULL);
}

// Uma falha recente de fonte como player_erro_fonte registra: a memoria do
// Resolver mais o erro nativo normalizado que so o Farol le.
static void falhar_fonte(const char *titulo, const char *dica,
                         FarolClasse classe, int http, const char *cod) {
  resolverregra_falha_registrar(RES_T_FONTE, titulo, dica, "T2:E5", (long)time(NULL));
  c_temErro = 1;
  memset(&c_erro, 0, sizeof c_erro);
  c_erro.classe = classe;
  c_erro.http = http;
  snprintf(c_erro.cod, sizeof c_erro.cod, "%s", cod ? cod : "");
}

static void apertar_ok(void) {
  SDL_Event evento = { .type = SDL_KEYDOWN };
  evento.key.keysym.sym = SDLK_RETURN;
  resolver_evento(&evento);
}

int main(void) {
  const int *est;
  FarolFatos fx;

  // ------------------------------------------------------------ cenario 1
  // Add-on instalado que voltou sem fonte: o motivo da busca tem de chegar
  // como RES_FALHOU na etapa Add-ons (antes a ponte perdia o diagnostico).
  zerar();
  snprintf(c_motivo, sizeof c_motivo, "%s",
           "4 add-ons responderam: nenhum tem este título");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  est = resolver_teste_estados();
  assert(est[RES_T_ADDONS] == RES_FALHOU);
  assert(resolver_teste_veredito() == FAROL_C_NADA_ACHADO);
  assert(strstr(resolver_teste_titulo(), "add-ons") != NULL);
  assert(strstr(resolver_teste_corpo(), "nenhum tem este título") != NULL);
  apertar_ok();
  assert(resolver_pedido() == RESOLVER_PEDIU_FONTES);
  assert(!resolver_aberto());

  // ------------------------------------------------------------ cenario 2
  // Falha recente de Fonte com causa nativa desconhecida: FC_NENHUMA +
  // "ums_outro". O filtro velho tratava como sem falha e a tela dizia
  // NADA_ACHADO; agora a etapa Fonte falha e o cartao vem da cadeia.
  zerar();
  falhar_fonte("A última fonte falhou.", "Escolha outra fonte.",
               FC_NENHUMA, 0, "ums_outro");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  est = resolver_teste_estados();
  assert(est[RES_T_FONTE] == RES_FALHOU);
  assert(resolver_teste_veredito() == FAROL_C_NADA_ACHADO);
  assert(strstr(resolver_teste_titulo(), "fonte falhou") != NULL);
  apertar_ok();
  assert(c_envioChamadas >= 1);

  // ------------------------------------------------------------ cenario 3
  // Mascara de sintoma: fonte DV + TV sem DV so pode dar IMAGEM_DV quando a
  // pessoa escolheu "cores estranhas" — nunca pelo RES_S_GERAL do registro.
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_WEBOS;
  fx.addonsQtd = 3;
  fx.fonteDv = 1;
  fx.tvDv = 0;
  fx.erro.classe = FC_HTTP;
  fx.erro.http = 403;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_GERAL);
  assert(resolver_teste_veredito() != FAROL_C_IMAGEM_DV);
  fx.erro.classe = FC_NENHUMA;
  fx.erro.http = 0;
  resolver_teste_fatos_farol(&fx);
  resolver_teste_testar(FAROL_S_CORES);
  assert(resolver_teste_veredito() == FAROL_C_IMAGEM_DV);
  // DV sem executor: o primario tem de cair no registro, nunca botao morto.
  assert(strstr(resolver_teste_titulo(), FT_IMAGEM_DV_L1) != NULL);
  apertar_ok();
  assert(c_envioChamadas >= 1);

  // ------------------------------------------------------------ cenario 4
  // DEBRID_DESCONECTOU: o cartao vem do playbook e o OK pede a tela de
  // login/QR que ja existe (RESOLVER_PEDIU_DEBRID) — despacho novo.
  zerar();
  assert(!fontesonda_host_debrid("https://real-debrid.com.outro.example/video"));
  assert(!fontesonda_host_debrid("https://torbox.app@outro.example/video"));
  for (int http = 401; http <= 403; http += 2) {
    falhar_fonte("A última fonte falhou.", "", FC_HTTP, http, "http");
    c_erroDebrid = fontesonda_host_debrid("https://CDN.real-debrid.com:443/video");
    resolver_abrir();
    resolver_teste_testar(RES_S_NAO_COMECA);
    assert(resolver_teste_veredito() == FAROL_C_DEBRID_DESCONECTOU);
    assert(strstr(resolver_teste_titulo(), FT_DEBRID_DESCONECTOU_L1) != NULL);
    assert(strstr(resolver_teste_corpo(), FT_DEBRID_DESCONECTOU_L2) != NULL);
    assert(resolver_teste_prim() == FAROL_A_RECONECTAR_DEBRID);
    apertar_ok();
    assert(resolver_pedido() == RESOLVER_PEDIU_DEBRID);
    assert(!resolver_aberto());
  }

  // ------------------------------------------------------------ cenario 5
  // PLAYER_EXTERNO no Android: TV_FORMATO com player instalado promove a
  // alternativa; a linha 2 dedicada promete o player externo e o OK pede
  // RESOLVER_PEDIU_PLAYER_EXT. Sem player, cai no registro (botao vivo).
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_ANDROID;
  fx.addonsQtd = 3;
  fx.erro.classe = FC_FORMATO;
  fx.falhasSeguidas = 1;
  fx.externoDisponivel = 1;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  assert(strstr(resolver_teste_titulo(), FT_TV_FORMATO_L1) != NULL);
  assert(strstr(resolver_teste_corpo(), FT_L2_PLAYER_EXTERNO) != NULL);
  apertar_ok();
  assert(resolver_pedido() == RESOLVER_PEDIU_PLAYER_EXT);
  assert(!resolver_aberto());

  // ------------------------------------------------------------ cenario 6
  // TV_FORMATO_REPETE no Android com externo: a acao da propria regra e
  // PLAYER_EXTERNO — o despacho sai igual.
  zerar();
  fx.falhasSeguidas = 2;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  assert(resolver_teste_sec() == FAROL_A_ENVIAR_REGISTRO);
  assert(strstr(resolver_teste_corpo(), FT_L2_PLAYER_EXTERNO) != NULL);
  apertar_ok();
  assert(resolver_pedido() == RESOLVER_PEDIU_PLAYER_EXT);

  // ------------------------------------------------------------ cenario 7
  // Sem player externo nem outra alternativa com executor: mesma regra, e o
  // primario vira o envio do registro — o contrato "sem botao morto".
  zerar();
  fx.externoDisponivel = 0;
  fx.falhasSeguidas = 1;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  apertar_ok();
  assert(c_envioChamadas >= 1);

  // ------------------------------------------------------------ cenario 8
  // O veredito entra no anel de eventos (o que a TV registra de verdade).
  {
    FarolEvento ev[FAROL_ANEL_N];
    int n = farolanel_ler(ev, FAROL_ANEL_N, 0), i, achou = 0;
    for (i = 0; i < n; i++)
      if (ev[i].ev == FE_FAROL_VEREDITO) achou = 1;
    assert(achou);
  }

  puts("farol_resolver: producao ok");
  return 0;
}
