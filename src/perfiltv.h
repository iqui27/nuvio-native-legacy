// PERFIL AUTOMATICO POR APARELHO: a UNICA tabela que diz quanto de textura,
// quantos fios de rede de arte e qual teto de heroi cada TV recebe.
//
// Existe porque a mesma regra morava em tres lugares (orcamentoMB e
// tetoPermitidoMB em tex_cache.c, e o otimizador de diagnostico.c) e eles ja
// tinham descasado uma vez: o degrau "< 1,2 GB" do teto em Ajustes nao era o
// mesmo do NV_TEX_MB_FIXO. Aqui nao ha SDL, GL nem arquivo: e aritmetica pura,
// testada por tests/diagnostico.sh nos dois alvos (LG e Tizen) sem aparelho.
//
// A tabela, e de onde veio cada numero, esta em perfiltv.c.
#ifndef NV_PERFILTV_H
#define NV_PERFILTV_H
#include <stddef.h>

typedef enum { PTV_LG = 0, PTV_TIZEN = 1 } PtvPlataforma;
typedef enum { PTV_QUALIDADE = 0, PTV_DESEMPENHO = 1 } PtvModo;

typedef struct {
  int texMb;      // orcamento de texturas, MB
  int fiosRede;   // fios de rede do cache de artes ativos (os criados sao o teto)
  int heroiLarg;  // teto de decodificacao da arte de tela cheia, px
} PtvPerfil;

// Plataforma do build (Tizen = __EMSCRIPTEN__).
PtvPlataforma ptv_plataforma(void);

// Orcamento automatico pela RAM (MemTotal na LG, deviceMemory no Tizen). 0 =
// RAM desconhecida.
int ptv_tex_auto_mb(PtvPlataforma p, long memMB);
// O MAXIMO que a RAM permite (Ajustes, NV_TEX_MB_FIXO e o otimizador). No
// Tizen e o proprio automatico: medido, teto maior decodifica mais devagar.
int ptv_tex_teto_mb(PtvPlataforma p, long memMB);
// Fios de rede de arte que o build CRIA (o teto de ptv_perfil*.fiosRede).
int ptv_fios_rede_max(PtvPlataforma p);
int ptv_heroi_max(PtvPlataforma p, long memMB);

// Os padroes do aparelho: o que vale no arranque com Ajustes em Automatico.
void ptv_padrao(PtvPlataforma p, long memMB, PtvPerfil *out);

// O CANDIDATO do diagnostico para o modo pedido, SEMPRE dentro dos tetos
// acima. `texTravadoMb` > 0 = o orcamento foi escolhido pela pessoa (Ajustes)
// ou cravado na build/ambiente: o candidato o mantem e so mexe no resto.
void ptv_candidato(PtvPlataforma p, long memMB, PtvModo modo,
                   int texTravadoMb, PtvPerfil *out);

// Prende um perfil qualquer (lido do disco, por exemplo) aos tetos do aparelho.
// Devolve 1 se precisou mudar algum campo.
int ptv_limitar(PtvPlataforma p, long memMB, PtvPerfil *pf);

// Uma medida da MESMA amostra de artes, com o cache em disco quente.
typedef struct {
  int artesMs;          // do pedido ate a ultima textura pronta (ou o prazo)
  int prontas;
  int falhas;           // falhou ou nao ficou pronta no prazo
  int piorQuadroMs;     // pior quadro da propria tela durante a janela
  int despejosQuentes;  // arte na tela jogada fora durante a janela
} PtvMedida;

// 1 = o DEPOIS e pior que o ANTES e o anterior tem de voltar. `motivo` recebe
// a CHAVE (portugues, i18n) do primeiro criterio que reprovou.
int ptv_depois_pior(const PtvMedida *antes, const PtvMedida *depois,
                    const char **motivo);

// O VEREDITO DO RETESTE, com MARGEM (histerese). "Nao piorou" nao basta para
// trocar de perfil: com a rede oscilando, cada rodada trocava a TV de perfil
// (96|4|1920 -> 160|4|1920 -> 96|2|1280 com 316/297/360 ms). A regra esta em
// perfiltv.c, em cima de ptv_decidir.
//   PTV_DEC_APLICAR   o candidato fica e vai para o disco;
//   PTV_DEC_RESTAURAR o reteste piorou (ptv_depois_pior): o anterior volta;
//   PTV_DEC_RUIDO     nao piorou, mas tambem nao ganhou o bastante: o anterior
//                     volta e o relatorio diz `aplicacao=mantido_ruido`.
// `despejosSessao` = artes VISIVEIS despejadas nesta sessao antes do teste (o
// sinal de falta de memoria que o agregador usa; o pico nao e sinal, o cache
// sempre enche ate o orcamento).
#define PTV_GANHO_PCT      15   // ganho minimo no tempo de artes, % do ANTES
#define PTV_GANHO_MIN_MS   80   // ... e nunca menos que isto
#define PTV_QUADRO_RUIDO_MS 17  // um quadro a 60 Hz de folga no pior quadro
typedef enum { PTV_DEC_APLICAR = 0, PTV_DEC_RESTAURAR, PTV_DEC_RUIDO } PtvDecisao;
PtvDecisao ptv_decidir(const PtvPerfil *perfAntes, const PtvPerfil *perfCand,
                       const PtvMedida *antes, const PtvMedida *depois,
                       long despejosSessao, const char **motivo);

