// Acoes de fonte do Farol (issue #15). Producao: farolacao_fonte.c, a politica
// real de fontefalha.c (sessao + filtro) e o filtro real. Fronteiras
// substituidas: a lista de streams (sintetica), o anel do Farol, a ilha e i18n.
#define _POSIX_C_SOURCE 200809L
#include "farolacao_fonte.h"
#include "farol.h"
#include "faroltexto.h"
#include "fontefalha.h"
#include "ilhaacao.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ---- fronteiras ------------------------------------------------------------
static FarolFonteLinha lista[16];
static int nLista;
static unsigned geracaoLista = 7;
static int recusaTroca;        // 1 = stream_farol_trocar recusa (lista mudou)
static int trocas, ultimaTroca = -1;
int stream_farol_fontes(FarolFonteLinha *dst, int max, unsigned *geracao) {
  assert(max >= nLista);
  memcpy(dst, lista, sizeof *dst * (size_t)nLista);
  if (geracao) *geracao = geracaoLista;
  return nLista;
}
int stream_farol_trocar(int indice, unsigned geracao) {
  if (recusaTroca || geracao != geracaoLista) return 0;
  trocas++; ultimaTroca = indice; return 1;
}
static int acoes[32], nAcoes, resultados[32], resultadoOk[32], nResultados;
void farol_registrar_acao(int acao) { assert(nAcoes < 32); acoes[nAcoes++] = acao; }
void farol_registrar_resultado(int acao, int ok) {
  assert(nResultados < 32); resultados[nResultados] = acao; resultadoOk[nResultados++] = ok;
}
static int ilhas; static char ilhaFrase[160]; static IlhaDesfazer ilhaFn;
void ilhaacao_feita(const IlhaAcao *a) {
  ilhas++; snprintf(ilhaFrase, sizeof ilhaFrase, "%s", a->frase); ilhaFn = a->desfazer;
}
const char *i18n(const char *s) { return s; }

static void zerarRegistros(void) {
  nAcoes = nResultados = ilhas = trocas = 0; ultimaTroca = -1; ilhaFn = NULL; recusaTroca = 0;
}

// ---- helpers -----------------------------------------------------------------
static char arquivoFalha[512];
static void ler(char *dst, size_t n) {
  FILE *f = fopen(arquivoFalha, "rb"); assert(f);
  size_t k = fread(dst, 1, n - 1, f); dst[k] = 0; fclose(f);
}
static void linha(int i, int idx, int atual, int ok, int dv, int dts, unsigned long long bytes, int kbps, int altura) {
  lista[i] = (FarolFonteLinha){ .indice = idx, .atual = (unsigned char)atual, .ok = (unsigned char)ok,
                                .dv = (unsigned char)dv, .dts = (unsigned char)dts,
                                .bytes = bytes, .kbps = kbps, .altura = altura };
  if (i + 1 > nLista) nLista = i + 1;
}
// 0 atual DV+DTS 4GB; 1 DV+DTS 3GB; 2 sem DV, DTS 5GB; 3 limpa 2GB;
// 4 sem DV/DTS mas fora do automatico.
static void listaPadrao(void) {
  nLista = 0;
  linha(0, 0, 1, 0, 1, 1, 4000000000ULL, 0, 2160);
  linha(1, 1, 0, 1, 1, 1, 3000000000ULL, 0, 2160);
  linha(2, 2, 0, 1, 0, 1, 5000000000ULL, 0, 2160);
  linha(3, 3, 0, 1, 0, 0, 2000000000ULL, 0, 1080);
  linha(4, 4, 0, 0, 0, 0, 1000000000ULL, 0, 720);
  geracaoLista = 7;
}
static uint64_t bitsTemp(int cat) {
  FonteFiltroConfig t; return farolacao_fonte_filtro_temporario(&t) ? t.excluir[cat] : 0;
}
#define BITS_DV  ((UINT64_C(1) << 0) | (UINT64_C(1) << 1) | (UINT64_C(1) << 5))
#define BITS_DTS ((UINT64_C(1) << 3) | (UINT64_C(1) << 4) | (UINT64_C(1) << 5) | (UINT64_C(1) << 6) | (UINT64_C(1) << 7))

