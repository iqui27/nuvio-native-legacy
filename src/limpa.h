#ifndef NV_LIMPA_H
#define NV_LIMPA_H

#include <stddef.h>

// TEXTO DE ADDON -> TEXTO QUE AS FONTES DO APP DESENHAM (#144).
//
// Os formatadores de nome/descricao de fonte (AIOStreams, Torrentio,
// MediaFusion...) enfeitam a linha com emoji, bandeiras, simbolos, tracos de
// caixa, letras matematicas e versalete. A Inter (e as outras familias
// embarcadas) nao tem quase nada disso: cada um saia como o retangulo do
// .notdef, ou puxava a linha inteira para a fonte de reserva, ou era cortado
// no meio pelo limite de bytes.
//
// Esta funcao e PURA e independe de SDL_ttf: decide por tabela, nao por glifo
// disponivel. A saida so tem ASCII, Latin, pontuacao comum, "· × ★ ✓ →" e
// letras de outras escritas (CJK, cirilico...) — estas ficam para a fonte de
// reserva de text.c, que sabe abri-las.
//
//   emoji e pictogramas      -> somem; entre dois textos viram " · "
//   bandeira 🇬🇧             -> codigo de idioma ("EN"); desconhecida: o pais
//   ✅ ✔ / ❌ ✖ / ⭐ 🌟      -> ✓ / × / ★
//   ⚡ ⚙ 🎞️ 📦 💾 🔊 🌐 ...  -> somem (o numero/palavra ao lado fica)
//   │ ┃ ▸ ◆ ● ▪ ...          -> separador " · " (sem repetir, sem ponta)
//   𝐇𝐃𝐑 ＨＤＲ ᴴᴰ ᴇ ꜱ ₀₁ Ⓐ   -> letras/digitos ASCII (dobra.c)
//   ZWJ, VS15/16, tom de pele, tags, controles, U+FFFD -> somem
//   bytes UTF-8 invalidos     -> somem (a saida e sempre UTF-8 valido)
//
// `flags`: NV_LIMPA_UMA_LINHA faz cada \n virar separador (a folha de fontes
// tem duas linhas de descricao e quebra por palavra); sem ela, \n e mantido e
// quem desenha quebra nele (txt_bloco).
//
// Devolve o tamanho escrito (sem o NUL). Se nao couber, corta NA FRONTEIRA de
// um codepoint, nunca no meio de uma sequencia UTF-8.
#define NV_LIMPA_UMA_LINHA 1
size_t nv_limpar_texto(const char *in, char *out, size_t tam, int flags);

// Add-on text mode: tidies ONE already-cleaned line. Spaces collapse, words made
// only of separators (| - / , • ·) are dropped at the start/end and doubled in
// the middle ("A |  | B" -> "A | B"), and separators glued to the first/last
// word go too ("31.4 GB | 43.2 Mbps |" -> "31.4 GB | 43.2 Mbps"). A hyphen
// inside a word (WEB-DL) is left alone. In place; returns the new length.
size_t nv_aparar_separadores(char *s);

#endif
