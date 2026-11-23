# Acompanhamento da release 2.1

Devin App interno, em português, independente do painel público de issues e do aplicativo de TV. Não altera código nativo, não faz merges e não publica o worker do Farol. Layout workbench com o acento rosa do Nuvio (PR #812, `.impeccable/design.json` e `src/layout.h` na integração); sem efeitos de vidro neste painel de coordenação.

## Executar

Python 3.10+; nenhuma dependência ou etapa de build:

```sh
PORT=3000 python3 tools/painel-21/servidor.py
```

O servidor de produção usa `ThreadingHTTPServer`, serve somente os quatro assets do painel e o JSON, não lista diretórios e não oferece API de escrita. A autenticação do Devin App controla o acesso da organização. Não publicar este inventário como site público.

## Uma única fonte de dados

`release.json` contém itens, dependências por ID, estados, ondas de execução A–E, fases numéricas do plano, lotes, referências de commits, fontes, observações, histórico e pendências do Henrique. A tela consulta o arquivo a cada 30 segundos; os totais e as dependências abertas são calculados dele. Busca e filtros não se perdem na atualização. Exportar JSON baixa uma cópia, sem alterar a fonte.

Inventário inicial: 56 itens Farol (o #42 não veio no escopo), PRs #815–#830, UX/UI #812/#813 e a fila externa da VPS. A lista de 61 linhas da fila contém #731 repetida; sua unidade foi deduplicada. D1 tem oito cartões, um por issue, com `reviewUnit: "D1"`; 762+720 tem dois cartões com a mesma unidade. Por isso, cartões não equivalem ao número de unidades de revisão. Commits só de teste estão identificados. Issues e portes de PRs com o mesmo número são itens distintos (por exemplo `issue-763` e `pr-763`).

`FEITO` significa integrado segundo a fonte, **não** teste funcional nem deploy. `integrationVerified` registra a ancestralidade verificada pela VPS; `verified` registra validação funcional. O D1 foi relatado como feito, mas sua ancestralidade permanece não verificada. As pendências de TV/worker são independentes do estado do código e continuam abertas após a integração. Ondas não informadas não são inventadas. Os fechos têm `allDependencies: true`.

## Aplicar relatos da VPS

```sh
python3 tools/painel-21/atualizar.py --check
python3 tools/painel-21/atualizar.py --check \
  '[IMPLEMENTANDO] Farol 6 · agente/farol-6@abcdef123 · motor em implementação'
python3 tools/painel-21/atualizar.py \
  '[IMPLEMENTANDO] Farol 6 · agente/farol-6@abcdef123 · motor em implementação'
python3 tools/painel-21/atualizar.py \
  '[LOTE] lote-21d · lote/21d@bbf45a751 · testes em andamento; PR ainda não aberta'
# Várias linhas: python3 tools/painel-21/atualizar.py --stdin < relatos.txt
```

Estados aceitos: `A_FAZER`, `IMPLEMENTANDO`, `REVISAO`, `CORRECAO`, `APROVADA`, `LOTE`, `PR_ABERTA`, `FEITO`, `BLOQUEADA`, `POR_ULTIMO`.

IDs explícitos são preferíveis: `farol-6`, `issue-714`, `pr-830`, `lote-21d`. Também aceita `Farol 6`, `PR 830`, números de correções não ambíguos e unidades `D1`, `762+720`, `707-r2`. Um número ambíguo falha em vez de mudar o item errado. `—` no campo de referência preserva a referência anterior. O campo observação pode conter mais separadores `·`.

O importador valida o lote inteiro antes de substituir o arquivo atomicamente. Se qualquer relato falhar, nada é escrito. Não cria itens a partir de textos desconhecidos, não infere merge a partir de PR aberta e não encerra pendências do Henrique. A execução deve ser serializada pela coordenação (um escritor por vez). Novos itens e decisões, PR de um lote e a integração publicada são atualizados explicitamente no JSON; o estado de um lote não muda automaticamente seus itens nem o SHA publicado.

Para confirmar um retorno de TV/deploy, editar a respectiva pendência em `gates` para `status: "feito"`, conservando a evidência em `note`; só fazer isso com confirmação. Para decisão de produto, adicionar uma pendência com `kind: "decision"`, `status: "pendente"` e os IDs relacionados. Manter a fonte e `meta.updatedAt` atuais.

O Bot pode encaminhar novos relatos à sessão que gerencia o App. Ela aplica o importador neste checkout, valida, faz commit e publica uma nova versão no mesmo nome do App. Alterações do JSON não exigem recompilar. O link fixo continua válido enquanto a sessão gerenciadora não for arquivada; abrir o App acorda a sessão quando estiver dormindo.

## Verificar

```sh
python3 -m unittest discover -s tools/painel-21 -p 'test_*.py' -v
python3 tools/painel-21/atualizar.py --check
node --check tools/painel-21/app.js
```

Não é necessário configurar as ferramentas de build de TV, o worker ou credenciais de produção para servir este painel.
