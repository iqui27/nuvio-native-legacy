// VAI TOCAR NESTA TV (pesquisa de mercado, secao 4.2), etapa 2: o registro
// ANONIMO de compatibilidade. Regra pura, sem rede, sem SDL e sem arquivo —
// quem coleta e envia e vtregenvio.c; aqui so se decide O QUE pode sair.
//
// O registro diz "neste aparelho, um arquivo com esta cara tocou (ou nao)":
// plataforma, modelo, firmware, conteiner, codec de video, codec de audio,
// HDR, faixa de bitrate, altura e resultado. NUNCA titulo, hash, endereco,
// nome de fonte, conta ou perfil — e isso e garantido por CONSTRUCAO:
//   - conteiner, codecs, HDR e resultado saem de vocabulario FECHADO; texto
//     que nao casa vira "outro", nunca o texto cru;
//   - modelo e firmware sao os unicos campos livres, e passam por uma
//     gramatica estrita: modelo e codigo tecnico conferido por plataforma
//     (codigo LG/Samsung, fabricante Android de lista fechada, arquitetura),
//     firmware e so versao numerica de ate 3 grupos; o resto (nome,
//     endereco, caminho, e-mail, hash, IP) vira "?";
//   - bitrate e altura viram faixas (o valor exato ajudaria a reconhecer o
//     arquivo);
//   - nao ha campo de texto livre na estrutura de saida.
// O servidor (servidor/recomendacoes/src/vtreg.js) repete a mesma gramatica
// e guarda so CONTAGEM por combinacao, sem linha por pessoa.
//
// O veredito por fonte ("vai tocar"/"nao vai") fica para depois de haver
// dados; nada aqui le resposta do servidor.
#ifndef VTREG_H
#define VTREG_H

#include <stddef.h>

typedef enum {
  VTR_TOCOU = 0,     // andou 2 s de video de verdade
  VTR_NAO_ABRIU,     // o player recusou antes do primeiro quadro
  VTR_CAIU,          // abriu, tocou e falhou depois
  VTR_PRAZO,         // nao respondeu no prazo de abertura
  VTR_TRAVOU,        // buffer parado alem do limite
  VTR_LENTA,         // a rede nao acompanha o bitrate
  VTR_DECODER,       // o decodificador do aparelho recusou
  VTR_TELA_PRETA,    // a pessoa pediu "Sem HDR" (som sem imagem)
  VTR_SEM_AUDIO,     // imagem sem som
  VTR_N
} VtRegResultado;

// O que o player sabe, CRU. Ponteiro NULL = desconhecido.
typedef struct {
  const char *plataforma, *modelo, *firmware;
  const char *container, *video, *audio, *hdr;
  int kbps, altura;
  int resultado;    // VtRegResultado
  int trocou;       // 1 = este resultado veio depois de uma troca de fonte
} VtRegEntrada;

// O que pode sair. So tokens; "?" = desconhecido.
typedef struct {
  char plataforma[12], modelo[48], firmware[32];
  char container[8], video[12], audio[12], hdr[12];
  int mbps;         // teto da faixa em Mbps: 0 desconhecido, 2 5 10 20 40 80 999
  int altura;       // 0 desconhecida, 480 720 1080 1440 2160 4320
  int resultado, trocou;
} VtRegRegistro;

// Sinais do instante da falha, para vtreg_classificar.
typedef struct {
  int motivo;        // o `motivo` de tentarProximaFonteVOD: 1 falhou, 2 prazo, 3 buffer, 4 rede lenta
  int abriu;         // a fonte ja tinha entregue video antes de falhar
  int erroDecoder;   // codigo de erro de decodificador do aparelho; 0 = nenhum
} VtRegSinais;

int  vtreg_classificar(const VtRegSinais *s);
// Normaliza. 0 quando nao sobra nada que valha um registro (resultado fora do
// vocabulario, ou nem conteiner nem codec de video conhecidos numa falha de
// rede — prazo/lenta/travou sem formato nao dizem nada sobre o aparelho).
int  vtreg_normalizar(const VtRegEntrada *e, VtRegRegistro *r);
// JSON de UMA linha, chaves fixas. Devolve o tamanho, 0 se nao coube.
size_t vtreg_json(const VtRegRegistro *r, char *out, size_t cap);
const char *vtreg_resultado_nome(int resultado);   // "outro" fora da faixa
// O filtro dos campos livres, exposto para o teste: copia `s` limpo em `out`
// ou escreve "?" e devolve 0 quando `s` tem cara de dado pessoal.
int  vtreg_texto_seguro(const char *s, char *out, size_t cap);
// Gramatica estrita dos dois campos livres (espelho de modeloTecnico e
// firmwareTecnico em servidor/recomendacoes/src/vtreg.js): devolve 1 e o
// valor tecnico em `out`, ou 0 e "?".
#define VTR_MODELO_MAX   32
#define VTR_FIRMWARE_MAX 11
// `plataforma` e o token ja normalizado (webos, tizen, android, linux, macos, outro).
int  vtreg_modelo_tecnico(const char *plataforma, const char *s, char *out, size_t cap);
int  vtreg_firmware_tecnico(const char *s, char *out, size_t cap);
// Impressao digital da combinacao (FNV-1a), para nao mandar a mesma duas vezes.
unsigned vtreg_chave(const VtRegRegistro *r);

// Fila de "ja mandei": um anel pequeno em memoria. 1 = novo (e passa a
// constar), 0 = repetido.
#define VTR_VISTOS 64
typedef struct { unsigned chave[VTR_VISTOS]; int n, prox; } VtRegVistos;
int  vtreg_novo(VtRegVistos *v, unsigned chave);

// A decisao de enviar: so com consentimento dado, ajuste ligado e servidor.
int  vtreg_pode_enviar(int consentiu, int ajusteLigado, int temServidor);

#endif
