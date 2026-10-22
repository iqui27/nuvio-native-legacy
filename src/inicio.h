// O INICIO DO STREAM: PRAZOS E EXPLICACAO (#202, "tempo de inicio").
//
// Medido no D1 (07/10, 367 inicios): do Play ao primeiro quadro a mediana e
// 5 a 10 s e o p90 15 a 27 s. Parte vem de espera morta (fonte que nao abre
// segurada por 20 a 30 s, add-on mudo). REGRA DO DONO: nunca baixar a
// qualidade que a pessoa configurou para ganhar tempo. Aqui so se corta
// espera, e se EXPLICA o que acontece para ela mudar o ajuste se quiser.
//
// Puro (sem SDL), para o teste do Mac (tests/inicio_rapido.sh).
#ifndef NV_INICIO_H
#define NV_INICIO_H

// --- WATCHDOG DE ABERTURA (filme/serie, fonte automatica) -----------------------
// Sem nenhum sinal de vida o prazo e CURTO (15 s: um MKV 4K de 10 GB levou
// 10,7-12,5 s para ficar pronto numa TCL, ver fontevolta.h); com dado chegando (buffer subindo) o
// longo de sempre. O curto so vale quando a proxima candidata nao e PIOR que a
// atual (mesma resolucao e Dolby Vision, ou melhor): trocar de fonte por
// demora para cair num 1080p seria baixar a qualidade por pressa, e isso so a
// pessoa decide. Sem proxima, nao ha para onde ir: o longo.
#define INICIO_ABRE_SEM_SINAL_MS 15000u
#define INICIO_ABRE_COM_DADO_MS  30000u
// 4K / Dolby Vision sem quadro ainda: TCL 12/10, Silo 2160p DV pelo debrid
// levou 12 s ate as faixas e 24 s ate o primeiro quadro; o prazo de 30 s
// descartou a fonte boa. Quadro ja desenhado nunca vence (dono, 12/10).
#define INICIO_ABRE_PESADO_MS    60000u

typedef struct {
  int tocou;             // a fonte ja tocou nesta sessao (nunca vence)
  int proximaSemPerda;   // existe candidata seguinte nao pior que a atual
  int dadoChegando;      // buffer subindo / pipeline pronto
  int quadro;            // o player ja desenhou quadro desta fonte (nunca vence)
  int pesado;            // fonte 4K ou Dolby Vision
  unsigned desdeMs;      // desde que a fonte foi entregue ao player
} InicioAbertura;

unsigned inicio_abre_prazo_ms(const InicioAbertura *g);
int      inicio_abre_vencida(const InicioAbertura *g);

// --- A EXPLICACAO NA ILHA --------------------------------------------------------
// Passou de INICIO_EXPLICA_MS sem imagem, ou houve troca de fonte: a ilha diz
// em palavras o que esta esperando. Transitoria e nunca bloqueia.
#define INICIO_EXPLICA_MS 4000u

enum {
  INI_NADA = 0,
  INI_ESPERA_UM,       // "Esperando PenguPlay (lento)"
  INI_ESPERA_MUDO,     // "WebStreamr nao respondeu da ultima vez"
  INI_ESPERA_VARIOS,   // "Esperando N add-ons"
  INI_NAO_RESPONDEU,   // "WebStreamr nao respondeu"
  INI_VERIFICANDO,     // "Verificando a fonte..."
  INI_FALLBACK,        // "Fonte falhou, tentando a proxima"
  INI_ABRINDO_DEBRID   // "Abrindo 4K pelo debrid..."
};
enum { INI_FASE_BUSCA = 1, INI_FASE_VERIFICA = 2, INI_FASE_ABRE = 3 };

typedef struct {
  unsigned desdeMs;   // desde o Play
  int fase;           // INI_FASE_*
  int pendentes;      // add-ons que a decisao automatica ainda espera
  int pendenteMudo;   // o unico que falta ja ficou sem responder na busca anterior
  int fontesChegaram; // outros add-ons ja trouxeram fontes nesta busca
  int semResposta;    // add-ons que desistiram nesta busca
  int tentativa;      // 1 = primeira fonte; 2+ = troca depois de falha
  int viaDebrid;      // a fonte aberta vem de debrid
} InicioSinais;

int inicio_motivo(const InicioSinais *s);

#endif
