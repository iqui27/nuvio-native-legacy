# Nuvio Casa — primeira etapa (spike)

Relatório de mercado, seção 4.4: um aparelho mais forte da casa ajuda a TV
fraca pela rede local, sem instalar servidor. O dono aprovou a ideia apesar do
veto a "proxy universal de mídia" em `docs/issues/PLANO-MERCADO.md`. Esta etapa
fica dentro do que o veto protege: o ajudante **não busca URL nenhuma a pedido**;
ele só entrega o que o motor P2P dele mesmo baixou.

## O que esta etapa faz

Um caso de ajuda, de ponta a ponta: **P2P pela casa**.

- Aparelho cujo pacote **não** traz o motor P2P (`p2pmotor_disponivel() == 0`),
  com *P2P ligado* e sem *Endereço do servidor P2P*: procura um ajudante na rede.
- Aparelho cujo pacote traz o motor (`-DNV_P2P_MOTOR`: hoje `tools/arm.sh`, o
  CMake do Android e o `.tpk` com `NUVIO_P2P_MOTOR=1`), com *P2P ligado* e
  **Ajudar outras TVs da rede** ligado: responde à procura e toca o torrent
  para a outra TV. O binário de desenvolvimento do Linux não tem motor, então
  nele só o lado cliente fica ativo.
- Na TV, o torrent sem link passa a ser escolhível na lista de fontes, como já
  era com um servidor Stremio digitado. Nada mais muda na tela.

```
TV (p2p.c)                               ajudante (casa_rede.c)
  UDP broadcast  "NUVIO-CASA/1 ?"  ───────────▶  porta 47470
  ◀──────  "NUVIO-CASA/1 ! porta= token= cap=p2p nome="
  GET  /<token>/settings           ───────────▶  é um ajudante?
  POST /<token>/<hash>/create?idx=&s=&e= ─────▶  p2pmotor_resolver (bloqueia)
  GET  /<token>/<hash>/0  (Range)  ───────────▶  repasse do 127.0.0.1 do motor
```

O contrato HTTP é o do servidor Stremio que `p2p.c` já falava; por isso o
cliente mudou pouco (de onde vem a base e o `?idx=&s=&e=` no create).

## Ajustes (append-only, locais, em Reprodução › P2P)

| Ajuste | Padrão | Efeito |
|---|---|---|
| Usar um ajudante Nuvio da rede (`casaUsarLocal`) | Ligado | Só age com P2P ligado, sem motor e sem endereço. Desligado: nenhum pacote sai. |
| Ajudar outras TVs da rede (`casaAjudarLocal`) | **Desligado** | Abre UDP 47470 e HTTP 47471 na rede local. |

O P2P segue desligado de fábrica, então ninguém ganha comportamento novo sem
ter ligado o P2P antes.

## Quem entra no ajudante

Origem em rede privada; `Host` IPv4 literal de rede privada com a porta certa
(DNS rebinding); sem cabeçalho `Origin` (página de navegador); token sorteado a
cada subida no caminho; só três rotas; repasse só para `http://127.0.0.1:<porta>`
do motor. Um pedido por vez; o motor em uso pelo dono do aparelho recusa (503).
O token é entregue a quem perguntar na rede: ele **não** separa pessoas da mesma
rede. Quem está no Wi-Fi da casa pode mandar o ajudante baixar um torrent.

## Medido (Linux, `bash tests/casa.sh`)

- Regra pura (`tests/casa.c`): descoberta, escolha, rotas, Host, corpo, URL do motor.
- Ponta a ponta (`tests/casa_ponta.c`): `p2p.c` de verdade → UDP → `casa_rede.c`
  de verdade → motor **falso** (HTTP em 127.0.0.1 com Range). O broadcast saiu
  pela placa da máquina e a resposta voltou pelo IP dela.
- Repasse em loopback, um fluxo, 48 MB: cerca de 1,7 GB/s (teto do código, não da rede).
- Resolver (settings + create + 64 KB) com motor falso: 1 ms.
- `SANITIZE=1` (ASan/UBSan) limpo.

## Não medido

- Nenhuma TV. Nem LG, nem Samsung, nem Android.
- Torrent de verdade pelo ajudante (o teste usa motor falso).
- Dois aparelhos diferentes: Wi-Fi com isolamento de clientes derruba o
  broadcast; Android pode filtrar broadcast sem `MulticastLock`.
- Firewall do Mac/Windows pedindo permissão para a porta de entrada.

## O que falta

1. Provar o ajudante num aparelho de verdade com motor (Android TV, LG): porta
   de entrada aceita pelo sistema? No Android precisa de serviço em primeiro
   plano; hoje o ajudante só vale com o app aberto. Um ajudante de PC
   (Linux/Mac/Windows com motor) ainda não tem pacote.
2. Pareamento: confirmar na tela do ajudante a primeira TV que pede, e guardar.
3. Conviver com o player do próprio ajudante (hoje um pedido local troca o
   torrent do motor e o da casa cai).
4. Mostrar na lista de fontes e no player que o vídeo vem do ajudante, e o nome dele.
5. Os outros casos da seção 4.4: DTS/TrueHD → AC-3, Dolby Vision em MKV, cache
   em disco, legenda. Cada um é uma rota nova com a própria regra de origem.
6. Samsung .wgt: o navegador não manda UDP; precisaria de endereço digitado.
