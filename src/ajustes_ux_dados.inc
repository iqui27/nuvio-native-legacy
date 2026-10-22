/* Dados da UX de Ajustes. Incluido depois de mudarValor(), sem rede, gravacao
 * ou aplicacao de efeitos. O padrao e a mesma lista imutavel que inicializa
 * valor[], nunca uma fotografia de preferencias ja carregadas do usuario. */

static int uxTemPadrao(int op) {
  if (op == AJ_XTREAM_CONEXOES || ajFalhaBackend(op) || ajLegendaPrincipal(op)) return 1;
  return op >= 0 && op < AJ_N &&
    (OPCOES[op].tipo == OP_ESCOLHA || OPCOES[op].tipo == OP_NUMERO) &&
    CHAVE[op] && CHAVE[op][0] && CHAVE[op][0] != '-' &&
    op != AJ_PERFIL_PESQ;
}

static int uxValorPadrao(int op) {
  if (op == AJ_SALVOS_DEST) return ajustes_fonte_resolver(valorPadrao[op], trakt_ativo(), simkl_ativo());
  return uxTemPadrao(op) ? valorPadrao[op] : 0;
}

/* Fileiras tem uma fonte propria. valor[] e apenas seu espelho na tela. */
static int uxValorAtual(int op) {
  if (op < 0 || op >= AJ_N) return 0;
  if (op == AJ_XTREAM_CONEXOES) return xtream_conexoes_modo();
  if (op == AJ_SALVOS_DEST) return ajustes_fonte_biblioteca();
  if(op==AJ_GUIA_CATEGORIAS || op==AJ_IPTV_ACOES || op==AJ_IPTV_EXCLUIR)return 0;
  if(ajIptvGerenciador(op))return ajIptvQuantos(ajIptvTipo(op))?ajIptvAtualTipo(ajIptvTipo(op)):1;
  if (ajLegendaPrincipal(op)) return ajLegendaValor(op);
  if (ajFalhaBackend(op)) return ajFalhaValor(op);
#ifndef NV_MINIATURAS
  if (op == AJ_SEEKR_LIGADO && !seekrChave[0]) return 1;
#endif
  return op == AJ_FIL_LIMITE ? fil_limite_gravado() : valor[op];
}

static int uxDiferente(int op) {
  return uxTemPadrao(op) && uxValorAtual(op) != uxValorPadrao(op);
}

/* O candidato pode ter alcance diferente do valor salvo. Espelha as excecoes
 * de ajustes_mesclar_blob/textoDoValor sem trocar valor[] para consulta-las.
 * Audio/legenda, inclusive "Da conta", continuam no conjunto dePerfil: a
 * escolha e guardada por perfil nesta TV, sem prometer que ja sincronizou. */
static const char *uxEscopoValor(int op, int v) {
  if (op == AJ_TEMA && v >= AJ_TEMA_DINAMICA) return "Só nesta TV";
  if (op == AJ_TMDB_IDIOMA && v == 0) return "Só nesta TV";
  return op >= 0 && op < AJ_N && dePerfil(op) ? "Conta/perfil" : "Só nesta TV";
}

static const char *uxEscopo(int op) {
  return uxEscopoValor(op, uxValorAtual(op));
}

/* Decodifica uma unidade UTF-8 sem ler depois do terminador. Sequencia
 * incompleta/invalida consome um byte, permitindo recuperar o restante. */
static size_t uxUtf8(const unsigned char *s, unsigned *cp) {
  unsigned c = s[0], v;
  size_t i, n;
  if (!c) { *cp = 0; return 0; }
  if (c < 0x80) { *cp = c; return 1; }
  if (c >= 0xc2 && c <= 0xdf) { n = 2; v = c & 0x1f; }
  else if (c >= 0xe0 && c <= 0xef) { n = 3; v = c & 0x0f; }
  else if (c >= 0xf0 && c <= 0xf4) { n = 4; v = c & 7; }
  else { *cp = 0xfffd; return 1; }
  for (i = 1; i < n; i++) {
    if (!s[i] || (s[i] & 0xc0) != 0x80) { *cp = 0xfffd; return 1; }
    v = (v << 6) | (s[i] & 0x3f);
  }
  if ((n == 3 && v < 0x800) || (n == 4 && v < 0x10000) ||
      (v >= 0xd800 && v <= 0xdfff) || v > 0x10ffff) {
    *cp = 0xfffd; return 1;
  }
  *cp = v;
  return n;
}

/* Mantem textos de resultado terminados e sem cortar um caractere UTF-8. */
static void uxTextoCopiar(char *dst, size_t tam, const char *src) {
  size_t p = 0;
  if (!dst || !tam) return;
  if (!src) src = "";
  while (*src) {
    unsigned cp;
    size_t n = uxUtf8((const unsigned char *)src, &cp);
    if (p + n >= tam) break;
    if (cp == 0xfffd) dst[p++] = '?';
    else { memcpy(dst + p, src, n); p += n; }
    src += n;
  }
  dst[p] = 0;
}

/* Renderiza qualquer candidato sem emprestar/mutar valor[]. Acoes e leituras
 * nunca usam textoLeitura(): ate o valor mascarado pode conter dados pessoais. */
