// TESTE DE VELOCIDADE: a CONTA, sem rede, sem SDL e sem arquivo.
//
// O pedido do dono: "medir a velocidade dos stream e addon para poder dizer o
// tamanho maximo e otimo dos titulos para tocar sem parar". A medida em si
// (baixar um trecho de fontes de verdade e contar bytes por segundo) mora em
// rede.c (rede_medir_vazao) e em diagnostico.c; aqui fica so o que transforma
// as amostras por segundo em "ate 12 GB por filme de 2 h", para
// tests/vazao.sh prender a regra sem aparelho.
//
// A REGRA, e por que cada numero:
//   - OTIMO  = p20 x 0,75. O p20 e o "segundo ruim" tipico da conexao: 80% dos
//     segundos medidos foram mais rapidos que ele. Um arquivo cuja taxa media
//     cabe em 75% disso toca sem parar mesmo quando a rede cai para o pior
//     trecho comum — a folga de 25% e o que o pico de bitrate de uma cena
//     pesada (remux chega a 2x a media) come antes de o buffer esvaziar.
//   - MAXIMO = mediana x 0,9. Cabe na velocidade tipica, mas nos trechos
//     ruins o buffer esvazia e o player pode pausar para carregar.
//   - GB = Mbps x segundos / 8 / 1000 (GB decimal, que e como addon e debrid
//     escrevem o tamanho do arquivo). Filme de 2 h = 7200 s; episodio de
//     45 min = 2700 s.
#ifndef NV_VAZAO_H
#define NV_VAZAO_H
#include <stddef.h>

// Amostras por fonte (1 por segundo) e no total (3 fontes).
#define VAZAO_SEG_MAX     16
#define VAZAO_FONTES_MAX  3
#define VAZAO_AMOSTRAS_MAX (VAZAO_SEG_MAX * VAZAO_FONTES_MAX)

#define VAZAO_FILME_S    7200   // 2 h
#define VAZAO_EPISODIO_S 2700   // 45 min

typedef struct {
  int n;            // amostras usadas
  int medianaKbps;
  int p20Kbps;
  int otimoKbps;    // p20 x 0,75
  int maximoKbps;   // mediana x 0,9
} VazaoResumo;

// Mediana e p20 (posicao mais proxima: o valor na posicao ceil(0,2 n) da
// lista ordenada) das amostras em kbps. Amostra negativa e ignorada; zero
// CONTA — um segundo sem byte nenhum e exatamente o trecho ruim que o otimo
// precisa enxergar. Devolve 0 (e zera `r`) sem amostra valida.
int vazao_resumir(const int *kbps, int n, VazaoResumo *r);

// GB decimais de `kbps` sustentados por `segundos`.
double vazao_gb(int kbps, int segundos);

// "38" / "4,5" / "0,8": uma casa abaixo de 10, inteiro dali para cima.
// `sep` e o separador decimal do idioma (',' ou '.').
void vazao_fmt_mbps(char *dst, size_t n, int kbps, char sep);
void vazao_fmt_gb(char *dst, size_t n, double gb, char sep);

// Endereco de AVISO e nao de conteudo, o mesmo criterio da verificacao de
// fonte (streams.c) mais os clipes de erro que o player descarta pela duracao:
// slate do AIOStreams/ElfHosted, downloading.mp4 do Debridio e os clipes por
// codigo de erro do static.debridio.com. Medir um desses mediria o servidor de
// aviso, nao a fonte.
int vazao_url_aviso(const char *url);

// "https://a.b:443" de "https://a.b:443/x?y" — a chave de "hosts diferentes".
// So o esquema e o host; o caminho (onde vai a chave do debrid) nunca sai
// daqui. Devolve 0 sem "://".
int vazao_host(const char *url, char *dst, size_t n);

// Dica de uma linha para a taxa ideal, em portugues (a tela passa por i18n).
const char *vazao_dica(int otimoKbps);

#endif
