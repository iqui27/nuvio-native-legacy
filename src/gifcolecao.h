// O GIF DO CARTAZ DE COLECAO EM FOCO: de onde ele sai e, quando nao anima,
// POR QUE (#141).
//
// O relato: nas fileiras de colecao da conta, Netflix anima e Apple TV,
// Paramount+, Prime Video, Crunchyroll e outras nao. O log da TV (Tizen, 1.4.7,
// 2 GB) nao dizia nada sobre os cartazes que ficavam parados, e o unico
// "[gif] ... decode nativo" dele era 498x448 -> 328x295 — largura de saida
// 328, que e o AVATAR da tela de perfis (perfilsel.c pede a largura do
// avatar), e nao um cartaz (home.c pede 480). Ou seja: nenhum cartaz de
// colecao chegou a abrir GIF naquela sessao, e o app ficou mudo sobre isso.
//
// DUAS COISAS aqui, as duas sem SDL nem GL (testadas no Mac, tests/gifcolecao.sh):
//
//   1. A FONTE. O cartaz animava so por `focusGifUrl`. Mas ha pasta cuja CAPA
//      (`coverImageUrl`) ja e o GIF animado, sem focusGifUrl nenhum. SUSPEITA,
//      nao prova: no log havia dois giphy.gif decodificados como textura
//      PARADA pelo cache de arte e nenhum "[gif]" de cartaz depois deles. Se
//      fossem o focusGif, o arquivo estaria no disco (GIF vai a arquivo) e o
//      quadro seguinte abriria o GIF, com linha no log; como capa, ninguem
//      nunca pedia a animacao. O "(saiu WxH)" daquelas linhas decide: largura
//      do card = capa, 128 = pedido de tex_arquivo.
//      Regra: focusGif quando ha; senao a capa, SE os bytes que chegaram dela
//      comecam com GIF8. A decisao e pelos BYTES, e nao pela URL: giphy manda
//      `giphy.gif?cid=...` (a extensao nem e o fim da URL), e o nome da URL
//      nao garante o formato do corpo. MEDIDO com curl em 25/09/2026: o
//      media*.giphy.com devolve image/gif e GIF89a com Accept */*, image/webp
//      ou o Accept de <img> do Chromium, com ou sem a query — nao negocia
//      formato, entao o cabecalho do XHR do Tizen nao e o problema la.
//
//   2. O MOTIVO. Uma linha por cartaz em foco que NAO anima, com a razao e a
//      URL saneada (host + ultimo trecho do caminho, sem query: a query do
//      giphy traz `cid` de sessao, e ha CDN que poe token nela).
#ifndef NV_GIFCOLECAO_H
#define NV_GIFCOLECAO_H
#include <stddef.h>

typedef enum {
  GC_ANIMA = 0,     // anima (ou ainda esta no atraso de 350 ms)
  GC_SEM_GIF,       // sem focusGif e a capa nao e GIF
  GC_CAPA_AINDA,    // sem focusGif e a capa ainda nao chegou: nao se sabe
  GC_ARQ_AINDA,     // o arquivo do GIF ainda nao chegou (so depois do prazo)
  GC_SUMIU,         // veio GIF, mas o arquivo nao esta no disco (poda)
  GC_FORMATO,       // os bytes nao sao GIF (WebP, JPEG, HTML...)
  GC_UM_QUADRO,     // GIF de um quadro so
  GC_ORCAMENTO,     // gif.c recusou: orcamento de RAM ou tela grande demais
  GC_REDUZIDAS,     // Ajustes > animacoes reduzidas (ou build NV_LEVE)
  GC_APARELHO       // gif_pode_animar() == 0: webOS, TV de 1 GB
} GcMotivo;

// Os 4 bytes sao de GIF ("GIF8")?
int gifcol_eh_gif(const unsigned char *magica);

// A FONTE da animacao do cartaz. `magicaCapa` sao os 4 primeiros bytes que
// chegaram da capa, ou NULL quando ainda nao chegaram. Devolve focusGif quando
// ele existe; senao a capa quando ela e GIF; senao NULL. `*daCapa` = 1 quando
// a fonte e a capa.
const char *gifcol_fonte(const char *focusGif, const char *capa,
                         const unsigned char *magicaCapa, int *daCapa);

// Motivo de NAO haver fonte: GC_SEM_GIF quando a capa ja chegou e nao e GIF,
// GC_CAPA_AINDA quando ainda nao se sabe.
GcMotivo gifcol_sem_fonte(const char *capa, const unsigned char *magicaCapa);

// Motivo de o arquivo do GIF, JA EM DISCO, nao animar: le a magica do proprio
// arquivo. `animado` e o que gif_animado devolveu. GC_ANIMA quando animado.
// `magica` (opcional) recebe os 4 bytes lidos.
GcMotivo gifcol_motivo_arquivo(const char *caminho, int animado, unsigned char magica[4]);

// URL -> "host/ultimo-trecho", sem esquema, usuario, query nem fragmento.
void gifcol_sanear(const char *url, char *dst, size_t tam);

const char *gifcol_motivo_texto(GcMotivo m);

// O CARTAZ EM FOCO (o que eram os `static` de desenhaAtalhos). `anima`:
// -1 = ainda nao perguntei ao arquivo, 0 = nao anima, 1 = anima.
typedef struct {
  int ultimo;           // id da pasta em foco; -1 nenhuma
  unsigned desde;       // ms em que ela recebeu o foco
  int anima;
  GcMotivo motivoArq;   // o que gifcol_motivo_arquivo disse, quando anima == 0
  unsigned char magicaArq[4];
  char fonte[512];      // a fonte que respondeu `anima` (focusGif ou capa)
} GcFoco;
void gifcol_foco_iniciar(GcFoco *e);
// Registra o foco em `id` com a fonte `fonte`. Devolve 1 quando o cartaz (ou a
// fonte dele) MUDOU: `anima` volta a -1, `desde` = agora, e quem chama solta o
// GIF anterior (gif_parar).
int gifcol_focar(GcFoco *e, int id, const char *fonte, unsigned agora);

// O prazo antes de dizer "ainda nao chegou": um GIF de alguns MB na rede da TV
// leva segundos, e o registro nao deve gritar por um download em andamento.
#define GIFCOL_PRAZO_MS 8000

// UMA LINHA POR CARTAZ POR SESSAO. `chave` identifica a pasta (id + grupo).
// Um motivo PROVISORIO (CAPA_AINDA, ARQ_AINDA) ainda deixa passar UM
// definitivo depois; definitivo registrado, o cartaz nao fala mais. Devolve 1
// quando escreveu. `magica` pode ser NULL.
int gifcol_registrar(const char *chave, const char *titulo, GcMotivo m,
                     const char *url, int daCapa, const unsigned char *magica);
// So para teste: esquece o que ja foi registrado.
void gifcol_registros_zerar(void);

#endif