static void elegibilidade_e_escolha(void) {
  FarolFonteElegib e;
  farolacao_fonte_encerrar(); listaPadrao();
  farolacao_fonte_elegibilidade(lista, nLista, geracaoLista, &e);
  assert(e.outra == 1 && e.menor == 1 && e.semDv == 1 && e.semDts == 1);
  assert(farolacao_fonte_escolher(FAROL_A_PROXIMA_FONTE, lista, nLista, geracaoLista) == 1);
  assert(farolacao_fonte_escolher(FAROL_A_FONTE_SEM_DV, lista, nLista, geracaoLista) == 2);
  assert(farolacao_fonte_escolher(FAROL_A_FONTE_SEM_DTS, lista, nLista, geracaoLista) == 3);
  assert(farolacao_fonte_escolher(FAROL_A_FONTE_MENOR, lista, nLista, geracaoLista) == 1);
  // A fora do automatico (indice 4) nunca e candidata, mesmo sendo a menor.
  assert(farolacao_fonte_escolher(FAROL_A_FONTE_MENOR, lista + 4, 1, geracaoLista) == -1);
  // Acao de outra familia ou invalida nao escolhe nada.
  assert(farolacao_fonte_escolher(FAROL_A_LIMPAR_CACHE, lista, nLista, geracaoLista) == -1);
  assert(farolacao_fonte_escolher(-3, lista, nLista, geracaoLista) == -1);
  assert(farolacao_fonte_escolher(FAROL_A_PROXIMA_FONTE, NULL, 0, geracaoLista) == -1);

  // MENOR: tamanho decide quando os dois sao conhecidos; maior em bytes mas de
  // resolucao menor NAO conta.
  farolacao_fonte_encerrar(); listaPadrao();
  linha(0, 0, 1, 0, 1, 1, 4000000000ULL, 6000, 2160);
  linha(1, 1, 0, 1, 1, 1, 9000000000ULL, 0, 720);
  linha(2, 2, 0, 1, 0, 1, 0, 8000, 2160);   // sem bytes: bitrate maior, nao conta
  linha(3, 3, 0, 1, 0, 0, 0, 3000, 2160);   // bitrate menor: conta
  assert(farolacao_fonte_escolher(FAROL_A_FONTE_MENOR, lista, nLista, geracaoLista) == 3);
  // Sem bytes nem bitrate na atual, a altura decide.
  farolacao_fonte_encerrar(); listaPadrao();
  linha(0, 0, 1, 0, 1, 1, 0, 0, 1080);
  linha(1, 1, 0, 1, 1, 1, 0, 0, 1080);      // mesma altura: nao conta
  linha(2, 2, 0, 1, 0, 1, 0, 0, 720);       // menor
  assert(farolacao_fonte_escolher(FAROL_A_FONTE_MENOR, lista, nLista, geracaoLista) == 2);
  puts("elegibilidade e escolha PASS");
}

static void sem_candidata(void) {
  FarolFonteElegib e;
  farolacao_fonte_encerrar(); zerarRegistros();
  // So a atual: nenhuma acao e oferecida.
  nLista = 0; linha(0, 0, 1, 0, 1, 1, 4000000000ULL, 0, 2160);
  farolacao_fonte_elegibilidade(lista, nLista, geracaoLista, &e);
  assert(!e.outra && !e.menor && !e.semDv && !e.semDts);
  // Lista vazia ou nula.
  farolacao_fonte_elegibilidade(lista, 0, geracaoLista, &e);
  assert(!e.outra && !e.menor && !e.semDv && !e.semDts);
  farolacao_fonte_elegibilidade(NULL, 3, geracaoLista, &e);
  assert(!e.outra && !e.menor && !e.semDv && !e.semDts);
  int acoesLista[] = { FAROL_A_PROXIMA_FONTE, FAROL_A_FONTE_SEM_DV, FAROL_A_FONTE_SEM_DTS, FAROL_A_FONTE_MENOR };
  for (unsigned i = 0; i < 4; i++) {
    zerarRegistros();
    assert(farolacao_fonte_executar(acoesLista[i], 1) == 0);
    assert(trocas == 0 && ilhas == 0);
    assert(nAcoes == 1 && acoes[0] == acoesLista[i]);
    assert(nResultados == 1 && resultados[0] == acoesLista[i] && resultadoOk[0] == 0);
    FonteFiltroConfig t; assert(!farolacao_fonte_filtro_temporario(&t));
  }
  // Candidatas existem, mas todas com DV: SEM_DV nao e oferecida; PROXIMA sim.
  farolacao_fonte_encerrar(); zerarRegistros();
  nLista = 0;
  linha(0, 0, 1, 0, 1, 1, 4000000000ULL, 0, 2160);
  linha(1, 1, 0, 1, 1, 0, 5000000000ULL, 0, 2160);
  linha(2, 2, 0, 1, 1, 0, 6000000000ULL, 0, 2160);
  farolacao_fonte_elegibilidade(lista, nLista, geracaoLista, &e);
  assert(e.outra && !e.semDv && e.semDts && !e.menor);
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 0) == 0);
  assert(farolacao_fonte_executar(FAROL_A_FONTE_MENOR, 0) == 0);
  assert(trocas == 0 && bitsTemp(FONTEFILTRO_VISUAL) == 0);
  puts("sem candidata: tem*==0, executor devolve 0 sem efeito PASS");
}

