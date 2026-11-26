// Farol #16: acoes de audio e de memoria que APLICAM a correcao.
// Compila src/farolacao_audio.c e src/memlog.c reais; o resto (ajustes, faixas,
// ilha, anel do Farol) e stub gravador. Cobre:
//   - decisao pura por plataforma (LG, .tpk 6+, .tpk 4.0-5.5, WASM, Android):
//     WASM, .tpk 4.0-5.5 e Android NUNCA oferecem conversao;
//   - CONVERTER_AUDIO: liga o ajuste, pede reabertura, par fazer/desfazer
//     devolve o valor ANTERIOR do ajuste (LG e .tpk), recusa fora da lista;
//   - TROCAR_FAIXA: faixa compativel vs ausencia de faixa compativel;
//   - ALIVIAR_REABRIR: memlog_pode_reabrir real, verdadeiro vs falso;
//   - farol_registrar_acao/resultado em toda execucao.
#include "farolacao_audio.h"
#include "farol.h"
#include "faroltexto.h"
#include "ilhaacao.h"
#include "memlog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// ------------------------------------------------------------------ stubs ----
const char *i18n(const char *s) { return s ? s : ""; }
// Os padroes de farolacao_audio_ops_padrao (ajustes.c, faixas.c): o teste so
// usa as ops injetadas, entao estes nunca devem rodar.
int  ajustes_dts_valor(void) { assert(0); return 0; }
void ajustes_dts_definir(int v) { (void)v; assert(0); }
int  faixas_audio_compativel(void) { assert(0); return -1; }
int  faixas_audio_trocar_compativel(void) { assert(0); return 0; }

#define EV_MAX 16
static int evAcao[EV_MAX], evRes[EV_MAX], nAcao, nRes, resOk[EV_MAX];
void farol_registrar_acao(int acao) { if (nAcao < EV_MAX) evAcao[nAcao++] = acao; }
void farol_registrar_resultado(int acao, int ok) {
  if (nRes < EV_MAX) { evRes[nRes] = acao; resOk[nRes++] = ok; }
}

static IlhaAcao ilha;           // ultima acao postada na ilha
static char ilhaCtx[64];
static int nIlha;
void ilhaacao_feita(const IlhaAcao *a) {
  ilha = *a;
  if (a->ctx && a->ctxN <= sizeof ilhaCtx) { memcpy(ilhaCtx, a->ctx, a->ctxN); ilha.ctx = ilhaCtx; }
  nIlha++;
}
// O que ilhaacao_botao(1) faz: chama o desfazer uma vez com a copia do contexto.
static void ilhaDesfazer(void) {
  IlhaDesfazer fn = ilha.desfazer;
  assert(fn);
  ilha.desfazer = NULL;
  fn(ilha.ctx);
}

// Ajuste do DTS, faixas e memoria simulados.
static int dtsValor, dtsGravacoes;
static int dtsAceita = 1;
static int dtsV(void) { return dtsValor; }
static void dtsD(int v) { dtsGravacoes++; if (dtsAceita) dtsValor = v; }

static int faixaIdx = -1, faixaTrocas;
static int fxComp(void) { return faixaIdx; }
static int fxTroca(void) { if (faixaIdx < 0) return 0; faixaTrocas++; return 1; }

static long memSeq[4];
static int memN, memPos, aliviou;
static long memLivre(void) { long v = memSeq[memPos < memN ? memPos : memN - 1]; memPos++; return v; }
static void memAliviar(void) { aliviou++; }

static int reabriu;
static void reabrir(void) { reabriu++; }

static void zerar(void) {
  nAcao = nRes = nIlha = 0;
  memset(&ilha, 0, sizeof ilha);
  dtsValor = 0; dtsGravacoes = 0; dtsAceita = 1;
  faixaIdx = -1; faixaTrocas = 0;
  memN = 1; memSeq[0] = 500; memPos = 0; aliviou = 0;
  reabriu = 0;
  farolacao_audio_quando_reabrir(NULL);
  (void)farolacao_audio_reabertura_consumir();
}

