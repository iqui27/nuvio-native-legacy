# Kits de Home importáveis por URL

Frente da pesquisa de mercado (seção 3, "Kits de Home importáveis por URL").
Um kit é um JSON público que acrescenta fileiras de pastas à Home, no mesmo
formato das coleções da conta. Primeiro recorte: importar, guardar, remover.

## O que está feito

- `src/homekit.c` (regra pura, `tests/homekit.sh`): lê e valida o JSON, monta
  `ColFolder`/`ColSource`, serializa só o que foi aceito.
- `src/homekitapp.c`: baixa em fio próprio, aplica na thread principal por
  `col_extra_definir_grupo`, guarda em `kit-home-p<N>.json` (por perfil, local,
  não sobe para a conta).
- Ajustes › Tela inicial › Coleções › **Kit de Home**: abre o teclado para o
  link. Enviar vazio remove o kit e as fileiras somem. Sem kit, nada muda.
- Exemplo válido: `tests/fixtures/kit-home.json`.
- No preview de mesa, `NUVIO_KIT_URL=<link>` importa sem passar pelo teclado.

## Formato

```json
{ "nuvioKit": 1, "name": "…", "author": "…",
  "collections": [ { "title": "…", "folders": [ {
      "title": "…", "tileShape": "POSTER|LANDSCAPE|SQUARE",
      "coverImageUrl": "https://…",
      "sources": [
        { "provider": "tmdb",  "tmdbSourceType": "NETWORK", "tmdbId": 213, "mediaType": "TV" },
        { "provider": "trakt", "traktListId": 123 },
        { "provider": "addon", "addonId": "com.linvo.cinemeta", "type": "movie", "catalogId": "top" }
      ] } ] } ] }
```

Uma lista de coleções na raiz (o export da conta) também é aceita.

## Regras de segurança

- O kit só acrescenta fileiras: não instala addon nem muda ajuste.
- Fonte de addon vale só pelo id de um addon **já instalado**; `addonBaseUrl`
  do kit é ignorado. Fonte de addon ausente é descartada e contada.
- Arte só por http(s). JSON truncado, com sobra ou de versão mais nova é
  recusado, e um kit recusado não troca o kit que já estava valendo.
- Tetos: 256 KB, 4 fileiras, 16 pastas, 8 fontes por pasta.

## Falta

- Prévia antes de aplicar (nome, autor, o que será ignorado) e aviso na tela
  do que ficou de fora; hoje o resumo vai só para o log.
- Vários kits ao mesmo tempo e atualização automática pelo link.
- Exportar a Home atual como kit; importar por QR/celular.
- Oferecer a instalação dos addons que o kit pede (decisão do dono).
- Galeria de kits (precisa de servidor).
- Medir em TV: nada desta frente foi testado em TV física.
