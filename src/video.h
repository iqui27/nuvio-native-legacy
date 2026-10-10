// Reproducao de video de verdade nesta TV (webOS 4.10), por LS2 direto.
//
// O modulo NAO desenha nada. O video vive num PLANO DE HARDWARE separado, atras
// da superficie GL do app; o que o app faz e abrir um buraco transparente por
// onde esse plano aparece (ver gfx_furo). Consequencia que economiza horas:
// glReadPixels e o servico de captura da TV NUNCA vao fotografar o video. Isso
// e o modelo, nao defeito — verificar reproducao se faz pelo estado, ou pelo
// access log de quem serve o arquivo.
//
// Por que LS2 direto e nao StarfishMediaAPIs, tudo medido no aparelho:
// o construtor da StarfishMediaAPIs chama exit(0) quando o processo nao casa com
// o "exeName" do papel LS2 (nao e crash: atexit dispara e o journal fica mudo),
// e mesmo com o papel certo ele nunca chega a falar com com.webos.media aqui —
// responde erro 202 "Media Not Found", que e string interna da propria lib.
// A sequencia abaixo, ao contrario, saiu de captura ls-monitor do navegador da
// TV tocando o mesmo arquivo: e o que comprovadamente funciona.
#ifndef NV_VIDEO_H
#define NV_VIDEO_H

// Registra no barramento e sobe o laco de eventos. 1 se deu certo.
// Falhar aqui nao e fatal: o app segue sem video.
int  video_iniciar(void);
// O MESMO, para quem se oferece sozinho (o trailer do detalhe): segue o recuo
// de lsregistro.h e desiste depois de poucas recusas, em vez de chamar
// LSRegister a cada 15 s pela sessao inteira (registros 1720-1774). Onde nao
// ha hub LS2 e so o video_iniciar.
int  video_iniciar_auto(void);
// 1 quando o hub LS2 negou o registro por PERMISSAO (LS_ERROR_CODE_PERMISSION)
// nesta sessao: nenhum video abre, e o player diz isso em vez de "nao foi
// possivel abrir a fonte" — a fonte nao tem culpa.
int  video_registro_negado(void);

// Comeca a tocar. `url` e http(s):// ou file://. NAO mandar mediaTransportType:
// o transporte sai do prefixo da URL, e mandar o campo faz o load aceitar,
// devolver mediaId e nunca buscar o arquivo — falha silenciosa.
int  video_tocar(const char *url);
#ifdef NV_ANDROID
// Posicao absoluta em segundos, zero = inicio/default. O Kotlin configura o
// MediaItem antes de prepare; o ack e da geracao desta abertura. Estado:
// 0 aguardando, 1 aceito, -1 abertura normal (seek tardio como fallback).
int  video_tocar_posicao(const char *url, double segundos);
// Como a de cima, mais o percentual (0..100) para quando nao ha segundos
// exatos (retomada da conta: Trakt/Simkl so dao percentual). O Kotlin aplica
// sobre a duracao do container ANTES do primeiro quadro; mesmo ack/estado.
int  video_tocar_retomada(const char *url, double segundos, double pct);
int  video_retomada_inicial_estado(void);
#endif

// Chamar UMA VEZ POR QUADRO. Hoje serve ao prazo do recuo de Dolby Vision
// (ver o comentario em video.c): sem esta batida, um arquivo que a TV recusa
// com DolbyHdrInfo ficaria sem imagem ate o usuario desistir e sair.
void video_bombear(void);
void video_parar(void);
void video_pausar(int pausado);
// Volume do pipeline, 0..100 (uMS setVolume). Existe para o trailer mudo no
// fundo da pagina de titulo (trailer.h); a reproducao normal nao mexe nisto.
void video_volume(int pct);
void video_buscar(double segundos);

