# P2P: janela de streaming (plano, #334)

## O que existe hoje (2.0.2)

- O nuvio-engine 0.1.3 (libtorrent 2.0.12) grava o arquivo escolhido INTEIRO em
  `<dados>/p2p/dados/<torrent>` (`posix_disk_io_constructor`, arquivo esparso).
  Nada do que ja tocou e apagado enquanto o player esta aberto.
- `disk_cache_capacity_bytes` do motor (nosso "mole") so despeja torrents
  INATIVOS inteiros (`DiskCacheManager::enforce`, LRU por pasta). O torrent
  tocando e protegido: a pasta cresce sem limite do lado do motor.
- O limite real e a nossa vigia (`p2pmotor.c`): pasta > teto duro = para.
  Teto duro: Automatico = min(1,5 GB, metade do livre); fixo (Ajustes, #334) =
  ate 2/4/8/16 GB deixando 512 MB livres.
- Pasta apagada ao parar o motor, ao subir de novo e (#334) no inicio do app.

Log do #334 (UE55AU7175, Tizen 6, 2.0.2): livre ao subir 1369-1379 MB, teto
duro 684-691 MB, a pasta cresce ~100-200 MB/min e o motor para em ~690 MB, cinco
vezes seguidas. Nenhuma sobra entre sessoes (livre igual a cada subida). Com
1,37 GB livres nenhum limite faz um 4K de varios GB caber: so a janela resolve.

## Janela: o que fazer no motor (C++, tools/p2p-motor)

Precisa de patch no nuvio-engine (fork no commit 02938d7); a API C nao expoe
pecas.

1. **Disk I/O proprio.** Trocar `posix_disk_io_constructor` por um
   `disk_interface` que embrulha o posix e conhece a janela. Ele:
   - escreve normalmente;
   - guarda quais pecas estao "despejadas" e, ao despejar, abre buraco no
     arquivo (`fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE)`; ext4 da
     Tizen/webOS/Android aceita; sem suporte = nao despeja e cai no teto);
   - em `async_read` de peca despejada devolve erro de leitura, nunca zeros
     (zeros iriam para o player e para os pares).
2. **Politica (no stream bridge, que ja sabe a posicao pedida pelo HTTP Range):**
   - janela = N MB atras do ultimo byte servido + o read-ahead que o
     `set_piece_deadline` ja pede; N = teto duro - cabeca - cauda;
   - protegidas para sempre: primeiros e ultimos ~8 MB do arquivo (cabecalho
     MKV/MP4, Cues/moov) e as pecas de borda com outros arquivos;
   - despeja a peca mais antiga ATRAS da posicao quando a pasta passa de 90%
     do teto; nunca a frente.
3. **Voltar para tras (seek para trecho despejado).** libtorrent 2.0 nao tem
   "desmarcar peca". Caminho: `save_resume_data`, `remove_torrent` (sem apagar
   arquivos), re-adicionar com `have_pieces` sem as despejadas. Custa alguns
   segundos de reconexao; o player ve espera, nao erro.
4. **Upload.** Pares pedindo peca despejada: o erro de leitura do item 1 faz o
   libtorrent recusar o pedido. Medir se isso nao derruba a sessao
   (`handle_disk_error` pausa o torrent em alguns erros: usar um codigo que ele
   trate como "nao tenho", ou tirar a peca do bitfield anunciado com
   `share_mode`/superseed off).
5. **API C nova (API_VERSION 4):** `config.streaming_window_bytes` (0 =
   comportamento de hoje). `p2pmotor_motor.c` passa o teto duro menos a
   reserva; a vigia continua como rede de seguranca.

## Quando nem a janela cabe

Janela minima util ~ 2x o read-ahead (+ cabeca/cauda), ~300 MB para 4K. Teto
duro abaixo disso: recusar antes de tocar, com o numero ("esta TV tem X livres,
o P2P precisa de 300 MB"), como ja faz o "Falta espaço livre" de 256 MB.

## Prova antes de publicar

- teste do motor (mac, tests/p2pmotor_real.sh) com teto de 200 MB num torrent
  de 2 GB tocando do inicio ao fim, e um seek para tras de 50%;
- TV Samsung 6+ com pouco livre (a do #334 e o caso).

## Feito (07/10, branch agente/203-334-janela)

O que mudou em relacao ao plano acima:

- **Sem remove/re-add.** libtorrent 2.0.12 nao tem "desmarcar peca" publico,
  mas tem `piece_picker::we_dont_have`. Um patch do libtorrent (o 9o do motor,
  `libtorrent-2.0.12-nuvio-drop-piece.patch`) expoe
  `torrent_handle::nuvio_drop_piece(p)`: tira a peca do picker, refaz o
  file_progress, rejeita pedidos dela e manda dont_have aos pares. Voltar para
  tras = a peca simplesmente falta, e o agendador a pede de novo.
- **Disco:** `WindowDiskIo` embrulha o `posix_disk_io` e anota as pecas
  escritas; leitura de peca apagada falha com `operation_aborted` (par:
  reject + dont_have, sem pausar o torrent; read_piece: erro para a ponte).
  Nunca zeros. Escrita nova tira a marca.
- **Politica (ponte):** bytes comprometidos (no disco + agendados) acima de
  85% da janela apagam as pecas mais longe de qualquer posicao ate 75%.
  Protegidas: 8 MB do inicio, 16 MB do fim, pecas de borda, pecas com leitor,
  o agendamento atual, 16-64 MB atras e o read-ahead a frente (que e
  dimensionado para caber). Arquivo menor que a janela: nada muda.
- **Buraco:** `fallocate64(PUNCH_HOLE|KEEP_SIZE)` (Linux/Android, 64 bits no
  armv7), `F_PUNCHHOLE` no Mac. Sistema sem suporte: a janela para e
  `stats.window_unsupported = 1` (o log da vigia diz); o teto duro segue.
- **API 4:** `config.streaming_window_bytes` (minimo 160 MB no motor),
  `stats.window_*`. Resume com a janela ligada perde o have-bitfield.
- **App:** janela = teto duro - 32 MB, sempre; arquivo maior que o teto com
  janela < 300 MB e recusado antes de tocar, com os numeros.
- **Fonte do motor:** `tools/p2p-motor/janela-334.patch` sobre o 02938d7,
  aplicado pelo `nv_p2p_fonte` (pasta.sh) em todos os build-*.sh. Mudou o
  motor: regerar o patch (`git diff 02938d7 HEAD` no clone).

Prova (Mac, `tests/streaming_window_test.cpp` do motor): arquivo de 320 MB,
web seed no proprio processo, janela de 160 MB, toca ate o fim e volta para
25%..50%: pico no disco 136-140 MB, 200 pecas apagadas, ~120 baixadas de
novo, 403 MB comparados byte a byte. Falta: TV (Samsung 6+ do #334).
A ajuda do limite já descreve a janela e a exceção FAT/exFAT (28 idiomas).
