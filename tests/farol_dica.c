// TESTE DO CARTAO DE DICA DO FAROL (P2 — lote sense, issue 48).
// Parte 1: nucleo real (faroldica.c + farolcena + farolpeca + faroltexto +
// farolrec + farolgosto + farolficha + farolinfo + farolacha + avisodisp +
// catalogo + progresso + salvos + diario + dados + js + mapa) com relogio
// injetado (3 dias simulados), todos os 6 acao.tipo, cota 1/dia/perfil,
// persistencia de dispensa (avisodisp) e concorrencia sob TSan.
#define _GNU_SOURCE
#include "faroldica.h"
#include "farol.h"
#include "farolsinal.h"
#include "avisodisp.h"
#include "catalogo.h"
#include "dados.h"
#include "diario.h"
#include "progresso.h"
#include "salvos.h"
#include "perfis.h"
#include "seguro.h"
#include "idioma.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ------------------------------------------------ dubles minimos do ambiente
static int stubPerfil = 1;
static int stubRecomendar = 1;
static int stubSeguro = 0;
static FarolGosto stubModelo;
static int stubTemModelo = 1;
static long long stubAgoraS = 1767225600LL + 100LL * 86400LL + 36000LL; // dia 100, 10h

static long long relogioFake(void) {
  return __atomic_load_n(&stubAgoraS, __ATOMIC_ACQUIRE);
}
static void definirAgora(long long s) {
  __atomic_store_n(&stubAgoraS, s, __ATOMIC_RELEASE);
}

int perfis_ativo(void) { return __atomic_load_n(&stubPerfil, __ATOMIC_ACQUIRE); }
static void definirPerfil(int p) {
  __atomic_store_n(&stubPerfil, p, __ATOMIC_RELEASE);
  salvos_perfil(p);
  salvos_iniciar();
}
int seguro_perfil_ativo(void) { return __atomic_load_n(&stubSeguro, __ATOMIC_ACQUIRE); }
int farolsinal_recomendar(void) { return __atomic_load_n(&stubRecomendar, __ATOMIC_ACQUIRE); }
int farolsinal_modelo(FarolGosto *out) {
  if (!stubTemModelo) return 0;
  if (out) *out = stubModelo;
  return 1;
}
const char *i18n(const char *s) { return s ? s : ""; }
int cacavisto_filme(const CatItem *ci) { (void)ci; return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_fonte_biblioteca(void) { return 0; }
const char *desc_genero_pt(const char *g) { return g ? g : ""; }
int ajustes_cw_concluido(void) { return 90; }
int ajustes_cw_minimo(void) { return 5; }
int ajustes_ocultar_nao_lancados(void) { return 0; }
const char *sessao_usuario(void) { return "teste"; }
int conta_ativa(void) { return 0; }
int trakt_ativo(void) { return 0; }
int simkl_ativo(void) { return 0; }
void cwo_marcar_futuro(const char *imdb, int futuro) { (void)imdb; (void)futuro; }
int cwo_e_futuro(const char *imdb) { (void)imdb; return 0; }
void cw_retido_definir(const char *imdb) { (void)imdb; }
int cw_retido_exclui(const char *imdb) { (void)imdb; return 0; }
void biblioteca_alternar_lista(int i) { (void)i; }

static int cbCmpChamou;
static char cbCmpImdb[FAROL_REF_TAM];
static int cbEscChamou;
static void onComparar(const char *imdb) {
  cbCmpChamou++;
  snprintf(cbCmpImdb, sizeof cbCmpImdb, "%s", imdb ? imdb : "");
}
static void onEscolha(void) { cbEscChamou++; }

static void montarCatalogoBase(void) {
  CatItem itens[4];
  memset(itens, 0, sizeof itens);
  strcpy(itens[0].imdb, "tt0903747"); strcpy(itens[0].tipo, "series");
  strcpy(itens[0].titulo, "Breaking Bad"); strcpy(itens[0].meta, "2008 · 47 min");
  strcpy(itens[1].imdb, "tt0133093"); strcpy(itens[1].tipo, "movie");
  strcpy(itens[1].titulo, "Matrix"); strcpy(itens[1].meta, "1999 · 136 min");
  strcpy(itens[2].imdb, "tt0468569"); strcpy(itens[2].tipo, "movie");
  strcpy(itens[2].titulo, "Cavaleiro das Trevas"); strcpy(itens[2].meta, "2008 · 95 min");
  strcpy(itens[3].imdb, "tt0111161"); strcpy(itens[3].tipo, "movie");
  strcpy(itens[3].titulo, "Um Sonho de Liberdade"); strcpy(itens[3].meta, "1994 · 90 min");
  cat_definir(itens, 4);
}

static void limparDir(const char *dir) {
  char cmd[512];
  snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system(cmd) != 0) {}
}

