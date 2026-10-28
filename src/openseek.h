// OPENSEEK: miniaturas da barra de tempo geradas por um servidor PROPRIO da
// pessoa (github: OpenSeek), ao lado do Seekr.tv. O servidor le o MESMO video
// que esta tocando (a URL vai num POST com TLS fixado) e devolve um WebVTT que
// cresce enquanto ele extrai quadros; prioriza o ponto para onde a pessoa esta
// avancando. Contrato: docs/api/CONTRACT-v1.md do OpenSeek; tela:
// docs/design/SCRUBBING.md. A parte pura (JSON, VTT, chaves) e openseekproto.c;
// o pareamento OSP1 e osp1.c.
//
// Entra no player pelo seekr.c (seekr_usar_provedor): o player continua
// chamando seekr_* e nao sabe qual provedor esta ativo.
//
// SEGURANCA: todo pedido usa rede_pedir com TLS fixado ao pino recebido no
// pareamento (CURLOPT_PINNEDPUBLICKEY, falha fechada), seguir = 0, prazo curto.
// Sem pino neste aparelho, nada sai (nem a URL do video). Endereco, pino e
// token ficam em openseek.txt (dados do app, so desta TV) e nunca vao a log.
#ifndef NV_OPENSEEK_H
#define NV_OPENSEEK_H
#include "seekr.h"
#include "openseekproto.h"
#include <stddef.h>

// A tabela que o seekr.c usa quando o provedor e o OpenSeek.
const SeekrProvedor *openseek_provedor(void);

// Ajustes. Le openseek.txt (uma vez; barato depois).
void openseek_carregar(void);
// O que a pessoa digitou ("nas.local", "192.168.1.5:8787"). 1 se valido e
// gravado; trocar de endereco desfaz o pareamento. "" apaga tudo.
int  openseek_definir_endereco(const char *digitado);
const char *openseek_endereco(void);   // "https://host:porta" ou ""
int  openseek_pareado(void);
// Pareamento OSP1 em fio proprio (PBKDF2 de 600 mil iteracoes nao roda no fio
// da interface). 0 quando o codigo e invalido ou ja ha um pareamento correndo.
int  openseek_parear(const char *codigo);
// Confere o pareamento (GET /v1/pair/status, fixado) em fio proprio.
void openseek_verificar(void);
void openseek_esquecer(void);
enum {
  OSU_NAO_CONFIGURADO = 0, OSU_NAO_PAREADO, OSU_PAREANDO, OSU_PAREADO,
  OSU_SEM_CONEXAO, OSU_FALHOU, OSU_CERTIFICADO, OSU_SEM_SUPORTE, OSU_VERIFICANDO,
  OSU_CONVITE, OSU_REVOGADO, OSU_LIMITE
};
// Estado para a linha de status dos Ajustes. `host` recebe so o host (sem
// porta nem esquema) para "Pareado com <host>".
int  openseek_status(char *host, size_t n);
// Rotulo (chave i18n em portugues) de um OSU_*.
const char *openseek_status_rotulo(int st);
// --- BIBLIOTECA COMPARTILHADA (aparelho convidado) ---------------------------------
// `openseek_parear` tambem aceita um CONVITE: a string openseek://join/1?... ou o codigo digitado de 16
// simbolos (o endereco vem do campo de servidor). O papel guardado vem do servidor ("guest"/"owner").
// Convidado: o aparelho NUNCA envia a URL do video nem cria trabalho no servidor do dono; pergunta
// (POST /v1/lookup, so ids + duracao) por miniaturas de outra versao do mesmo titulo e le o VTT pronto.
// Sem correspondencia: a reproducao segue normal, sem miniaturas.
int  openseek_papel(void);          // OS_PAPEL_DONO / OS_PAPEL_CONVIDADO
typedef struct {
  int  valido;                      // ha uma previa compartilhada conhecida (a do video atual ou a ultima)
  int  aproximado, verificado;
  char tipo[24];
  long offsetMs;                    // b do mapeamento; negativo = miniaturas mais TARDE que o cache
  char rotulo[112];                 // frase honesta para a tela
} OpenSeekCompartInfo;
int  openseek_compart_info(OpenSeekCompartInfo *o);
// Ajustes so deste aparelho (API de offset por aparelho). deslocaMs > 0: miniaturas mais tarde. Em fio proprio.
// O player informa a saude da reproducao (buffer a frente, rebuffering) para o CONTRIBUINTE: o servidor segura a extracao
// dele enquanto a reproducao dele sofre. Barato; so envia algo quando ha uma contribuicao em andamento.
void openseek_contrib_telemetria(long bufferMs, int rebuffering);
int  openseek_compart_ajustar(long deslocaMs);
int  openseek_compart_zerar(void);
int  openseek_compart_rejeitar(void);     // "video errado": esta fonte nao e mais oferecida a este aparelho
// Resultado da ultima acao (para avisar a pessoa): 0 nada, 1 ok, 2 falhou. `msg` recebe o texto.
int  openseek_compart_ultimo(char *msg, size_t n);
// --- FONTE ALTERNATIVA PARA MINIATURAS (opcional; desligada por padrao) ---------
// Liga com o arquivo "openseek-alt.txt" (conteudo 1) nos dados do app. Chamadas so da
// thread do player. `atualizar` guarda a saude da reproducao e, quando `play`/`c` vem,
// a lista de candidatos (SO metadados; a URL so segue se o servidor pedir aquele).
int  openseek_alt_ligado(void);
void openseek_alt_atualizar(const OpenSeekAltCand *play, const OpenSeekAltCand *c, int n,
                            long bufferMs, int rebuffering, const char *host);
// 1 quando o servidor pediu um candidato (cid) e ainda falta a URL; entregue-a UMA vez.
int  openseek_alt_pedido(char *cid, size_t n);
void openseek_alt_entregar(const char *cid, const char *url, const char *cabecalhos);
#endif