// Retangulo do plano de video, em coordenadas de tela 1920x1080. Fica preso a
// tela: pedir origem negativa ou tamanho maior que o painel APAGA o plano — um
// plano de hardware nao recorta o excedente. Para ampliar, use a funcao abaixo.
void video_janela(int x, int y, int w, int h);

// Zoom de verdade: recorta a FONTE (coordenadas do quadro decodificado, ver
// video_largura/video_altura) e desenha no destino (coordenadas de tela). Pedir
// um pedaco menor da fonte para o mesmo destino e o que amplia a imagem, e o
// que tira da vista a barra preta embutida no quadro.
void video_janela_fonte(int sx, int sy, int sw, int sh,
                        int dx, int dy, int dw, int dh);

// 1 quando o alvo CONSEGUE recortar a fonte; 0 quando so sabe encaixar e
// esticar. Quem oferece os modos de aspecto pergunta antes, para nao anunciar
// um zoom que nao vai acontecer.
//
// MEDIDO na QN85Q70AAGXZD (Tizen 6.0), testando a API na propria TV:
//   * setVideoRoi EXISTE mas so vale para video 360. Valores fora de 0..1 dao
//     InvalidValuesError e valores DENTRO de 0..1 dao NotSupportedErr — a troca
//     de erro e a prova: 0..1 passa na validacao e a implementacao recusa.
//   * setDisplayRect aceita retangulo MAIOR que a tela, mas exige x e y >= 0.
//     "-521,-79 2966x1242" -> InvalidValuesError; "0,0,2966x1242" -> OK. Sem
//     origem negativa nao ha como centralizar o recorte.
//   * setDisplayMethod so tem LETTER_BOX, FULL_SCREEN e AUTO_ASPECT_RATIO.
//     CROPPED_FULL, ORIGIN_SIZE, DST_ROI e ORIGIN_OR_LETTER: InvalidValuesError.
//
// Com essas tres, encaixar e esticar saem; recortar e ampliar nao saem.
int  video_recorte_fonte(void);
// Manda de novo ao plano o ultimo par fonte/destino, sem o dedup. Existe para
// o trailer (trailer.c): o pipeline prende o plano em mais de um ponto depois
// do load e pode engolir um recorte pedido cedo.
void video_recorte_reaplicar(void);
// Tamanho da superficie (drawable) em que o destino do plano de video e lido; o
// layout e 1920x1080 e o retangulo e escalado por isso (issue #176). Chamar uma
// vez, depois de criar a janela. Ver video_escala.h.
void video_escala_definir(int sw, int sh);

// URL da reproducao corrente ("" quando nao ha). Existe para a folha de
// faixas mandar o mkvass.c ler a legenda ASS de dentro do MESMO arquivo que
// esta tocando — e a unica coisa que identifica o arquivo, nos dois alvos.
const char *video_url_atual(void);
double video_pos(void);      // segundos decorridos
double video_duracao(void);  // 0 enquanto desconhecida

// Segundo em que comecam os CREDITOS, ou 0 quando o arquivo nao diz. Sai do
// capitulo final do Matroska, lido no mesmo trecho de cabecalho que ja e
// baixado para descobrir o idioma das faixas — ver mkv.h. Vale so para MKV com
// capitulos; quem chama precisa de um plano B.
double video_creditos(void);
double video_buffer_fim(void); // ate onde o buffer cobre (s); 0 se desconhecido
// Ha quanto tempo (ms) o pipeline esta parado esperando dados, do par de
// eventos bufferingStart/bufferingEnd do uMS; 0 quando NAO esta bufferizando.
//
// Existe porque encher e esvaziar o buffer nao passa por `paused` nem por
// erro: uma fonte que morre no meio da reproducao deixa o app com imagem
// congelada e video_falhou() em 0 para sempre. Quem vigia fonte de canal
// (app.c) precisa deste numero para saber que a fonte morreu sem dizer.
unsigned video_bufferando_ms(void);
// Afirmacao de Dolby Vision da FONTE escolhida (o addon descreve o arquivo).
// Chamar ANTES de video_tocar/definir_fonte: e o que decide o hdrType que o
// ACB descreve ao tv.display.
void video_definir_dv(int dv);