// Teste 1: 3 dias simulados com relogio injetado, cota 1/dia/perfil e avisodisp.
static void testeTresDiasECota(const char *dir) {
  const long long dia1 = 1767225600LL + 100LL * 86400LL + 36000LL;
  const long long dia2 = dia1 + 86400LL;
  const long long dia3 = dia1 + 2LL * 86400LL;
  FarolDicaFato fatos[3];
  FarolDica d;

  memset(fatos, 0, sizeof fatos);
  // Prioridade: PAROU (d1) > SALVO (d2) > CURTO (d3)
  fatos[0].tipo = FC_DICA_PAROU;
  fatos[0].acao = FP_ACAO_ABRIR;
  fatos[0].id = "d_dia1_parou";
  fatos[0].alvo = "tt0903747";
  fatos[0].l1a[0] = "tt0903747"; fatos[0].l1a[1] = "2"; fatos[0].l1a[2] = "4";
  fatos[0].l1na = 3;
  fatos[0].l2a[0] = "5"; fatos[0].l2na = 1;

  fatos[1].tipo = FC_DICA_SALVO;
  fatos[1].acao = FP_ACAO_ABRIR;
  fatos[1].id = "d_dia2_salvo";
  fatos[1].alvo = "tt0133093";
  fatos[1].l1a[0] = "5M"; fatos[1].l1na = 1;
  fatos[1].l2a[0] = "2h16"; fatos[1].l2na = 1;

  fatos[2].tipo = FC_DICA_CURTO;
  fatos[2].acao = FP_ACAO_TOCAR;
  fatos[2].id = "d_dia3_curto";
  fatos[2].alvo = "tt0468569";
  fatos[2].l1a[0] = "tt0468569"; fatos[2].l1na = 1;
  fatos[2].l2na = 0;

  // --- DIA 1, PERFIL 1 ---
  definirAgora(dia1);
  definirPerfil(1);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 1);
  assert(faroldica_atual(&d) == 1);
  assert(!strcmp(d.id, "d_dia1_parou"));
  assert(d.acao == FP_ACAO_ABRIR);
  assert(d.focavel == 1);
  assert(strstr(d.l1, "Breaking Bad") != NULL);
  assert(strstr(d.l1, "tt0903747") == NULL);   // nunca id cru na tela
  assert(strstr(d.l2, "5") != NULL);

  // Tentar adotar uma 2a dica no MESMO dia 1 -> bloqueada (max 1/dia/perfil)
  assert(faroldica_adotar_fatos(&fatos[1], 2, 2) == 0);
  assert(faroldica_atual(&d) == 1);
  assert(!strcmp(d.id, "d_dia1_parou"));

  // Dispensar a dica do dia 1 -> some e grava em avisodisp; 2a dica NAO entra hoje
  assert(faroldica_dispensar() == 1);
  assert(faroldica_dispensada("d_dia1_parou") == 1);
  assert(avisodisp_tem("faroldica:d_dia1_parou") == 1);
  assert(faroldica_atual(&d) == 0);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 0);
  assert(faroldica_atual(&d) == 0);

  // Simular reinicio do app NO MESMO DIA 1: disco mantem cota e dispensa!
  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  avisodisp_esquecer();
  assert(faroldica_dispensada("d_dia1_parou") == 1);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 0);
  assert(faroldica_atual(&d) == 0);

  // --- DIA 1, PERFIL 2 (cota e dispensa sao POR PERFIL) ---
  definirPerfil(2);
  assert(faroldica_dispensada("d_dia1_parou") == 0);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 1);
  assert(faroldica_atual(&d) == 1);
  assert(!strcmp(d.id, "d_dia1_parou"));
  assert(d.perfil == 2);
  // Perfil 2 tambem tem teto de 1 por dia
  assert(faroldica_adotar_fatos(&fatos[1], 2, 2) == 0);
  // Voltar ao Perfil 1 no mesmo Dia 1: nao vaza dica do Perfil 2!
  definirPerfil(1);
  assert(faroldica_atual(&d) == 0);

  // --- DIA 2, PERFIL 1 (relogio +24h) ---
  definirAgora(dia2);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 1);
  assert(faroldica_atual(&d) == 1);
  // d_dia1_parou foi dispensada no dia 1 -> NAO VOLTA; sobe d_dia2_salvo!
  assert(!strcmp(d.id, "d_dia2_salvo"));
  assert(strstr(d.l1, "5M") != NULL);
  assert(strstr(d.l2, "2h16") != NULL);
  // Dispensa no dia 2 -> nao entra a 3a no dia 2
  assert(faroldica_dispensar() == 1);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 0);
  assert(faroldica_atual(&d) == 0);

  // --- DIA 3, PERFIL 1 (relogio +48h) ---
  definirAgora(dia3);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 1);
  assert(faroldica_atual(&d) == 1);
  // Nem dia 1 nem dia 2 voltam; sobe d_dia3_curto!
  assert(!strcmp(d.id, "d_dia3_curto"));
  assert(d.acao == FP_ACAO_TOCAR);
  assert(strstr(d.l1, "Cavaleiro das Trevas") != NULL);
  // Dispensa no dia 3 -> todas dispensadas, nenhuma sobe mais
  assert(faroldica_dispensar() == 1);
  definirAgora(dia3 + 86400LL);
  assert(faroldica_adotar_fatos(fatos, 3, 2) == 0);
  assert(faroldica_atual(&d) == 0);
  (void)dir;
}

