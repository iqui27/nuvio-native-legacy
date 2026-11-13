# Fechamento — importação Matis94 na integração TV

Pedido: “vamos trazer o que falta do fork pra nossa integração” / “continua até integrar tudo”. Oito lotes A–H foram adaptados, revisados e integrados. Integração de código encerrada; não houve release, push, instalação ou escrita em conta. Os 59 itens continuam sendo linhas de comparação das 11 releases contra o snapshot inicial, não porcentagem de produto nem contagem de issues.

## Cobertura auditável

[STATUS.md](STATUS.md) mantém exatamente 59 IDs únicos: R01–R26, F01–F20 e D01–D13. Classificações iniciais foram preservadas: 13 presentes, 27 parciais, 17 ausentes e dois não comprovados.

- Treze linhas preexistentes foram mantidas: R01/R06/R09/R10/R12/R17/R21, F01/F07/F12 e D01/D04/D10.
- Quarenta e três linhas estão associadas aos lotes adaptados A–H; evidência e limitações constam por linha e por lote. Isto não significa que cada uma atingiu paridade integral: R20 permanece parcial por rota DV tardia; F16 tem P2 de restauração de faixa recuperada; demais P2 e limites físicos continuam registrados.
- F13 preserva diferença intencional: modo manual exige escolha/consentimento explícito, sem abrir fonte automaticamente por matching da release.
- F05 continua sem resultado de fluidez/FPS em TV com mais de 2.000 fontes. D13 continua sem prova final ARM/ABI/IPK/P2P. São resultados não demonstrados, sem conversão de teste host em prova física.

P7/mapping e DV MP4 permanecem opt-in/OFF; visual e preferências existentes preservados. Exceção autorizada de default: conversão Standard(1) apenas ausente/reset. Proteção obrigatória de RAM não pode ser desativada. Controles novos usam componentes existentes; não foi criada nova tela/prancha.

## Evidência por lote

| Lote | Merge | Gate | Pós-merge |
|---|---|---|---|
| [A](lote-a-integracao.md) | c0abc4a28 | 5/5 | 5/5 |
| [B](lote-b-integracao.md) | a521dd91f | 6/6 | 3/3 |
| [C](lote-c-integracao.md) | 0cefb7bc4 | 13/13 + complemento 12/12 | 5/5 |
| [D](lote-d-integracao.md) | fe82fe5e4 | 12/12 | 3/3 |
| [E](lote-e-integracao.md) | 1ac01b8b5 | 6/6 e 12 checks host | 3/3 |
| [F](lote-f-integracao.md) | 26eec9dbb | 8/8 | 3/3 |
| [G](lote-g-integracao.md) | 6aa346ec3 | 8/8 | 3/3 |
| [H](lote-h-integracao.md) | 8a16234bc | 15/15 | 3/3 |

Cada lote teve revisão e verificador distintos. Somar contagens não representa testes únicos: há sobreposição entre suítes. Apenas P0/P1 confirmados bloquearam; P2 permaneceram documentados, sem corte de escopo.

## O que esta entrega não prova

Host usa FFmpeg 9 e não prova FFmpeg 7.1.5 fixado, ABI ARM, áudio nativo da TV/soundbar, continuidade real de imagem, GREEN físico, QR em scanner/TV, equivalência visual ou FPS. `hevc_annexb_engine.sh` continua rc 1 por controle negativo não reproduzido (`bad=0`), sem pass ou waiver. SDK local tem biblioteca FFmpeg, mas falta manifesto `nuvio-audio-config` exigido pelo guard: nenhum pacote válido foi afirmado.

Código do serviço de logs de G foi integrado e testado offline; sem provisionamento/deploy/upload real, link público de log não é serviço operacional demonstrado. Hooks documentais preexistentes atualizam painel LAN e roadmap; isso não publica build do app.

Nenhuma issue fechada, nenhum push/release/build instalado, nenhum dado de pessoa alterado, nenhuma conta real ou de teste escrita. Aprovação de código não estabelece aceite de issue ou prova física.