// CABECALHOS QUE O ADDON EXIGE (behaviorHints.proxyHeaders), uma linha
// "Nome: valor" por cabecalho. Chamar ANTES de video_tocar, junto com
// video_definir_dv. String vazia limpa.
//
// MEDIDO NA C9 em 18/09, com o addon de um relato: o CDN responde 403 sem
// Referer e 200 com ele — e cobra em CADA SEGMENTO, nao so na playlist, entao
// nao adianta so buscar a lista com cabecalho e entregar o resto ao pipeline.
// Provado que a TV sabe faze-lo:
//   gst-launch-1.0 souphttpsrc location=<url> ! fakesink            -> Forbidden
//   ... extra-headers="headers,Referer=(string)\"<ref>\"" ! fakesink -> baixa
void video_definir_cabecalhos(const char *cabs);

// A fonte e MP4? Chamar ANTES de video_tocar, junto com video_definir_dv.
//
// Serve para NAO sondar o cabecalho Matroska num arquivo que nunca vai ter um.
// Essa sonda baixa o inicio do arquivo pela MESMA conexao que esta
// transmitindo, e num MP4 ela e trabalho garantidamente perdido — o proprio
// log dizia "nenhuma faixa lida" toda vez.
void video_definir_mp4(int ehMp4);
// RECONEXAO quando a rede cai no meio da reproducao (video_reconexao.h). Chamar
// ANTES de video_tocar, como video_definir_dv; vale so para a proxima fonte
// (video_tocar consome). Quem nao pede — trailer, canal ao vivo, que tem o
// watchdog proprio em app.c — segue caindo em video_falhou na hora.
void video_definir_reconexao(int sim);
// MODO DO LOAD para o PROXIMO video_tocar (#158, LG C4: dado chega, VDEC
// concedido, decoder nunca anuncia). 0 = o de sempre; 1 = sem o selectTrack
// de video logo depois do load; 2 = o 1 e mais o payload enxuto de live
// (mediaTransportType HLS/URI, sem useSeekableRanges/bufferControl). Vale UMA
// vez (video_modo_live_consumir zera); so a webOS le. Ver video_modo.c.
void video_definir_modo_live(int modo);
int  video_modo_live_consumir(void);
// Numero da tentativa (1..3) enquanto uma queda esta em curso, esperando ou
// recarregando; 0 fora disso. Enquanto nao e 0, video_falhou fica em 0.
int    video_reconectando(void);
int    video_tocando(void);
// Pausa comprovada pelo backend da sessao atual, nao a intencao local de
// video_pausar. Alvos sem confirmacao confiavel devolvem 0.
int    video_pausa_confirmada(void);
int    video_pronto(void);   // 1 depois do loadCompleted
#ifdef NV_ANDROID
int    video_superficie_estavel(void);   // 1 depois da ultima recriacao da Surface (HDR/DV)
#endif
int    video_ativo(void);    // 1 assim que ha mediaId — e o que abre o furo
int    video_falhou(void);   // 1 depois de um errorText real na fonte atual
// O ULTIMO ERRO REAL do pipeline na fonte atual, como o pipeline o disse
// ("40403 server error:40403", "100 Playing error"); "" sem erro. Existe para
// o cartao de erro do canal dizer o que o servidor respondeu em vez do
// generico (#158). Vale ate o proximo video_tocar/video_parar.
const char *video_erro_texto(void);
#ifdef NV_ANDROID
// Codigo do erro atual apenas se o renderer e de VIDEO; 0 para audio/desconhecido.
int video_erro_decoder_codigo(void);
#endif
// 1 depois que o decoder se anunciou (videoInfo do uMS) na fonte atual. Existe
// para o watchdog de canal separar "abre devagar" de "chega dado e o decoder
// nunca comeca" — o sintoma do #158, com bufferRange subindo e nenhum
// videoInfo. Onde a plataforma nao da o sinal (Tizen), 1: nao afirma nada.
int video_decoder_anunciou(void);
int video_dts_legenda_desenhar(double seconds, int delay_ms, float x, float y, float w, float h, float alpha);
// Dolby Vision in MKV (webOS, Settings > Playback): 1 while our demux feeds
// the TV with the file's Dolby Vision; video_dv_recuo_consumir says once why
// it handed playback back to the TV player (1 slow source, 2 did not start).
int video_dv_ativo(void);
int video_dv_recuo_consumir(void);

