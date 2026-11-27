# Marcar cena e mandar para um amigo (pesquisa 4.7)

## Etapa 1 (entregue)

- Tecla amarela do controle (ou `M` no teclado) no player de filme/episódio guarda o instante.
  Ajuste "Marcar cena no player" (local, ligado por padrão) desliga a tecla.
- As marcas ficam por perfil em `cenas_<perfil>.txt` (até 200; marcas a menos de 5 s se fundem).
- Apertar de novo em até 8 s abre a escolha de amigo e envia a cena pelo serviço de
  recomendações que já existe, como texto livre (`modelo -1`): `cena 0h12m34s t1 e3 d3600`.
  O formato só usa `[a-z0-9 ]` e cabe em 60 caracteres, então passa pela normalização do
  servidor sem mudança de servidor.
- Quem recebe vê "Cena marcada em 12:34" no cartão; abrir o cartão e dar Play começa naquele
  segundo. Clientes antigos mostram o texto cru.
- `nuvio-legacy://play/...?t=<segundos>` abre no instante (só com id IMDb).
- Risco de corte/versão: a marca leva a duração da origem; se a fonte local difere mais de 3%,
  o player avisa que a cena pode estar um pouco antes ou depois.
- Regra pura em `src/marcacena.c` (`tests/marcacena.sh`); cola em `src/cenas.c` (`tests/cenas.sh`).

## Falta

- Tela para ver e reabrir as próprias marcas (hoje só `cenas_n`/`cenas_item`), e entrada no Diário:
  o Diário guarda sessões assistidas, não instantes; a tela de mês dele ainda está pendente.
- Campo próprio de instante no servidor de recomendações (hoje viaja no texto livre).
- Mapa de cenas agregado por título (precisa de servidor e de cuidado com spoiler).
- Ancorar por legenda em vez de só avisar quando a duração difere.
- Conferir a tecla amarela nas TVs (webOS, Tizen, Android TV): não foi testada em aparelho.
- No Linux não há manipulador de link do sistema; `NUVIO_DEEPLINK=<url>` injeta um link ao iniciar.
