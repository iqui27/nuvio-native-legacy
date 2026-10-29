// Included by the existing native detail fixture; shared renderer, offline data.
if (getenv("NUVIO_CHIPFONTE_PROOF")) {
  abrir(1,0,0,0);
  Stream sources[3]={{0}};
  for (int i=0;i<3;i++) {
    snprintf(sources[i].rotulo,sizeof sources[i].rotulo,"Project Hail Mary 4K Dolby Vision Atmos 7.1");
    snprintf(sources[i].arquivo,sizeof sources[i].arquivo,"Project.Hail.Mary.2160p.%d.mkv",i);
    snprintf(sources[i].provedor,sizeof sources[i].provedor,"AIOStreams");
    snprintf(sources[i].url,sizeof sources[i].url,"https://example.invalid/%d.mp4",i);
    sources[i].altura=2160; sources[i].dolbyVision=sources[i].dolbyAtmos=sources[i].mp4=1;
    sources[i].badges=badges_detectar(sources[i].rotulo);
    sources[i].tamanhoBytes=UINT64_C(19756849562)-i*UINT64_C(1073741824);
    sources[i].conferencia=i==2 ? FA_RUIM : FA_CONFERINDO;
  }
  stream_definir_alvo(itens[1].imdb); stream_definir_lista(sources,3);
  vt_montar(&vaiTocar,itens[1].imdb,0,0);
  focoVersao=1; versaoFoco=1; hVaiVis=56;
  quadros(30); gravar("chip-checking");
  assert(stream_falha_aplicar(0,stream_lista_geracao(),-1,sources[0].url));
  vt_montar(&vaiTocar,itens[1].imdb,0,0);
  quadros(30); gravar("chip-ready");
  stream_folha_nome(itens[1].titulo); stream_folha_abrir();
  quadros(100); gravar("sources-status");
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
  return 0;
}