static void uxValorTexto(int op, int v, char *buf, size_t tam) {
  const Opcao *o;
  const char *texto = "";
  if (!buf || !tam) return;
  buf[0] = 0;
  if (op < 0 || op >= AJ_N) return;
  if(op==AJ_GUIA_CATEGORIAS) {
    snprintf(buf,tam,"%s",v>=0 && v<uxNCategorias?uxCategorias[v].nome:i18n("Nenhuma"));return;
  }
  if(op==AJ_IPTV_CONTA){snprintf(buf,tam,"%s",ajIptvNome(v));return;}
  if(ajIptvGerenciador(op)) {
    int tipo=ajIptvTipo(op),n=ajIptvQuantos(tipo);
    const char *s=v==(n?n:1)?i18n(ajIptvAdicionarNome(tipo)):!n?i18n("Nenhuma"):ajIptvNome(ajIptvDoTipo(tipo,v));
    snprintf(buf,tam,"%s",s);return;
  }
  o = &OPCOES[op];
  if (o->tipo == OP_ACAO) texto = i18n("Abrir");
  else if (o->tipo == OP_LEITURA) texto = i18n("Ver detalhes");
  else if (op == AJ_FILTRO_TAM_MAX && v == 0) texto = i18n("Sem limite de tamanho");
  else if (op == AJ_HERO_TRAILER_ESPERA) {
    snprintf(buf, tam, i18n("%d,%d s"), v / 10, v % 10);
    return;
  } else if (op == AJ_FIL_LIMITE && fil_limite_ilimitado(v)) {
    texto = i18n("Ilimitado");
  } else if (o->tipo == OP_NUMERO) {
    snprintf(buf, tam, "%d%s", v, o->sufixo ? o->sufixo : "");
    return;
  } else if (op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2 || op == AJ_AUD_LINGUA || op == AJ_AUD_LINGUA2) {
    const char *cod = ling_opcao_codigo(v >= 0 && v < ling_opcao_n() ? v : 0);
    texto = !cod || !cod[0] ? "Da conta" : !strcmp(cod, "*") ? "Todas"
      : !strcmp(cod, "~") ? "Original do título" : ling_nome(cod);
    texto = i18n(texto);
  } else if (op == AJ_PLAYER_EXTERNO || op == AJ_PLAYER_ANIME) {
    texto = i18n(ppNome(op, v));
  } else if (op == AJ_SELOS_PACOTE) {
    texto = v > 0 && v <= selospacote_n() ? selospacote_nome(v - 1) : i18n("Do Nuvio");
  } else if (op == AJ_TEX_MB && v > 0 && v < o->n && texAcima(v)) {
    char t[160];
    texValorTexto(v, t, sizeof t);
    uxTextoCopiar(buf, tam, t);
    return;
  } else if (o->valores && o->n > 0) {
    /* Nao resolve Automatico nem o modo seguro: mostra o que foi salvo. */
    if (v < 0 || v >= o->n) v = 0;
    texto = i18n(o->valores[v] ? o->valores[v] : "");
  }
  uxTextoCopiar(buf, tam, texto);
}

/* Normalizacao PT independente do locale do sistema: maiusculas, acentos
 * precompostos e marcas combinantes. Outros alfabetos UTF-8 sao preservados. */
static void uxNormalizar(const char *src, char *dst, size_t tam) {
  size_t p = 0;
  int espaco = 0;
  if (!dst || !tam) return;
  if (!src) src = "";
  while (*src) {
    unsigned cp;
    char ascii = 0;
    size_t n = uxUtf8((const unsigned char *)src, &cp);
    if (cp >= 0x300 && cp <= 0x36f) { src += n; continue; }
    if (cp >= 'A' && cp <= 'Z') ascii = (char)(cp + ('a' - 'A'));
    else if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')) ascii = (char)cp;
    else if ((cp >= 0xc0 && cp <= 0xc5) || (cp >= 0xe0 && cp <= 0xe5)) ascii = 'a';
    else if (cp == 0xc7 || cp == 0xe7) ascii = 'c';
    else if ((cp >= 0xc8 && cp <= 0xcb) || (cp >= 0xe8 && cp <= 0xeb)) ascii = 'e';
    else if ((cp >= 0xcc && cp <= 0xcf) || (cp >= 0xec && cp <= 0xef)) ascii = 'i';
    else if (cp == 0xd1 || cp == 0xf1) ascii = 'n';
    else if ((cp >= 0xd2 && cp <= 0xd6) || (cp >= 0xf2 && cp <= 0xf6)) ascii = 'o';
    else if ((cp >= 0xd9 && cp <= 0xdc) || (cp >= 0xf9 && cp <= 0xfc)) ascii = 'u';
    else if (cp == 0xdd || cp == 0xfd || cp == 0xff) ascii = 'y';
    else if (cp < 0x80 || cp == 0xfffd || cp == 0xa0 || cp == 0xb7 ||
             (cp >= 0x2000 && cp <= 0x206f)) {
      espaco = p > 0;
      src += n;
      continue;
    }
    if (p + (espaco ? 1 : 0) + (ascii ? 1 : n) >= tam) break;
    if (espaco) dst[p++] = ' ';
    espaco = 0;
    if (ascii) dst[p++] = ascii;
    else { memcpy(dst + p, src, n); p += n; }
    src += n;
  }
  dst[p] = 0;
}