// --- A TELA DO DOLBY VISION EM MKV (dvtela.h) --------------------------------
// O que o caminho do DV esta fazendo AGORA, para a tela que cobre a troca do
// player da TV (HDR10) pelo nosso demux. Tudo e estado real do backend; nada
// aqui e relogio. Fora da webOS (e no Mac sem simulacao) volta zerado.
enum {
  VIDEO_DV_NAO_NADA = 0,
  VIDEO_DV_NAO_SEM_DV,    // o cabecalho do arquivo nao tem Dolby Vision
  VIDEO_DV_NAO_PERFIL,    // perfil 7 com camada de realce (ou outro que nao 5/8)
  VIDEO_DV_NAO_AUDIO,     // nenhuma faixa de audio que o caminho alimenta
  VIDEO_DV_NAO_SONDA,     // o cabecalho nao veio (a sonda desistiu)
  VIDEO_DV_NAO_LENTO,     // o caminho abriu e a fonte nao acompanhou
  VIDEO_DV_NAO_FALHOU,    // o caminho nao abriu
  VIDEO_DV_NAO_PESSOA,    // a pessoa escolheu HDR10
  VIDEO_DV_NAO_CONTENTOR  // o arquivo nao e Matroska (MP4/TS sem extensao na URL)
};
typedef struct {
  int sessao;          // muda a cada video_tocar (fonte nova)
  int sondado;         // o cabecalho do MKV ja foi lido
  int perfil, el;      // do dvcC do arquivo (0 = sem Dolby Vision)
  int audioTrocado;    // a faixa da TV nao passa no caminho e outra entrou
  char audioDe[16], audioPara[16];   // CodecID do Matroska ("A_TRUEHD", "A_EAC3")
  int caminho;         // nosso demux foi pedido (iniciarDts em modo DV)
  int fonteAberta;     // o demux abriu o arquivo (stage source-opened)
  int carregado;       // a TV aceitou o fluxo (loadCompleted no caminho)
  int dvConfirmado;    // o pipeline disse hdrType DolbyVision
  int tocando;         // `playing` no caminho
  int recusa;          // VIDEO_DV_NAO_*: o DV desta fonte nao vai acontecer
  int falhou;          // a fonte falhou de vez (video_falhou)
} VideoDvFase;
void video_dv_fase(VideoDvFase *f);
// 1 quando a PROXIMA abertura de `url` vai tentar Dolby Vision em MKV: a fonte
// afirmou DV (video_definir_dv), nao e MP4 (video_definir_mp4 e a extensao) e
// a opcao esta ligada e liberada nesta TV. Chamar DEPOIS de video_definir_dv e
// video_definir_mp4, como video_tocar.
int  video_dv_candidato(const char *url);
// A tela esta cobrindo a abertura: o player da TV que toca o HDR10 enquanto o
// cabecalho e lido fica MUDO (uMS setVolume 0). 0 devolve o volume se a fonte
// ficar no player da TV.
void video_dv_tela(int cobrindo);
// A pessoa preferiu HDR10 agora: esta fonte nao tenta mais Dolby Vision. Se o
// caminho ja abriu, volta ao player da TV na mesma posicao.
void video_dv_recusar(void);
// O PLAYER AINDA NAO TERMINOU DE RETOMAR: segura a troca para o caminho do DV
// ate o seek do ponto salvo ter sido pedido ao player da TV (o caminho nasce na
// posicao dele). Chamado por quadro pelo player.
void video_dv_segurar(int segurar);
// O caminho proprio (DV/DTS) carregou e ainda nao tocou o primeiro quadro, sem
// pausa nossa: e o preroll do nosso demux, NAO uma pausa. O painel de pausa e a
// retomada "de quem abre pausado" perguntam isto.
int  video_iniciando(void);
// 1 once the current source has played on webOS (kept across reloads of it).
int video_fonte_tocou(void);
const char *video_dts_saida(void);  // actual local conversion output, empty for native playback
int    video_audio_nao_suportado(void);  // uMS errorCode 200: video segue sem som
int    video_seek_desistiu(void);  // webOS: seek recusado 3x; segue tocando de onde esta
int    video_terminou(void); // 1 depois do fim de fluxo (endOfStream) da fonte atual
// 1 depois que OUTRO app tomou o video da TV nesta sessao (so o .tpk sabe:
// "interrompido: ResourceConflict" do host). Pegajoso ate o fim da sessao.
int    video_conflito_recurso(void);
#ifdef NV_TPK
void   video_tpk_log_host(const char *linha);   // tpk.c repassa cada linha do host
int    video_tpk_codec_recusado(const char *codecId);  // #313: DTS/TrueHD/MLP, que a Samsung nao toca
// Zoom/recorte do plano de video (ROI), experimental (#241): liga/desliga em
// execucao; o padrao e NV_TPK_ZOOM_ROI (0). Loga "[trailer] tpk zoom ROI: ...".
void   video_tpk_zoom_roi_definir(int ligado);
int    video_tpk_zoom_roi(void);
// trailer.c marca que o que toca agora e um trailer (so ele pode ter ROI fora da tela).
void   video_tpk_trailer_marcar(int sim);
int    video_recorte_fonte_trailer(void);   // so o trailer: o ajuste em execucao
#else
static inline int video_recorte_fonte_trailer(void) { return video_recorte_fonte(); }
#endif

