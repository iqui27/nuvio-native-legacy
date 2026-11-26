# Código de 6 dígitos para configurar a TV de outra pessoa (2.1)

Origem: pesquisa de mercado, seção 3 (dor nº 6). Estende o login por QR que já
existia (`src/login.c`, `src/sessao.c`); não troca nada dele.

## O que é

A tela de entrar mostra, à direita do QR, seis números e um endereço curto.
Quem está na frente da TV dita os números; quem ajuda, de qualquer lugar, abre o
endereço, digita os números e cai na mesma página de autorização que o QR abre.
A conta que entrar ali é a que fica na TV.

## Como funciona

- TV: `POST {NV_REC_URL}/v1/parear {url}` com o `web_url` do login → `{codigo, pagina, validade}`.
- Quem ajuda: `GET /c` (página) → `POST /v1/parear/abrir {codigo}` → `{url}` → redireciona.
- O Worker só guarda `codigo → endereço` por 10 minutos. Não autoriza nada: a
  autorização continua na página da conta, e a TV continua no polling de sempre.
- Regras puras em `src/parear.c` (teste `tests/parear.sh`); rede em
  `src/parearrede.c`; servidor em `servidor/recomendacoes/src/parear.js`
  (teste `servidor/recomendacoes/teste-parear.mjs`).

## Segurança

- O endereço guardado tem de começar por `PAREAR_BASES` (fronteira de caminho),
  então o código nunca leva a um site de terceiros.
- 10 tentativas de abrir por IP a cada 10 min; 20 códigos por IP por hora;
  código vale 10 min e a TV pede outro antes de vencer.
- Endereço e código não vão para o log do app.

## Desligar

- Ajustes → Conta → "Código de ajuda no login" (local, ligado por padrão).
- Sem `NV_REC_URL` no pacote, ou sem `PAREAR_BASES` no Worker (503), a tela é a
  de antes, sem erro.

## Para ligar em produção (depende do dono)

1. Aplicar `servidor/recomendacoes/migracao-021-parear.sql` no D1.
2. Definir `PAREAR_BASES` (o mesmo valor de `TV_LOGIN_WEB_BASE_URL`) e, se
   houver domínio curto, `PAREAR_PAGINA`; publicar o Worker.

## Verificado e não verificado

- Verificado no Linux: testes acima; build `tools/linux.sh`; app contra um
  servidor local (Worker real em Node + Supabase falso) mostrando o código e
  `/v1/parear/abrir` devolvendo o endereço do login.
- Não verificado: TV física, Worker publicado, página da conta real, builds
  webOS/Tizen/Android, tela em retrato (o bloco não aparece ali).

## Falta

- Em TVs onde `http://`/CORS passa por proxy (`src/rede.c`), conferir o POST.
- Retrato/toque: hoje sem o código.
- Reaproveitar o mesmo laço para vincular serviços (PLANO-ECOSSISTEMA §6).
