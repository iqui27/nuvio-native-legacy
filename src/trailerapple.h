// TRAILER PELA API DA APPLE TV (UTS), como HLS.
//
// Pedido do dono (20/09/2026): "usar a API da Apple TV para os filmes e
// deixar so YouTube como fallback" — e a fonte que o app dele para tvOS ja
// usa (NuvioTVOS, AppleTrailerService.swift). tv.apple.com/api/uts/v3 e o
// backend do proprio app da Apple TV, sem documentacao publica: busca por
// titulo (pfm=appletv, senao so aparecem originais Apple), casa titulo
// normalizado EXATO e ano +-1, e a pagina do filme traz
// data.content.backgroundVideo.assets.hlsUrl — um master HLS limpo (video +
// audio, sem chave, sem login) com HEVC ate 3840 de largura, MATTED no
// aspecto do filme (3836x1606, 1920x804): sem tarja embutida, ao contrario
// do MP4 do IMDb. O `utsk` da query e exigido mas nao validado hoje.
//
// Nota que o dono ja viu e aceitou no tvOS: o robots.txt da Apple nao
// permite, e nao ha licenca de conteudo promocional cobrindo isto. Se este
// arquivo incomodar, apaga-lo deixa o resto da escada (IMDb, YouTube) de pe.
//
// Cache em memoria por IMDb e em disco (dados_dir()/trailer/<imdb>-apple):
// acerto vale 1 h, falta vale 12 h ("este filme nao tem trailer na Apple" e
// propriedade do catalogo, nao falha passageira).
#ifndef NV_TRAILERAPPLE_H
#define NV_TRAILERAPPLE_H

// Pede em fio proprio. `meta` e a linha "2022 · 3 temporadas" do catalogo,
// de onde sai o ano; sem ano nao ha busca (a Apple tem cinco "Weapons").
void        trailerapple_pedir(const char *imdb, const char *titulo, const char *meta, int serie);
const char *trailerapple_url(const char *imdb);
int         trailerapple_respondeu(const char *imdb);

#endif