static const char *uxSinonimos(int op) {
  if (ajLegendaPrincipal(op) || op == AJ_LEG1_RESTAURAR || ajLegendaSecundaria(op))
    return "legenda estilo fonte tamanho cor opacidade fundo posicao borda contorno negrito atraso subtitle style font size color opacity position outline bold delay";
  switch (op) {
    case AJ_IDIOMA: return "idioma interface menus language portugues ingles espanhol";
    case AJ_AUD_LINGUA: return "idioma audio dublado dublagem original voz language";
    case AJ_AUD_LINGUA2: return "idioma audio segundo secundario dublado dublagem voz language second secondary";
    case AJ_LEG_LINGUA: return "idioma legenda legendado subtitle subtitles cc closed captions";
    case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA:
      return "legenda secundaria segunda dupla dual posicao topo junto empilhada tamanho cor fundo borda contorno estilo second subtitle position size color";
    case AJ_FONTE_IDIOMA_AUDIO: return "fonte automatica idioma audio dublado dublagem dual multi anime ingles japones preferir dub language";
    case AJ_FONTE_PRIORIDADE: case AJ_FONTE_HDR: return "fonte automatica prioridade qualidade equilibrio fluidez hdr dolby vision dv hdr10 sdr cor escolha melhor resolucao addon aiostreams ordem primeira order first";
    case AJ_LOGO_APP: return "logo marca simbolo novo classico icone identidade nuvio tv retro play gradiente splash";
    case AJ_ABERTURA: return "abertura splash inicio animacao zoom esmaecer fade direto rapido logo entrada boot arranque";
    case AJ_ESMAECER: return "esmaecer escurecer apagar tela parado ocioso inatividade protecao oled burn-in screensaver descanso tempo minutos dim";
    case AJ_MANTER_PRAZO: return "tempo video pronto sem limite pausa retencao ilimitado minutos memoria";
    case AJ_RETOMADA_MORNA: return "retomada rapida morna cache disco quadro conexao ilha retomar";
    case AJ_MANTER_VIDEO: return "manter video pronto sair retomar rapido instantaneo pausado reter memoria pipeline ilha trailer avancado";
    case AJ_DESCANSO_ESTILO: return "descanso screensaver protetor vitrine relogio hora filmes escurecer estilo";
    case AJ_DESCANSO_FONTE: return "descanso vitrine titulos lista continuar catalogo filmes series";
    case AJ_PROPORCAO_PADRAO: return "proporcao padrao aspecto aspect ratio zoom original recortar esticar preencher cinema tarja barras pretas formato tela tamanho imagem player";
    case AJ_HDR_UI: return "brilho interface hdr dolby vision dv branco esbranquicado leitoso vidro cor player legenda";
    case AJ_BRILHO_PLAYER: return "brilho interface player controles barra escurecer forte claro oled hdr osd luz";
    case AJ_DICASCONTROLE: return "controle dicas atalhos remote tips CH+ CH- toque";
    case AJ_NOVIDADES20: return "novidades novo versao 2.0 tour guia o que mudou mudancas whats new";
    case AJ_DTS_AC3: return "dts truehd convertido ac3 aac dolby digital 5.1 surround estereo audio som barra de som receiver arc earc conversao lg samsung desligado converted stereo off";
    case AJ_PS_CONTINUAR: return "continuar escolha perfil cartao card tela abertura inicial assistindo retomar remover esconder desligar quem esta assistindo continue watching profile picker";
    case AJ_DV_MKV: return "dolby vision dv dovi mkv matroska perfil 8 5 hdr hdr10 experimental imagem cores";
    case AJ_CW_RETIDO_TAMBEM: return "continuar assistindo tambem ilha retomar agora fileira duplicar repetir aparecer dois lugares titulo continue watching also island resume";
    case AJ_HIST_CONTA: return "historico historico vistos progresso conta nuvio enviar scrobble simkl trakt perfil sincronizar sync history watched";
    case AJ_SOCIAL: return "social amigos amigo atividade recomendar recomendacoes friends activity recommend presenca desligar remover";
    case AJ_ABAS_ESTILO: return "abas estilo colecao streaming chips menu suspenso dropdown barra central paginas carrossel tabs style collection pages carousel";
    case AJ_LEG_CONTORNO_COR: return "legenda contorno sombra borda cor preto branco cinza outline edge shadow subtitle color";
    case AJ_MENU_TOPO: return "menu barra topo alto cima lateral esquerda rail posicao navegacao top bar toolbar sidebar left position";
    case AJ_EP_TAMANHO: return "tamanho episodio cards cartao moderno pequeno medio grande serie episode card size small medium large";
    case AJ_ABERTURA_ADDON: return "addon add-on provedor fonte nome abertura cinema tela carregando tentativa provider name opening source";
    case AJ_TITULO_FIL: return "titulo fileira fileiras estilo classico ilha editorial acento pilula vidro home row title style island accent classic";
    case AJ_VIDRO_QUALIDADE: return "qualidade vidro automatica desempenho alta fumaca esfumacado desfoque fosco glass quality automatic performance high smoked blur";
    case AJ_FUNDO_DIN: case AJ_FUNDO_DIN_COR: return "fundo dinamica cinza cor imersivo apple tv background dynamic gray color immersive";
    case AJ_ABERTURA_ESTILO: return "estilo tela abertura carregando logo centro canto esquerda anel opening style center corner";
    case AJ_ABERTURA_FUNDO: return "fundo arte backdrop imagem titulo abertura cinema tela carregando desligado nitida borrada desfocada preto opening backdrop blurred sharp off";
    case AJ_TELA_ABERTURA: return "tela abertura abrindo carregando loading cinema detalhada fonte play transicao opening source screen";
    case AJ_PLR_CLASSIF: return "classificacao indicativa idade faixa etaria 16 12 18 livre guia parental avisos violencia nudez player content rating age parental guide";
    case AJ_APOIAR: return "apoiar apoio doar doacao patreon kofi ko-fi cafe contribuir ajudar projeto qr support donate";
    case AJ_BUSCA_NUVIO: return "busca pesquisa nuvio catalogo resultados primeiro ultimo desligar ocultar mover baixo aiometadata addon search results first last disable";
    case AJ_BUSCA_ORIGEM: return "busca pesquisa fonte origem de from rotulo legenda grupo resultados ocultar esconder mostrar source label caption";
    case AJ_BUSCA_CINEMETA: return "busca pesquisa cinemeta catalogo nuvio addon excluir desligar resultados procurar search";
    case AJ_ENQUETES: return "enquete enquetes votacao votar pesquisa opiniao notificacao notificacoes aviso convite ilha relogio poll";
    case AJ_LEG_LINGUA2: return "idioma legenda secundaria segunda dupla dual second subtitle subtitles";
    case AJ_TRAILER_ZOOM_TPK: return "trailer zoom barras pretas tarja ampliar recorte tela preta samsung experimental";
    case AJ_TECLADO_ANDROID: return "teclado android app grade ime sistema digitar keyboard virtual tela";
    case AJ_MIC_BUSCA: return "microfone mic voz falar ditar ditado busca buscar teclado samsung controle remoto voice";
    case AJ_FONTE_ORDEM: case AJ_FONTE_ORDEM_USO:
      return "ordem ordenar subir descer prioridade add-on addon plugin primeiro desempate estrita auto-play autoplay automatico order priority tie strict";
    case AJ_CW_PROXIMO: return "proximo episodio continuar assistindo a seguir next up fileira retomada esconder series so em andamento continue watching";
    case AJ_LAYOUT_AJUSTES: return "layout arranjo lista painel grade linhas uma embaixo da outra antigo anterior visual ajustes configuracoes list rows settings classic";
    case AJ_PULAR_ABERTURA: case AJ_PULAR_RESUMO: return "pular abertura resumo intro recap anteriormente automatico sozinho creditos skip auto";
    case AJ_PULAR_CREDITOS: return "pular creditos automatico sozinho final encerramento skip credits auto ending";
    case AJ_PROXIMO_AUTO: case AJ_PROXIMO_CONTAGEM: return "proximo episodio automatico autoplay creditos contagem netflix sozinho next episode auto play countdown";
    case AJ_CW_MINIMO: return "minimo continuar assistindo porcentagem percentual entrar fileira retomada progresso quanto assistir comecou piso minimum percent continue watching threshold row";
    case AJ_FONTE_PREPARAR: return "preparar fonte ao abrir titulo antecipar resolver antecipado play instantaneo inicio rapido debrid painel arquivo prepare source ahead instant start";
    case AJ_COLECAO_EXIBICAO: return "colecao colecoes exibicao padrao automatico grade linha tempo ranking saga decadas mural vitrine global collection collections default view automatic grid timeline showcase";
    case AJ_LOG_REDE: return "registro rede local pagina log ip navegador local network log page browser seek";
    case AJ_LINK_REUSAR: case AJ_LINK_PRAZO: return "reusar ultimo link sem busca retomar validade cache horas reuse last link cache duration";
    case AJ_FONTE_GRUPO: case AJ_FONTE_GRUPO_SESSOES: return "grupo bingegroup binge proximo episodio mesma fonte lembrar fonte escolhida lancamento release sessoes reusar prefer binge group remember picked source same release next episode reuse sessions";
    case AJ_FONTE_PRECARREGAR: return "precarregar video buffer episodio proximo creditos preload video next episode";
    case AJ_FONTE_CONFERIR_VARIAS: return "conferir varias fontes ao mesmo tempo paralelo paralela simultaneo verificar verificacao automatico rapido inicio debrid painel arquivos parallel check several sources speed";
    case AJ_FONTE_AQUECER: return "aquecer conexoes conexao rede dns tls antecipar preparar abrir titulo inicio rapido lento demora warm preconnect connection network speed";
    case AJ_FONTE_TOCAR_CONFERINDO: return "tocar enquanto confere verificar verificacao abrir player mais rapido rapido inicio demora lento comecar play paralelo start fast verify while playing speed";
    case AJ_FONTE_ESCOPO: case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: case AJ_FONTE_OUTROS:
      return "auto-play autoplay automatico fonte addon add-on plugin permitidos permitido escopo idioma espanhol spanish preferidos allowed addons plugins source scope";
    case AJ_FONTE_REGEX: case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO:
      return "regex expressao regular padrao filtro nome idioma espanhol latino castellano multi dual remux auto-play autoplay regex match pattern";
    case AJ_TAM_MAX: case AJ_TAM_MIN: return "tamanho maximo minimo gb gigabyte arquivo franquia dados internet limite cota plano economizar fonte automatica size quota data cap";
    case AJ_FILTROS_FONTES: case AJ_FILTRO_TAM_MAX: case AJ_FILTRO_GRUPOS:
      return "filtros excluir exclusao resolucao qualidade tamanho tags visuais hdr dolby vision audio canais encode idiomas linguas grupos lancamento antes limite links filter exclude resolution quality size visual audio channels codec encode language release group";
    case AJ_FALHA_PROVEDORES: case AJ_FALHA_PROVEDOR_ORDEM: case AJ_FALHA_RD_LIMITE: case AJ_FALHA_TB_LIMITE: case AJ_FALHA_PM_LIMITE: case AJ_FALHA_AD_LIMITE:
      return "falha erro provedor debrid trocar ordem prioridade limite links timeout failover fallback retry real-debrid torbox premiumize alldebrid";
    case AJ_FALHA_ADDONS: case AJ_FALHA_ADDON_ORDEM: case AJ_FALHA_ADDON_LIMITE:
      return "falha erro provedor debrid addon add-on trocar ordem prioridade limite links timeout failover fallback retry";
    case AJ_FONTE_ORDEM_ADDON: return "ordem fontes qualidade addon aiostreams resolucao tamanho instalacao source order quality";
    case AJ_P2P_LIMITE: return "limite espaco p2p torrent armazenamento disco cheio memoria buffer cache tamanho maximo gb space limit storage full";
    case AJ_P2P_DESTINO: return "cache p2p pendrive usb armazenamento interno disco externo flash drive storage location";
    case AJ_ESTAT_TAMANHO: return "estatisticas ao vivo stats nerds tamanho completo compacto minimo pequeno linha canto fixar velocidade buffer painel live statistics size full compact minimal";
    case AJ_ESTAT_LADO: return "estatisticas ao vivo stats nerds lado direita esquerda direito esquerdo posicao painel live statistics side left right position";
    case AJ_PLAYER_ANIME: return "player anime externo igual vlc mx outro app video kitsu mal";
    case AJ_PLAYER_EXTERNO: return "player externo interno perguntar vlc mx outro app video external ask";
    case AJ_BUFFER: return "buffer tamanho memoria 64 128 256 mb travando carregando rede instavel stats nerds";
    case AJ_CACHE_SEEK: return "cache seek buffer disco armazenamento voltar avancar travando carregando";
    case AJ_TMDB_IDIOMA: return "idioma metadados titulo sinopse traducao tmdb";
    case AJ_HERO_GENERO: return "genero destaque hero logo titulo categoria esconder mostrar card genre";
    case AJ_HERO_SINOPSE: return "sinopse destaque hero texto descricao resumo esconder mostrar synopsis description";
    case AJ_HERO_SINOPSE_TAM: return "sinopse tamanho destaque hero texto grande normal synopsis size";
    case AJ_HERO_NOTAS: return "nota notas imdb destaque hero avaliacao rating ratings esconder mostrar";
    case AJ_HERO_ANO: return "ano destaque hero lancamento year data esconder mostrar";
    case AJ_HERO_DURACAO: return "duracao tempo destaque hero runtime minutos horas esconder mostrar";
    case AJ_HERO_TRAILER: return "trailer trailers destaque topo hero autoplay automatico";
    case AJ_FOCO_TRAILER: return "trailer trailers cartaz poster foco autoplay";
    case AJ_DET_TRAILER_AUTO: return "trailer trailers pagina titulo detalhes autoplay automatico";
    case AJ_DET_TRAILER: return "trailer trailers botao manual";
    case AJ_HERO_TRAILER_SOM: case AJ_DET_TRAILER_SOM:
      return "trailer trailers som audio mudo silencio volume";
    case AJ_HERO_TRAILER_ESPERA: return "trailer trailers espera tempo demora atraso";
    case AJ_LEG_FORCADA: return "legenda forcada forcadas forced letreiros placas audio idioma automatica subtitle";
    case AJ_LEG_SYNC_AUTO: case AJ_LEG_SYNC_AUDIO: return "sincronia sincronizar legenda audio fala falas atraso automatico sync subtitle speech";
    case AJ_AVANCADAS: return "avancado avancada avancados avancadas tecnico tecnica opcoes ocultas mostrar esconder advanced";
    case AJ_LOGO_TRAILER: return "trailer trailers logo marca titulo oled queimar burn-in esconder canto";
    case AJ_TRAILER_FONTE: return "trailer trailers fonte origem provedor apple imdb youtube";
    case AJ_TRAILER_AUDIO: return "trailer trailers audio idioma dublado dublagem original";
    case AJ_TRAILER_QUAL: return "trailer trailers qualidade resolucao definicao";
    case AJ_TRAILER_ASPECTO: return "trailer trailers formato proporcao zoom tarja";
    case AJ_VIDRO: return "lento lenta lentidao travando trava desempenho vidro blur transparencia";
    case AJ_RESOLUCAO: return "lento lenta lentidao travando desempenho resolucao 4k 1080p 720p";
    case AJ_QUALIDADE_IMG: return "lento lenta lentidao travando desempenho imagem cartaz poster memoria ram";
    case AJ_FIL_LIMITE: case AJ_ITENS_FILEIRA:
      return "lento lenta lentidao travando desempenho fileira catalogo home memoria ram";
    case AJ_GPU_EFEITOS: return "lento lenta lentidao travando desempenho gpu efeitos brilho blur desfoque";
    case AJ_TEX_MB: case AJ_ESPACO: return "lento lenta lentidao travando memoria ram textura cache imagens";
    case AJ_DIAGNOSTICO: case AJ_VELOCIDADE:
      return "lento lenta lentidao travando trava falha erro desempenho diagnostico rede internet";
    case AJ_SEEKR_LIGADO: case AJ_SEEKR_FITA: case AJ_SEEKR_AJUSTE:
      return "seekr miniatura barra tempo previa avanco retrocesso";
    default: return "";
  }
}

