# Plano de entrega da 1.0.57 — polir, ligar e testar na TV

Escrito em 16/09/2026, com seis frentes prontas na árvore e **nada commitado**.
O plano existe porque o risco desta entrega não é escrever código: é que muita
coisa nova chegou junto, e três dos pedaços (serieaud, seriefrases, Worker)
**não estão ligados a nada** — passam em teste e não aparecem no app.

---

## 0. O que já está na árvore, e em que estado

| frente | código | testado no Mac | ligado no app | na TV |
|---|---|---|---|---|
| Agenda + lembrete + cartão de aviso | pronto | sim (11 capturas) | **sim** | não |
| Biblioteca: listas Trakt/Simkl/Nuvio, cartão×lista, fixar | pronto | sim (12 capturas) | **sim** | não |
| Social: descobrível + sugestões | pronto | sim (42 testes de servidor) | **sim, cliente** | não |
| Worker `nuvio-recomendacoes` (avatar, nota, descobrivel) | pronto | sim | **NÃO — não deployado** | — |
| `serieaud` (arco, radar, digital) | pronto | sim (10 grupos) | **NÃO — sem fio no detail.c** | não |
| `seriefrases` (frases + ficha) | pronto | sim (9 grupos) | **NÃO — sem fio no detail.c** | não |
| Ícone do lembrete minimalista | agente rodando | — | — | — |
| Frases em forma de citação | agente rodando | — | — | — |

Duas coisas de sessões anteriores também entram nesta versão e **nunca foram
para a TV**: o botão "Atualizar o aplicativo" nos Ajustes e a conversão de foco
contornado para foco preenchido (fontes, painel lateral, biblioteca, canal).

---

## 1. Decisões que são suas, e que travam o resto

Não vou escolher por você, porque cada uma muda o que vai para a TV.

1. **Varredura automática dos seguidos do Trakt.** O agente do Social a
   removeu do arranque: ela transformava em contato, dos dois lados e sem
   ninguém apertar nada, toda pessoa que você segue no Trakt e que usa o
   serviço. Manter removida, ou repor? (Repor é uma linha; o comentário no
   código diz qual.)
2. **Comentários do Trakt na Agenda.** Existe fonte (`extras.c:398` já usa),
   custa **1 pedido por série da lista** — agenda de 20 séries = 20 pedidos,
   sem cache hoje. Entra, entra com cache, ou fica fora?
3. **Gosto parecido / ver histórico.** Os dois exigem mandar o que você assiste
   para o nosso servidor, coisa que hoje nunca sai do aparelho. A recomendação
   do agente é o caminho sem escalada: vitrine dos **5 títulos que a pessoa
   mais recomendou**, tirada da tabela `rec` que já existe, zero coluna nova.
4. **Idioma das frases.** O Wikiquote tem página em português para poucos
   títulos; hoje as falas saem em inglês, no original. Fica assim?

O passo 2 e o 4 podem ser decididos depois da TV — os outros dois não, porque
mudam o servidor e o pacote.

---

## 2. Ligar o que está solto (o maior buraco)

**2.1 `serieaud` e `seriefrases` no `detail.c`.** Hoje os cinco painéis existem
e nenhum é alcançável. Falta: seção nova na página de título, navegação de foco
entrando e saindo dela, ciclo de vida (`serieaud_abrir` ao ENTRAR na seção,
nunca ao abrir a página — o custo de rede depende disso), e `*_fechar()` ao
sair do título. `serieaud_abrir()` recebe as notas por episódio por parâmetro,
que vêm de `extras_ep_nota()` e já estão em memória.

**2.2 `trakt_cliente()`.** `/stats` funciona só com o client id, mas o único
caminho para a chave neste app é `trakt_cabecalhos()`, que exige token. Sem
isso os três painéis de audiência **só funcionam com Trakt vinculado**. Não é
regressão (as notas por episódio já dependiam disso), mas é uma linha em
`trakt.c` que faz o recurso existir para quem não vinculou. Decidir se entra
nesta versão.