static void ops(void) {
  FarolAcaoAudioOps o;
  o.dts_valor = dtsV; o.dts_definir = dtsD;
  o.faixa_compativel = fxComp; o.faixa_trocar = fxTroca;
  o.mem_livre_mb = memLivre; o.mem_aliviar = memAliviar;
  farolacao_audio_teste_ops(&o);
}

static FarolFatos fatos(FarolPlat p, int os) {
  FarolFatos f;
  farolregra_fatos_iniciar(&f);
  f.plat = p; f.osMaior = os;
  f.faixaDts = 1; f.conversaoPossivel = 1; f.outraFaixa = 1;
  return f;
}

// ---------------------------------------------------------------- decisao ----
static void decisao_plataforma(void) {
  const int motor[] = {0, 1}, falhou[] = {0, 1}, recusado[] = {0, 1};
  int m, fa, r, os;
  for (m = 0; m < 2; m++) for (fa = 0; fa < 2; fa++) for (r = 0; r < 2; r++) {
    // WASM (.wgt), Android e outras: NUNCA, nenhum fato muda isso (-1 = sem conversao).
    for (os = -1; os <= 12; os++) {
      assert(farolacao_audio_conversao_possivel(FP_TIZEN_WGT, os, motor[m], falhou[fa], recusado[r]) == -1);
      assert(farolacao_audio_conversao_possivel(FP_ANDROID, os, motor[m], falhou[fa], recusado[r]) == -1);
      assert(farolacao_audio_conversao_possivel(FP_OUTRA, os, motor[m], falhou[fa], recusado[r]) == -1);
    }
    // .tpk 4.0-5.5 (e desconhecido): nunca 1.
    for (os = -1; os <= 5; os++)
      assert(farolacao_audio_conversao_possivel(FP_TIZEN_TPK, os, motor[m], falhou[fa], recusado[r]) == 0);
    // .tpk 6+: 1 so com o motor pronto e sem falha.
    for (os = 6; os <= 9; os++)
      assert(farolacao_audio_conversao_possivel(FP_TIZEN_TPK, os, motor[m], falhou[fa], recusado[r]) ==
             (motor[m] && !falhou[fa] ? 1 : 0));
  }
  // LG: release >= 5 converte antes de tocar; < 5 (ou desconhecido) so depois da recusa.
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, 5, 1, 0, 0) == 1);
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, 11, 1, 0, 0) == 1);
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, 26, 1, 0, 0) == 1);
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, 4, 1, 0, 0) == 0);
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, -1, 1, 0, 0) == 0);
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, 4, 1, 0, 1) == 1);
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, 6, 0, 0, 1) == 0);   // sem motor
  assert(farolacao_audio_conversao_possivel(FP_WEBOS, 6, 1, 1, 1) == 0);   // ja falhou

  assert(farolacao_audio_plataforma_converte(FP_WEBOS, -1));
  assert(farolacao_audio_plataforma_converte(FP_TIZEN_TPK, 6));
  assert(!farolacao_audio_plataforma_converte(FP_TIZEN_TPK, 5));
  assert(!farolacao_audio_plataforma_converte(FP_TIZEN_TPK, -1));
  assert(!farolacao_audio_plataforma_converte(FP_TIZEN_WGT, 9));
  assert(!farolacao_audio_plataforma_converte(FP_ANDROID, 14));

  // O alvo do ajuste: LG preserva AAC/AC3 e leva original/desligado a AAC; .tpk liga o HLS.
  assert(farolacao_audio_dts_alvo(FP_WEBOS, 0) == 1);
  assert(farolacao_audio_dts_alvo(FP_WEBOS, 1) == 1);
  assert(farolacao_audio_dts_alvo(FP_WEBOS, 2) == 2);
  assert(farolacao_audio_dts_alvo(FP_WEBOS, 3) == 1);
  // .tpk 6+: 1 (5.1) e 2 (Estereo AAC) ja convertem e sao preservados;
  // desligado ou fora do intervalo liga o 5.1.
  assert(farolacao_audio_dts_alvo(FP_TIZEN_TPK, 0) == 1);
  assert(farolacao_audio_dts_alvo(FP_TIZEN_TPK, 1) == 1);
  assert(farolacao_audio_dts_alvo(FP_TIZEN_TPK, 2) == 2);
  assert(farolacao_audio_dts_alvo(FP_TIZEN_TPK, 3) == 1);
  assert(farolacao_audio_dts_alvo(FP_TIZEN_TPK, -1) == 1);
  assert(farolacao_audio_dts_alvo(FP_TIZEN_WGT, 0) == -1);
  assert(farolacao_audio_dts_alvo(FP_ANDROID, 0) == -1);
  puts("ok  decisao pura por plataforma (WASM, .tpk 4.0-5.5 e Android nunca convertem)");
}