// Teste 2: cada um dos 6 acao.tipo faz exatamente o que diz.
static void testeTodasAcoes(void) {
  long long dia = 1767225600LL + 120LL * 86400LL + 36000LL;
  FarolDica d;
  FarolDicaExec ex;
  char cmpImdb[FAROL_REF_TAM];

  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  faroldica_esquecer_disco();
  definirPerfil(3);

  // 0) Sem rota registrada de #49/#50: comparar e escolha ficam indisponiveis,
  //    nao gastam cota, nao prometem acao e faroldica_executar devolve ok=0,
  //    enquanto abrir/tocar/salvar/nada seguem ativos.
  definirAgora(dia);
  assert(faroldica_acao_disponivel(FP_ACAO_ABRIR) == 1);
  assert(faroldica_acao_disponivel(FP_ACAO_TOCAR) == 1);
  assert(faroldica_acao_disponivel(FP_ACAO_SALVAR) == 1);
  assert(faroldica_acao_disponivel(FP_ACAO_NADA) == 1);
  assert(faroldica_acao_disponivel(FP_ACAO_COMPARAR) == 0);
  assert(faroldica_acao_disponivel(FP_ACAO_ESCOLHA) == 0);
  assert(faroldica_rotulo_acao(FP_ACAO_COMPARAR)[0] == 0);
  assert(faroldica_rotulo_acao(FP_ACAO_ESCOLHA)[0] == 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_esc_sem_rota\","
    "\"l1\":{\"m\":\"FT_DICA_PERGUNTAS\"},"
    "\"acao\":{\"tipo\":\"escolha\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_cmp_sem_rota\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt0133093\"]},"
    "\"acao\":{\"tipo\":\"comparar\",\"alvo\":\"tt0133093\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 0);
  assert(faroldica_atual(&d) == 0);
  {
    FarolDica fake;
    memset(&fake, 0, sizeof fake);
    fake.ativa = 1; fake.focavel = 1; fake.perfil = 3;
    fake.dia = faroldica_dia_atual();
    fake.acao = FP_ACAO_ESCOLHA;
    strcpy(fake.id, "a_fake_esc");
    assert(faroldica_focavel(&fake) == 0);
    ex = faroldica_executar(&fake);
    assert(ex.ok == 0 && faroldica_pediu_escolha() == 0);
    fake.acao = FP_ACAO_COMPARAR;
    strcpy(fake.id, "a_fake_cmp");
    strcpy(fake.alvo, "tt0133093");
    assert(faroldica_focavel(&fake) == 0);
    ex = faroldica_executar(&fake);
    assert(ex.ok == 0 && faroldica_pediu_comparar(cmpImdb, sizeof cmpImdb) == 0);
  }
  // Cota NAO foi gasta pelas tentativas sem rota: abrir no mesmo dia sobe!
  faroldica_ao_comparar(onComparar);
  faroldica_ao_escolha(onEscolha);
  assert(faroldica_acao_disponivel(FP_ACAO_COMPARAR) == 1);
  assert(faroldica_acao_disponivel(FP_ACAO_ESCOLHA) == 1);
  assert(faroldica_rotulo_acao(FP_ACAO_COMPARAR)[0] != 0);
  assert(faroldica_rotulo_acao(FP_ACAO_ESCOLHA)[0] != 0);

  // 1) abrir (item no catalogo)
  definirAgora(dia);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_abrir\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt0133093\"]},"
    "\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt0133093\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1 && faroldica_focavel(&d) == 1);
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_ABRIR && ex.ok == 1 && ex.catIdx >= 0);
  assert(!strcmp(ex.imdb, "tt0133093"));
  assert(!strcmp(cat_item(ex.catIdx)->imdb, "tt0133093"));

  // 1b) abrir (item FORA do catalogo -> acrescentado via cat_acrescentar)
  dia += 86400LL; definirAgora(dia);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_abrir_fora\","
    "\"l1\":{\"m\":\"FT_DICA_SALVO_L1\",\"a\":[\"3M\"]},"
    "\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt9990001\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1);
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_ABRIR && ex.ok == 1 && ex.catIdx >= 0);
  assert(!strcmp(cat_item(ex.catIdx)->imdb, "tt9990001"));

  // 1c) abrir/tocar serie FORA do catalogo com tipo/T/E vindos do Diario ->
  //     preserva tipo="series", temporada e episodio no CatItem acrescentado.
  dia += 86400LL; definirAgora(dia);
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7770001");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 2;
    de.episodio = 4;
    de.quando = dia - 35LL * 86400LL;
    assert(diario_adicionar(3, &de) >= 0);
    diario_nome_definir("tt7770001", "Ruptura", 2022, "");
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_serie_fora\","
    "\"l1\":{\"m\":\"FT_DICA_REVER\",\"a\":[\"tt7770001\"]},"
    "\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt7770001\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1);
  assert(!strcmp(d.alvoTipo, "series"));
  assert(d.alvoTemporada == 2 && d.alvoEpisodio == 4);
  assert(!strcmp(d.alvoTitulo, "Ruptura"));
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_ABRIR && ex.ok == 1 && ex.catIdx >= 0);
  {
    const CatItem *ci = cat_item(ex.catIdx);
    assert(ci && !strcmp(ci->imdb, "tt7770001"));
    assert(!strcmp(ci->tipo, "series"));
    assert(ci->temporada == 2 && ci->episodio == 4);
  }
  // Virada de dia invalida execucao da copia vencida da dica!
  {
    FarolDica vencida = d;
    definirAgora(dia + 86400LL);
    assert(faroldica_atual(&d) == 0);
    ex = faroldica_executar(&vencida);
    assert(ex.tipo == FD_EXEC_NADA && ex.ok == 0);
    dia += 86400LL;
  }

  // 2) tocar
  dia += 86400LL; definirAgora(dia);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_tocar\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt0468569\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt0468569\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1 && faroldica_focavel(&d) == 1);
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_TOCAR && ex.ok == 1 && ex.catIdx >= 0);
  assert(!strcmp(ex.imdb, "tt0468569"));

  // 3) salvar -> grava em salvos e marca naLista no catalogo
  dia += 86400LL; definirAgora(dia);
  assert(salvos_tem("tt0111161") == 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_salvar\","
    "\"l1\":{\"m\":\"FT_DICA_REVER\",\"a\":[\"tt0111161\"]},"
    "\"acao\":{\"tipo\":\"salvar\",\"alvo\":\"tt0111161\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1 && faroldica_focavel(&d) == 1);
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_SALVAR && ex.ok == 1);
  assert(salvos_tem("tt0111161") == 1);
  assert(cat_item(cat_indice_por_imdb("tt0111161"))->naLista == 1);

  // 4) comparar -> dispara callback e pedido pendente consumido uma vez
  dia += 86400LL; definirAgora(dia);
  cbCmpChamou = 0; cbCmpImdb[0] = 0;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_comp\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt0133093\"]},"
    "\"acao\":{\"tipo\":\"comparar\",\"alvo\":\"tt0133093\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1 && faroldica_focavel(&d) == 1);
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_COMPARAR && ex.ok == 1);
  assert(cbCmpChamou == 1 && !strcmp(cbCmpImdb, "tt0133093"));
  assert(faroldica_pediu_comparar(cmpImdb, sizeof cmpImdb) == 1);
  assert(!strcmp(cmpImdb, "tt0133093"));
  assert(faroldica_pediu_comparar(cmpImdb, sizeof cmpImdb) == 0);

  // 5) escolha -> dispara callback e pedido pendente consumido uma vez
  dia += 86400LL; definirAgora(dia);
  cbEscChamou = 0;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_esc\","
    "\"l1\":{\"m\":\"FT_DICA_PERGUNTAS\"},"
    "\"acao\":{\"tipo\":\"escolha\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1 && faroldica_focavel(&d) == 1);
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_ESCOLHA && ex.ok == 1);
  assert(cbEscChamou == 1);
  assert(faroldica_pediu_escolha() == 1);
  assert(faroldica_pediu_escolha() == 0);

  // 6) nada -> NAO entra na ordem de foco (focavel == 0) e executar == NADA
  dia += 86400LL; definirAgora(dia);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_nada\","
    "\"l1\":{\"m\":\"FT_DICA_TRAKT\"},"
    "\"acao\":{\"tipo\":\"nada\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  assert(faroldica_atual(&d) == 1);
  assert(d.acao == FP_ACAO_NADA);
  assert(d.focavel == 0);
  assert(faroldica_focavel(&d) == 0);
  assert(faroldica_rotulo_acao(FP_ACAO_NADA)[0] == 0);
  ex = faroldica_executar(&d);
  assert(ex.tipo == FD_EXEC_NADA && ex.ok == 0);

  // Validacao dura: JSON com acao fora do enum ou nada+alvo e rejeitado!
  dia += 86400LL; definirAgora(dia);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_url\","
    "\"l1\":{\"m\":\"FT_DICA_TRAKT\"},"
    "\"acao\":{\"tipo\":\"url\",\"alvo\":\"https://evil.invalid\"}}]}",
    0, NULL, NULL, NULL) == 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"a_nada_alvo\","
    "\"l1\":{\"m\":\"FT_DICA_TRAKT\"},"
    "\"acao\":{\"tipo\":\"nada\",\"alvo\":\"tt0133093\"}}]}",
    0, NULL, NULL, NULL) == 0);
}

