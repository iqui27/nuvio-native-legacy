// Transporte do Supabase, sem saber nada sobre sessao.
//
// FATO lido no app web (js/data/remote/supabase/supabaseApi.js): TODO o sync
// passa por uma unica forma — POST em /rest/v1/rpc/<funcao> com corpo JSON e
// dois cabecalhos, `apikey` e `Authorization`. Nao ha WebSocket nem SDK: o que
// o nativo precisa e POST com cabecalhos (rede.c ja tem) e JSON (js.c le,
// jsw.c escreve). Por isso este modulo cabe em duzias de linhas em vez de
// exigir uma biblioteca.
//
// Quem escolhe o Bearer e quem chama. Esta camada NAO conhece o token da
// sessao de proposito: sessao.c e que decide entre a chave anonima e o token
// do usuario, e e la que mora a regra de "401 significa renovar e repetir".
// Separar assim evita a dependencia circular obvia (sessao precisa de HTTP,
// HTTP precisaria da sessao).
#ifndef NV_NUVEM_H
#define NV_NUVEM_H

// Le a configuracao. A ordem e: valores de compilacao (-DNV_SUPABASE_URL e
// -DNV_SUPABASE_ANON_KEY, gerados de local.properties por tools/env.sh) e, se
// existir, `art/nuvem.txt` sobrescrevendo — o arquivo e para desenvolvimento,
// nao para o pacote. Formato: tres linhas, url / anon key / base do login.
// Devolve 1 quando ha url e chave.
int nuvem_configurar(const char *dirArte);

int         nuvem_pronta(void);
const char *nuvem_url(void);
const char *nuvem_anon(void);
// Base da pagina web que a pessoa abre no celular para autorizar a TV
// (TV_LOGIN_WEB_BASE_URL no web). "" quando nao configurada.
const char *nuvem_base_login(void);
// Client id do APLICATIVO no Trakt (nao e da pessoa). Vem compilado, como no
// app web; o token do usuario vem do sync.
const char *nuvem_trakt_cliente(void);
// Segredo do APLICATIVO no Trakt. So o fluxo de device code precisa dele, na
// troca do codigo pelo token; nenhuma outra chamada o usa.
const char *nuvem_trakt_segredo(void);
// Chave e nome do aplicativo no Simkl. O Simkl nao usa segredo: os dois vao na
// QUERY de todo pedido, e sem eles a resposta e erro sem explicacao.
const char *nuvem_simkl_cliente(void);
const char *nuvem_simkl_app(void);

// POST com Bearer explicito. Devolve o corpo (free pelo chamador) ou NULL
// quando a requisicao nem saiu. `status` recebe o codigo HTTP — 0 quando nao
// houve resposta. O corpo de erro VOLTA: e nele que o PostgREST diz qual
// funcao nao existe, e essa string e o unico jeito de distinguir "servidor
// antigo" de "parametro errado".
char *nuvem_post(const char *caminho, const char *corpoJson,
                 const char *bearer, int *status);

// Falha de transporte do ultimo nuvem_post/nuvem_tabela deste fio ("curl 35"), "" se nao houve.
const char *nuvem_ultimo_erro(void);

// Atalho para /rest/v1/rpc/<funcao>.
char *nuvem_rpc_com(const char *funcao, const char *corpoJson,
                    const char *bearer, int *status);

// GET em /rest/v1/<tabela>?<consulta>. MEDIDO: nem toda superficie tem RPC —
// `sync_pull_addons` NAO EXISTE neste servidor (PGRST202), e a lista de addons
// so sai lendo a tabela `addons` direto, como o app web faz no caminho feliz.
char *nuvem_tabela(const char *tabela, const char *consulta,
                   const char *bearer, int *status);

// Update only enabled on an existing addon row; never insert or replace a list.
char *nuvem_addon_alternar(const char *consulta, int ativo, const char *bearer, unsigned prazo_ms, int *status);

// Escapa um valor para entrar numa consulta de URL. Sem isto um id com "+" ou
// "&" quebraria o filtro em silencio e a leitura voltaria a tabela inteira ou
// vazia — nenhum dos dois se parece com erro.
void nuvem_url_escapar(const char *valor, char *dst, unsigned tam);

// 1 quando a resposta de erro indica que a FUNCAO ou a TABELA nao existe neste
// servidor (PGRST202/PGRST205). O web usa isso para cair em tabela legada;
// aqui serve para dizer "esta superficie nao existe" em vez de tentar de novo
// para sempre.
int nuvem_erro_ausente(const char *corpoErro);

// Freio compartilhado. Uma falha de rede derruba TODAS as superficies juntas
// por um tempo: sem isso, um servidor fora do ar vira doze laços de retry em
// paralelo, cada um consumindo o mesmo tempo de CPU que o desenho precisa.
void nuvem_falhou(void);
void nuvem_ok(void);
int  nuvem_freio_ativo(void);

#endif
