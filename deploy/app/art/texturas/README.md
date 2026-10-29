# texturas/ — materiais CC0 do tema "Textura"

Substitutos do recorte da arte quando ele e FRACO (chapado, sem material —
recorte com p90 - p10 de luminancia abaixo de 0,12, ver `fraca` em corviva.c).
Escolhidos pelo matiz do acento: `material-<graus>.jpg`, faixas de 30 em 30
graus (000 = vermelho... 330 = rosa); `material-neutro.jpg` para acento sem
cor. Sem rede em runtime: estes JPG viajam no pacote.

- **Fonte:** ambientCG "Paper002" (displacement), https://ambientcg.com/view/Paper002
- **Licenca:** CC0 1.0 Universal — ver ../licencas/texturas-cc0.txt
- **Geracao:** `tools/build-texturas.py <zip-extraido>/Paper002_1K-JPG_Displacement.jpg`
  (Precisa de Pillow so no build.) Percentis medidos do arquivo final:
  `NV_TX_ASSET_P10 0.1861 / NV_TX_ASSET_P90 0.6935` — se regenerar, atualize
  as constantes em src/ajustes.c.