static void executor_e_tentadas(void) {
  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros();
  assert(!farolacao_fonte_executar(-1, 0) && !farolacao_fonte_executar(FAROL_A_LIMPAR_CACHE, 0));
  assert(nAcoes == 0 && nResultados == 0);  // acao alheia: nem registro

  assert(farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 0) == 1);
  assert(trocas == 1 && ultimaTroca == 1);
  assert(nAcoes == 1 && acoes[0] == FAROL_A_PROXIMA_FONTE);
  assert(nResultados == 1 && resultadoOk[0] == 1);
  assert(bitsTemp(FONTEFILTRO_VISUAL) == 0 && bitsTemp(FONTEFILTRO_AUDIO) == 0);  // PROXIMA nao filtra
  assert(farolacao_fonte_tentada(geracaoLista, 0) && farolacao_fonte_tentada(geracaoLista, 1));

  // A troca aconteceu: a fonte 1 e a atual agora. A antiga (0) volta a ser
  // "ok" no snapshot, mas ja foi tentada e NAO pode voltar.
  linha(0, 0, 0, 1, 1, 1, 4000000000ULL, 0, 2160);
  linha(1, 1, 1, 0, 1, 1, 3000000000ULL, 0, 2160);
  assert(farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 0) == 1 && ultimaTroca == 2);
  linha(2, 2, 1, 0, 0, 1, 5000000000ULL, 0, 2160);
  linha(1, 1, 0, 1, 1, 1, 3000000000ULL, 0, 2160);
  assert(farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 0) == 1 && ultimaTroca == 3);
  linha(3, 3, 1, 0, 0, 0, 2000000000ULL, 0, 1080);
  linha(2, 2, 0, 1, 0, 1, 5000000000ULL, 0, 2160);
  zerarRegistros();
  // 0,1,2 tentadas; 3 atual; 4 fora do auto -> acabou.
  assert(farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 0) == 0 && trocas == 0);
  assert(nResultados == 1 && resultadoOk[0] == 0);
  FarolFonteElegib e;
  farolacao_fonte_elegibilidade(lista, nLista, geracaoLista, &e);
  assert(!e.outra && !e.menor && !e.semDv && !e.semDts);

  // Tentada de outra lista nao conta.
  assert(farolacao_fonte_tentada(geracaoLista, 0) && !farolacao_fonte_tentada(geracaoLista + 1, 0));
  farolacao_fonte_elegibilidade(lista, nLista, geracaoLista + 1, &e);
  assert(e.outra);

  // A troca falhou (lista mudou): sem efeito, o filtro volta ao que era.
  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros();
  FonteFiltroConfig previo; memset(&previo, 0, sizeof previo);
  previo.excluir[FONTEFILTRO_AUDIO] = UINT64_C(1) << 8;   // TrueHD, de uma acao anterior
  assert(farolacao_fonte_filtro_temporario_definir(&previo));
  recusaTroca = 1;
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 0) == 0);
  assert(bitsTemp(FONTEFILTRO_VISUAL) == 0 && bitsTemp(FONTEFILTRO_AUDIO) == (UINT64_C(1) << 8));
  assert(nResultados == 1 && resultadoOk[0] == 0);
  assert(!farolacao_fonte_tentada(geracaoLista, 2));
  puts("executor, tentadas que nao voltam e troca recusada PASS");
}