// --- faixas -----------------------------------------------------------------
// Tudo isto sai do evento sourceInfo da assinatura do uMS: o addon nao informa
// nada disso, e so o pipeline sabe o que ha DENTRO do arquivo.

// 32 e nao 12 (#92): um "Multi-Subs" de anime passa de doze legendas, e a
// lista da TV cortada em 12 nunca casa pelo ordinal com o arquivo inteiro —
// nenhuma faixa ASS ia ao overlay do app, todas ficavam com a TV.
#define NV_FAIXA_MAX 32

typedef struct {
  char rotulo[48];   // "Ingles · Atmos 5.1" ou "Legenda 3"
  char idioma[8];    // "en"; vazio quando o arquivo nao etiqueta
  int  numero;       // indice que o selectTrack espera
  int  ordinalMkv;   // ordinal da legenda (AVPlay; no .tpk, o do arquivo, faixasmkv.c); -1 se nao informado
  // CodecID do Matroska ("S_TEXT/ASS", "S_TEXT/UTF8", "S_HDMV/PGS"), lido do
  // cabecalho do MKV; vazio fora de MKV. A folha de faixas marca a legenda
  // ASS com isto (#92): e a faixa que o pipeline da TV desenha mal.
  char codec[24];
  // Faixa so de LETREIROS ("Signs", "Songs", "Signs & Songs", FlagForced): nao
  // traduz o dialogo. A folha rotula como tal e lista por ultimo.
  int  letreiro;
  // LING_LEG_* (#287): forcada, letreiros, SDH, completa ou comum. A lista
  // mostra ao lado de "Embutida"; a escolha automatica pesa. 0 = comum/nao se sabe.
  int  tipoLeg;
  int canais;        // original audio channels, 0 unknown
  int stream_index;  // source demux index for DTS sessions, -1 in native webOS
  int stream_id;     // original container track ID, -1 when unknown
} VideoFaixa;