/* Onde a opcao morava ANTES da reorganizacao de 2.0.2 (categoria e bloco
 * antigos). Entra no indice da busca, em portugues e traduzido, para quem
 * procura pelo nome que ja conhecia: "contas", "chaves", "escolha da fonte". */
static const char *uxCaminhoAntigo(int op) {
  switch (op) {
    case AJ_ADDONS: case AJ_PLUGINS: case AJ_ADDONS_PRINCIPAL:
      return "Contas e serviços";
    case AJ_PROVEDORES_ESCOPO: case AJ_DEBRID_RD: case AJ_DEBRID_TB: case AJ_DEBRID_PM: case AJ_DEBRID_AD: case AJ_DEBRID_AD_TESTAR:
    case AJ_DEBRID_RD_TESTAR: case AJ_DEBRID_TB_TESTAR: case AJ_DEBRID_PM_TESTAR:
    case AJ_MDB_CHAVE: case AJ_FANART_CHAVE:
      return "Chaves";
    case AJ_P2P_LIGADO: case AJ_P2P_URL: case AJ_P2P_TESTAR: case AJ_P2P_LIMITE: case AJ_P2P_DESTINO:
    case AJ_JF_LIGADO: case AJ_JF_SERVIDOR: case AJ_JF_ENTRAR: case AJ_JF_SAIR:
    case AJ_EM_SERVIDOR: case AJ_EM_ENTRAR: case AJ_EM_SAIR:
    case AJ_PX_ENTRAR: case AJ_PX_SERVIDOR: case AJ_PX_SAIR:
    case AJ_PERFIL_ATIVO: case AJ_SYNC: case AJ_PERFIL_PESQ: case AJ_PERFIL_EDITAR: case AJ_SAIR:
    case AJ_TRAKT: case AJ_SIMKL: case AJ_DISCORD: case AJ_SALVOS_DEST: case AJ_CW_FONTE: case AJ_CW_PROXIMO: case AJ_ENQUETES:
    case AJ_HIST_CONTA: case AJ_SOCIAL:
      return "Contas e serviços";
    case AJ_FONTE_MANUAL: case AJ_FONTE_AUTO: case AJ_FONTE_REPOR: case AJ_FONTE_PRIORIDADE: case AJ_FONTE_TOCAR_CONFERINDO: case AJ_FONTE_AQUECER: case AJ_FONTE_CONFERIR_VARIAS: case AJ_FONTE_PREPARAR: case AJ_FONTE_PRECARREGAR:
    case AJ_FONTE_GRUPO: case AJ_FONTE_GRUPO_SESSOES:
    case AJ_LINK_REUSAR: case AJ_LINK_PRAZO:
    case AJ_FONTE_HDR: case AJ_FONTE_PRAZO: case AJ_FONTE_TEXTO: case AJ_FONTE_ORDEM_ADDON: case AJ_TAM_MAX: case AJ_TAM_MIN:
    case AJ_FONTE_ESCOPO: case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: case AJ_FONTE_OUTROS:
    case AJ_FONTE_REGEX: case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO:
    case AJ_FONTE_ORDEM: case AJ_FONTE_ORDEM_USO: case AJ_FONTE_IDIOMA_AUDIO:
    case AJ_SELOS_CORES: case AJ_SELOS_PACOTE: case AJ_SELOS_PACOTE_ADD: case AJ_SELOS_PACOTE_REM:
      return "Fontes e addons";   // 2.0.2; desde a 2.0.3 a escolha da fonte mora em Reproducao
    case AJ_BUSCA_CINEMETA: case AJ_BUSCA_NUVIO: case AJ_BUSCA_ORIGEM: return "Busca";
    case AJ_AVANCADAS: return "Aparência";
    case AJ_IDIOMA: return "Idioma";
    // 2.0.3: Cartazes e arte + Trailers viraram Cartazes e trailers; Desempenho
    // desta TV virou Esta TV; Conta e perfis virou Conta e servicos.
    case AJ_HERO_TRAILER: case AJ_FOCO_TRAILER: case AJ_DET_TRAILER_AUTO: case AJ_DET_TRAILER:
    case AJ_HERO_TRAILER_SOM: case AJ_DET_TRAILER_SOM: case AJ_TRAILER_FONTE: case AJ_HERO_TRAILER_ESPERA:
    case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO: case AJ_TRAILER_ZOOM_TPK: case AJ_LOGO_TRAILER:  case AJ_TRAILER_AUDIO:
      return "Trailers";
    case AJ_MENU_CARTAZ: case AJ_EXPANDIR: case AJ_EXPANDIR_ATRASO: case AJ_BORDA_FOCO: case AJ_GRAD_CLASSICO:
    case AJ_LARGURA_DP: case AJ_RAIO_DP: case AJ_PROF: case AJ_PROF_BORDA: case AJ_PROF_BRILHO:
    case AJ_PROF_COBERTURA: case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS: case AJ_PROF_ELENCO:
    case AJ_PROF_TRAILERS: case AJ_HERO_FUNDO: case AJ_HERO_ARTE_DIF: case AJ_POSTER_PROV:
    case AJ_ADDON_POSTER: case AJ_ADDON_FUNDO: case AJ_ADDON_LOGO: case AJ_COL_ARTE_CONTA:
    case AJ_POSTER_INST: case AJ_POSTER_TOKEN: case AJ_POSTER_EXTRA: case AJ_POSTER_CHAVE:
    case AJ_POSTER_MODELO: case AJ_POSTER_TESTAR:
      return "Cartazes e arte";
    case AJ_AMPLIAR_UI: case AJ_AMPLIACAO_ALGO: case AJ_AMPLIACAO_NITIDEZ:
    case AJ_RESOLUCAO: case AJ_QUALIDADE_IMG: case AJ_GPU_EFEITOS: case AJ_NAV_RAPIDA: case AJ_MEDIDOR:
    case AJ_DIAGNOSTICO: case AJ_VELOCIDADE: case AJ_ESPACO: case AJ_TEX_MB:
      return "Desempenho desta TV";
    default: return "";
  }
}