static void decisao_faixa(void) {
  const signed char a[] = {1, 0, 1}, b[] = {1, 1, 1}, c[] = {0, 0};
  assert(farolacao_audio_faixa_compativel(3, 0, 1, a) == 1);      // atual DTS: pula DTS
  assert(farolacao_audio_faixa_compativel(3, 1, 0, a) == 0);      // atual nao-DTS: qualquer outra
  assert(farolacao_audio_faixa_compativel(3, 0, 1, b) == -1);     // so DTS: nenhuma
  assert(farolacao_audio_faixa_compativel(1, 0, 1, a) == -1);     // faixa unica
  assert(farolacao_audio_faixa_compativel(0, -1, 0, a) == -1);
  assert(farolacao_audio_faixa_compativel(2, -1, 0, c) == 0);     // sem faixa atual
  puts("ok  consulta pura: ha outra faixa que a TV toca");
}

// --------------------------------------------------------------- executores --
static void converter_lg_com_desfazer(void) {
  FarolFatos f = fatos(FP_WEBOS, 6);
  zerar(); ops();
  dtsValor = 3;   // Desligado
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1);
  assert(dtsValor == 1 && dtsGravacoes == 1);
  assert(farolacao_audio_reabertura_consumir() == 1);
  assert(nIlha == 1 && ilha.desfazer && ilha.ctxN > 0 && ilha.frase[0]);
  assert(nAcao == 1 && evAcao[0] == FAROL_A_CONVERTER_AUDIO);
  assert(nRes == 1 && evRes[0] == FAROL_A_CONVERTER_AUDIO && resOk[0] == 1);
  // Desfazer devolve o valor ANTERIOR (3) e reabre outra vez.
  ilhaDesfazer();
  assert(dtsValor == 3 && dtsGravacoes == 2);
  assert(farolacao_audio_reabertura_consumir() == 1);

  // Original (0) tambem volta a 0.
  zerar(); dtsValor = 0;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1 && dtsValor == 1);
  ilhaDesfazer();
  assert(dtsValor == 0);

  // A pessoa mexeu no ajuste depois do Farol: o Desfazer nao pisa na escolha dela.
  zerar(); dtsValor = 3;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1);
  dtsValor = 2;   // AC3 5.1 escolhido a mao
  (void)farolacao_audio_reabertura_consumir();
  ilhaDesfazer();
  assert(dtsValor == 2 && farolacao_audio_reabertura_consumir() == 0);

  // O ajuste recusou a restauracao do Desfazer (fora do intervalo da build):
  // nada voltou ao que era, entao NAO se pede reabertura.
  zerar(); dtsValor = 3;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1 && dtsValor == 1);
  (void)farolacao_audio_reabertura_consumir();
  dtsAceita = 0;
  ilhaDesfazer();
  assert(dtsValor == 1 && farolacao_audio_reabertura_consumir() == 0);
  puts("ok  CONVERTER_AUDIO na LG: liga, reabre e Desfazer devolve o ajuste original");
}