**2.3 Aparecer/desaparecer nos Ajustes.** O interruptor "Aparecer para outras
pessoas" hoje só existe no fim da aba Social. Duas linhas na seção de conta o
põem também nos Ajustes; o estado é um só. Opcional.

---

## 3. Fechar a árvore

1. Mesclar os pares de i18n que os dois agentes em voo deixarem
   (`scratchpad/i18n-icone.txt`, `scratchpad/i18n-frases.txt`) —
   **um merge só, com o script que valida a ordem antes e depois**. A tabela
   está em 1164 pares e já quebrou uma vez hoje por ordenação sem decodificar
   os escapes.
2. `bash tools/mac.sh` — a árvore inteira, que passa de 10 minutos aqui.
3. `bash tools/testa-tudo.sh` — verde, com as exceções conhecidas anotadas
   (`conta.sh` falha por falta de `NUVIO_SUPABASE_URL`/`ANON_KEY` no ambiente,
   e falha igual em HEAD limpo).
4. Commits separados por frente, não um commit só. Quem for procurar o defeito
   depois precisa poder reverter uma frente sem levar as outras.

---

## 4. Servidor — ordem obrigatória

```
# 1) migração PRIMEIRO
npx wrangler@4 d1 execute nuvio-recomendacoes --remote \
  --config servidor/recomendacoes/wrangler.toml \
  --file servidor/recomendacoes/migracao-002-descobrivel.sql

# 2) conferir que a coluna existe
#    PRAGMA table_info(pessoa) — a migração 001 já rodou em silêncio uma vez
#    hoje sem aplicar nada, e só o PRAGMA pegou

# 3) só então
npx wrangler@4 deploy --config servidor/recomendacoes/wrangler.toml
```

Invertido dá **erro de SQL em toda sondagem de toda TV**: o código novo lê
`p.descobrivel` no registro. A migração é só aditiva; rodar duas vezes responde
"duplicate column name" e não estraga nada.

Depois: `bash servidor/recomendacoes/teste.sh` contra o ambiente remoto, e uma
sondagem manual de `/v1/sugestoes` para ver que o padrão é **não aparecer**.

---

## 5. Pacote e instalação na C9

1. Subir a versão nos **dois** arquivos — `deploy/app/appinfo.json` e
   `tools/tizen-config.xml`. `tools/env.sh` aborta se discordarem.
2. `git status` **antes** de empacotar: `arm.sh` embarca a árvore inteira, então
   o pacote leva tudo que estiver não commitado. Sem isso eu não sei dizer o
   que está de fato na TV.
3. `bash tools/arm.sh --ipk` (compila, empacota e instala na C9).
   TV em `192.168.1.32` (`tv-lg`), **ProxyJump por `zimaos`** — timeout que
   parece TV caída costuma ser o Tailscale parado; `-o ProxyCommand=none` pela
   LAN resolve.
4. Conferir o que subiu por `/proc/<pid>/exe`, **não pelo título no log do
   SAM** — o app manager cacheia o appinfo até reinstalar.
5. Conferir credencial no pacote antes de qualquer publicação:
   `ar p <ipk> data.tar.gz | tar tz | grep 'art/<arquivo>$'` para `trakt.txt`,
   `addons.txt`, `tmdb.txt`, `mdblist.txt`, `sessao.txt` e `collections.json`.
   Tudo tem de dar **0**.

---

## 6. Roteiro de teste na TV

Captura na LG é `.bmp`, e injetar tecla exige `/tmp/nuvio-key` com conteúdo e
`chown 5152`.

**Agenda**
- [ ] Item novo no menu lateral abre a Agenda
- [ ] Eixo, numerais e cabeçalho de mês legíveis **do sofá**, não no monitor
- [ ] "HOJE" ancorado no topo; linha em foco preenche só a coluna de conteúdo
- [ ] Sinopse abre na linha em foco e fecha ao sair, com reticências e não
      cortada no meio da palavra