static void uxIndiceAcrescentar(char *indice, size_t tam, const char *texto) {
  char normal[1536];
  size_t usado = strlen(indice), n;
  uxNormalizar(texto, normal, sizeof normal);
  n = strlen(normal);
  if (!n || usado + 2 >= tam) return;
  if (usado) indice[usado++] = ' ';
  uxTextoCopiar(indice + usado, tam - usado, normal);
}

static int uxContem(const char *texto, const char *termo) {
  const char *p = texto;
  size_t n = strlen(termo);
  if (!n) return 0;
  while ((p = strstr(p, termo))) {
    /* Siglas curtas como CC nao devem casar dentro de outra palavra. */
    if (n > 2 || ((p == texto || p[-1] == ' ') && (!p[n] || p[n] == ' '))) return 1;
    p++;
  }
  return 0;
}

/* Desempate dentro da mesma faixa: quem traz o termo mais cedo vem antes
 * ("idioma" acha "Idioma do app" antes de "Preferir o idioma do audio"). */
static int uxPosicao(const char *texto, const char *termo) {
  const char *p = strstr(texto, termo);
  int pos = p ? (int)(p - texto) : 99;
  return pos > 99 ? 99 : pos;
}

static int uxTodasPalavras(const char *texto, const char *consulta) {
  const char *p = consulta;
  while (*p) {
    char termo[256];
    size_t n = strcspn(p, " ");
    if (n >= sizeof termo) return 0;
    memcpy(termo, p, n); termo[n] = 0;
    if (!uxContem(texto, termo)) return 0;
    p += n;
    if (*p) p++;
  }
  return 1;
}

