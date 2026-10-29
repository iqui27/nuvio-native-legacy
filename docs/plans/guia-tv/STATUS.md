# Guia de TV — estado do MVP (M1–M9)

Branch `feat/guia-tv` (worktree `/Volumes/ExternalSSD/nv-guia-tv`), com `agente/202-lgcanais` (b68bad96) mesclado. Nada em TV.

| # | Item | Estado |
|---|---|---|
| M1 | Modelo por fonte | Feito. `src/ltv.c` (arena de strings, chaves estáveis, prefs por fonte); `guia.c` com GCanal de ponteiros, fontes addon/Stalker/Xtream/M3U, grupos por fonte, cache por fonte `ltv-<chave>-p<N>.bin` + índice. Favoritos em `ltv-fav.txt` (chave+id), `guia-fav.txt` migrado na 1ª publicação. Lembretes acham o canal por id+base (sem mudar o arquivo). |
| M2 | Sem corte silencioso | Feito. Teto 30k webOS/Android, 20k .tpk, 8k .wgt, 5k em TV < 1,2 GB. Xtream: grupos inteiros na ordem do painel (`ltv_grupos_admitir`), escolha da pessoa por fonte respeitada (prefs `g=`); fonte marcada `cortada` e a barra diz "Mostrando N de M". Tela de marcar grupos feita (segurar OK na fonte ▸ Escolher grupos): caixas, contador "N de M canais" ao vivo, recusa o que estoura o teto, Aplicar grava `g=` e recarrega. |
| M3 | Barra de fontes | Feito. "★ Favoritos · Todas · fontes" com ponto de saúde e contagem, linha da fonte (canais, grupos, grade %, atualizada há, aviso de corte), filtro só da sessão, Todas por padrão, "Grupo · Fonte" em Todas. Segurar OK no chip: ordem dos grupos (provedor/A–Z), horário da grade, recarregar. |
| M4 | XMLTV em fluxo | Feito. `src/xmltv.c` (gzip/XML em pedaços, 1 elemento por vez), `rede_baixar_fluxo`, filtro por ids/nomes da lista, janela −48 h/+7 d, teto de memória por plataforma, cache binário `epg-x-<chave>.bin` (sem URL), cache velho quando a rede cai. |
| M5 | Casamento em cascata | Feito. id (sem caixa, fonte própria primeiro) → tvg-name → nome → apelido/regras (agora por índice, sem varrer a grade) → manual. Fuzzy só sugere. "Escolher grade deste canal" no segurar OK do canal. Cobertura por fonte no log. |
| M6 | Fuso + linha `[ltv]` | Feito. Fuso por fonte (±30 min, −12/+14 h) aplicado em agora/próximo/faixa; `[ltv] fonte=… canais=… grade=…% (id, nome, apelido, manual) fonte_grade=… idade=… fuso=…`. O OSD do player agora usa a mesma cascata e o fuso da fonte (`guia_programa_agora/proximo/faixa`), com o casamento por nome antigo só de reserva. |
| M7 | Teclas | Feito. Baixo já abria o mini guia (Azul continua). "0" na Live TV = último canal; no VOD continua proporção; botão "Último canal" no OSD (proporção já estava no OSD). Segurar OK no guia = opções (na faixa sobre o vídeo continua favorito). Digitar número feito: selo no canto do player ("1 0 1" + canal, "fonte · número" se repetido), 1,5 s ou OK confirma, Voltar cancela; `0` só entra com digitação em curso. |
| M8 | Diagnóstico | Feito. Escolhe sozinho até o máximo: fileira em foco + um canal por grupo da mesma fonte; "Aplicar" grava formato/modo/proxy na fonte, e o player usa o modo da fonte. Android: BEHIND_LIVE_WINDOW volta à borda ao vivo (até 3×/min). |
| M9 | M3U e XMLTV por URL | Parcial. M3U em fluxo (#EXTINF, tvg-*, group-title, tvg-chno, url-tvg, \|user-agent, EXTVLCOPT) entra como fonte com grade pelo url-tvg; XMLTV por URL entra no EPG para todas as fontes; lista por perfil `ltv-urls-p<N>.txt`. Cadastro em Ajustes ▸ TV ao vivo ▸ "Listas e guias por URL" (escolher, adicionar M3U/XMLTV com teclado da TV ou celular, remover; só o host aparece) e canal M3U toca em app.c com URL/User-Agent/Referer guardados (`resolverCanalM3u`). |

## Testes
- `tests/ltv.sh` — arena, chaves, teto, grupos, numeração (decisão 7), dígitos, favoritos, prefs, M3U em fluxo (20 mil canais), URLs.
- `tests/guiaepg.sh` — regras antigas + XMLTV de 100 MB em fluxo filtrado, cascata, sugestão, cache binário, rede fora com cache velho.
- `tests/xtream.sh`, `tests/aovivo.sh`, `tests/spotlight_canais.sh` (addon falso, rede + cache) — passam.
- Capturas (`tests/guia_shot.sh`): `-fontes.bmp` (barra com Xtream cortado/âmbar), `-grade-manual.bmp`, `-faixa.bmp` (mini guia).

## Não verificado
- Nada em TV. Memória real de 30k canais / XMLTV de 100 MB numa C9: só conta e teste no Mac.
- Nada disto em TV: Ajustes ▸ URLs (M9), tela de grupos (M2), digitar número (M7), reprodução de canal M3U real (só o contrato de id/URL/UA testado), OSD com fuso.
- Capturas: `tests/guia_shot.sh` gera `-grupos.png` e `-numero.png`; `NUVIO_SHOT_URLS=1 NUVIO_AJ_QUADROS="op:urlLista=1" bash tests/ajustes_shot.sh` gera a linha de Ajustes.