int  video_n_audio(void);
int  video_n_legenda(void);
const VideoFaixa *video_audio(int i);
const VideoFaixa *video_legenda(int i);
int  video_legenda_ordinal_mkv(int i); // ordinal Tizen para casar com TrackEntry
// Sonda do cabecalho do MKV (idioma, codec e ordinal das legendas): 0 = ainda
// nao voltou (pendente ou rodando), 1 = voltou (com ou sem par), 2 = nao ha
// sonda (fonte MP4, sem URL). A folha de legendas espera o 1 antes de decidir
// se a faixa ASS vai ao overlay do app ou fica com a TV (#92).
int  video_mkv_sondado(void);
// Dispara a sonda ja, sem esperar o gatilho de buffer. Inocuo se ja rodou.
void video_sondar_mkv_agora(void);
int  video_audio_atual(void);
// A faixa e DTS? 0 nao, 1 DTS, 2 DTS-HD (MA/HRA/Express), 3 DTS:X. Le o codec
// e o rotulo (o uMS e o MKV escrevem "DTS", "A_DTS/LOSSLESS", "DTS-HD MA";
// a sessao convertida, o nome do FFmpeg "dts"). Codigo puro, todos os alvos.
int  video_faixa_dts(const VideoFaixa *f);
// O que aconteceu com o audio DTS da faixa atual (marca DTS no player e o
// aviso da ilha). A marca aparece em TODOS os estados diferentes de NENHUM:
// o DTS e da fonte, e o estado so diz como ele chega (pedido do dono, 06/10).
enum {
  VIDEO_DTS_NENHUM = 0,     // faixa atual nao e DTS (ou nada tocando)
  VIDEO_DTS_NATIVO,         // a TV decodifica o DTS sozinha
  VIDEO_DTS_PREPARANDO,     // a conversao local (PR #259) esta abrindo
  VIDEO_DTS_CONVERTIDO,     // DTS -> AAC estereo convertido no app
  VIDEO_DTS_FALHOU,         // a conversao falhou; voltou ao nativo sem som
  VIDEO_DTS_SEM_SOM         // a TV recusa DTS e a conversao esta desligada aqui
};
int  video_dts_estado(void);
int  video_legenda_atual(void);   // -1 = desligada

void video_escolher_audio(int i);
void video_escolher_legenda(int i);   // -1 desliga
// Texto da legenda EMBUTIDA em vigor quando o player nativo nao a desenha
// (Samsung: o AVPlay so entrega o texto no onsubtitlechange, issue #122).
// Devolve 1 e preenche `dst` quando ha fala na tela agora; 0 no webOS (o uMS
// desenha) e no Mac.
int  video_legenda_nativa(char *dst, int tam);

// Legenda de arquivo externo (OpenSubtitles). O uMS baixa e sincroniza
// sozinho; o app so passa a URL.
void video_legenda_externa(const char *url);
// O uMS identifica o formato pela extensao da URI. Addons costumam entregar
// /file/123 sem .srt; esta funcao torna a URI reconhecivel sem mudar o arquivo.
void video_normalizar_url_legenda(const char *url, char *dst, unsigned tam);

