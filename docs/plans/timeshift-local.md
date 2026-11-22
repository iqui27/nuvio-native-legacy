# Timeshift local da TV ao vivo — etapa 1 (spike)

Origem: pesquisa de mercado, seção 3 (Sparkle TV). Pausar e voltar um canal
que o provedor não deixa voltar, gravando no pendrive enquanto se assiste.
Não confundir com o catch-up do provedor (`docs/plans/guia-tv/`), que continua
igual.

## O que a etapa 1 faz

- Ajuste **Pausar e voltar a TV ao vivo** (`timeshiftLocal`, local da TV,
  padrão **desligado**). Desligado, nada muda.
- Ligado e com pendrive gravável (o mesmo que o cache USB acha, `p2pmotor_usb_info`),
  o canal que passa pelo proxy HLS→TS (`src/proxyts.c`) também é gravado em
  `<pendrive>/nuvio-timeshift/ts-<n>.ts`, um arquivo por ~6 s.
- No player do canal: pausar, retomar de onde parou, **Voltar 30 s** e
  **Voltar ao vivo**. O player reabre a mesma sessão do proxy com `&atras=N`
  e o proxy serve do disco a partir do começo do arquivo que cobre o pedido.
- Orçamento (`src/timeshift_regras.h`): espaço livre − 512 MB de reserva, teto
  de 4 GB, mínimo de 256 MB; cheio, apaga o arquivo mais antigo. Erro de
  escrita desliga só a gravação. Trocar ou sair do canal apaga tudo.
- Em pausa a ingestão segue sem leitor por até 30 min.

## Verificado (Linux, sem TV)

| Teste | O que prova |
|---|---|
| `tests/timeshift_regras.sh` | orçamento, corte de segmentos, expurgo, posição N s atrás |
| `tests/timeshift_disco.sh` | anel em disco: 200 MB num teto de 64 MB, leitor lento, `/dev/full` |
| `tests/proxyts.sh` | gravação byte a byte igual ao ar, `&atras=N`, apagar ao parar, desligado = igual a antes |
| `tests/timeshift_e2e.sh` | HLS ao vivo do ffmpeg: assistir 12 s, pausar 15 s, retomar 15 s atrás, voltar ao vivo, sair |

Medidas do e2e (720p, 3 Mbit/s, disco em `/tmp`): 15 s de pausa = +6 MB em 3
arquivos; retomada 15 s atrás entrega 26 s de vídeo em 5 s de relógio; voltar
ao vivo entrega 8 s. O anel em `/tmp` escreve e lê na casa dos GB/s, o que
não diz nada sobre pendrive: `NUVIO_TS_DIR=/caminho bash tests/timeshift_disco.sh`
mede outro volume.

`bash tools/linux.sh --build` compila. No PC, `NUVIO_TIMESHIFT_DIR=/pasta`
substitui o pendrive.

## Não verificado / o que falta

- **Nenhuma TV física.** Não medido: velocidade de escrita em pendrive real
  (FAT32/exFAT/NTFS), desgaste, e como o player nativo de cada plataforma
  reage a reabrir a URL local (tempo de troca, quadro preto).
- A interface do player (botões, OSD com a janela) não foi exercitada com
  vídeo real nem por captura de tela; só compilada. O caminho de dados foi.
- Só canais que o proxy converte (HLS). TS contínuo segue por 302 e **não** é
  gravado; gravar exigiria o proxy ficar no meio desse caminho.
- Samsung `.wgt` (WASM), Android e Windows: sem proxy nativo ou sem a sonda de
  pendrive, o ajuste não tem efeito. Decidir por plataforma.
- O atraso é por arquivo (~6 s de grão), sem busca fina nem barra de tempo.
- Sem aviso na tela quando falta pendrive ou espaço (hoje só no log).
- A gravação não sobrevive à troca de canal (voltar ao canal anterior começa do zero).
