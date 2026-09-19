# Canal de avisos (`avisos.json`)

O app lê `avisos.json` deste repositório (branch `master`, via
raw.githubusercontent.com) a cada 30 minutos com o app aberto e mostra cada
entrada na central de avisos (toast + lista; AZUL na LG, CH+ na Samsung).
Serve para avisar de defeito conhecido enquanto não está consertado, ou de
qualquer coisa que valha dizer a quem está na frente da TV.

```json
[
  {
    "id": "2026-09-19-guia-lento",
    "desde": "2026-09-19",
    "ate": "2026-10-01",
    "plataforma": "todas",
    "ate_versao": "1.3.1",
    "titulo": "Guia de TV demorando",
    "titulo_en": "TV Guide slow to open",
    "texto": "Na 1.3.1 o guia espera a rede a cada abertura. A 1.3.2 corrige.",
    "texto_en": "On 1.3.1 the guide waits for the network on every open. 1.3.2 fixes it."
  }
]
```

- `id`: único e estável. É a marca de "já vi": trocar o id faz o aviso
  reaparecer para todo mundo.
- `desde` / `ate`: datas ISO; fora do intervalo o aviso não aparece. Podem
  faltar.
- `plataforma`: `todas`, `lg` ou `tizen`.
- `ate_versao`: só aparece para quem está nessa versão ou em uma mais antiga.
  Pode faltar.
- `titulo_en` / `texto_en`: opcionais; sem eles a interface em inglês mostra
  o português.

Um aviso removido do arquivo some da lista na próxima leitura.
