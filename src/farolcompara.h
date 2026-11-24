// FAROLCOMPARA — tela P4 "Comparar": 2 ou 3 titulos lado a lado (lote sense,
// issue 49; PLANO-GOSTO secoes 4.2 P4, 4.3, 4.4 e 4.5).
//
// Tela comum da casca 2.1 (src/tela21.h): kicker com o ponto de luz, titulo
// FT_CMP_TITULO, veredito opcional, 2 ou 3 colunas de ate 560 px (cartaz,
// titulo, metadados e as linhas dos eixos) e o botao "Ver qualidade".
//
// A CENA e a de sempre: farolcena_comparar (44) monta a P4 com o veredito
// das regras e farolpeca_ler (39) valida antes de subir. Tambem abre com uma
// P4 ja pronta (assistente #50, menu do cartaz #51, LLM) — a cena manda nos
// itens, eixos, veredito e escolha; os fatos de cada coluna vem do chamador.
//
// REGRAS DA 4.2 QUE ESTE MODULO GARANTE
//   - Recusa abrir com menos de 2 titulos; corta em 3; sem repetidos.
//   - Lista fechada de 8 eixos (FP_EIXO_*). Eixo sem dado em NENHUMA coluna
//     some da tabela; com dado em uma, a linha aparece (celula vazia nas
//     outras — nunca "nao sei").
//   - `qualidade` so com fontes JA consultadas nesta sessao (ou passadas na
//     entrada). Abrir NUNCA pergunta aos add-ons. Faltando em alguma coluna
//     (e sem player aberto), aparece o botao "Ver qualidade"; o OK nele
//     consulta as que faltam num fio proprio, cancelavel a qualquer momento
//     com Voltar (o fio larga os add-ons que faltam e nada e publicado).
//
// FOCO (4.5): nasce na coluna `escolha` (0 sem escolha). <- -> entre colunas
// sem sair da faixa; OK pede a pagina do titulo; Play toca direto a coluna
// focada; v desce para "Ver qualidade" quando existe e ^ volta para a coluna
// de onde saiu; Voltar cancela a consulta em curso e, sem consulta, fecha e
// devolve a tela de origem com o foco dela intocado.
//
// Desenho: sem FBO, sem blur, sem textura alem do cache de cartazes;
// animacoes reduzidas desligam toda mola. Nenhum titulo, perfil ou gosto em
// log — este modulo nao loga.
#ifndef NV_FAROLCOMPARA_H
#define NV_FAROLCOMPARA_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include "arteurl.h"
#include "farolinfo.h"
#include "farolpeca.h"

#define FCMP_COL_MAX    3
#define FCMP_FONTE_MAX  24
#define FCMP_URL_TAM    NV_ARTE_URL_MAX
#define FCMP_CEL_TAM    192

// Os fatos de UMA coluna, ja medidos pelo chamador (catalogo, ficha, onde
// ver, amigos). Texto vazio = sem dado. `ref` e o id do titulo ("tt...") que
// identifica a coluna na cena e na consulta de fontes; `tipo` e "movie" ou
// "series" (so a consulta usa). nFontes < 0 = as fontes deste titulo NAO
// foram consultadas nesta sessao; >= 0 = consultadas (0 = nenhuma serve).
typedef struct {
  char    ref[FAROL_REF_TAM];
  char    tipo[8];
  char    idFontes[64];          // id da consulta ("tt..:T:E" em serie); "" = ref
  int     catIndice;             // -1 = fora do catalogo
  char    titulo[160];
  char    poster[FCMP_URL_TAM];
  char    meta[96];              // "2022 · 2 h 12 min" — ja localizado
  int     minutos;               // duracao (0 = sem dado)
  int     notaPct;               // 0..100 (0 = sem dado)
  int32_t pontos;                // Q15 do motor ou do LLM (veredito 5.2)
  char    clima[64];
  char    porque[FCMP_CEL_TAM];
  char    onde[64];
  char    amigos[FCMP_CEL_TAM];
  int     nFontes;
  FarolInfoFonte fontes[FCMP_FONTE_MAX];
} FarolCompItem;

