// A ORDEM EM QUE O AUDIO E ESCOLHIDO SOZINHO (dono, 15/10/2026), em todos os
// alvos:
//   1. a faixa que a pessoa escolheu a mao neste titulo (o filme, ou a serie
//      inteira: os outros episodios tambem; troca de fonte, reconexao,
//      retomada, ida e volta do convertido). Gravada por perfil
//      (audiomemoria.h), como o NuvioTV oficial (TrackPreferenceDataStore,
//      por contentId): fechar o app ou abrir outro titulo no meio nao a perde.
//      Com o MESMO bingeGroup da fonte em que ela foi escolhida (a mesma
//      release, o mesmo arranjo de faixas) vale a faixa exata pelo numero;
//      em outra fonte, a parecenca de idioma/rotulo/codec/canais;
//   2. o idioma de audio preferido (ling_audio);
//   3. o segundo idioma de audio (ling_audio2);
//   4. o idioma original do titulo (ling_original, do TMDB);
//   5. nada: fica a que o arquivo/player escolheu.
// Em cada idioma (2-4), uma faixa que nao e DTS/TrueHD ganha; DTS/TrueHD so
// quando e a unica naquele idioma. No Android (o FFmpeg do player decodifica
// tudo) nao ha essa regra: o alvo marca dts = 0 e vale a primeira do idioma. Uma faixa que o alvo nao consegue tocar
// (`pode` = 0: DTS/TrueHD no .wgt, ou no .tpk com a conversao desligada) nao
// entra. Quem toca a escolhida (converter ou nao) e o alvo.
#ifndef NV_AUDIOESCOLHA_H
#define NV_AUDIOESCOLHA_H

typedef struct {
  const char *idioma;   // "en"; "" sem etiqueta
  const char *codec;    // qualquer grafia (audioinfo_codec): "A_DTS", "eac3", "audio/ac3"
  const char *rotulo;   // o da lista ("Ingles · DTS-HD MA 5.1")
  int canais;           // 0 = nao se sabe
  int dts;              // DTS ou TrueHD/MLP (audioesc_dts_truehd)
  int pode;             // o alvo toca (ou converte) esta faixa
  int numero;           // audio memory identity; -1 = unknown (no exact track)
} AudioCand;

// De onde veio a escolha (para o log).
enum { AUDIOESC_NADA = 0, AUDIOESC_LEMBRADA, AUDIOESC_PREF1, AUDIOESC_PREF2, AUDIOESC_ORIGINAL };
const char *audioesc_origem_nome(int origem);

// 1 quando o codec/rotulo e DTS (qualquer variante) ou TrueHD/MLP.
int audioesc_dts_truehd(const char *codec, const char *rotulo);

// A escolha lembrada. `binge` e o bingeGroup da fonte em que a pessoa a fez
// ("" = a fonte nao declarou: nunca ha faixa exata) e `numero` o
// audio memory identity at that source (video_audio_numero_memoria).
typedef struct {
  char idioma[16], codec[24], rotulo[48];
  int canais;
  int numero;
  char binge[128];
} AudioLembrada;

// O titulo aberto agora (imdb da serie ou do filme, ou o nome). Outro titulo
// esquece a escolha da sessao e le a gravada dele (audiomemoria.h); o mesmo a
// mantem.
void audioesc_titulo(const char *chave);
// A pessoa escolheu esta faixa a mao (folha de audio): vale ate outro titulo.
void audioesc_lembrar(const char *idioma, const char *codec, const char *rotulo, int canais);
// The same with the exact track: `numero` (audio memory identity) at this source,
// cujo bingeGroup vem de audioesc_fonte_binge. Grava no perfil.
void audioesc_lembrar_faixa(const char *idioma, const char *codec, const char *rotulo, int canais,
                            int numero);
// Ha escolha lembrada para o titulo atual?
int  audioesc_tem_lembrada(void);
// A escolha lembrada para o titulo atual, ou NULL.
const AudioLembrada *audioesc_lembrada(void);

// O bingeGroup da fonte que esta tocando ("" = nao declarou; NULL = nenhuma).
// app.c copia a cada troca de fonte, no fio do app: a escolha do audio roda no
// fio do Kotlin (Android) e nao pode ler a lista de fontes de la.
void audioesc_fonte_binge(const char *binge);
// Onde a escolha fica gravada (app.c liga audiomemoria.h). `ler` devolve a do
// titulo (chave de audioesc_titulo) ou NULL; `gravar` guarda uma escolha nova.
// Sem os dois (testes, alvos sem disco) a escolha vive so na sessao.
void audioesc_definir_memoria(const AudioLembrada *(*ler)(const char *titulo),
                              void (*gravar)(const char *titulo, const AudioLembrada *m));
// Solta a escolha da sessao sem gravar nada (troca de perfil: a do perfil
// anterior nao vale; a proxima abertura le a do perfil novo).
void audioesc_soltar(void);

// O indice escolhido entre `c[0..n)`, ou -1 (nao mexer). `origem` (opcional)
// diz o passo. Usa a escolha lembrada e as preferencias em vigor.
int audioesc_escolher(const AudioCand *c, int n, int *origem);

// O mesmo, puro, com tudo explicito (teste). `lembrada` pode ser NULL; prefs
// vazias ou repetidas sao puladas. Sem bingeGroup (escolher_com) nao ha faixa
// exata; com `binge` igual ao da lembrada, a faixa do mesmo numero (e do mesmo
// idioma, quando os dois tem etiqueta) vence a parecenca.
int audioesc_escolher_com(const AudioCand *c, int n, const AudioLembrada *lembrada,
                          const char *pref1, const char *pref2, const char *original, int *origem);
int audioesc_escolher_binge(const AudioCand *c, int n, const AudioLembrada *lembrada, const char *binge,
                            const char *pref1, const char *pref2, const char *original, int *origem);
#endif
