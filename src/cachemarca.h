#ifndef NV_CACHEMARCA_H
#define NV_CACHEMARCA_H

// EM CACHE / FORA DE CACHE PELO TEXTO DO ADDON (dono, 17/11/2026; regras de
// claude/formatter-cached-markers.md).
//
// Os formatadores dizem o estado do cache com ~40 marcas diferentes (⚡ ⏳
// [RD+] [RD download] 🚀 INSTANT ✦ ✧ ☑ ☒ ✅ 🔄 ► ▼ 🎫 🎟...), e varias delas
// querem dizer outra coisa em outro formatador (✦ = HDR no Tamtaro, 🚀 = FHD no
// Prism, ⬇️ = "link de download" nos quadrados coloridos). Por isso:
//
//   1. Marca SEM AMBIGUIDADE vale no nome e na descricao: [RD+] [RD⚡ RD ⚡
//      ⚡ [RD] [RD C] INSTANT ⚡Ready (Your Media) "Cached" / [RD download]
//      [RD⬇ [RD⏳ RD ⏳ UNCACHED "Not Cached" "Not Ready" "MAY NOT BE AVAILABLE"
//      [RD U] "⛔ Cached".
//   2. SIMBOLO SOZINHO so vale no NOME: ⚡ 🚀 ✦ ☑ ✅ ➠ ► ▻ 🎫 / ⏳ ✧ ☒ 🔄 ⌛ ▼ ▽
//      🎟 ❌ — fora "⚡ Forced", "✦ HDR", "🚀 FHD", "[🔄]" e "[❌]".
//   3. "Fora de cache" so desfaz "em cache" NA MESMA LINHA. O nome decide;
//      sem decisao no nome, a descricao. Linhas que discordam = desconhecido.
//   4. Marca de BIBLIOTECA (☁ 📌 🗄 ✳ 🅛 "• Library" "(Your Media)") conta como
//      em cache. 📚 nao (e "pacote" no Core Builds e no luckynumb3rs).
//   5. Nome de arquivo nunca conta ("Instant.Family.mkv", "...download]").
//   6. "[RD]" sem marca = fora de cache, mas so nos addons cujo "em cache" e
//      conhecido (StremThru, Jackettio, Debridio, MediaFusion, Peerflix).
//
// Funcao PURA (sem SDL). Testada em tests/cachemarca.c.

// +1 em cache, -1 fora de cache, 0 nao se sabe. `nome` e `descricao` com \n
// entre as linhas, como o addon mandou; `provedor` = nome do addon instalado
// (pode ser NULL). `*biblioteca` (pode ser NULL) = 1 quando a decisao veio de
// uma marca de biblioteca.
int cachemarca_fonte(const char *nome, const char *descricao, const char *provedor,
                     int *biblioteca);

// So as marcas "fora de cache" num texto qualquer (regra 1 e os simbolos da
// regra 2): o antigo stream_texto_fora_de_cache.
int cachemarca_fora(const char *texto);

#endif