// --- ESTILO DA LEGENDA -------------------------------------------------------
//
// PROVADO NO APARELHO (LG C9, webOS 4.10, 2026-09-02) com um filme tocando: os
// cinco metodos abaixo mudam a legenda na tela de verdade. O teste foi visual e
// nao pelo retorno, porque o uMS responde `returnValue:true` PARA QUALQUER
// COISA — ele aceitou ate valores que eu inventei para `charEdgeType`. Nesta
// API o codigo de retorno nao e evidencia de nada.
//
// A legenda e desenhada pelo PIPELINE, abaixo da superficie GL: ela nao aparece
// em glReadPixels, igual ao video. Verificar mudanca aqui exige olhar a tela.
//
// Os valores sao os indices das opcoes oferecidas na folha, nao os do uMS: a
// traducao para o vocabulario do aparelho mora em video.c, que e quem conhece o
// pipeline.
typedef struct {
  int tamanho;    // 50..200%, passo 10 (120 = padrao)
  int cor;        // indice em VIDEO_LEG_CORES
  int fundo;      // 0 nenhum; 1..4 = escuro 25/50/75/100%
  int posicao;    // 0..7  -> position -3..4 no uMS
  int borda;      // 0 nenhuma, 1 contorno, 2 sombra
  int atrasoMs;   // negativo adianta
  int opacidade;  // 0..3 = texto 100/75/50/25%
  int familia;    // TxtFamilia; aplicada ao overlay externo (OpenSubtitles)
  int negrito;    // 1 = peso negrito no overlay (so o que o app desenha)
} VideoLegendaEstilo;

#define VIDEO_LEG_NCORES 6
// Nomes que o uMS aceita em charColor. Expostos porque a folha desenha os
// rotulos e precisa da mesma ordem.
extern const char *const VIDEO_LEG_CORES[VIDEO_LEG_NCORES];
extern const char *const VIDEO_LEG_CORES_PT[VIDEO_LEG_NCORES];

// Aplica agora, se houver sessao. O estilo fica GUARDADO e e reaplicado a cada
// load: o pipeline nasce de novo a cada video e nao carrega o ajuste anterior.
void video_legenda_estilo(const VideoLegendaEstilo *e);

// Verdade sobre o fluxo, para os selos da tela nao mentirem.
int  video_tem_atmos(void);
// 1 so quando o hdrType do PIPELINE diz DolbyVision. A afirmacao da fonte
// (video_definir_dv) nao entra aqui de proposito: ver o comentario em video.c.
int  video_tem_dolby_vision(void);
const char *video_hdr(void);   // hdrType cru: "none", "HDR10", "DolbyVision"...
// Dimensoes do QUADRO decodificado, do videoInfo. Sao elas que dao a proporcao
// usada pelos modos de zoom do player.
int  video_largura(void);
int  video_altura(void);

// --- TELA PRETA COM AUDIO TOCANDO -------------------------------------------
//
// NAO DA PARA DETECTAR, e isso foi MEDIDO neste aparelho, nao suposto: o uMS
// reporta videoInfo, sourceInfo e loadCompleted normalmente nos arquivos que
// ficam sem imagem (caso registrado: videoInfo 3840x1606 hdrType=DolbyVision,
// loadCompleted em 3212 ms, tela preta com o audio correndo). Nao existe no uMS
// sinal de QUADRO EXIBIDO — o currentTime avanca puxado pelo audio. A tentativa
// de recuo automatico por prazo esta registrada como removida em video.c.
//
// Os dois recuos automaticos que EXISTEM cobrem o caso em que o pipeline
// desmente a fonte (pedimos DolbyVision e o hdrType volta HDR10 ou none). O
// caso que sobra e o pior: o pipeline CONFIRMA DolbyVision, o ACB aceita, e nao
// ha quadro. Recuar automaticamente ali quebraria o DV que funciona de verdade
// nos MP4 perfil 5 e 8 — e por isso a saida e a pessoa dizer que a tela esta
// preta, em vez de o app adivinhar.
//
// video_forcar_sdr recarrega a MESMA fonte, na posicao atual, sem afirmar HDR
// nenhum. A escolha vale pela sessao: uma recuperacao posterior do pipeline nao
// traz o Dolby Vision de volta pelas costas.
//
// video_pode_forcar_sdr diz se o alvo tem o que renegociar — so o webOS tem.
// No Tizen o HDR e decidido pelo AVPlay do firmware e nao ha equivalente; a
// interface usa isto para nao oferecer um botao que nao faz nada.
int  video_pode_forcar_sdr(void);
#ifdef NV_SHOT_HOOKS
void video_shot_pode_forcar_sdr(int sim);   // so o coto do Mac (capturas)
#endif
void video_forcar_sdr(void);