// Teste 3: portoes ("Recomendar nesta TV" desligado, modo seguro) e coleta
// deterministica real via faroldica_preparar() (serie parada -> salvo antigo).
static void testePrepararEPortoes(void) {
  long long dia = 1767225600LL + 150LL * 86400LL + 36000LL;
  FarolDica d;

  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  faroldica_esquecer_disco();
  definirPerfil(4);
  definirAgora(dia);

  // Sem rota de #50 e sem fatos elegiveis, preparar() NAO emite d_perguntas
  // nem gasta a cota: um catalogo curto posterior no mesmo dia ainda sobe.
  {
    CatItem longos[1];
    memset(longos, 0, sizeof longos);
    strcpy(longos[0].imdb, "tt9910001"); strcpy(longos[0].tipo, "movie");
    strcpy(longos[0].titulo, "Longo"); strcpy(longos[0].meta, "2024 · 160 min");
    faroldica_teste_reiniciar();
    faroldica_teste_relogio(relogioFake);
    faroldica_esquecer_disco();
    definirPerfil(6);
    definirAgora(dia);
    cat_definir(longos, 1);
    assert(faroldica_preparar() == 0);
    assert(faroldica_atual(&d) == 0);
    // Agora chega filme curto (90 min) no mesmo dia -> cota estava livre!
    montarCatalogoBase();
    assert(faroldica_preparar() == 1);
    assert(faroldica_atual(&d) == 1);
    assert(d.acao == FP_ACAO_TOCAR);
  }
  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  faroldica_esquecer_disco();
  definirPerfil(4);
  definirAgora(dia);

  // Grava progresso de serie parada ha 35 dias (5 semanas)
  {
    ProgRegistro pr;
    memset(&pr, 0, sizeof pr);
    pr.perfil = 4;
    strcpy(pr.chave, "tt0903747_s2e4");
    strcpy(pr.contentId, "tt0903747");
    strcpy(pr.tipo, "series");
    pr.temporada = 2; pr.episodio = 4;
    pr.posSeg = 1200; pr.durSeg = 2800;
    pr.lastWatchedMs = (dia - 35LL * 86400LL) * 1000LL;
    pr.tocou = 1;
    assert(prog_aplicar_remoto(&pr) == 1);
  }

  // Portao: recomendar desligado -> nada
  __atomic_store_n(&stubRecomendar, 0, __ATOMIC_RELEASE);
  assert(faroldica_preparar() == 0);
  assert(faroldica_atual(&d) == 0);
  __atomic_store_n(&stubRecomendar, 1, __ATOMIC_RELEASE);

  // Portao: modo seguro -> nada
  __atomic_store_n(&stubSeguro, 1, __ATOMIC_RELEASE);
  assert(faroldica_preparar() == 0);
  assert(faroldica_atual(&d) == 0);
  __atomic_store_n(&stubSeguro, 0, __ATOMIC_RELEASE);

  // Preparar no dia 150 -> sobe a serie parada (Breaking Bad, T2E4, 5 semanas)
  assert(faroldica_preparar() == 1);
  assert(faroldica_atual(&d) == 1);
  assert(!strncmp(d.id, "d_parou_tt0903747", 17));
  assert(strstr(d.l1, "Breaking Bad") != NULL);
  assert(strstr(d.l2, "5") != NULL);

  // Repetir faroldica_preparar no mesmo dia nao muda a geracao (sem custo)
  {
    unsigned gAntes = faroldica_geracao();
    assert(faroldica_preparar() == 1);
    assert(faroldica_geracao() == gAntes);
  }
}