static void filtro_temporario(void) {
  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros();
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 0) == 1 && ultimaTroca == 2);
  assert(bitsTemp(FONTEFILTRO_VISUAL) == BITS_DV && bitsTemp(FONTEFILTRO_AUDIO) == 0);
  // DTS soma, nao substitui.
  linha(2, 2, 1, 0, 0, 1, 5000000000ULL, 0, 2160);
  linha(0, 0, 0, 1, 1, 1, 4000000000ULL, 0, 2160);
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DTS, 0) == 1 && ultimaTroca == 3);
  assert(bitsTemp(FONTEFILTRO_VISUAL) == BITS_DV && bitsTemp(FONTEFILTRO_AUDIO) == BITS_DTS);
  assert(ilhas == 0);   // temporario: nada a avisar
  assert(farolacao_fonte_filtro_temporario_definir(NULL));
  assert(bitsTemp(FONTEFILTRO_VISUAL) == 0);
  FonteFiltroConfig ruim; memset(&ruim, 0, sizeof ruim);
  ruim.excluir[FONTEFILTRO_VISUAL] = UINT64_C(1) << 40;   // fora da taxonomia
  assert(!farolacao_fonte_filtro_temporario_definir(&ruim) && bitsTemp(FONTEFILTRO_VISUAL) == 0);

  // Limpeza ao encerrar/trocar a reproducao: filtro e tentadas.
  farolacao_fonte_encerrar(); listaPadrao();
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 0) == 1);
  assert(bitsTemp(FONTEFILTRO_VISUAL) == BITS_DV && farolacao_fonte_tentada(geracaoLista, 2));
  farolacao_fonte_encerrar();
  assert(bitsTemp(FONTEFILTRO_VISUAL) == 0 && !farolacao_fonte_tentada(geracaoLista, 2));
  FonteFiltroConfig t; assert(!farolacao_fonte_filtro_temporario(&t));
  puts("filtro temporario soma e limpa com a reproducao PASS");
}

static void sessao_fontefalha(const char *dir) {
  // O filtro do executor entra na sessao real, vale so nela e nao toca no disco.
  char antes[4096], depois[4096];
  fontefalha_dir(dir);
  FonteFiltroConfig base; memset(&base, 0, sizeof base); base.cache = FONTEFILTRO_CACHE_TODAS;
  base.maxGB = 50;
  assert(fontefalha_filtro_definir(&base));
  snprintf(arquivoFalha, sizeof arquivoFalha, "%s/fontefalha.txt", dir);
  ler(antes, sizeof antes);
  struct stat st0; assert(!stat(arquivoFalha, &st0));

  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros();
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 0) == 1);
  assert(farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 0) == 1);
  assert(farolacao_fonte_filtro_temporario_definir(NULL));
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DTS, 0) == 1);
  FonteFiltroConfig temp; assert(farolacao_fonte_filtro_temporario(&temp));

  FonteFalhaConfig cfg; fontefalha_config(&cfg);
  cfg.provedores = 0; cfg.addons = 1;
  FonteFalhaSessao *s = fontefalha_sessao_nova(&cfg, 0, -1);
  assert(s);
  FonteFalhaCandidata c[2]; memset(c, 0, sizeof c);
  c[0].indice = 10; c[0].identidade = "dts/a"; strcpy(c[0].addon, "A");
  c[0].filtro.valores[FONTEFILTRO_AUDIO] = UINT64_C(1) << 7;  // DTS
  c[1].indice = 11; c[1].identidade = "limpa/b"; strcpy(c[1].addon, "A");
  FonteFalhaRota r;
  // Antes do filtro temporario a DTS (primeira da fila) e a escolhida.
  FonteFalhaSessao *ref = fontefalha_sessao_clonar(s);
  assert(fontefalha_proxima(ref, c, 2, 1, &r) == FONTEFALHA_PROXIMA && r.indice == 10);
  fontefalha_sessao_liberar(ref);
  // Com o filtro temporario so a limpa passa; um clone herda; outra sessao nao.
  assert(fontefalha_sessao_filtro_temporario(s, &temp));
  FonteFalhaSessao *clone = fontefalha_sessao_clonar(s);
  FonteFalhaSessao *outra = fontefalha_sessao_nova(&cfg, 0, -1);
  assert(fontefalha_proxima(s, c, 2, 1, &r) == FONTEFALHA_PROXIMA && r.indice == 11);
  assert(fontefalha_proxima(clone, c, 2, 1, &r) == FONTEFALHA_PROXIMA && r.indice == 11);
  assert(fontefalha_proxima(outra, c, 2, 1, &r) == FONTEFALHA_PROXIMA && r.indice == 10);
  assert(fontefalha_tentar(s, &c[1], FONTEFALHA_DIRETA));
  assert(fontefalha_proxima(s, c, 2, 1, &r) == FONTEFALHA_FIM);   // tentada nao volta; DTS filtrada
  // Remover o temporario devolve a candidata DTS (nada ficou gravado).
  assert(fontefalha_sessao_filtro_temporario(s, NULL));
  assert(fontefalha_proxima(s, c, 2, 1, &r) == FONTEFALHA_PROXIMA && r.indice == 10);
  FonteFiltroConfig invalido; memset(&invalido, 0, sizeof invalido);
  invalido.excluir[FONTEFILTRO_AUDIO] = UINT64_C(1) << 40;
  assert(!fontefalha_sessao_filtro_temporario(s, &invalido));
  assert(!fontefalha_sessao_filtro_temporario(NULL, &temp));
  fontefalha_sessao_liberar(s); fontefalha_sessao_liberar(clone); fontefalha_sessao_liberar(outra);

  // fontefalha.txt e o filtro persistente seguem exatamente como estavam.
  ler(depois, sizeof depois);
  assert(!strcmp(antes, depois));
  struct stat st1; assert(!stat(arquivoFalha, &st1) && st0.st_mtime == st1.st_mtime && st0.st_size == st1.st_size);
  FonteFiltroConfig g; fontefalha_config_ler(); fontefalha_filtro(&g);
  assert(!memcmp(&g, &base, sizeof g));
  puts("sessao fontefalha: filtro temporario so na sessao, arquivo intocado PASS");
}

