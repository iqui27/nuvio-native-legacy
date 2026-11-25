# Roteiro de Teste Físico em TV — Farol (`lote/21g` + `farol-56`)

Documento de verificação em aparelho físico para o lote completo do Farol (**25 issues do `lote/21g`** somadas ao fechamento do **`farol-56`**), cobrindo o fluxo do gosto, as ações de diagnóstico, o assistente de escolha, o servidor de falhas controladas e os novos contadores locais.

---

## 1. Preparação de bancada e servidor de falhas

Suba o servidor local de falhas (`tests/farol_falhas_server.py`) na mesma LAN da TV e aponte uma fonte/addon de teste para o IP da máquina de bancada:

```bash
python3 tests/farol_falhas_server.py --port 8765
```

Cobertura obrigatória do servidor de falhas controladas antes de liberar a build:
- **HTTP 403 / hotlink expirado**: validar detecção sem atribuir abandono ao gosto do perfil.
- **Sem `Accept-Ranges` (`200` inteiro, sem `206`)**: validar detecção de *seek* / barra sem corromper posição nem contar como "terminou".
- **Banda limitada a 8 Mbps**: medir comportamento de *buffer* / veredito de rede e oferta de ação de redução de qualidade / fonte menor.
- **Add-on lento (atraso de 12 s na resposta)**: validar indicação de lentidão de add-on e opção de quarentena temporária (#17).
- **Debrid HTTP 401 (token expirado/revogado)**: validar classificação de debrid desconectado e ausência de crédito negativo de gosto.
- **MKV truncado (EOF precoce no meio do stream)**: validar diagnóstico de falha de fonte sem marcar abandono real no modelo.
- **DTS na LG webOS C9**: forçar faixa DTS e confirmar que o player acusa recusa/ausência de suporte sem falso positivo em `abandono`, sem incrementar `recusas`/`terminadas` no gosto e mantendo o áudio alternativo / Stereo PCM ou aviso consistente (#16).

---

## 2. Roteiro dos 9 passos do gosto (PLANO-GOSTO)

Execute sequencialmente em uma TV limpa (pasta de dados nova ou após **Zerar o gosto**):

1. **Perfil novo (frio 0)**: criar/selecionar perfil sem histórico; confirmar que nenhuma fileira `farol_gosto*` sobe na Home (somente assistência de partida / Trakt).
2. **Importação Trakt (≤ 3 min)**: vincular Trakt na conta/perfil, sincronizar biblioteca/vistos/avaliações e cronometrar até aparecerem as fileiras personalizadas do Farol e a seção "O que o Farol aprendeu" preenchida (< 3 min).
3. **Concluir filme + reação positiva**: abrir filme recomendado da fileira do Farol, assistir até ≥ 90 % (`Terminadas` +1, motivo do gosto coerente) e marcar positivo/nota alta; verificar reforço de gênero/pessoa/tema em "O que o Farol aprendeu".
4. **Abandono aos 5 min vs. falha de fonte (`tests/farol_falhas_server.py`)**:
   - Caso A (abandono real aos ~5 min sem falha técnica): o sinal negativo pesa no modelo do perfil após a janela de saída.
   - Caso B (queda via `tests/farol_falhas_server.py`, ex. 403 ou MKV truncado aos 5 min): o erro vai para a Ilha / Diagnóstico (#14/#19/#21/#33), **não** conta como abandono nem altera o gosto.
5. **"Não é para mim" + Desfazer**: abrir menu de contexto em cartaz do Farol → "Não é para mim" → conferir saída imediata da fileira e incremento de `Recusas` (+1); tocar **Desfazer** no toast → título retorna à fileira, sai da lista de recusados e `Recusas` volta ao valor anterior (#51).
6. **Série a meio + fatos de qualidade (P6)**: abrir série em andamento pelo catálogo/detalhe; confirmar exibição do progresso, retomada de episódio/contexto sem *spoiler* (#57) e linha técnica de qualidade/duração/idioma/compatibilidade da TV (#46).
7. **Assistente por D-pad (`Achar o que ver`) → Comparar → Play**: abrir assistente (#50/#54), responder às perguntas só com D-pad (ou ditado/teclado/QR conforme plataforma), entrar em **Comparar com…** (#51), iniciar Play e validar retorno de foco ao fechar o player.
8. **Modo offline**: desligar Wi-Fi/cabo na TV; conferir composição 100 % local com títulos de modelo (zero travamento da UI, zero tela congelada, incremento em `Cenas montadas na TV`).
9. **Desempenho e memória na LG C9 (30 s scroll @ 60 fps + RAM ≤ base + 1 MB)**: rolar continuamente Home + fileiras Farol por 30 s na LG C9 com medição ativa; frame time estável (60 fps sem engasgo em transição de motivo/ponto de luz) e delta de RAM em relação ao build base `integracao/2.1` ≤ 1 MB.

---

## 3. Verificação dos novos contadores locais e vaga de exploração (`farol-56`)

1. Abrir **Ajustes → Sobre e ajuda → Gosto → O que o Farol aprendeu** e rolar via D-pad até a seção **"Uso do Farol nesta TV"**:
   - Confirmar saída/retorno no botão **Voltar** sem beco de foco.
   - Em perfil virgem, confirmar estado vazio `"Nada contado ainda"` e nota `"Fica só nesta TV. Zerar o gosto apaga."`.
2. Na Home, exibir a fileira do Farol (`Fileiras vistas` +1), abrir OK num cartaz (`Abertas` +1), iniciar Play a partir dele (`Tocadas` +1), concluir ≥ 90 % (`Terminadas` +1), recusar um item (`Recusas` +1) e conferir `Cenas montadas na TV` vs. `Cenas montadas no servidor` (quando `"Só nesta TV"` desligado e LLM habilitado).
3. Desligar **"Recomendar nesta TV"**: validar que os contadores **param** de subir; relugar / trocar de perfil: confirmar isolamento estrito entre perfis; fazer logout: confirmar limpeza completa; em perfil infantil: nada conta e nada é mostrado; sob trava parental: **Zerar o gosto** e recusa exigem desbloqueio.
4. Após ≥ 10 exposições da vaga **"Fora do seu costume"** sem nenhuma abertura, validar recuo para a última posição elegível (posição 8) e alternância em 1 de cada 2 fileiras; após aberturas/toques acima da taxa normal, validar promoção para a posição 5.

---

## 4. Checklist consolidado por plataforma — 25 issues do `lote/21g`

Referência das issues integradas em `lote/21g` (`#14, #15, #16, #17, #18, #19, #20, #21, #22, #23, #25, #27, #31, #32, #33, #34, #48, #49, #50, #51, #52, #53, #54, #55, #57`):

### A. LG webOS (C9 / CX)
- [ ] **Diagnóstico, anel, perfil e envio (`#14, #18, #19, #20, #21, #22, #23, #25, #27, #33`)**: captura de erro de fonte/vídeo sem bloquear quadro; redator sem título/perfil/URL; fila offline reenvia ao voltar rede; cartão de erro com ação primária e verificação de conserto (`#33`); política de silêncio em **Ajustes → Ajuda → Avisos do Farol** (`#27`) e botão Voltar respeitando camada ativa (`#20`).
- [ ] **Ações de player, áudio e rede (`#15, #16, #17, #34`)**: recusa de DTS na C9 sem corromper o gosto e queda limpa para Estéreo/faixa compatível (`#16`); failover sem loop na fonte abandonada (`#15`); isolamento de addon em quarentena 24 h (`#17`); cache/buffer/SDR quando HDR falha (`#34`).
- [ ] **Gosto na TV, cartões P2/P3/P6, consentimento e compositor (`#48, #49, #50, #51, #52, #53, #57`)**: fileiras `farol_gosto1`/`farol_gosto2` e cartão de dica P2 sem TOCTOU (`#48`); P3+P6 no detalhe (`#49`) e retomada sem spoiler (`#57`); consentimento `"Só nesta TV"` ligado por padrão (`#53`) e compositor remoto LLM (`#52`) só ativo após aceite explícito em perfil adulto.
- [ ] **Entrada de frase P5 via QR no celular (`#32, #55`)**: na C9/CX sem mic dedicado, abrir Assistente por frase → escanear QR no celular → enviar frase curta → respostas fechadas no assistente sem congelar e sem vazar URL/identidade.

### B. Samsung Tizen (`.tpk` e `.wgt`)
- [ ] **Pacotes `.tpk` e `.wgt` (`#14–#25, #27, #33, #34`)**: validar áudio Estéreo AAC no TPK (`#16`), pipeline AVPlay sem *crash* sob troca rápida de canal/VOD, ausência de `_Thread_local` no loader ELF do TPK40 e comportamento do FarolRegistro após queda de segmento.
- [ ] **Teclado IME, microfone e QR no Tizen (`#32, #55`)**:
  - `.tpk` / `.wgt` com teclado IME da TV: preservação do limite de 200 codepoints UTF-8, sem duplicação de foco ao fechar IME.
  - Entrada via microfone do controle Samsung (quando disponível) e fallback para QR celular (`#32`, `#55`): abertura fluida da UI P5 sem travar no foco.
- [ ] **Gosto, ações e privacidade (`#48–#54, #57`)**: fileiras na Home, menu "Mais assim" / "Não é para mim" + Desfazer, "Comparar com…", seção "Uso do Farol nesta TV" em **O que o Farol aprendeu**, e perfil infantil 100 % isolado.

### C. Android TV (TCL / Nvidia Shield)
- [ ] **Ditado por voz e teclado IME (`#31, #54, #50`)**: no Assistente (`Achar o que ver` / P5), acionar botão de microfone -> estado `"Ouvindo…"` + parciais -> cancelar com Voltar garante **0 POSTs** de rede; enviar fala real -> validação estrita LLM/regras -> resultados e playback (`#31`, `#54`); categoria `"Mistério"` mapeando Tensão/Intenso corretamente.
- [ ] **Diagnóstico Android (`#14, #15, #17–#23, #25, #33, #34`)**: verificação de erro MediaCodec/ExoPlayer no cartão de erro (`#21`), reabertura sem perda de foco, e captura de `FarolRegistro` sanitizada (sem token/URL/título/perfil no corpo).
- [ ] **Controles parentais, perfis e contagem local (`#51, #53, #56`)**: alternância rápida entre perfil adulto, segundo perfil adulto e perfil infantil (nada contado/mostrado em infantil; Zerar gosto protegido sob trava parental).