int ajustes_buscar(const char *consulta, AjusteBuscaResultado *resultados, int capacidade) {
  struct UxBuscaItem { int op, pontos; const char *secao, *grupo; } encontrados[AJ_N];
  unsigned char visto[AJ_N] = {0};
  const char *secao = "", *grupo = "";
  char busca[256];
  int i, total = 0;
  if (!resultados || capacidade <= 0) return 0;
  uxNormalizar(consulta, busca, sizeof busca);
  if (!busca[0]) return 0;
  for (i = 0; i < AJ_N_TELA; i++) {
    const Item *item = &TELA[i];
    char indice[4096] = "", titulo[320], tituloPt[320], aliases[768];
    int op, pontos, j;
    if (item->tipo == IT_SEC) { secao = item->titulo; grupo = ""; continue; }
    if (item->tipo == IT_GRP || item->tipo == IT_ROT) { grupo = item->titulo; continue; }
    if (item->tipo != IT_OPC) continue;
    op = item->op;
    if (op < 0 || op >= AJ_N || visto[op]) continue;
    if (!ajOpcaoDisponivel(op)) continue;
    visto[op] = 1;
    if (op == AJ_ICONE_APP && !apoiador_ativo()) continue;
    if ((op == AJ_PERFIL_PESQ || op == AJ_PERFIL_EDITAR) && !recomenda_ativo()) continue;
    if (op == AJ_BUSCA_CINEMETA && valor[op] == 0 && !cinemetaInstalado()) continue;
    uxNormalizar(i18n(rotuloOpcao(op)), titulo, sizeof titulo);
    uxNormalizar(rotuloOpcao(op), tituloPt, sizeof tituloPt);
    uxNormalizar(uxSinonimos(op), aliases, sizeof aliases);
    uxIndiceAcrescentar(indice, sizeof indice, titulo);
    uxIndiceAcrescentar(indice, sizeof indice, tituloPt);
    uxIndiceAcrescentar(indice, sizeof indice, aliases);
    // 2.0.3: o nome de antes da linha (ROTULO NOVO), em portugues e traduzido.
    if (rotuloTecnico(op)) {
      uxIndiceAcrescentar(indice, sizeof indice, rotuloTecnico(op));
      uxIndiceAcrescentar(indice, sizeof indice, i18n(rotuloTecnico(op)));
    }
    uxIndiceAcrescentar(indice, sizeof indice, uxCaminhoAntigo(op));
    uxIndiceAcrescentar(indice, sizeof indice, i18n(uxCaminhoAntigo(op)));
    uxIndiceAcrescentar(indice, sizeof indice, secao);
    uxIndiceAcrescentar(indice, sizeof indice, i18n(secao));
    uxIndiceAcrescentar(indice, sizeof indice, grupo);
    uxIndiceAcrescentar(indice, sizeof indice, i18n(grupo));
    /* Lista positiva: ajudaOpcao(AJ_XTREAM_USUARIO) contem dado da conta.
     * Acoes/leituras entram so por rotulo/caminho; nao tocar seus valores. */
    if (uxTemPadrao(op) || op == AJ_FIL_LIMITE) {
      uxIndiceAcrescentar(indice, sizeof indice, ajudaOpcao(op));
      uxIndiceAcrescentar(indice, sizeof indice, i18n(ajudaOpcao(op)));
      if (OPCOES[op].tipo == OP_ESCOLHA) {
        int n = op == AJ_AUD_LINGUA || op == AJ_AUD_LINGUA2 || op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2 ? ling_opcao_n() : nValores(op);
        for (j = 0; j < n; j++) {
          char texto[160];
          uxValorTexto(op, j, texto, sizeof texto);
          uxIndiceAcrescentar(indice, sizeof indice, texto);
        }
      }
    }
    if (!uxTodasPalavras(indice, busca)) continue;
    pontos = !strcmp(titulo, busca) || !strcmp(tituloPt, busca) ? 1000
      : uxContem(titulo, busca) || uxContem(tituloPt, busca)
        ? 800 - (uxPosicao(titulo, busca) < uxPosicao(tituloPt, busca) ? uxPosicao(titulo, busca) : uxPosicao(tituloPt, busca))
      : uxContem(aliases, busca) ? 600 - uxPosicao(aliases, busca)
      : uxTodasPalavras(titulo, busca) || uxTodasPalavras(tituloPt, busca) ? 500 : 100;
    j = total;
    while (j > 0 && encontrados[j - 1].pontos < pontos) {
      encontrados[j] = encontrados[j - 1]; j--;
    }
    encontrados[j] = (struct UxBuscaItem){op, pontos, secao, grupo};
    total++;
  }
  if (total > capacidade) total = capacidade;
  for (i = 0; i < total; i++) {
    const struct UxBuscaItem *item = &encontrados[i];
    AjusteBuscaResultado *r = &resultados[i];
    char caminho[512];
    memset(r, 0, sizeof *r);
    r->op = item->op;
    uxTextoCopiar(r->titulo, sizeof r->titulo, i18n(rotuloOpcao(r->op)));
    if (item->grupo[0]) snprintf(caminho, sizeof caminho, "%s › %s", i18n(item->secao), i18n(item->grupo));
    else snprintf(caminho, sizeof caminho, "%s", i18n(item->secao));
    uxTextoCopiar(r->caminho, sizeof r->caminho, caminho);
    uxValorTexto(r->op, uxValorAtual(r->op), r->valor, sizeof r->valor);
    r->avancado = uxAvancada(r->op);
    r->bloqueado = inativa(r->op);
    { int k;
      for (k = 0; k < AJ_N_TELA; k++)
        if (TELA[k].tipo == IT_OPC && TELA[k].op == r->op) {
          uxTextoCopiar(r->icone, sizeof r->icone, TELA[secIni[secDoItem[k]]].icone);
          break;
        } }
    uxTextoCopiar(r->ajuda, sizeof r->ajuda, i18n(ajudaOpcao(r->op)));
  }
  return total;
}
