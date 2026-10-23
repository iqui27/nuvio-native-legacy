# Nuvio 2.1: Conquistas e emblemas (Perfil › Conquistas)

Mockups estáticos, mesmo quadro de TV do resto de `design/2.1`. Abra `conquistas.html` (ou pelo `index.html`; o ícone de perfil do rail em `listas.html` e `trilhas.html` leva para lá, e a trilha tem um botão de troféu e a linha "Emblema" que abrem o emblema). A tecla **D**, ou o botão "dados" da barra, contorna cada elemento pela origem do dado, igual a `NOTAS-LISTAS.md`:

- **verde · hoje**: dá para fazer só no app, com o que Trakt, TMDB e o nosso servidor já entregam;
- **âmbar · nosso**: precisa de trabalho no `servidor/recomendacoes` (tabela, rota, script ou texto de curadoria);
- **vermelho · bloqueado**: a fonte não oferece isso a apps, ou só raspando, e raspagem está fora.

Nomes de pessoas, @handles, diretores, filmes, contagens ("412 terminaram", "3,1 mil") e textos são inventados. "Nova onda japonesa" é um recorte real; os 8 filmes e "Kenji Ōmura" não existem.

Pedido do dono: *"As trilhas e novas melhorias vão estar ligadas à gamificação de ganhar badges etc e conquistas"*.

**Não confundir:** `src/badges.c` são os selos de qualidade da fonte (4K, HDR). Nada a ver com conquistas.

## O pedido e onde está

| Parte | Quadro |
|---|---|
| 1. Hub de conquistas no perfil | 8A (abas, cartões, "Retomar", mapa de descobertas), 8B (seis famílias) |
| 2. Emblema por trilha, lista curada, diretor, explorador, prêmio | 8B, 8C (cinco níveis de uma trilha) |
| 3. Momentos: aviso depois dos créditos, passo e trilha concluídos | 8D, 8E, 8F |
| 4. Social: perfil do amigo, ritmo na mesma trilha, duelo gentil, emblemas de comunidade ligados à moderação | 8G, 8H, 8I |
| 5. Desafios de temporada (opcional) | 8J |
| Tudo desligável | 8K (Ajustes › Conquistas) |

## Princípios

1. **Premiar curiosidade e descoberta, não tempo de tela.** Entra: ver um filme de um movimento novo, terminar a filmografia de alguém, ver um país que você nunca viu. Fora: horas assistidas, maratona, hora do dia.
2. **Sem sequência que pune, sem FOMO.** Nada de "não perca o dia". Trilha de temporada fica aberta o ano todo; só o carimbo do ano é enfeite. Emblema de assistir nunca some.
3. **Tudo se esconde em Ajustes.** Liga/desliga geral, aviso, quem vê, secretos, raridade, temporadas, progresso com amigos, apagar tudo. Com tudo desligado trilhas e listas funcionam igual.
4. **Legível a 3 m, D-pad primeiro.** Fonte mínima de 15 px no quadro de 1920 (18 px ou mais no que se lê de verdade); o aviso não rouba o foco.
5. **Calmo.** Sem confete, sem som, sem contagem regressiva. Respeita "reduzir movimento".
6. **Honesto sobre a origem.** Todo elemento tem marca de origem do dado (`data-d`).
7. **Privacidade como no resto do app.** Emblema só aparece para amigos se a pessoa deixou (`pessoa.alcance` ≥ 1). Emblema "engraçado" nunca fica visível (regra que já existe na Caça).
8. **Competição sem placar.** O duelo mostra os dois lados sem vencedor, sem ranking, sem aviso para o outro.

## O que já existe no código

- Caça, `servidor/recomendacoes/src/conquista.js`:
  - `GET /v1/filmografia`, `GET /v1/trilhas`, `GET /v1/trilha?id=` (prêmios do Wikidata, CC0, e top 250 do Trakt).
  - `POST /v1/conquista {chave, nivel 1..5, visivel, nome, vistos, total}`, enviado **só ao desbloquear**. Nível só sobe.
  - Chave aceita: `^(dir|ator|saga|top|trakt|trilha):[A-Za-z0-9._\/-]{1,80}$`.
  - Tabela `conquista` na `migracao-013`, **não aplicada**.
  - `conquistasDe()` devolve até 6 recentes e visíveis em `/v1/amigo`, só se o dono tem `alcance >= 1`.
- Trilhas de prêmio: `oscar-melhor-filme` com 98 filmes e `oscar-filme-internacional` com 71 (conferido em `trilhas-dados.js`).
- O cliente da Caça **não está nesta branch**: os nomes dos cinco níveis do mockup (Primeiro passo, Meio caminho, Reta final, Trilha completa, Em ordem) são inferidos, e a regra de cada nível precisa de confirmação.

## Catálogo (rascunho)

Raridade em quatro faixas, pelo % de pessoas que têm o emblema: **Comum** (> 30%), **Incomum** (10–30%), **Rara** (1–10%), **Raríssima** (< 1%). Os cortes são palpite. "Name en" é o nome em inglês.