// ------------------------------------------------------------ abertura
// Monta a P4 com farolcena_comparar + farolpeca_ler. `tempoMin` = quanto a
// pessoa tem agora (0 = sem dado: sem veredito). Repetidos (mesmo ref) caem;
// alem de 3, os primeiros 3. Devolve 1 se abriu; 0 se recusou (< 2 titulos
// distintos, cena recusada pelo validador ou player aberto).
int  farolcompara_abrir(const FarolCompItem *itens, int n, int tempoMin);
// Abre a partir de uma cena P4 ("tela":"comparar") — a primeira peca
// "comparar" aceita pelo validador decide itens, eixos, veredito e escolha.
// "c<n>" resolve em itens[n] (nItens = nCandidatos da cena); "tt..." resolve
// pelo `ref` do item. 1 = abriu.
int  farolcompara_abrir_cena(const char *json, const FarolCompItem *itens, int nItens);
// Cola do app: monta os itens a partir do catalogo (CatItem/farolficha),
// ondever, amigostitulo e das fontes ja consultadas nesta sessao (lista do
// alvo atual, cache VOD e a memoria desta tela). `pontos` pode ser NULL
// (usa a nota como pontuacao). Nao vai a rede.
int  farolcompara_abrir_titulos(const int *catIndices, int n,
                                const int32_t *pontos, int tempoMin);
void farolcompara_fechar(void);

// ------------------------------------------------------------ casca (app.c)
int  farolcompara_aberto(void);
void farolcompara_evento(const SDL_Event *e);
void farolcompara_atualizar(float dt, Uint32 agora);
void farolcompara_desenhar(Uint32 agora);
// Pedidos consumidos pelo roteador (1 = havia pedido). catIndice -1 = titulo
// fora do catalogo: o roteador abre pelo `ref`.
int  farolcompara_pediu_abrir(int *catIndice, char *ref, size_t cap);
int  farolcompara_pediu_tocar(int *catIndice, char *ref, size_t cap);

// ------------------------------------------------------------ inspecao
int  farolcompara_n_colunas(void);
int  farolcompara_foco(void);            // coluna focada (mesmo com o botao)
int  farolcompara_foco_botao(void);      // 1 = foco em "Ver qualidade"
int  farolcompara_escolha(void);         // -1 = sem escolha
int  farolcompara_veredito(char *dst, size_t cap);   // 0 = sem veredito
int  farolcompara_n_eixos(void);         // linhas visiveis
int  farolcompara_eixo(int i);           // FP_EIXO_* da linha i (-1 fora)
int  farolcompara_tem_eixo(int eixo);
int  farolcompara_celula(int col, int eixo, char *dst, size_t cap);
int  farolcompara_botao_visivel(void);
int  farolcompara_consultando(void);
int  farolcompara_qualidade_consultada(int col);

// Memoria da sessao: fontes de um titulo ja consultadas (outra tela pode
// registrar). Some na troca de sessao de perfil.
void farolcompara_lembrar_fontes(const char *ref, const FarolInfoFonte *f, int n);

// ------------------------------------------------------------ ganchos de teste
// Consulta de fontes de UM titulo. BLOQUEIA (roda no fio da consulta);
// `cancelado(ctx)` deve ser consultado entre um add-on e outro. Devolve o
// numero de fontes em `out` (<= max) ou -1 cancelada/indisponivel.
typedef int (*FarolCompConsulta)(const char *id, const char *tipo,
                                 int (*cancelado)(void *), void *ctx,
                                 FarolInfoFonte *out, int max);
void farolcompara_teste_consulta(FarolCompConsulta fn);   // NULL = add-ons reais
void farolcompara_teste_caps(int hdr, int dv);            // -2 = stream_capacidades
void farolcompara_teste_relogio(long long (*epochS)(void));
void farolcompara_teste_player(int (*aberto)(void));
// NULL = nv_perfil_sessao_ler real. A troca de sessao com a tela aberta
// fecha o Comparar (itens e memoria sao da sessao antiga).
void farolcompara_teste_sessao(uint64_t (*sessao)(void));
int  farolcompara_teste_fios(void);            // fios de consulta vivos
// Espera o fio da consulta terminar (ms); 1 = nenhum fio vivo.
int  farolcompara_teste_esperar(Uint32 ms);
void farolcompara_teste_reset(void);

#endif
