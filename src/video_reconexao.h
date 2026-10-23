// RECONEXAO QUANDO A REDE CAI NO MEIO DA REPRODUCAO.
//
// Antes disto, qualquer erro do pipeline com o video ja tocando virava
// `falhou` na hora: o Wi-Fi da TV piscava no meio do episodio e o player caia
// na tela de erro (ou o automatico de fontes trocava de link), mesmo com a
// fonte perfeitamente boa. A unica queda que se recuperava era o "pipeline
// morto" do webOS (video.c, `recuperando`).
//
// A regra: erro de rede/stream DEPOIS que a reproducao comecou recarrega a
// MESMA URL, na MESMA posicao, com as mesmas faixas — ate NV_RECON_MAX vezes,
// esperando 3 s, 10 s e 25 s antes de cada uma. So depois disso `falhou`. O
// primeiro prepare fica de fora: ali quem decide e o automatico de fontes.
//
// Os tres alvos (video.c, video_tpk.c, video_tizen.c) usam esta mesma maquina;
// cada um so traduz o SEU erro de rede e faz o recarregar do SEU jeito. Tudo
// roda no fio principal (video_bombear): os eventos dos pipelines chegam de
// outros fios e so anotam.
//
// Funcoes puras e sem SDL, para o teste compilar no Mac (tests/reconexao.sh).
#ifndef NV_VIDEO_RECONEXAO_H
#define NV_VIDEO_RECONEXAO_H

#include <string.h>

#define NV_RECON_MAX 3
#define NV_RECON_PRAZO_MS 30000u
// Quanto a reproducao precisa andar, alem do ponto em que caiu, para a queda
// contar como superada. Sem esta folga um recarregar que abre e cai de novo
// logo em seguida zeraria o contador a cada vez, e a rede morta viraria um
// laco infinito de "reconectando".
#define NV_RECON_FIRME_SEG 10.0

typedef struct {
  int      tentativa;  // tentativas usadas nesta queda (0 = nenhuma queda)
  int      pendente;   // 1 = esperando o prazo para recarregar
  int      esgotou;    // 1 = as NV_RECON_MAX foram e nao voltou: desistir
  unsigned quando;     // instante (ms) do proximo recarregar
  unsigned abriu;      // last reopen; bounded even without a backend error
  double   alvo;       // posicao (s) em que caiu: onde o recarregar retoma
} NvReconexao;

static inline void nv_recon_zerar(NvReconexao *r) { memset(r, 0, sizeof *r); }

// Espera antes da tentativa n (1..NV_RECON_MAX).
static inline unsigned nv_recon_espera_ms(int n) {
  return n <= 1 ? 3000u : n == 2 ? 10000u : 25000u;
}

// Houve erro. `ehRede` diz se o erro e da classe de rede/stream. Devolve 1 =
// reconexao agendada (ou ja agendada), 0 = desistir (falhou).
//
// Erro que chega DURANTE uma queda (o recarregar nao abriu) conta como a
// proxima tentativa seja qual for a classe: com a rede fora, o open falha com
// o erro que o firmware quiser.
static inline int nv_recon_erro(NvReconexao *r, int ehRede, unsigned agora, double pos) {
  if (r->esgotou) return 0;
  if (r->pendente) return 1;
  if (!r->tentativa) {
    if (!ehRede) return 0;
    r->alvo = pos;
  }
  if (r->tentativa >= NV_RECON_MAX) { r->esgotou = 1; return 0; }
  r->tentativa++;
  r->pendente = 1;
  r->quando = agora + nv_recon_espera_ms(r->tentativa);
  return 1;
}

// Chamar por quadro: 1 UMA VEZ quando a espera venceu (quem chama recarrega).
static inline int nv_recon_vencida(NvReconexao *r, unsigned agora) {
  if (!r->pendente || (int)(agora - r->quando) < 0) return 0;
  r->pendente = 0;
  r->abriu = agora;
  return 1;
}

// A backend that never reports READY/error must still consume its retry.
static inline int nv_recon_expirou(const NvReconexao *r, unsigned agora) {
  return r->tentativa && !r->pendente && !r->esgotou &&
         agora - r->abriu >= NV_RECON_PRAZO_MS;
}

// Posicao corrente da reproducao. Passou NV_RECON_FIRME_SEG alem do ponto da
// queda: superada, a proxima comeca de novo pela espera de 3 s.
static inline void nv_recon_progresso(NvReconexao *r, double pos) {
  if (!r->tentativa || r->pendente || r->esgotou) return;
  if (pos >= r->alvo + NV_RECON_FIRME_SEG) r->tentativa = 0;
}

// 1 enquanto ha uma queda em curso (esperando ou recarregando).
static inline int nv_recon_ativa(const NvReconexao *r) {
  return r->tentativa > 0 && !r->esgotou;
}

// --- classe do erro, por alvo -------------------------------------------------

static inline int nv_recon_tem(const char *t, const char *agulha) {
  size_t n = strlen(agulha);
  for (; t && *t; t++) {
    size_t i;
    for (i = 0; i < n; i++) {
      char a = t[i], b = agulha[i];
      if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
      if (a != b) break;
    }
    if (i == n) return 1;
  }
  return 0;
}

// webOS (uMS). NAO HA no repositorio um codigo de rede MEDIDO: os erros vistos
// com o video ja andando sao o generico 100 "Playing error" e o "server
// error:40403" do servidor. Por isso a regra e por EXCLUSAO: a familia 2xx e
// o que o proprio ARQUIVO decide (200 audio, 201/203 codec e formato, 210
// legenda) e recarregar so repete a recusa; o resto, com a reproducao ja
// andando, e tratado como fonte que parou de entregar.
// SO OS CODIGOS MEDIDOS (logs da LG, 30/09, ~22 mil eventos de erro): 300
// "Network Error" e a familia 4xxxx "server error:40400/40403" do servidor
// de midia. Todo o resto (100 "Playing error", 203 "AV Type Not Founded",
// 204 "Fail To Demultiplex", 700 "seek Failure", 600...) e defeito da FONTE ou
// do pipeline: recarregar a mesma URL tres vezes so atrasaria em ~38 s a troca
// de fonte do automatico. A primeira versao contava todo codigo fora de 2xx.
static inline int nv_recon_rede_ums(double codigo) {
  return codigo == 300 || (codigo >= 40000 && codigo < 50000);
}

// .tpk (Tizen.Multimedia.Player do host .NET). O host loga "erro " + e.Error
// antes do evento 5 (tizen-tpk/Video.cs), e o evento traz (int)e.Error.
// PLAYER_ERROR_CONNECTION_FAILED = (TIZEN_ERROR_PLAYER | 0x20) | 0x06, com
// TIZEN_ERROR_PLAYER = -0x01940000: 0xFE6C0026 como int.
#define NV_RECON_TPK_CONNECTION_FAILED ((int)0xFE6C0026u)
static inline int nv_recon_rede_tpk(int codigo, const char *linhaHost) {
  if (codigo == NV_RECON_TPK_CONNECTION_FAILED) return 1;
  return linhaHost && nv_recon_tem(linhaHost, "ConnectionFailed");
}

// .wgt (AVPlay): o onerror entrega o nome do erro, ex.
// "PLAYER_ERROR_CONNECTION_FAILED".
static inline int nv_recon_rede_avplay(const char *texto) {
  return texto && (nv_recon_tem(texto, "CONNECTION_FAILED") ||
                   nv_recon_tem(texto, "NETWORK") ||
                   nv_recon_tem(texto, "TIMEOUT") ||
                   nv_recon_tem(texto, "TIMED_OUT"));
}

#endif