static void converter_tpk6(void) {
  FarolFatos f = fatos(FP_TIZEN_TPK, 6);
  int os;
  zerar(); ops();
  dtsValor = 0;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1);
  assert(dtsValor == 1 && ilha.desfazer);
  ilhaDesfazer();
  assert(dtsValor == 0);
  for (os = 7; os <= 9; os++) {
    f = fatos(FP_TIZEN_TPK, os);
    zerar(); dtsValor = 0;
    assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1 && dtsValor == 1);
  }

  // Estereo AAC (2) ja converte no .tpk 6+: o valor fica, so reabre, e nao
  // ha o que desfazer (antes do conserto o alvo pisava 2 -> 1 e o Desfazer
  // nao devolvia o 2 — FAROL16B).
  zerar(); dtsValor = 2;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1);
  assert(dtsValor == 2 && dtsGravacoes == 0 && ilha.desfazer == NULL && nIlha == 1);
  assert(farolacao_audio_reabertura_consumir() == 1);

  // A restauracao recusada pelo ajuste nao reabre nada.
  zerar(); dtsValor = 0;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 1 && dtsValor == 1);
  (void)farolacao_audio_reabertura_consumir();
  dtsAceita = 0;
  ilhaDesfazer();
  assert(dtsValor == 1 && farolacao_audio_reabertura_consumir() == 0);
  puts("ok  CONVERTER_AUDIO no .tpk 6+");
}

static void converter_recusa(void) {
  struct { FarolPlat p; int os; } r[] = {
    {FP_TIZEN_WGT, 6}, {FP_TIZEN_WGT, -1}, {FP_ANDROID, 14}, {FP_OUTRA, 0},
    {FP_TIZEN_TPK, 4}, {FP_TIZEN_TPK, 5}, {FP_TIZEN_TPK, -1},
  };
  size_t i;
  FarolFatos f;
  for (i = 0; i < sizeof r / sizeof *r; i++) {
    // Mesmo com conversaoPossivel forcado a 1 (fato adulterado), o executor recusa.
    f = fatos(r[i].p, r[i].os);
    zerar(); ops(); dtsValor = 0;
    assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 0);
    assert(dtsValor == 0 && dtsGravacoes == 0 && reabriu == 0 && nIlha == 0);
    assert(farolacao_audio_reabertura_consumir() == 0);
    assert(nAcao == 1 && nRes == 1 && resOk[0] == 0);
  }
  // Sem faixa DTS, sem conversao possivel, fatos nulos, ajuste que recusa gravar.
  f = fatos(FP_WEBOS, 6); f.faixaDts = 0;
  zerar(); ops();
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 0 && dtsGravacoes == 0);
  f = fatos(FP_WEBOS, 6); f.conversaoPossivel = 0;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 0 && dtsGravacoes == 0);
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, NULL) == 0);
  f = fatos(FP_WEBOS, 6);
  zerar(); dtsValor = 3; dtsAceita = 0;
  assert(farolacao_audio_executar(FAROL_A_CONVERTER_AUDIO, &f) == 0);
  assert(dtsValor == 3 && farolacao_audio_reabertura_consumir() == 0 && nIlha == 0);
  puts("ok  CONVERTER_AUDIO recusa WASM, .tpk 4.0-5.5, Android e fatos inuteis");
}

static void trocar_faixa(void) {
  FarolFatos f = fatos(FP_WEBOS, 6);
  zerar(); ops();
  faixaIdx = 1;
  assert(farolacao_audio_executar(FAROL_A_TROCAR_FAIXA, &f) == 1);
  assert(faixaTrocas == 1 && nIlha == 1 && ilha.desfazer == NULL);
  assert(nAcao == 1 && evAcao[0] == FAROL_A_TROCAR_FAIXA && resOk[0] == 1);

  // Ausencia de faixa compativel: nada troca, resultado 0.
  zerar(); faixaIdx = -1;
  assert(farolacao_audio_executar(FAROL_A_TROCAR_FAIXA, &f) == 0);
  assert(faixaTrocas == 0 && nIlha == 0 && nRes == 1 && resOk[0] == 0);

  // O fato diz que nao ha (outraFaixa 0): nem consulta.
  zerar(); faixaIdx = 1; f.outraFaixa = 0;
  assert(farolacao_audio_executar(FAROL_A_TROCAR_FAIXA, &f) == 0 && faixaTrocas == 0);
  f.outraFaixa = -1;
  assert(farolacao_audio_executar(FAROL_A_TROCAR_FAIXA, &f) == 0 && faixaTrocas == 0);

  // A faixa sumiu entre o veredito e o clique (consulta ao vivo manda).
  zerar(); f.outraFaixa = 1; faixaIdx = -1;
  assert(farolacao_audio_executar(FAROL_A_TROCAR_FAIXA, &f) == 0 && faixaTrocas == 0);
  // Trocar faixa nao reabre nada.
  assert(farolacao_audio_reabertura_consumir() == 0);
  puts("ok  TROCAR_FAIXA: com faixa compativel troca; sem ela recusa");
}