- [ ] Cauda "sem data" com trilho tracejado e anel vazado
- [ ] OK liga/desliga o lembrete; sobrevive a fechar e reabrir o app
- [ ] Com "reduzir animações" ligado: sem tremor, sem pulso, estado legível
- [ ] Série que acabou/foi cancelada aparece na cauda, não como data falsa
- [ ] Cartão de aviso aparece no dia, uma vez só, e não volta depois de fechado

**Lembrete (botão do herói)**
- [ ] Só o relógio, sem rótulo
- [ ] Desligado / ligado / em foco nos dois estados — quatro capturas
- [ ] Verde legível na TV real (a calibragem da C9 não é a do monitor)
- [ ] Linha de estado acima diz nome + estado quando o relógio está em foco

**Biblioteca**
- [ ] Abas Saved / Collection / Listas: escolhida **preenchida, sem contorno**
- [ ] Os três degraus de brilho distinguíveis do sofá
- [ ] Wordmark do Trakt limpo, sem borrão, claro no escuro e escuro no foco
- [ ] Listas do Trakt carregam; busca pública funciona **sem** conta vinculada
- [ ] Simkl: **ponto de maior risco** — o parser foi escrito contra o schema
      documentado e **nunca viu resposta real** (não há token aqui). Testar com
      conta real, ou aceitar que a aba pode vir vazia
- [ ] Cartão × lista: trocar e o modo sobreviver ao reinício, por perfil
- [ ] Fixar na Biblioteca e adicionar à Home; a fileira **sobrevive a um pull
      da conta** (o defeito mudo que o teste cobre)
- [ ] Sair da conta leva as listas fixadas junto

**Social**
- [ ] Primeira entrada mostra a pergunta; "Não" vem primeiro
- [ ] Recusar mantém recomendação funcionando nos dois sentidos
- [ ] Aceitar faz aparecer para quem te segue no Trakt
- [ ] Sugestão de amigo-de-amigo mostra **um** intermediário
- [ ] Adicionar sugerido funciona; id fora da lista dá 403
- [ ] Interruptor no fim da aba desliga e some das sugestões dos outros

**Série — audiência e frases** (depende do passo 2)
- [ ] Painéis só buscam ao **entrar na seção**
- [ ] Curva cresce durante a busca; sair no meio interrompe
- [ ] Reabrir usa cache e pede só o que falta
- [ ] Rótulos honestos visíveis: "não é o IMDb", "não é a audiência geral",
      "não é curiosidade escrita por nós"
- [ ] Título sem página no Wikiquote mostra o estado vazio, não tela em branco
- [ ] **Rede lenta na TV nunca foi exercitada** — só no Mac. Testar com a TV no
      Wi-Fi, não no cabo

**Não regredir**
- [ ] Home, player, fontes, painel lateral, guia de canais
- [ ] Botão "Atualizar o aplicativo" nos Ajustes
- [ ] Cartão de canal em duas cores
- [ ] Memória: `tex_estatisticas` depois de passear pela Agenda, Biblioteca em
      cartaz e uma série com os painéis abertos

---

## 7. Publicar

Só depois de a TV confirmar. Notas em inglês, `## Fixed` / `## Added`, balas
curtas — o cartão do app mostra 3 linhas por bala. `git push`, `git tag`,
`gh release create` com o `.ipk` e o `.wgt`. O `.wgt` do Tizen sai por
`tools/tizen.sh` + `NUVIO_WGT_NOME="NuvioTV-1.0.57-tizen" tools/tizen-wgt.sh`.

**Não dizer "corrigido" em nenhuma issue antes de a release existir com os
pacotes anexados.**

---

## 8. O que este plano NÃO cobre, de propósito

- **webOS 3**: o worktree `../nuvio-native-webos3` vai precisar de outro merge,
  e a regra continua: `grep -rn SDL_CreateRGBSurfaceWithFormat src/` tem de dar
  zero fora do `sdlcompat.h`.
- **Tizen**: nenhuma Samsung aqui. "Empacota" não é "funciona".
- **Notícias sobre a série**: não existe fonte. Não foi construído.
- **Pulo para a minutagem da fala**: medido e descartado — casamento de 13% a
  56% conforme o filme, e o tempo vale só para aquele arquivo de legenda.