| # | Família | Nome pt | Name en | Regra | Faixa |
|---|---|---|---|---|---|
| 1 | Trilhas e listas | Primeiro passo | First step | 1º filme de qualquer trilha curada (nível 1 de cada trilha) | Comum |
| 2 | Trilhas e listas | Meio caminho | Halfway there | metade da trilha (nível 2) | Incomum |
| 3 | Trilhas e listas | Reta final | Home stretch | 75% da trilha (nível 3) | Incomum |
| 4 | Trilhas e listas | Trilha completa | Trail complete | todos os filmes, em qualquer ordem (nível 4) | Rara |
| 5 | Trilhas e listas | Em ordem | In order | todos, na ordem do curador (nível 5) | Rara |
| 6 | Trilhas e listas | Lista concluída | List finished | 100% de uma lista pública com 10+ filmes | Incomum |
| 7 | Trilhas e listas | Fiel ao curador | Loyal to the curator | concluir 3 listas do mesmo curador | Rara |
| 8 | Trilhas e listas | Curador convidado | Guest curator | convite da Curadoria Nuvio, depois de revisão (manual) | Raríssima |
| 9 | Diretores | De ponta a ponta | End to end | todos os longas de um diretor | Rara |
| 10 | Diretores | Completista | Completist | 3 diretores completos | Raríssima |
| 11 | Diretores | Retrospectiva | Retrospective | 5+ filmes de um diretor em ordem de lançamento | Incomum |
| 12 | Diretores | Mesmo olhar | Same eye | 3 filmes seguidos do mesmo diretor | Comum |
| 13 | Explorador | Décadas | Decades | filmes de 8 décadas diferentes | Rara |
| 14 | Explorador | Volta ao mundo | Around the world | filmes de 10 países | Rara |
| 15 | Explorador | Aluno de movimento | Movement student | 5 filmes de um mesmo movimento (Nouvelle Vague, Cinema Novo...) | Incomum |
| 16 | Explorador | Fora do idioma | Beyond English | 10 filmes sem falar a sua língua nem inglês | Incomum |
| 17 | Explorador | Do silêncio | From the silent era | 3 filmes anteriores a 1930 | Rara |
| 18 | Explorador | Documentário de verdade | Real life | 5 documentários | Comum |
| 19 | Prêmios · Caça | Melhor Filme, em ordem | Best Picture, in order | trilha `oscar-melhor-filme` (98 filmes) | Raríssima |
| 20 | Prêmios · Caça | Filme Internacional | International Feature | trilha `oscar-filme-internacional` (71) | Raríssima |
| 21 | Prêmios · Caça | Palma de Ouro | Palme d'Or | Palma de Ouro (precisa do script do Wikidata) | Rara |
| 22 | Prêmios · Caça | Top 250 | Top 250 | trilha `top:` do Trakt: 25, 50, 100, 250% (níveis) | Rara |
| 23 | Comunidade | Boa conversa | Good conversation | 6 de 10 curtidas em comentários seus (nos últimos 10) | Incomum |
| 24 | Comunidade | Spoiler fechado | Spoiler closed | 10 comentários com spoiler marcado | Incomum |
| 25 | Comunidade | Lista adotada | Adopted list | 25 pessoas fixaram uma lista sua | Rara |
| 26 | Comunidade | Bom olho | Good eye | 3 notas de curador aceitas em listas da Curadoria | Rara |
| 27 | Temporadas | Outubro do terror | Horror October | trilha sazonal de outubro, 8 filmes | Incomum |
| 28 | Temporadas | Março das diretoras | Women Directors March | trilha sazonal de março | Incomum |
| 29 | Temporadas | Palma em maio | Palme in May | trilha sazonal de maio (Cannes) | Incomum |
| 30 | Secretos | Dois lados | Both sides | ver e dar nota a dois filmes de visões opostas do mesmo tema (dica no cartão, se ligado) | Raríssima |

Fora do catálogo de propósito: sequência de dias, horas assistidas, maratona, "primeiro a ver", hora do dia, nota alta ou baixa.

## Viabilidade por elemento