// Teste 4: concorrencia (TSan) entre leitura de geracao/atual e adocao/dispensa.
static void *fioLeitor(void *u) {
  int i;
  (void)u;
  for (i = 0; i < 400; i++) {
    FarolDica d;
    (void)faroldica_geracao();
    (void)faroldica_dia_atual();
    if (faroldica_atual(&d)) {
      (void)faroldica_focavel(&d);
    }
  }
  return NULL;
}

static void *fioEscritor(void *u) {
  int i;
  long long base = 1767225600LL + 200LL * 86400LL;
  (void)u;
  for (i = 0; i < 40; i++) {
    FarolDicaFato f;
    memset(&f, 0, sizeof f);
    f.tipo = FC_DICA_CURTO;
    f.acao = FP_ACAO_TOCAR;
    f.id = "d_tsan_c";
    f.alvo = "tt0468569";
    f.l1a[0] = "tt0468569";
    f.l1na = 1;
    definirAgora(base + (long long)i * 86400LL);
    (void)faroldica_adotar_fatos(&f, 1, 2);
    faroldica_esquecer();
  }
  return NULL;
}

static void testeConcorrencia(void) {
  pthread_t t1, t2;
  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  definirPerfil(5);
  assert(pthread_create(&t1, NULL, fioLeitor, NULL) == 0);
  assert(pthread_create(&t2, NULL, fioEscritor, NULL) == 0);
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
}

int main(void) {
  const char *tmp = getenv("TMPDIR");
  char dir[256];
  if (!tmp || !tmp[0]) {
    fprintf(stderr, "farol_dica: TMPDIR obrigatorio\n");
    return 1;
  }
  snprintf(dir, sizeof dir, "%s/nuvio-farol-dica-%ld", tmp, (long)getpid());
  limparDir(dir);
  assert(mkdir(dir, 0700) == 0);
  setenv("HOME", dir, 1);
  setenv("NUVIO_DADOS", dir, 1);

  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  avisodisp_esquecer();
  memset(&stubModelo, 0, sizeof stubModelo);
  stubModelo.titulos = 12;
  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  montarCatalogoBase();

  testeTresDiasECota(dir);
  testeTodasAcoes();
  testePrepararEPortoes();
  testeConcorrencia();

  limparDir(dir);
  puts("farol_dica (nucleo): ok");
  return 0;
}
