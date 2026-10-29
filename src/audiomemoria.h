// A FAIXA DE AUDIO ESCOLHIDA A MAO, gravada por perfil.
//
// O DEFEITO: anime com audio duplo (ingles dublado e japones) na mesma fonte,
// mesmo bingeGroup episodio apos episodio — e a pessoa trocava para japones a
// mao em TODO episodio. audioescolha.c ja segura a escolha para a serie
// inteira, mas so na memoria: fechar o app, ou abrir outro titulo no meio,
// voltava ao idioma preferido (ingles) e a dublagem.
//
// O NuvioTV oficial grava a escolha por perfil e por contentId (a serie):
// TrackPreferenceDataStore, audio_lang/audio_name/audio_track_id, aplicada a
// todo episodio por findMatchingTrackIndex (faixa exata, depois nome+idioma,
// depois idioma). Aqui e o mesmo, com uma diferenca deliberada: a faixa exata
// (numero) so vale com o MESMO bingeGroup em que ela foi escolhida — numero de
// faixa de outra release nao diz nada. Fora dele vale a parecenca de sempre
// (audioescolha.c: idioma, rotulo, codec, canais).
//
// Um arquivo por perfil (audiomemoria-p<N>.txt, como legmemoria-p<N>.txt e
// fontepref-p<N>.txt): o japones de um nao e o do outro. Liga-se a
// audioescolha.c por audioesc_definir_memoria (app.c).
#ifndef NV_AUDIOMEMORIA_H
#define NV_AUDIOMEMORIA_H
#include "audioescolha.h"

// 150 titulos x ~300 bytes = ~45 KB, estatico. Cheia, sai a mais antiga.
#define AUDIOMEM_MAX 150

// Perfil corrente (perfis.c). Solta a tabela (relida na proxima consulta) e a
// escolha da sessao (audioesc_soltar): a do perfil anterior nao vale aqui.
void audiomem_definir_perfil(int perfil);
// Apaga do aparelho, de todos os perfis (sync_esquecer_usuario).
void audiomem_esquecer(void);

// A escolha gravada deste titulo, ou NULL. "tt123:2:4" vale a serie tt123.
const AudioLembrada *audiomem_ler(const char *titulo);
// Guarda (substitui) a escolha deste titulo e grava o arquivo do perfil.
void audiomem_gravar(const char *titulo, const AudioLembrada *m);
#endif
