# Pré-download da legenda automática

A lista de addon agora pode disparar o download durante o preparo do vídeo, no próximo quadro de `faixas_atualizar`. Apenas a faixa exata de addon lembrada para o título é antecipada: `legendaExata` é compartilhada com a automática e chama `legmem_exata`. A escolha por idioma e as embutidas mantêm o fluxo existente.

`legenda.c` guarda um candidato por play. `legenda_carregar_com` adota o corpo ou aguarda o fio em curso quando o link final coincide, sem outra requisição. Conversão, parser e callback `pronto` continuam no fio do carregador; AutoSync recebe o mesmo corpo UTF-8 e a geração do overlay. Outra escolha descarta o candidato.

Novo play, legenda desligada e troca/descarte de fonte invalidam o candidato por geração. A primeira definição de fonte, sem vídeo anterior, preserva o candidato iniciado durante a espera; `player_definir_fonte` só descarta quando `comVideo` já está ativo. Referências sob mutex liberam corpo e pedido mesmo quando a rede termina depois do descarte. A requisição descartada pode terminar até o prazo existente de 20 s; seu corpo não é publicado. O limite de uma tentativa por play só rearma num novo play.

Validação local com rede falsa:

- `tests/legpre.sh`: passou; início antes do vídeo, corpo pronto e fio em curso adotados sem segundo download, callback, descarte por título/fonte e escolha final diferente. O harness executa `player_definir_fonte` extraído do código real: a primeira fonte preserva corpo/fio e uma troca efetiva descarta. Contra o descarte incondicional, esse roteiro falhou com rc 134 (segunda requisição); com a guarda, passou.
- Mesmo roteiro contra `faixas.c` anterior: falhou aguardando o início do download antes do vídeo (timeout/assert), como esperado.
- `SANITIZE=1 bash tests/legpre.sh`: passou com ASan/UBSan.
- `tests/legenda2.sh`, `tests/autosync.sh` (572 verificações) e `tests/legsync.sh`: passaram.
- `tests/legmemoria.sh` e `tests/legmem_383.sh`: passaram.
- Sintaxe de `src/player.c` e `git diff --check`: passaram.

Sem teste em TV, publicação ou push. Nenhuma dependência nova. O ganho real de tempo depende da rede e será medido no aparelho; o harness prova a sobreposição e a ausência da segunda requisição.
