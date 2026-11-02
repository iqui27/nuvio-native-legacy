// Included by Jellyfin/Emby/Plex fake-server integration tests. File-only migration
// exercises the same persisted contract with pre-change adapters too.
static void playbackFile(const char *name, const char *prefs) {
  char *old = dados_ler(name), *line, *save;
  assert(old);
  char *updated = calloc(strlen(old) + strlen(prefs) + 2, 1); assert(updated);
  for (line = strtok_r(old, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
    if (!strncmp(line, "playDirect=", 11) || !strncmp(line, "playBitrate=", 12)) continue;
    strcat(updated, line); strcat(updated, "\n");
  }
  strcat(updated, prefs);
  assert(dados_gravar_leve(name, updated));
  memset(updated, 0, strlen(updated)); free(updated);
  memset(old, 0, strlen(old)); free(old);
}
#ifdef SERVER_PLAYBACK_PLEX
static void testarPlaybackPlex(void) {
  const char *name = "plex-p1.txt";
  PxConta c; int p;
  PxPlayback *pb = malloc(sizeof *pb); assert(pb);
  free(controle("refuse?media="));
  playbackFile(name, ""); plex_carregar(); px_setup_conta(&c, &p);
  assert(px_fontes(&c, NULL, "101", pb) == PX_OK && pb->n == 4);
  assert(strstr(pb->fonte[1].url, "maxVideoBitrate=20000"));
  playbackFile(name, "playDirect=1\nplayBitrate=5000000\n"); plex_carregar();
  unsigned g = px_setup_conta(&c, &p);
  assert(px_fontes(&c, NULL, "101", pb) == PX_OK && pb->n == 2);
  for (int i = 0; i < pb->n; i++) assert(pb->sessao[i].metodo == PX_METODO_DIRETO);
  assert(plex_fontes_pedir("px:01234567.101"));
  Stream *sources = NULL; int count, state;
  unsigned long long until = ms() + 5000;
  while ((state = plex_fontes_colher("px:01234567.101", &sources, &count)) == PX_FONTES_PENDENTE && ms() < until) dormir(10);
  assert(state == PX_FONTES_PRONTO && count == 2);
  char playing[4096]; snprintf(playing, sizeof playing, "%s", sources[0].url); free(sources);
  plex_reproducao_tick(playing, 1, 7200, 1);
  char *before = controle("events"); int stops = conta(before, "\"state\": \"stopped\""); free(before);
  assert(px_setup_aceitar(&c, p, g) && !px_setup_aceitar(&c, p, g));
  plex_reproducao_fim(playing, 2, 7200);
  until = ms() + 5000;
  while (plex_relatorios_pendentes() && ms() < until) dormir(10);
  char *after = controle("events"); assert(conta(after, "\"state\": \"stopped\"") == stops + 1); free(after);
  char *stored = dados_ler(name); assert(stored);
  assert(strstr(stored, "playDirect=1\n") && strstr(stored, "playBitrate=5000000\n"));
  memset(stored, 0, strlen(stored)); free(stored);
  playbackFile(name, "playDirect=0\nplayBitrate=5000000\n"); plex_carregar(); px_setup_conta(&c, &p);
  assert(px_fontes(&c, NULL, "101", pb) == PX_OK && pb->n == 4);
  for (int i = 0; i < pb->n; i++) if (pb->sessao[i].metodo != PX_METODO_DIRETO)
    assert(strstr(pb->fonte[i].url, "maxVideoBitrate=5000"));
  char *events = controle("events"); assert(strstr(events, "\"bitrate\": \"5000\"")); free(events);
  perfilAtivo = 78; plex_perfil_trocou(); px_setup_conta(&c, &p); assert(!c.token[0]);
  perfilAtivo = 1; plex_perfil_trocou(); px_setup_conta(&c, &p);
  assert(px_fontes(&c, NULL, "101", pb) == PX_OK && strstr(pb->fonte[1].url, "maxVideoBitrate=5000"));
  // The selected PMS identity resets the previous server's preferences.
  strcpy(c.servidorId, "deadbeef");
  char serverBase[320]; snprintf(serverBase, sizeof serverBase, "%s", c.base);
  assert(px_setup_manual(&c, serverBase, NULL) == PX_OK);
  assert(px_fontes(&c, NULL, "101", pb) == PX_OK && strstr(pb->fonte[1].url, "maxVideoBitrate=20000"));
  playbackFile(name, "playDirect=no\nplayBitrate=999999999999999999999999\n"); plex_carregar(); px_setup_conta(&c, &p);
  assert(px_fontes(&c, NULL, "101", pb) == PX_OK && pb->n == 4 && strstr(pb->fonte[1].url, "maxVideoBitrate=20000"));
  memset(&c, 0, sizeof c); memset(pb, 0, sizeof *pb); free(pb);
}
#else
static void testarPlaybackLocal(int emby) {
  const char *name = emby ? "emby-p1.txt" : "jellyfin-p1.txt";
  const char *item = emby ? "1001" : "00000000000000000000000c0ffee001";
  JfConta c; int p;
  JfPlayback *pb = malloc(sizeof *pb); assert(pb);
  // Legacy files retain both original and HLS sources.
  playbackFile(name, "");
  if (emby) emby_carregar(); else jellyfin_carregar();
  jf_setup_conta(emby, &c, &p);
  assert(jf_playbackinfo(&c, NULL, item, JF_BACKEND_WEBOS, pb) == JF_OK && pb->n > 1);
#ifdef SRV_JF_BITRATE_MAX
  assert(!jf_playback_hls(emby));
  char availableTarget[JFID_MAX]; snprintf(availableTarget, sizeof availableTarget, "%s%.8s.%s", emby ? "em:" : "jf:", c.servidorId, item);
  assert(emby ? emby_fontes_pedir(availableTarget) : jellyfin_fontes_pedir(availableTarget));
  Stream *available = NULL; int availableCount, availableState;
  unsigned long long availableUntil = ms() + 5000;
  while ((availableState = emby ? emby_fontes_colher(availableTarget, &available, &availableCount) : jellyfin_fontes_colher(availableTarget, &available, &availableCount)) == JF_FONTES_PENDENTE && ms() < availableUntil) dormir(10);
  assert(availableState == JF_FONTES_PRONTO && availableCount > 1); free(available);
  assert(jf_playback_hls(emby));
  unsigned availableGeneration = jf_setup_conta(emby, &c, &p);
  assert(jf_setup_aceitar(emby, &c, p, availableGeneration) && jf_playback_hls(emby));
#endif
  // The fake deliberately ignores flags: the client still removes all HLS.
  playbackFile(name, "playDirect=1\nplayBitrate=5000000\n");
  if (emby) emby_carregar(); else jellyfin_carregar();
  unsigned g = jf_setup_conta(emby, &c, &p);
  assert(jf_playbackinfo(&c, NULL, item, JF_BACKEND_WEBOS, pb) == JF_OK && pb->n == 1);
  assert(pb->sessao[0].metodo == JF_METODO_DIRETO);
  char *events = controle("events");
  assert(strstr(events, "\"direct\": true") && strstr(events, "\"stream\": false") &&
         strstr(events, "\"transcoding\": false") && strstr(events, "\"bitrate\": 5000000") &&
         strstr(events, "\"profileBitrate\": 5000000") && strstr(events, "\"staticBitrate\": 120000000"));
  free(events);
  char target[JFID_MAX]; snprintf(target, sizeof target, "%s%.8s.%s", emby ? "em:" : "jf:", c.servidorId, item);
  assert(emby ? emby_fontes_pedir(target) : jellyfin_fontes_pedir(target));
  Stream *sources = NULL; int count, state;
  unsigned long long until = ms() + 5000;
  while ((state = emby ? emby_fontes_colher(target, &sources, &count) : jellyfin_fontes_colher(target, &sources, &count)) == JF_FONTES_PENDENTE && ms() < until) dormir(10);
  assert(state == JF_FONTES_PRONTO && count == 1);
#ifdef SRV_JF_BITRATE_MAX
  assert(jf_playback_hls(emby)); // Previously observed capability survives a same-connection save.
#endif
  char playing[4096]; snprintf(playing, sizeof playing, "%s", sources[0].url); free(sources);
  if (emby) emby_reproducao_tick(playing, 1, 7200, 1); else jellyfin_reproducao_tick(playing, 1, 7200, 1);
  char *before = controle("events"); int stops = conta(before, "\"Stopped\""); free(before);
  assert(jf_setup_aceitar(emby, &c, p, g));
  if (emby) emby_reproducao_fim(playing, 2, 7200); else jellyfin_reproducao_fim(playing, 2, 7200);
  until = ms() + 5000;
  while ((emby ? emby_relatorios_pendentes() : jellyfin_relatorios_pendentes()) && ms() < until) dormir(10);
  char *after = controle("events"); assert(conta(after, "\"Stopped\"") == stops + 1); free(after);
  assert(!jf_setup_aceitar(emby, &c, p, g)); // stale UI snapshot rejected
  char *stored = dados_ler(name); assert(stored);
  assert(strstr(stored, "playDirect=1\n") && strstr(stored, "playBitrate=5000000\n"));
  memset(stored, 0, strlen(stored)); free(stored);
  perfilAtivo = 78;
  if (emby) emby_perfil_trocou(); else jellyfin_perfil_trocou();
  jf_setup_conta(emby, &c, &p); assert(!c.token[0]);
#ifdef SRV_JF_BITRATE_MAX
  assert(!jf_playback_hls(emby));
#endif
  perfilAtivo = 1;
  if (emby) emby_perfil_trocou(); else jellyfin_perfil_trocou();
  jf_setup_conta(emby, &c, &p);
  assert(jf_playbackinfo(&c, NULL, item, JF_BACKEND_WEBOS, pb) == JF_OK && pb->n == 1);
#ifdef SRV_JF_BITRATE_MAX
  assert(!jf_playback_hls(emby)); // A fresh Direct Play-only connection offers no HLS control.
#endif
  // A different server must not inherit the previous server's settings.
  strcpy(c.servidorId, "deadbeefdeadbeefdeadbeefdeadbeef");
  char serverBase[512]; snprintf(serverBase, sizeof serverBase, "%s", c.base);
  assert(jf_publico(serverBase, NULL, &c) == JF_OK);
  assert(jf_playbackinfo(&c, NULL, item, JF_BACKEND_WEBOS, pb) == JF_OK && pb->n > 1);
  // Invalid persisted input safely restores original negotiation ceilings.
  playbackFile(name, "playDirect=garbage\nplayBitrate=999999999999999999999999\n");
  if (emby) emby_carregar(); else jellyfin_carregar();
  jf_setup_conta(emby, &c, &p);
  assert(jf_playbackinfo(&c, NULL, item, JF_BACKEND_WEBOS, pb) == JF_OK && pb->n > 1);
  events = controle("events"); assert(strstr(events, "\"bitrate\": 120000000")); free(events);
  const char *query[] = {"VideoCodec=copy&AudioCodec=copy", "VideoCodec=copy&AudioCodec=aac&AllowAudioStreamCopy=false",
    "VideoCodec=h264&AllowVideoStreamCopy=false", "VideoCodec=h264&AudioCodec=aac"};
  const char *labels[] = {"Remux (HLS)", "Direct stream (HLS)", "Transcode (HLS)", "HLS"};
  for (int i = 0; i < 4; i++) {
    char fixture[768];
    snprintf(fixture, sizeof fixture, "{\"MediaSources\":[{\"Id\":\"m1\",\"SupportsDirectStream\":true,\"TranscodingUrl\":\"/videos/1/master.m3u8?%s\"}]}", query[i]);
    assert(jf_ler_playbackinfo(&c, item, fixture, strlen(fixture), pb) == JF_OK && pb->n == 1);
    assert(!strcmp(pb->fonte[0].rotulo, labels[i]));
  }
  const char *directOnly = "{\"MediaSources\":[{\"Id\":\"m1\",\"SupportsDirectPlay\":true,\"SupportsDirectStream\":false,\"SupportsTranscoding\":false,\"TranscodingUrl\":\"/videos/1/master.m3u8\"}]}";
  assert(jf_ler_playbackinfo(&c, item, directOnly, strlen(directOnly), pb) == JF_OK && pb->n == 1);
  assert(pb->sessao[0].metodo == JF_METODO_DIRETO);
  memset(&c, 0, sizeof c); memset(pb, 0, sizeof *pb); free(pb);
}
#endif
