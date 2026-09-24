# Triagem automática de logs e issues

Roteiro para a rotina agendada do Claude (nuvem). Ela clona este repositório,
lê os registros novos no D1, cruza com as issues abertas e publica um resumo
AGREGADO. O estado entre rodadas fica na tabela `triagem` do D1
(`servidor/recomendacoes/migracao-004-triagem.sql`), nunca no git: o
repositório é público e os registros trazem ids de pessoa.

## Ambiente

- `CLOUDFLARE_API_TOKEN` (permissão só de D1, conta do projeto) e
  `CLOUDFLARE_ACCOUNT_ID`, como variáveis do ambiente da rotina.
- Acesso ao GitHub pelo app do Claude (issues e comentários).
- Banco: `nuvio-recomendacoes`. Comandos a partir de `servidor/recomendacoes`:
  `npx --yes wrangler d1 execute nuvio-recomendacoes --remote --json --command "<SQL>"`.
  A saída tem um prefixo de ruído antes do JSON: ler a partir do primeiro `[`
  seguido de quebra de linha. Erro 7403 é transitório: repetir após alguns
  segundos. Buscar `texto` um registro por vez (é lento e pesado).

## Tabelas

- `registro(id, pessoa, versao, plataforma, quando, texto, criado)`: logs.
  - `quando` com `(auto)`: envio periódico da sessão atual.
  - `(anterior)`: log da sessão anterior, mandado a cada abertura. Não é crash.
  - `(manual)`: a pessoa mandou, geralmente para mostrar um problema. Ler
    esses primeiro.
  - `texto` começando com `diagnostico=v2`: relatório do Diagnóstico, em chave=valor.
  - Crash de verdade: na abertura seguinte aparece
    `[avisos] a sessao anterior (...) nao se despediu`. Na LG, sessão normal
    termina com `fim`.
  - Excluir `pessoa = 'trakt:iqui27'` (o dono).
- `triagem(criado, ultimo_log, padrao, versao, plataforma, ocorrencias,
  pessoas, issue, estado)`: o que a rodada anterior viu.
  - `estado`: `novo`, `conhecido`, `corrigido-em-X.Y.Z` ou `regrediu`.

## Rodada

1. `SELECT max(ultimo_log) FROM triagem` → ler `registro` com `id >` esse valor.
2. Agrupar por padrão, com contagem de ocorrências e de pessoas distintas, por
   versão e plataforma. Padrões conhecidos:
   - `Aborted(`, `RuntimeError`: crash WASM (Samsung).
   - `nao se despediu`: sessão morta.
   - `[video] avplay erro`, `errorText` diferente de `"No Error"`: erro de player.
   - `[mkvass]` e `[legenda]` com falha ou fallback para a TV: legenda ASS (#92).
   - `montagem descartada`: ler o motivo; `(addons)` e `(identidade)` são
     esperados.
   - `padrao fora da lista`: ajuste zerado (#129; corrigido na 1.4.4).
   - `decode falhou` com caminho que não começa por `/` ou `http`:
     caminho corrompido.
   - `[debrid] ... baixando no TorBox`, `fora de cache`, `PLAN_RESTRICTED`:
     TorBox/P2P.
   - `longtask-max=` acima de 2000 ms: travada longa (Samsung).
   - `[desc] ... de fora por cota`: catálogos cortados pelo limite por addon (#126).
   - Nos relatórios `diagnostico=v2`: contar os valores de `aplicacao`
     (`aplicada`, `ja_no_perfil`, `mantido_ruido`, `restaurada_auto`).
3. Comparar com a `triagem` anterior. Padrão marcado como `corrigido-em-X`
   que volta numa versão maior ou igual a X vira `regrediu`.
4. Gravar uma linha por (padrão, versão, plataforma) em `triagem`, com o novo
   `ultimo_log`.
5. `gh issue list --state open`: ligar padrões a issues pelo número ou pelo
   assunto.
6. Comentar na issue fixada "Relatório de logs" (label `triagem`; criar se
   não existir):
   - só números agregados, padrões novos, regressões e issues relacionadas;
   - nunca id de pessoa, trecho de log com URL de addon ou debrid, nem token.

## Não fazer

- Não commitar logs, ids de pessoa ou resultado bruto no repositório.
- Não responder nem fechar issues de usuários: só o comentário na issue fixada.
- Não dizer "corrigido" sem a versão publicada que contém o conserto.
