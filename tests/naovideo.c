// Fonte que nao e video (2.0.2): mime da resposta final e nome de aviso.
#include <assert.h>
#include <stdio.h>
#include "../src/naovideo.h"

#define NOME(r, d, a, m) naovideo_nome(r, d, a, m)

int main(void) {
  // --- mime ---
  assert(naovideo_mime("text/html", "https://x/a", -1));
  assert(naovideo_mime("text/html; charset=utf-8", "https://x/a", -1));
  assert(naovideo_mime("Application/JSON", "https://x/a", -1));
  assert(naovideo_mime("text/plain", "https://x/a.mp4", -1));
  assert(!naovideo_mime("text/plain", "https://x/lista.m3u8?t=1", -1));   // manifesto com tipo errado
  assert(!naovideo_mime("video/mp4", "https://x/a.mp4", -1));
  assert(!naovideo_mime("application/octet-stream", "https://x/a.mkv", -1));
  assert(!naovideo_mime("application/vnd.apple.mpegurl", "https://x/a.m3u8", -1));
  assert(!naovideo_mime("", "https://x/a.mp4", -1));                      // servidor mudo: nao julga
  assert(!naovideo_mime(NULL, "https://x/a.mp4", -1));
  assert(naovideo_mime("video/mp4", "https://x/a.mp4", 12));              // corpo minusculo
  assert(!naovideo_mime("video/mp4", "https://x/a.mp4", -1));             // 206: desconhecido
  assert(!naovideo_mime("application/x-mpegurl", "https://x/a.m3u8", 30)); // playlist curta: playlistVazia decide
  // --- nome: positivos (linhas de aviso) ---
  assert(NOME("✨ | support the project!", "", 0, 0));
  assert(NOME("Donation needed", "", 0, 0));
  assert(NOME("Join the Discord", "discord.gg/abc", 0, 0));
  assert(NOME("Unavailable", "", 0, 0));
  assert(NOME("Addon", "Não disponível no momento", 0, 0));
  assert(NOME("Addon", "nao disponivel", 0, 0));
  assert(NOME("Torrentio", "Error: token expired", 0, 0));
  assert(NOME("Addon", "Please configure the addon first", 0, 0));
  assert(NOME("Addon", "Install the addon on the website", 0, 0));
  assert(NOME("Addon", "Support us on Patreon / Ko-fi", 0, 0));
  assert(NOME("Addon", "Telegram: t.me/canal", 0, 0));
  assert(NOME("Addon", "Doação: apoie o projeto", 0, 0));
  assert(NOME("Addon", "Invalid API key", 0, 0));
  assert(NOME("Addon", "Note: Start...", 0, 0));
  assert(NOME("Embed69 - {}", "", 0, 0));
  assert(NOME("Meteor - Not configured", "", 0, 0));
  assert(NOME("Addon", "Please configure", 0, 0));
  // --- nome: negativos (filmes de verdade) ---
  assert(!NOME("Discord", "Discord.2019.1080p.WEB-DL.x265", 0, 0));       // token de video
  assert(!NOME("Addon", "Donation.2021.4K.HDR.mkv", 0, 0));
  assert(!NOME("Addon", "Support Your Local Gunfighter 720p", 0, 0));
  assert(!NOME("Addon", "Error 404", 1080, 0));                           // altura conhecida
  assert(!NOME("Addon", "Unavailable", 0, 1500));                         // tamanho conhecido
  assert(!NOME("Addon", "The Terror S01E01 12.4 GB", 0, 0));
  assert(!NOME("Torrentio", "Dune Part Two 2024 REMUX", 0, 0));
  assert(!NOME("Addon", "Terror na Cidade", 0, 0));                       // "error" nao dentro de palavra
  assert(!NOME("Addon", "Instalação Mortal", 0, 0));
  assert(!NOME("Addon", "", 0, 0));
  assert(!NOME("", "", 0, 0));
  assert(!NOME(NULL, NULL, 0, 0));
  puts("naovideo: ok");
  return 0;
}
