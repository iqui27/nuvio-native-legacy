  0, 0, 0,          /* qualidade, DV, Atmos */
  0,                /* idioma de legenda: 0 = seguir a conta */
  // AUDIO NO ORIGINAL POR PADRAO (LING_OPC_ORIGINAL, o ULTIMO item de
  // OPCOES_COD em linguas.c — ele fica no fim porque este arquivo grava o
  // INDICE da opcao, e inserir no meio reinterpreta ajustes.txt ja salvo).
  // Pedido do dono: "o que eu quero que seja padrao e tocar o audio na
  // linguagem original". Antes era 0 = "Da conta", e uma conta com "Portugues"
  // salvo fazia o app pular para a dublagem sem ninguem ter pedido naquele
  // filme.
  //
  // SO VALE PARA INSTALACAO NOVA: quem ja tem ajustes.txt continua com o que
  // esta gravado la, porque o arquivo ganha do padrao — de proposito, senao
  // toda atualizacao desfaria escolha de usuario.
  LING_OPC_ORIGINAL, /* idioma de audio: Original do titulo */
  0,                /* painel ao pausar: ligado (o default do web) */
  1,                /* escolher a fonte ao reproduzir: DESLIGADO (V_LIGA: 1 = Desligado) */
  0,                /* fonte automatica: melhor fonte (a regra de sempre) */
  2,                /* outra fonte se falhar: ate 2 (eram 7 fixas ate a 1.4.3) */
  0,                /* texto das fontes: do Nuvio */

  0,                /* posteres deitados: LIGADO (perfil do dono; fabrica: desligado) */
  0,                /* fundo em tela cheia: LIGADO (perfil; fabrica: desligado) */
  0,                /* background do hero: seleção automática */
  1,                /* destaque com outra arte: DESLIGADO (mesma foto do card, 19/09) */
  1,                /* trailer no destaque: DESLIGADO (V_LIGA: 1 = Desligado) */

  FIL_LIMITE_PADRAO,/* limite de fileiras: 7, o pedido do dono (espelho de fileiras.c) */
  0,                /* ordenar fileiras: acao */
  // AQUI HAVIA `1, /* resetar foco ao iniciar */`, UMA OPCAO QUE NAO EXISTE
  // no enum (entrou em 876742e so neste vetor). Esta lista e POSICIONAL: cada
  // padrao de AJ_RAIL ate AJ_RESOLUCAO caia na opcao SEGUINTE, e so voltava
  // a alinhar porque faltava o de AJ_TEMA la embaixo. Quem instalava do zero
  // nascia com barra fixa, barra moderna ligada, animacoes reduzidas e o
  // cartao com largura 0 e ARREDONDAMENTO 126 dp — a "pilula" que o dono viu
  // na janela do tests/heroarte_shot.sh (22/09), que roda sem ajustes.txt.
  // Quem ja tem ajustes.txt nao muda: o arquivo ganha do padrao.
  // tests/ajustes_padroes.sh confere os padroes desta faixa um a um.

  0,                /* barra lateral: recolhida (perfil; fabrica: fixa) */
  1,                /* barra lateral moderna: desligada */
  0,                /* desfoque da barra moderna: ligado (perfil) */
  0,                /* mostrar destaque: ligado */
  0,                /* catalogos do destaque: leitura */
  0,                /* fundo da escolha de perfil: automatico (padrao de fabrica, #90) */
  0,                /* local do descobrir: na busca */
  // DESLIGADO por padrao: o cartaz ja traz o titulo impresso na arte, e repetir
  // o nome logo abaixo e a mesma informacao duas vezes ocupando altura de
  // fileira. Continua sendo ajuste — quem quiser o rotulo liga em Ajustes.
  1,                /* rotulos nos posteres: desligado */
  0,                /* nome do addon: ligado */
  0,                /* tipo de conteudo: ligado */
  1,                /* ocultar nao lancados: desligado */
  0,                /* avaliacoes gerais: mostrar (SHOW_ALL) */
  1,                /* gradiente de foco classico: desligado */

  0,                /* continuar assistindo: ligado */
  0,                /* OK no card: retomar (cwOkLocal; 1 = abrir pagina) */
  0,                /* fonte do continuar: ambas (o comportamento de sempre) */
  0,                /* estilo: card */
  0,                /* miniatura do episodio: ligada */
  1,                /* desfocar proximo: desligado */
  0,                /* proximo do episodio mais alto: ligado */
  0,                /* mostrar nao exibidos: ligado */
  0,                /* ordenacao: padrao */

  1,                /* desfocar nao assistidos: desligado */
  0,                /* botao de trailer: ligado */
  0,                /* metadados externos: ligado */
  0,                /* data completa: ligada */
  100,              /* escurecimento do fundo do titulo: vinheta inteira */
  0,                /* trailer automatico na pagina de titulo: ligado */
  0,                /* qualidade do trailer: maxima */
  0,                /* proporcao do trailer: zoom cinema */
  0,                /* fonte do trailer: automatico (Apple -> IMDb -> YouTube) */

  0,                /* expandir poster ao focar: ligado (DEFAULT do web) */
  3,                /* atraso: 3s */
  1,                /* navegacao horizontal rapida: desligada (fabrica) */
  0,                /* borda no cartaz em foco: ligada (o foco de sempre) */

  1,                /* efeito de profundidade: desligado (fabrica) */
  28,               /* brilho da borda */
  10,               /* reflexo */
  0,                /* cobertura da borda */
  0, 0, 0, 0, 0,    /* profundidade em posters, cw, episodios, elenco, trailers */

  126,              /* largura do item, dp (fabrica; o perfil do dono usa 120) */
  12,               /* arredondamento, dp */
  1,                /* qualidade da imagem: Padrão (0 Baixa, 1 Padrão, 2 Alta) */

  // Idioma 0 = Automatico: a conta (tmdb_language, depois o idioma de legenda),
  // depois o idioma da TV, depois English. O padrao NAO e o do dono do pacote:
  // quem instala vem do release publico, e ler uma interface em portugues sem
  // ter escolhido e pior do que ler em ingles sem ter escolhido — por isso o
  // ultimo degrau e o ingles. Quem prefere outro troca em Ajustes -> Interface,
  // e a escolha fica gravada (e desliga o automatico).
  0, 0, 0,          /* idioma, animacoes, resolucao (0 = 1080p) */
  AJ_TEMA_DINAMICA, /* tema (cor de destaque): Da arte desde a 2.0 (dono, 05/10/2026: era Imersiva) */
  0,                /* cor da logo (so com tema dinamico): ligada */
  // O COMENTARIO ANTIGO AQUI ESTAVA ERRADO, e o erro so nao machucou por sorte.
  // Ele dizia `0, 0, /* versao, espaco */` logo depois do idioma, mas esta
  // lista e POSICIONAL: entre AJ_ANIM e AJ_VERSAO_I existem SETE opcoes de
  // conta (perfil, sync, addons, onde o + salva, trakt, simkl, sair). Aqueles
  // dois zeros caiam em AJ_PERFIL_ATIVO e AJ_SYNC, nao em versao e espaco — e
  // versao e espaco ficavam com o zero da inicializacao parcial, que por acaso
  // e o valor certo para uma linha de leitura. A primeira opcao com padrao
  // DIFERENTE de zero nesta faixa (a de agora) teria caido no lugar errado.
  // Explicitados um a um, e nao contados de cabeca.
  0, 0, 0,          /* perfil, sincronizacao, addons: linhas de leitura/acao */
  // AS SETE DO STALKER E DO XTREAM FALTAVAM AQUI (#149). Elas entraram no enum
  // entre os addons e "onde o + salva" sem entrar nesta lista, e dali para
  // baixo cada padrao caiu sete casas antes: o 1 de "envio sozinho" pousava em
  // "onde o + salva" (que por acaso tambem nasce 1, e por isso nada se viu), e
  // o envio ficava no 0 da inicializacao parcial. O remendo que forcava o
  // desligado em ajustes_iniciar apagava a escolha da pessoa a cada abertura
  // da tela, que e o #149.
  0, 0, 0,          /* portal Stalker, MAC, remover: acoes */
  0, 0, 0, 0,       /* servidor, usuario, senha Xtream, remover: acoes */
  0, 0,             /* conta Xtream (leitura); grade: automatica */
  1,                /* onde o + salva: watchlist do Trakt (ver V_SALVOS) */
  0, 0, 0,          /* trakt, simkl, sair: acoes */
  // ENVIO SOZINHO LIGADO DE FABRICA (dono, 26/09/2026: "melhor deixar
  // automatico o log, isso que ajuda a gente"). O registro nao leva senha nem
  // chave, e e dele que sai a triagem. No Tizen o cartao de consentimento
  // (telemetria.c) continua perguntando na primeira abertura, e o nao grava.
  0, 0, 0, 0,       /* versao, atualizar, registro, envio sozinho: LIGADO */
  0,                /* espaco */
  0,                /* memoria para imagens: automatico */

  // Integracoes — TMDB. Tudo LIGADO de fabrica neste app: o enriquecimento por
  // TMDB sempre foi incondicional aqui, e nascer desligado removeria da tela
  // dados que o usuario ja ve (elenco com foto, ficha, trailers). A escolha da
  // conta continua mandando quando a chave chega no blob — este e so o ponto
  // de partida de quem nunca abriu o ajuste em lugar nenhum.
  0,                /* tmdb ligado */
  0,                /* idioma: da interface */
  0, 0, 0, 0, 0, 0, 0, /* arte, basico, ficha, datas, elenco, produtoras, redes */
  0, 0, 0, 0,       /* episodios, trailers, mais como este, colecoes */
  0,                /* enriquecer continuar assistindo */
  // MDBList: idem — a conta que traz a chave, e a consulta sempre correu.
  0,                /* mdblist ligado */
  0,                /* chave: leitura */
  0, 0, 0, 0, 0, 0, 0, 0, /* trakt, imdb, tmdb, letterboxd, tomatoes,
                           audiencia, metacritic, mal */
  0,                /* chave do fanart.tv: acao (o valor mora em fanart.txt) */
  0,                /* diagnostico */
  0,                /* teste de velocidade: acao */
  0, 0, 0, 0,       /* explorar, guia, agenda, perfil na barra lateral: ligados */
  1,                /* trailer do cartaz em foco: desligado (DEFAULT do web) */
  0,                /* itens por fileira: 12, como sempre foi (ver V_ITENS_FIL) */
  TXT_FAMILIA_INTER,/* fonte da interface: independente da legenda */
  0,                /* efeitos visuais do .tpk: automatico (adaptativo, ate o nivel 1) */
  1,                /* interface de vidro: DESLIGADA (V_LIGA: 1 = Desligado) */
  1,                /* servidor P2P: DESLIGADO (V_LIGA: 1 = Desligado) */
  0, 0,             /* endereco, testar: acoes */
  0,                /* posteres personalizados: DESLIGADO (indice 0) */
  0, 0, 0, 0, 0, 0,/* endereco, token, parametros, chave, modelo, testar: acoes */
  0,                /* layout da home: Moderna (a de sempre) */
  0, 0, 0, 0, 0,    /* chaves de debrid (AllDebrid, testar, RD, TB, PM): acoes */
  1,                /* usar sempre o Cinemeta: DESLIGADO (V_LIGA: 1 = Desligado) -> catalogo primeiro */
  // Notas na linha do titulo: de fabrica so o que a linha ja mostrava antes de
  // existir a escolha (IMDb, Rotten Tomatoes, Trakt). V_LIGA: 0 = Ligado.
  0, 0, 1, 1, 1,   /* imdb, tomates, popcornmeter, metacritic, metacritic usuarios */
  0, 1, 1, 1, 1, 1,/* trakt, tmdb, letterboxd, mal, ebert, nota do mdblist */
  1,                /* perfil pesquisavel: DESLIGADO (V_LIGA: 1 = Desligado). Padrao de todo mundo. */
  0,                /* meu perfil publico: acao */
  0,                /* contorno do vidro: LIGADO (V_LIGA: 0 = Ligado), o visual de antes */
  1,                /* som do trailer no destaque: DESLIGADO (V_LIGA: 1 = Desligado) */
  22,               /* espera do trailer no destaque: 2,2 s (em decimos de segundo) */
  90,               /* percentual assistido: 90%, o numero fixo de antes */
  1,                /* addons do perfil principal: DESLIGADO (V_LIGA: 1); dono 09/10 */
  0,                /* transicao do destaque: Deslizar */
  // Arte do addon: todas DESLIGADAS (V_LIGA: 1 = Desligado) — o visual de antes.
  1, 1, 1, 1,       /* posteres, fundo, logo do addon; arte das pastas da conta */
  // LIGADO: e o que o oficial mostra com o pacote de selos da wiki importado,
  // e o pedido do #198. Quem prefere a fileira branca de antes desliga.
  1,                /* selos coloridos: DESLIGADO (V_LIGA: 1 = Desligado); Glass UI = todos brancos */
  1,                /* som do trailer na pagina do titulo: DESLIGADO (V_LIGA: 1 = Desligado) */
  0, 0, 0,          /* Live TV: resolucao, formato do Xtream e espera automaticos */
  0,                /* diagnostico da Live TV: acao */
  0,                /* modo do player da Live TV: A, o load de sempre */
  0,                /* proxy de TS da Live TV: LIGADO (V_LIGA: 0 = Ligado) */
  0,                /* relogio na tela: LIGADO (V_LIGA: 0 = Ligado), o de sempre */
  0,                /* posicao do relogio: Automatica, a de sempre */
  0,                /* selo de assistido nos posteres: LIGADO (V_LIGA: 0 = Ligado) */
  // Seekr DESLIGADO (V_LIGA: 1 = Desligado): sem chave nao ha o que mostrar, e
  // cada titulo aberto gasta uma consulta da cota diaria da pessoa.
  1,                /* miniaturas do Seekr */
  0, 0,             /* chave e teste do Seekr: acoes */
  1,                /* fita de miniaturas: DESLIGADA (V_LIGA: 1 = Desligado), uma so */
  0,                /* sincronia da miniatura: 0 s (a documentacao: nada automatico) */
  0,                /* ao sair do player: a home, minimizando na ilha (com o relogio) */
  0,                /* perguntar o que achou nos creditos: LIGADO (V_LIGA: 0 = Ligado) */
  2,                /* espera pelos add-ons: 5 s (#221; indice 2 da lista do #202) */
  0,                /* ver o registro na tela: acao */
  0,                /* medidor de desempenho: DESLIGADO (V_MEDIDOR: 0 = Desligado) */
  0,                /* guia de uso: acao */
  0, 0, 0,          /* pacote de selos (espelho de selospacote_ativo), adicionar e remover: o embutido */
  0,                /* tamanho da interface: 100%, o de sempre */
  2,                /* fundo: Frost desde a 2.0 (FUNDO_FROST; dono, 05/10/2026) */
  2,                /* opacidade do vidro: 78%, a de sempre */
  0,                /* vidro fosco: desligado */

  0, /* app icon: Original */

  0, /* Discord action; appended to preserve option indices */
  0, /* Settings size: 80%, independent of global UI zoom */
  1, /* hide corner logo during trailer: Desligado (V_LIGA: 1 = Desligado) */
  0, /* second subtitle language: Da conta (F04) */
  1, /* subtitle sync by audio: Desligado (V_LIGA: 1 = Desligado) (F06) */
  0, /* seek cache on disk: Desligado (F07; free disk is unknown until the player opens) */
  1, /* experimental trailer zoom, .tpk only: Desligado (V_LIGA: 1 = Desligado) (#241) */
  0, /* plugins: action (state lives in plugins.c, F09) */
  1, 0, 0, 0, /* personal servers (F11): OFF (V_LIGA: 1 = Desligado); actions */
  1, /* show advanced options (R6): Desligado (V_LIGA: 1 = Desligado) */
  0, 0, 0, 0, 0, /* second subtitle (R4): No topo; Automatico; Igual a principal x3 */
  0, 0, 0, 0, 0, 0, /* Emby (address, sign in, sign out) and Plex (sign in, server, sign out): actions */
  0, 0, /* automatic source (R9b): Equilibrio; HDR e Dolby Vision: Preferir */
  0, 0, /* 2.0 (N1): app logo: Novo; app opening: Padrao */
  0, /* receive polls (N3): Ligado (V_LIGA: 0 = Ligado) */
  0, /* search Cinemeta (#231): Ligado (V_LIGA: 0 = Ligado), as before */
  3, /* rest screen delay (OLED): 2 min */
  1, /* player UI brightness (OLED): 80% */
  0, /* 2.0 guide (Sobre e ajuda › Novidades 2.0): action */
  1, /* keep the video ready on exit: Desligado (V_LIGA: 1); retention costs memory */
  0, /* rest screen style: Vitrine */
  0, /* showcase titles: Catalogo */
  0, /* clock format: 24 horas */
  0, 0, /* automatic source size range: max and min, Sem limite */
  0, /* support the project (Sobre e ajuda): action */
  0, /* forced subtitle when the audio is in your language (#287): Ligado (V_LIGA: 0 = Ligado) */
  0, /* content rating in the player: Ligado (V_LIGA: 0) */
  0, /* auto-play scope (#202): Todas as fontes (ALL_SOURCES, the official default) */
  0, 0, /* allowed add-ons / plugins: actions (lists live in fonteregra.c; empty = all) */
  0, /* use the others when the allowed have none: Ligado (V_LIGA: 0 = Ligado) */
  0, /* regex match: Desligada */
  0, 0, /* regex pattern (action) and preset mirror (Personalizado) */
  100, 100, /* Home spacing: between rows and between titles, 100% = as measured */
  0, /* default aspect ratio: Ultimo usado */
  1, /* also in Continue watching (2.0.2): Desligado (V_LIGA: 1 = Desligado) = one title, one place */
  0, /* continue on the profile picker (#303): Ligado (V_LIGA: 0 = Ligado) = as before */
  0, /* DTS converted to: Estereo (AAC) = the proven path */
  1, /* Dolby Vision in MKV (2.0.2, experimental): Desligado */
  0, /* send history to the Nuvio account (2.0.2, per profile): Ligado */
  0, /* social features (2.0.2, per profile): Ligado */
  0, 0, /* add-on order (action; list lives in fonteregra.c) and use of it: Nao */
  0, /* play while verifying the first auto source (2.0.2): Ligado */
  0, /* warm connections when a title opens (2.0.2): Ligado */
  0, /* check several sources at once (2.0.2; only sources ready on the debrid): Ligado */
  1, /* prepare the source when a title opens (2.0.2): Desligado */
  0, /* next episode in Continue watching (#203): Ligado */
  0, 0, /* search (#311): Nuvio results First; show the source on result groups: Ligado */
  0, /* Settings layout (#339): Painel = the card grid and the art beside the list */
  0, /* P2P space limit (#334): Automatico */
  0, /* Source display order (#400): Por qualidade, local */
  0, /* Sincronia automática da legenda: Ligado, local */