| Quadro | Elemento | Fonte | Situação | Nota |
|---|---|---|---|---|
| 8A | Hub, abas, "Retomar" | emblemas locais + progresso local | hoje | |
| 8A | Raridade ("Rara · 5%") | contagem agregada no servidor | nosso | anônima; ver pergunta 1 |
| 8A | Mapa de descobertas (décadas, países, movimentos) | histórico do Trakt + Wikidata (P135, P495, P577) | nosso | script irmão de `trilhas-wikidata.py` |
| 8B | Diretor, ator, prêmio, top 250 | `/v1/filmografia`, `/v1/trilhas` | hoje | família já suportada pelas chaves `dir`, `ator`, `top`, `trakt` |
| 8B | Trilha curada e lista | curadoria + listas Trakt | nosso (curada) / hoje (lista Trakt) | chave `lista:` é nova |
| 8B | Explorador (movimento, década, país) | Wikidata | nosso | chaves `mov:`, `dec:`, `pais:` são novas |
| 8B | Comunidade | servidor + moderação | nosso | chave `com:` nova |
| 8B | Temporadas | trilha sazonal no servidor | nosso | chave `sazon:` nova |
| 8B | Importar emblemas do Letterboxd ou Simkl | nenhuma das duas oferece | bloqueado | |
| 8C | Cinco níveis, "Como conta" | histórico do Trakt ou "Já vi" | hoje | trilha curada: catálogo nosso |
| 8C | Amigos nesta trilha (X de 8) | `/v1/amigo` estendido | nosso | respeita alcance |
| 8D | Aviso depois dos créditos | fim da reprodução detectado no app | hoje | 6 s, não toma foco, agrupa vários |
| 8E, 8F | Passo e trilha concluídos | progresso local | hoje | |
| 8G | Emblemas do amigo (6 recentes) | `conquistasDe()` | hoje | já existe; migração 013 pendente |
| 8G | Trilhas em comum, progresso por trilha | servidor | nosso | |
| 8H | Duelo gentil (você × amigo) | os dois progressos | nosso | sem placar, sem aviso ao outro |
| 8I | Comentário em análise pausa o emblema | moderação do servidor | nosso | depende de comentários nas nossas listas (decidido em `NOTAS-LISTAS.md`) |
| 8J | Trilha sazonal | curadoria | nosso | |
| 8K | Interruptores de Ajustes | local; "apagar" também no servidor | hoje (local) / nosso (apagar) | |
| 8K | Apagar minhas conquistas | rota de exclusão no servidor | nosso | histórico do Trakt não é mexido |

**Resumo:** o núcleo (diretor, ator, prêmios, top 250, trilha de lista do Trakt, aviso, perfil do amigo) já cabe no que a Caça entrega. O que dá vida (trilha curada, raridade, explorador, comunidade, temporadas, duelo) precisa do nosso servidor. Nada depende de raspagem; o único bloqueio é importar emblemas de outros serviços.

## Ordem sugerida

1. **Só app + Caça atual:** hub, emblemas de diretor, ator, prêmio e top 250, aviso depois dos créditos, perfil do amigo, Ajustes. Aplicar a migração 013.
2. **Servidor:** raridade agregada, trilha curada (mesma tabela de listas curadas de `NOTAS-LISTAS.md`), estender o regex da chave (`mov:`, `dec:`, `pais:`, `com:`, `sazon:`, `lista:`), script de Wikidata para movimento, país e Palma de Ouro, progresso de amigo por trilha.
3. **Depois:** duelo gentil, comunidade (depende de moderação pronta), temporadas.
4. **Não fazer:** importar emblemas de outros serviços; qualquer coisa baseada em horas.

## Mudanças fora da tela nova

- `index.html`: cartão 8 e o texto do rodapé.
- `trilhas.html`: o ícone de perfil do rail leva a `conquistas.html#a`; botão de troféu ao lado de "salvo"; a linha "Emblema" de "Sobre a trilha" abre o emblema (`#c`).
- `listas.html`: o ícone de perfil do rail leva a `conquistas.html#a` (3 quadros).
- `tv.js`: ícones novos `trofeu`, `claquete`, `chat`, `fantasma`, `bandeira`.

## Perguntas em aberto

1. **Raridade:** o denominador é "todas as TVs que ligaram conquistas" ou "todas que abriram a trilha"? E a contagem anônima serve, ou só conta quem tem alcance ≥ 1? (mesma dúvida da pergunta 8 de `NOTAS-LISTAS.md`)
2. **Regex da chave:** aceitar estender com `mov:`, `dec:`, `pais:`, `com:`, `sazon:`, `lista:`? Cada família nova é mudança de servidor.
3. **Nomes dos níveis:** os cinco nomes são inferidos. Confirmar com o cliente da Caça (não está nesta branch) e, se for outro, ajustar o mockup.
4. **Histórico do Trakt vale retroativo?** O mockup diz que filme visto antes de abrir a trilha conta para os níveis 1 a 4, e o aviso vem uma vez, no fim da sessão. Aceitável, ou só conta a partir da ativação?
5. **Séries em trilhas:** o desenho só tem filmes (mesma pergunta 7 de `NOTAS-LISTAS.md`).
6. **Moderação dos emblemas de comunidade:** critério, quem decide e prazo. "Bom olho" ainda corre o risco de premiar quem denuncia demais; pode sair do catálogo.
7. **Dono do conteúdo sazonal:** quem escreve e publica as temporadas, e com que antecedência.
8. **"Esconder" em Ajustes:** só oculta na tela ou também para de calcular e enviar? O mockup diz que desligar esconde o perfil, avisos e emblemas dos amigos, e religar recalcula do histórico. Confirmar que "recalcula" é aceitável.
9. **Migração 013 não aplicada:** em que ordem entra, e com que dados de teste.
10. **Contagens 98 e 71** das trilhas de prêmio estão certas hoje, mas o Wikidata muda. Fixar a contagem na hora de gerar?
11. **Secretos:** dica no cartão por padrão ou só ao desbloquear? O mockup deixa "Com dica" como padrão.