// Perfil aprovado em disco: "versao=2\ntex_mb=..\nfios_rede=..\nheroi=..\n".
int ptv_serializar(const PtvPerfil *pf, const char *modo, char *dst, size_t cap);
// 1 se leu os tres campos. Nao limita: quem le chama ptv_limitar.
int ptv_ler(const char *txt, PtvPerfil *pf);

// FONTES DE ARTE (pedido do dono, 22/09: "Destaque com outra arte" ligado e
// "esta demorando muito para baixar as artes"). Os indices 1..7 sao os mesmos
// ARTEHERO_* de artehero.h (o gravado em "Background do hero"); 8 e o OUTRO
// backdrop do TMDB (ARTEHERO_TMDB_OUTRO, o que "TMDB" vira com outra arte
// ligada); o logo entra como 9 so para ser medido, nao e fonte de fundo.
#define PTV_FONTE_CATALOGO   1
#define PTV_FONTE_METAHUB    2
#define PTV_FONTE_TMDB       3
#define PTV_FONTE_TRAKT      4
#define PTV_FONTE_APPLE      5
#define PTV_FONTE_FANART     6
#define PTV_FONTE_ANIME      7
#define PTV_FONTE_TMDB_OUTRO 8
#define PTV_FONTE_LOGO       9
#define PTV_N_FONTES        10
#define PTV_FONTE_FUNDO_MAX  8   // 1..8 sao fundo; o resto nao entra em sugestao

typedef struct {
  int ok, falhas;
  int resolveMs;    // consulta da url virtual (TMDB, Trakt, Apple, fanart, anime)
  int downloadMs;   // so o download da imagem
  long bytes;
  int largura, altura;  // da ultima imagem que respondeu
  // Em quantos titulos da amostra a arte foi a MESMA do card (mesma url real
  // ou mesmos bytes). Qualquer um > 0 e `igual_ao_card=1` no relatorio: com
  // "outra arte" ligado ela nao e outra arte e nunca e sugerida.
  int iguais;
} PtvFonte;

// Nome curto para o relatorio ("catalog", "metahub", "tmdb", "trakt",
// "apple", "fanart", "anime", "tmdb_outro", "logo").
const char *ptv_fonte_nome(int fonte);
// Chave i18n (portugues) para a tela.
const char *ptv_fonte_rotulo(int fonte);
// Custo medio por arte que respondeu (resolucao + download), ms; -1 sem nenhuma.
int ptv_fonte_ms(const PtvFonte *f);
// Fonte de fundo pelo HOST da url (metahub, TMDB, Trakt, Apple, fanart,
// anime, virtual); o resto e catalogo. So para quando a url nao casa com
// nenhuma das medidas.
int ptv_fonte_da_url(const char *url);
// O valor de "Background do hero" que faz o destaque usar a fonte medida `f`
// (o outro do TMDB e "TMDB" com outra arte ligada). -1 = nenhum.
int ptv_ajuste_da_fonte(int f, int diferente);

// PROPOSTA PARA O DESTAQUE. "Claramente mais lenta" = mais que o DOBRO da base
// E acima de 800 ms por arte (ou so falhou onde a base respondeu).
//   diferente ligado: base = a fonte do card. Dispara quando a do destaque e
//     LENTA frente ao card ou e a MESMA imagem do card (iguais > 0). Propoe a
//     fonte de fundo mais rapida que seja realmente diferente (iguais == 0),
//     nao lenta frente ao card, e que nao seja a do ajuste atual; sem nenhuma
//     e com a do destaque lenta, propoe desligar "Destaque com outra arte".
//   diferente desligado com fonte escolhida: base = a fonte mais rapida
//     medida, que nao seja a atual; propoe trocar para ela.
// NUNCA propoe o valor que o ajuste ja tem (o relatorio 1669 propos
// "alvo:metahub" com o destaque ja em Metahub).
#define PTV_MOTIVO_LENTA 0
#define PTV_MOTIVO_IGUAL 1
typedef struct {
  int ativa;
  int fonte, diferente;      // o que vai ficar nos Ajustes
  int lenta, base;           // fontes comparadas
  int msLenta, msBase;
  int alvo;                  // a fonte MEDIDA que o destaque passa a usar (0 = nenhuma)
  int motivo;                // PTV_MOTIVO_*
} PtvSugestao;
int ptv_sugerir_destaque(const PtvFonte f[PTV_N_FONTES], int fonteHero,
                         int fonteCard, int fonteAjuste, int diferente,
                         PtvSugestao *out);

// Largura e altura pelo cabecalho (JPEG, PNG, WebP, GIF). 1 se reconheceu.
int ptv_dimensoes(const unsigned char *b, long n, int *w, int *h);

#endif