// VELOCIDADE DE REPRODUCAO (#202), em centesimos (150 = 1,5x; velocidade.h).
// video_velocidade guarda o pedido e o backend o aplica quando o pipeline
// aceita (pronto; na LG tambem tocando) e de novo a cada pipeline novo da
// MESMA reproducao (reconexao, troca de fonte): o pipeline novo nasce em 1x.
// Quem volta a 1x e o player (titulo novo, fechar).
//   suportada: 0 = esconder a linha. O .wgt (AVPlay setSpeed e trick play
//              inteiro, sem audio) nunca tem; LG e .tpk passam a 0 quando a
//              plataforma RECUSA o pedido (resposta do uMS / excecao do host),
//              e ai a velocidade valendo volta a 100.
//   atual:     o pedido valendo (100 depois de uma recusa).
int  video_velocidade_suportada(void);
void video_velocidade(int centesimos);
int  video_velocidade_atual(void);
// O PLAYER mediu que o pipeline nao acelerou (velocidade.h, VelMedidor) apesar
// do "ok" da plataforma: o backend passa a se comportar como depois de uma
// recusa (suportada = 0, atual = 100) e devolve o pipeline a 1x.
void video_velocidade_recusada(void);
// 1 = a velocidade nao e possivel AGORA, com motivo conhecido: o audio sai em
// passthrough para o receptor (Android: o ExoPlayer nao muda o tempo de um
// fluxo EAC3/AC3 que ele nao decodifica). A linha fica esmaecida com o motivo.
int  video_velocidade_bloqueada(void);

void video_encerrar(void);

// SO NO MAC (capturas tests/*_shot.c): o coto do pipeline passa a responder o
// que estiver aqui — faixas, tamanho, HDR, Atmos, buffer — para as telas do
// player serem desenhadas com dados de um video "de verdade". Zerado (o padrao)
// e o mesmo coto mudo de sempre. Nos alvos de TV a funcao nao existe.
#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)
typedef struct {
  int nAudio, nLeg, audioAtual, legAtual;
  VideoFaixa audio[8], leg[8];
  int largura, altura, atmos, dv;
  char hdr[16];
  int pronto, reconectando;
  unsigned bufferandoMs;
  double pos, duracao, bufferFim;
  int dts;           // VIDEO_DTS_* que video_dts_estado devolve
  int aceita;               // video_tocar devolve isto (1 = o player tem video)
  int tocando, iniciando;   // video_tocando / video_iniciando
  int dvCandidato;          // video_dv_candidato
  VideoDvFase dvFase;       // video_dv_fase
} VideoSimulacao;
// Capturas/testes: o que video_dv_tela e video_dv_recusar receberam (contagem
// de chamadas e ultimo valor), para provar o mudo e a escolha do HDR10.
int video_simulado_dv_tela(void);
int video_simulado_dv_recusas(void);
void video_simular(const VideoSimulacao *s);
#endif

// One-reply webOS LS2 request; callback runs on the GLib thread.
#if !defined(NV_TPK) && !defined(NV_ANDROID)
int video_luna(const char *uri, const char *payload, void (*callback)(const char *, void *), void *context);
#endif
#endif

#if defined(NV_TPK) && defined(NV_ASPECTO_DIAG)
int video_tpk_aspecto_diag_ativo(void);
void video_tpk_aspecto_diag_ciclar(int metodo);
void video_tpk_aspecto_diag_aplicar(void);
void video_tpk_aspecto_diag_desligar(void);
const char *video_tpk_aspecto_diag_rotulo(void);
#endif
