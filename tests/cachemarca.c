// Cache pelo texto do addon (src/cachemarca.c): cada linha das tabelas de
// claude/formatter-cached-markers.md (formatadores, addons diretos, colisoes).
//   bash tests/cachemarca.sh
#include "cachemarca.h"
#include <stdio.h>
#include <string.h>

typedef struct { const char *nome, *desc, *prov; int esperado, lib; const char *qual; } Caso;
#define C(n, d, p, e, l, q) { n, d, p, e, l, q }
static const Caso CASOS[] = {
  // --- 2. formatadores estilo AIOStreams ---
  C("[RD+] Torrentio 1080p", "", "AIOStreams", 1, 0, "AIO Torrentio cached"),
  C("[RD download] Torrentio 1080p", "", "AIOStreams", -1, 0, "AIO Torrentio uncached"),
  C("Torrentio 1080p", "", "AIOStreams", 0, 0, "AIO Torrentio unknown"),
  C("TorBox (Instant RD) 1080p", "", "AIOStreams", 1, 0, "AIO TorBox cached"),
  C("TorBox (RD) 1080p", "", "AIOStreams", 0, 0, "AIO TorBox uncached = sem marca"),
  C("TorBox (Your Media) 1080p", "", "AIOStreams", 1, 1, "AIO TorBox library"),
  C("[RD⚡] GDrive 4K", "", "AIOStreams", 1, 0, "GDrive cached"),
  C("[RD⏳] GDrive 4K", "", "AIOStreams", -1, 0, "GDrive uncached"),
  C("☁️ GDrive 4K", "", "AIOStreams", 1, 1, "Light GDrive library"),
  C("🎫 1080p", "", "AIOStreams", 1, 0, "Minimalistic cached"),
  C("🎟️ 1080p", "", "AIOStreams", -1, 0, "Minimalistic uncached"),
  C("Prism 1080p", "⚡Ready · 💾 4 GB", "AIOStreams", 1, 0, "Prism cached"),
  C("Prism 1080p", "❌ Not Ready · 🌱 30", "AIOStreams", -1, 0, "Prism uncached"),
  C("Prism 1080p", "📌 Library", "AIOStreams", 1, 1, "Prism library"),
  C("⚡ TB 〈4K〉", "☁︎ Movie\n✦ HDR ♬ DTS", "AIOStreams", 1, 0, "Tamtaro cached"),
  C("\xe2\x80\x8d⏳\xe2\x80\x8b TB 〈4K〉", "➤ Movie", "AIOStreams", -1, 0, "Tamtaro uncached (ZWJ/ZWSP)"),
  C("TB 〈4K〉", "☁︎ Movie\n✦ HDR", "AIOStreams", 1, 1, "Tamtaro library only"),
  C("⚡ 4K", "", "AIOStreams", 1, 0, "luckynumb3rs cached"),
  C("⏳ 4K", "⇋ 12", "AIOStreams", -1, 0, "luckynumb3rs uncached"),
  C("🗄️ 4K", "🚀 INSTANT", "AIOStreams", 1, 1, "Core Nexus cached + library"),
  C("4K", "⚠️ UNCACHED", "AIOStreams", -1, 0, "Core Nexus uncached"),
  C("✦ 4K", "『 🚀 INSTANT 』", "AIOStreams", 1, 0, "Core Sigma cached"),
  C("✧ 4K", "『 ⚠️ UNCACHED 』", "AIOStreams", -1, 0, "Core Sigma uncached"),
  C("⚡ Iɴsᴛᴀɴᴛ 4K", "", "AIOStreams", 1, 0, "Core Zenith cached"),
  C("⏳ Uɴᴄᴀᴄʜᴇᴅ 4K", "", "AIOStreams", -1, 0, "Core Zenith uncached"),
  C("✦ 4K", "", "AIOStreams", 1, 0, "Core Syntax cached"),
  C("✧ 4K", "", "AIOStreams", -1, 0, "Core Syntax uncached"),
  C("⚡️ skz 4K", "⚡ ❯ 4 GB", "AIOStreams", 1, 0, "skz cached"),
  C("skz 4K", "• Library", "AIOStreams", 1, 1, "skz library"),
  C("⚡RD 4K", "", "AIOStreams", 1, 0, "c8RdRgRP cached"),
  C("⛔ Cached 4K", "", "AIOStreams", -1, 0, "c8RdRgRP uncached says Cached"),
  C("⚡ 4K", "🟢 GOOD CHOICE", "AIOStreams", 1, 0, "custom-formatter cached"),
  C("⏳ 4K", "🔴 MAY NOT BE AVAILABLE", "AIOStreams", -1, 0, "custom-formatter uncached"),
  C("☑ RD 4K", "", "AIOStreams", 1, 0, "Askeladd 1.4 cached"),
  C("☒ RD 4K", "", "AIOStreams", -1, 0, "Askeladd 1.4 uncached"),
  C("Askeladd", "☑ [RD] 4K", "AIOStreams", 0, 0, "Askeladd 2.0 (D, simbolo sozinho nao conta)"),
  C("➠ 4K", "", "AIOStreams", 1, 0, "invisible tags cached"),
  C("⌛︎ 4K", "", "AIOStreams", -1, 0, "invisible tags uncached"),
  C("⚡ Sterzeck", "🔄 preloading", "AIOStreams", 1, 0, "Sterzeck cached"),
  C("⚡ 4K", "✏️ Movie\n⬇️ DL", "AIOStreams", 1, 0, "colour squares cached + ⬇ = link"),
  C("|⚡️ 4K", "", "AIOStreams", 1, 0, "pipes cached"),
  C("|❌️ 4K", "", "AIOStreams", -1, 0, "pipes uncached"),
  C("|📌| 4K", "", "AIOStreams", 1, 1, "pipes library"),
  C("✅ Full HD", "", "AIOStreams", 1, 0, "Full HD cached"),
  C("🔄 Full HD", "", "AIOStreams", -1, 0, "Full HD uncached"),
  C("✳️ Full HD", "", "AIOStreams", 1, 1, "Full HD library"),
  C("► 4K", "", "AIOStreams", 1, 0, "Chad cached"),
  C("▻ 4K", "", "AIOStreams", 1, 0, "Chad cached preloading"),
  C("▼ 4K", "", "AIOStreams", -1, 0, "Chad uncached"),
  C("▽ 4K", "", "AIOStreams", -1, 0, "Chad uncached preloading"),
  C("Chad 4K", "🅛 Movie", "AIOStreams", 1, 1, "Chad library"),
  // --- 3. addons diretos ---
  C("[RD+] Torrentio\n1080p", "", "Torrentio", 1, 0, "Torrentio cached"),
  C("[DL+] Torrentio\n1080p", "", "Torrentio", 1, 0, "Torrentio DL cached"),
  C("[PIO+] Torrentio", "", "Torrentio", 1, 0, "Torrentio put.io"),
  C("[RD download] Torrentio", "", "Torrentio", -1, 0, "Torrentio uncached"),
  C("[RD⚡] Comet 1080p", "", "Comet", 1, 0, "Comet cached"),
  C("[RD⬇️] Comet 1080p", "", "Comet", -1, 0, "Comet uncached"),
  C("[🔄] Comet", "", "Comet", 0, 0, "Comet info"),
  C("[RD🔄] Comet Sync", "", "Comet", 0, 0, "Comet sync"),
  C("[❌] Comet", "", "Comet", 0, 0, "Comet error"),
  C("[RD C] 1080p", "", "Comet", 1, 0, "Comet Kodi cached"),
  C("[RD U] 1080p", "", "Comet", -1, 0, "Comet Kodi uncached"),
  C("Comet", "⚡ Instant\n💾 4 GB", "Comet", 1, 0, "Comet chilllink cached"),
  C("Comet", "⬇️ Not Cached", "Comet", -1, 0, "Comet chilllink uncached"),
  C("MediaFusion 🧲 RD ⚡️", "", "MediaFusion", 1, 0, "MediaFusion cached"),
  C("MediaFusion 🧲 RD ⏳", "", "MediaFusion", -1, 0, "MediaFusion uncached"),
  C("[🧲RD⚡] 4K", "", "MediaFusion", 1, 0, "MediaFusion preset cached"),
  C("[🧲RD] 4K", "", "MediaFusion", -1, 0, "MediaFusion preset uncached"),
  C("🧲 RD 4K", "", "MediaFusion | ElfHosted", -1, 0, "MediaFusion compacto uncached"),
  C("MF", "🧲 Torrent • Cached", "MediaFusion", 1, 0, "MediaFusion • Cached"),
  C("MF", "🧲 Torrent • Not Cached", "MediaFusion", -1, 0, "MediaFusion • Not Cached"),
  C("⚡️ [RD] StremThru", "", "StremThru Torz", 1, 0, "StremThru cached"),
  C("[RD] StremThru", "", "StremThru Torz", -1, 0, "StremThru uncached"),
  C("[RD+] Jackettio 1080p", "", "Jackettio", 1, 0, "Jackettio cached"),
  C("[RD] Jackettio 1080p", "⬇️ 45% 3 MB/s", "Jackettio", -1, 0, "Jackettio uncached"),
  C("[RD ⚡] Debridio", "", "Debridio", 1, 0, "Debridio cached"),
  C("[RD] Debridio", "", "Debridio", -1, 0, "Debridio uncached"),
  C("[RD] Some addon", "", "Unknown Addon", 0, 0, "codigo sem marca, addon desconhecido"),
  C("🚀 Orion [RD]", "", "Orion", 1, 0, "Orion cached"),
  C("🪐 Orion 📺 1080p", "", "Orion", 0, 0, "Orion sem foguete"),
  C("Usenet", "⚡ Instant", "Usenet Streamer", 1, 0, "Usenet Streamer"),
  C("WebStreamr", "⏳ timeout", "WebStreamr", 0, 0, "WebStreamr ⏳ na descricao nao conta"),
  // --- 6. colisoes ---
  C("Sterzeck", "⚡ Forced", "AIOStreams", 0, 0, "⚡ Forced (legenda)"),
  C("⚡ Forced 4K", "", "AIOStreams", 0, 0, "⚡ Forced no nome"),
  C("Tamtaro ✦ HDR", "", "AIOStreams", 0, 0, "✦ HDR no nome (Chillio)"),
  C("🚀 FHD", "", "AIOStreams", 0, 0, "🚀 = resolucao (Prism)"),
  C("1080p", "📚 Pack 40 GB", "AIOStreams", 0, 0, "📚 nao conta"),
  C("⚡ 4K", "Movie.2026.download].mkv", "AIOStreams", 1, 0, "arquivo nunca conta"),
  C("4K", "Instant.Family.2018.1080p.mkv", "AIOStreams", 0, 0, "Instant num arquivo"),
  C("4K", "Instant Family (2018)", "AIOStreams", 0, 0, "Instant num titulo"),
  C("[RD+] uncached", "", "Torrentio", -1, 0, "fora vence na mesma linha"),
  C("⏳ TB", "⚡ seeds", "AIOStreams", -1, 0, "nome decide"),
  C("AIOStreams 1080p", "[TB download] Not cached | Movie.⚡.1080p.mkv", "AIOStreams", -1, 0, "descricao decide"),
  C("1080p", "WEB-DL HDR10+ DD+ 5.1 Disney+", "AIOStreams", 0, 0, "+ fora de codigo"),
  C("1080p", "1080p WEB-DL · Cached\n2.3 GB", "AIOStreams", 1, 0, "Cached sozinho"),
};

int main(void) {
  int falhas = 0, n = (int)(sizeof CASOS / sizeof *CASOS);
  for (int i = 0; i < n; i++) {
    int lib = -1, r = cachemarca_fonte(CASOS[i].nome, CASOS[i].desc, CASOS[i].prov, &lib);
    if (r != CASOS[i].esperado || (r > 0 && lib != CASOS[i].lib)) {
      printf("FALHA %-45s -> %d lib=%d (esperado %d lib=%d)\n", CASOS[i].qual, r, lib, CASOS[i].esperado, CASOS[i].lib);
      falhas++;
    }
  }
  if (!cachemarca_fora("Not cached") || cachemarca_fora("cached WEB-DL Download") ||
      !cachemarca_fora("[TB download] Torrentio") || !cachemarca_fora("⏳ UHD")) {
    puts("FALHA cachemarca_fora"); falhas++;
  }
  printf("cachemarca: %d casos, %d falhas\n", n, falhas);
  return falhas != 0;
}