static void esconder_dv_persistente(const char *dir) {
  char antes[4096], meio[4096], depois[4096];
  fontefalha_dir(dir);
  FonteFiltroConfig base; memset(&base, 0, sizeof base);
  base.excluir[FONTEFILTRO_VISUAL] = UINT64_C(1) << 12;   // SDR ja excluido pela pessoa
  assert(fontefalha_filtro_definir(&base));
  snprintf(arquivoFalha, sizeof arquivoFalha, "%s/fontefalha.txt", dir);
  ler(antes, sizeof antes);

  // Padrao: so temporario, mesmo com persistir=1 em outras acoes.
  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros();
  assert(farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 1) == 1);
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DTS, 1) == 1);
  assert(ilhas == 0); ler(depois, sizeof depois); assert(!strcmp(antes, depois));

  // Escolha explicita: grava os bits de DV, avisa na ilha com Desfazer.
  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros();
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 1) == 1 && ultimaTroca == 2);
  assert(ilhas == 1 && ilhaFn && !strcmp(ilhaFrase, FT_ACAO_ESCONDER_DV));
  assert(bitsTemp(FONTEFILTRO_VISUAL) == BITS_DV);   // a reproducao atual tambem
  FonteFiltroConfig g; fontefalha_config_ler(); fontefalha_filtro(&g);
  assert(g.excluir[FONTEFILTRO_VISUAL] == (BITS_DV | (UINT64_C(1) << 12)));
  ler(meio, sizeof meio); assert(strcmp(antes, meio));
  // Desfazer tira so o que a acao acrescentou e devolve o arquivo ao que era.
  ilhaFn(NULL);
  fontefalha_config_ler(); fontefalha_filtro(&g);
  assert(g.excluir[FONTEFILTRO_VISUAL] == (UINT64_C(1) << 12));
  ler(depois, sizeof depois); assert(!strcmp(antes, depois));
  assert(!farolacao_fonte_desfazer_dv());   // nada mais a desfazer

  // Troca recusada: nada e gravado nem anunciado.
  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros(); recusaTroca = 1;
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 1) == 0);
  assert(ilhas == 0 && bitsTemp(FONTEFILTRO_VISUAL) == 0);
  ler(depois, sizeof depois); assert(!strcmp(antes, depois));

  // Se a pessoa ja escondia o DV, o Desfazer nao e oferecido (nada a desfazer).
  base.excluir[FONTEFILTRO_VISUAL] |= BITS_DV;
  assert(fontefalha_filtro_definir(&base));
  farolacao_fonte_encerrar(); listaPadrao(); zerarRegistros();
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 1) == 1);
  assert(ilhas == 1 && ilhaFn == NULL);
  puts("esconder DV nesta TV: opt-in, ilha com Desfazer, reverte so o acrescentado PASS");
}

int main(int argc, char **argv) {
  assert(argc == 2);
  elegibilidade_e_escolha();
  sem_candidata();
  executor_e_tentadas();
  filtro_temporario();
  sessao_fontefalha(argv[1]);
  esconder_dv_persistente(argv[1]);
  puts("farol_acao_fonte: PASS");
  return 0;
}