static void aliviar_reabrir(void) {
  FarolFatos f = fatos(FP_WEBOS, 6);
  // Memoria folgada: autoriza, reabre.
  zerar(); ops(); farolacao_audio_quando_reabrir(reabrir);
  memN = 1; memSeq[0] = 400;
  assert(farolacao_audio_executar(FAROL_A_ALIVIAR_REABRIR, &f) == 1);
  assert(aliviou >= 1 && reabriu == 1 && nIlha == 1);
  // Pilula no tempo passado, nao no imperativo do botao (FAROL16B).
  assert(!strcmp(ilha.frase, FT_MEMORIA_LIBERADA));
  assert(nAcao == 1 && evAcao[0] == FAROL_A_ALIVIAR_REABRIR && resOk[0] == 1);

  // Abaixo do piso, o alivio recupera: memlog_pode_reabrir autoriza.
  zerar(); farolacao_audio_quando_reabrir(reabrir);
  memN = 2; memSeq[0] = 90; memSeq[1] = 260;   // 1a leitura baixa; depois do alivio, folga
  assert(farolacao_audio_executar(FAROL_A_ALIVIAR_REABRIR, &f) == 1);
  assert(aliviou >= 1 && reabriu == 1);

  // Continua abaixo do piso: NAO autoriza, NAO reabre (reabrir aqui fecha o app).
  zerar(); farolacao_audio_quando_reabrir(reabrir);
  memN = 1; memSeq[0] = 90;
  assert(farolacao_audio_executar(FAROL_A_ALIVIAR_REABRIR, &f) == 0);
  assert(aliviou >= 1 && reabriu == 0 && nIlha == 0);
  assert(nRes == 1 && resOk[0] == 0);
  zerar();
  memN = 1; memSeq[0] = 90;
  assert(farolacao_audio_executar(FAROL_A_ALIVIAR_REABRIR, &f) == 0);
  assert(farolacao_audio_reabertura_consumir() == 0);

  // Memoria desconhecida (-1) conta como "pode".
  zerar(); memN = 1; memSeq[0] = -1;
  assert(farolacao_audio_executar(FAROL_A_ALIVIAR_REABRIR, &f) == 1);
  assert(farolacao_audio_reabertura_consumir() == 1);

  puts("ok  ALIVIAR_REABRIR: so reabre se memlog_pode_reabrir autorizar");
}

static int avisoRodou;
static void avisoCb(void) { avisoRodou++; }

static void memlog_alivio(void) {
  memlog_quando_aviso(avisoCb);
  memlog_aliviar();
  assert(avisoRodou == 1);
  memlog_quando_aviso(NULL);
  memlog_aliviar();
  assert(avisoRodou == 1);
  puts("ok  memlog_aliviar usa o callback do aviso");
}

static void outras_acoes(void) {
  FarolFatos f = fatos(FP_WEBOS, 6);
  zerar(); ops();
  assert(farolacao_audio_executar(FAROL_A_TENTAR_DE_NOVO, &f) == 0);
  assert(farolacao_audio_executar(FAROL_A_NENHUMA, &f) == 0);
  assert(farolacao_audio_executar(FAROL_A_N + 3, &f) == 0);
  assert(nAcao == 0 && nRes == 0 && nIlha == 0);
  puts("ok  acoes alheias ao modulo nao registram nada");
}

int main(void) {
  decisao_plataforma();
  decisao_faixa();
  converter_lg_com_desfazer();
  converter_tpk6();
  converter_recusa();
  trocar_faixa();
  aliviar_reabrir();
  memlog_alivio();
  outras_acoes();
  puts("farol_acao_audio: ok");
  return 0;
}
